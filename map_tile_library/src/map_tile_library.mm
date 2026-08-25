#include "map_tile_library.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
using namespace std;

namespace
{
struct TileEntry
{
    string theme_name;
    string class_name;
    string relative_path;
};

using TileKey = tuple<int, int, int, int>;
using TileCatalog = map<TileKey, TileEntry>;

vector<string> split_tab(const string& line)
{
    vector<string> fields;
    string field;
    istringstream stream(line);
    while (getline(stream, field, '\t'))
    {
        fields.push_back(field);
    }
    return fields;
}

TileCatalog load_catalog(const string& library_root)
{
    const string file_path = library_root + "/data/catalogue.tsv";
    ifstream file(file_path);
    if (!file)
    {
        throw runtime_error("Cannot open tile catalogue: " + file_path);
    }

    TileCatalog catalog;
    string line;
    getline(file, line); // column headings
    while (getline(file, line))
    {
        const vector<string> fields = split_tab(line);
        if (fields.size() < 8)
        {
            throw runtime_error("Malformed tile catalogue row: " + line);
        }

        const int theme = stoi(fields[0]);
        const int tile_class = stoi(fields[2]);
        const int subclass = stoi(fields[4]);
        const int orientation = stoi(fields[5]);
        catalog.emplace(
            TileKey{theme, tile_class, subclass, orientation},
            TileEntry{fields[1], fields[3], fields[7]}
        );
    }
    return catalog;
}

TileCatalog::const_iterator resolve_tile(const TileCatalog& catalog, const TileKey& requested)
{
    const auto exact = catalog.find(requested);
    if (exact != catalog.end())
    {
        return exact;
    }

    // A singleton visual subclass has no meaningful orientation distinction.
    // Let callers use any orientation code (for example 1..4) and receive its
    // sole tile. Multi-orientation subclasses stay strict.
    const int theme = get<0>(requested);
    const int tile_class = get<1>(requested);
    const int subclass = get<2>(requested);
    const auto first = catalog.lower_bound(TileKey{theme, tile_class, subclass, 0});
    if (first == catalog.end() || get<0>(first->first) != theme ||
        get<1>(first->first) != tile_class || get<2>(first->first) != subclass)
    {
        return catalog.end();
    }
    const auto second = next(first);
    if (second == catalog.end() || get<0>(second->first) != theme ||
        get<1>(second->first) != tile_class || get<2>(second->first) != subclass)
    {
        return first;
    }
    return catalog.end();
}

void validate_grid(const fe_tiles::IntGrid& grid, int rows, int columns,
                   const char* name)
{
    if (static_cast<int>(grid.size()) != rows)
    {
        throw invalid_argument(string(name) + " has the wrong row count.");
    }
    for (const vector<int>& row : grid)
    {
        if (static_cast<int>(row.size()) != columns)
        {
            throw invalid_argument(string(name) + " has the wrong column count.");
        }
    }
}

CFURLRef file_url(const string& path)
{
    return CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(path.data()),
        static_cast<CFIndex>(path.size()),
        false
    );
}

CGImageRef load_png(const string& path)
{
    CFURLRef url = file_url(path);
    if (url == nullptr)
    {
        throw runtime_error("Cannot create URL for tile: " + path);
    }
    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (source == nullptr)
    {
        throw runtime_error("Cannot read tile PNG: " + path);
    }
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (image == nullptr)
    {
        throw runtime_error("Cannot decode tile PNG: " + path);
    }
    return image;
}

CGImageRef image_from_rgba(const vector<unsigned char>& rgba, int width, int height)
{
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    const CGBitmapInfo format = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big;
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        nullptr, rgba.data(), rgba.size(), nullptr
    );
    CGImageRef image = CGImageCreate(
        width, height, 8, 32, width * 4, colors, format, provider,
        nullptr, false, kCGRenderingIntentDefault
    );
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colors);
    if (image == nullptr)
    {
        throw runtime_error("Could not construct an image from the tile canvas.");
    }
    return image;
}

void write_png_rgba(const string& path, const vector<unsigned char>& rgba,
                    int width, int height)
{
    CGImageRef image = image_from_rgba(rgba, width, height);
    CFURLRef url = file_url(path);
    if (url == nullptr)
    {
        CGImageRelease(image);
        throw runtime_error("Cannot create output URL: " + path);
    }
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
        url, CFSTR("public.png"), 1, nullptr
    );
    CFRelease(url);
    if (destination == nullptr)
    {
        CGImageRelease(image);
        throw runtime_error("Cannot open PNG output: " + path);
    }
    CGImageDestinationAddImage(destination, image, nullptr);
    const bool wrote = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CGImageRelease(image);
    if (!wrote)
    {
        throw runtime_error("Could not write PNG: " + path);
    }
}
}

namespace fe_tiles
{
TileCanvas::TileCanvas(int rows, int columns, int source_tile_pixels)
    : rows_(rows), columns_(columns), tile_pixels_(source_tile_pixels)
{
    if (rows_ <= 0 || columns_ <= 0 || tile_pixels_ <= 0)
    {
        throw invalid_argument("TileCanvas dimensions and tile size must be positive.");
    }
    rgba_.assign(static_cast<size_t>(rows_) * columns_ * tile_pixels_ * tile_pixels_ * 4, 0);
}

int TileCanvas::rows() const { return rows_; }
int TileCanvas::columns() const { return columns_; }
int TileCanvas::tile_pixels() const { return tile_pixels_; }
int TileCanvas::pixel_width() const { return columns_ * tile_pixels_; }
int TileCanvas::pixel_height() const { return rows_ * tile_pixels_; }
const vector<unsigned char>& TileCanvas::rgba() const { return rgba_; }

void TileCanvas::draw(int theme, const IntGrid& class_layer,
                      const IntGrid& tile_layer, const string& library_root)
{
    validate_grid(class_layer, rows_, columns_, "class_layer");
    validate_grid(tile_layer, rows_, columns_, "tile_layer");

    const TileCatalog catalog = load_catalog(library_root);
    const int width_pixels = columns_ * tile_pixels_;
    const int height_pixels = rows_ * tile_pixels_;
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    const CGBitmapInfo format = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big;
    CGContextRef context = CGBitmapContextCreate(
        rgba_.data(), width_pixels, height_pixels, 8, width_pixels * 4,
        colors, format
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        throw runtime_error("Could not create the tile canvas drawing context.");
    }

    CGContextSetInterpolationQuality(context, kCGInterpolationNone);

    try
    {
        for (int y = 0; y < rows_; ++y)
        {
            for (int x = 0; x < columns_; ++x)
            {
                const int tile_class = class_layer[y][x];
                if (tile_class == EMPTY)
                {
                    continue;
                }
                if (tile_class < EMPTY)
                {
                    throw invalid_argument("Tile class cannot be negative.");
                }
                const int tile_code = tile_layer[y][x];
                if (tile_code < 0)
                {
                    throw invalid_argument("Packed subclass/orientation code cannot be negative.");
                }
                const int subclass = subclass_from_code(tile_code);
                const int orientation = orientation_from_code(tile_code);

                const TileKey key{theme, tile_class, subclass, orientation};
                const auto entry = resolve_tile(catalog, key);
                if (entry == catalog.end())
                {
                    ostringstream error;
                    error << "No tile exists for theme=" << theme
                          << ", class=" << tile_class << ", subclass=" << subclass
                          << ", orientation=" << orientation
                          << " at grid {x=" << x << ", y=" << y << "}.";
                    throw out_of_range(error.str());
                }

                CGImageRef tile = load_png(library_root + "/" + entry->second.relative_path);
                const CGRect destination = CGRectMake(
                    x * tile_pixels_,
                    height_pixels - (y + 1) * tile_pixels_,
                    tile_pixels_, tile_pixels_
                );
                CGContextDrawImage(context, destination, tile);
                CGImageRelease(tile);
            }
        }
    }
    catch (...)
    {
        CGContextRelease(context);
        throw;
    }
    CGContextRelease(context);
}

void TileCanvas::write_png(const string& output_png, int scale) const
{
    if (scale <= 0)
    {
        throw invalid_argument("PNG scale must be positive.");
    }

    const int source_width = columns_ * tile_pixels_;
    const int source_height = rows_ * tile_pixels_;
    if (scale == 1)
    {
        write_png_rgba(output_png, rgba_, source_width, source_height);
        return;
    }

    const int output_width = source_width * scale;
    const int output_height = source_height * scale;
    vector<unsigned char> scaled(
        static_cast<size_t>(output_width) * output_height * 4
    );
    for (int y = 0; y < source_height; ++y)
    {
        for (int x = 0; x < source_width; ++x)
        {
            const size_t source = (static_cast<size_t>(y) * source_width + x) * 4;
            for (int yy = 0; yy < scale; ++yy)
            {
                for (int xx = 0; xx < scale; ++xx)
                {
                    const size_t destination =
                        (static_cast<size_t>(y * scale + yy) * output_width + x * scale + xx) * 4;
                    copy_n(rgba_.begin() + source, 4, scaled.begin() + destination);
                }
            }
        }
    }
    write_png_rgba(output_png, scaled, output_width, output_height);
}

int subclass_count(int theme, int tile_class, const string& library_root)
{
    const TileCatalog catalog = load_catalog(library_root);
    vector<int> subclasses;
    for (const auto& [key, entry] : catalog)
    {
        if (get<0>(key) == theme && get<1>(key) == tile_class)
        {
            subclasses.push_back(get<2>(key));
        }
    }
    sort(subclasses.begin(), subclasses.end());
    subclasses.erase(unique(subclasses.begin(), subclasses.end()), subclasses.end());
    return static_cast<int>(subclasses.size());
}

int orientation_count(int theme, int tile_class, int subclass,
                      const string& library_root)
{
    const TileCatalog catalog = load_catalog(library_root);
    int count = 0;
    for (const auto& [key, entry] : catalog)
    {
        if (get<0>(key) == theme && get<1>(key) == tile_class &&
            get<2>(key) == subclass)
        {
            ++count;
        }
    }
    return count;
}

string describe_tile(int theme, int tile_class, int subclass, int orientation,
                          const string& library_root)
{
    const TileCatalog catalog = load_catalog(library_root);
    const auto entry = resolve_tile(catalog, TileKey{theme, tile_class, subclass, orientation});
    if (entry == catalog.end())
    {
        throw out_of_range("The requested theme/class/tile combination does not exist.");
    }
    return entry->second.theme_name + " / " + entry->second.class_name +
           " / subclass " + to_string(subclass) +
           " / orientation " + to_string(orientation);
}
}
