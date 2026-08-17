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

namespace
{
struct TileEntry
{
    std::string theme_name;
    std::string class_name;
    std::string relative_path;
};

using TileKey = std::tuple<int, int, int, int>;
using TileCatalog = std::map<TileKey, TileEntry>;

std::vector<std::string> split_tab(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);
    while (std::getline(stream, field, '\t'))
    {
        fields.push_back(field);
    }
    return fields;
}

TileCatalog load_catalog(const std::string& library_root)
{
    const std::string file_path = library_root + "/data/catalogue.tsv";
    std::ifstream file(file_path);
    if (!file)
    {
        throw std::runtime_error("Cannot open tile catalogue: " + file_path);
    }

    TileCatalog catalog;
    std::string line;
    std::getline(file, line); // column headings
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = split_tab(line);
        if (fields.size() < 8)
        {
            throw std::runtime_error("Malformed tile catalogue row: " + line);
        }

        const int theme = std::stoi(fields[0]);
        const int tile_class = std::stoi(fields[2]);
        const int subclass = std::stoi(fields[4]);
        const int orientation = std::stoi(fields[5]);
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
    const int theme = std::get<0>(requested);
    const int tile_class = std::get<1>(requested);
    const int subclass = std::get<2>(requested);
    const auto first = catalog.lower_bound(TileKey{theme, tile_class, subclass, 0});
    if (first == catalog.end() || std::get<0>(first->first) != theme ||
        std::get<1>(first->first) != tile_class || std::get<2>(first->first) != subclass)
    {
        return catalog.end();
    }
    const auto second = std::next(first);
    if (second == catalog.end() || std::get<0>(second->first) != theme ||
        std::get<1>(second->first) != tile_class || std::get<2>(second->first) != subclass)
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
        throw std::invalid_argument(std::string(name) + " has the wrong row count.");
    }
    for (const std::vector<int>& row : grid)
    {
        if (static_cast<int>(row.size()) != columns)
        {
            throw std::invalid_argument(std::string(name) + " has the wrong column count.");
        }
    }
}

CFURLRef file_url(const std::string& path)
{
    return CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(path.data()),
        static_cast<CFIndex>(path.size()),
        false
    );
}

CGImageRef load_png(const std::string& path)
{
    CFURLRef url = file_url(path);
    if (url == nullptr)
    {
        throw std::runtime_error("Cannot create URL for tile: " + path);
    }
    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (source == nullptr)
    {
        throw std::runtime_error("Cannot read tile PNG: " + path);
    }
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (image == nullptr)
    {
        throw std::runtime_error("Cannot decode tile PNG: " + path);
    }
    return image;
}

CGImageRef image_from_rgba(const std::vector<unsigned char>& rgba, int width, int height)
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
        throw std::runtime_error("Could not construct an image from the tile canvas.");
    }
    return image;
}

void write_png_rgba(const std::string& path, const std::vector<unsigned char>& rgba,
                    int width, int height)
{
    CGImageRef image = image_from_rgba(rgba, width, height);
    CFURLRef url = file_url(path);
    if (url == nullptr)
    {
        CGImageRelease(image);
        throw std::runtime_error("Cannot create output URL: " + path);
    }
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
        url, CFSTR("public.png"), 1, nullptr
    );
    CFRelease(url);
    if (destination == nullptr)
    {
        CGImageRelease(image);
        throw std::runtime_error("Cannot open PNG output: " + path);
    }
    CGImageDestinationAddImage(destination, image, nullptr);
    const bool wrote = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CGImageRelease(image);
    if (!wrote)
    {
        throw std::runtime_error("Could not write PNG: " + path);
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
        throw std::invalid_argument("TileCanvas dimensions and tile size must be positive.");
    }
    rgba_.assign(static_cast<std::size_t>(rows_) * columns_ * tile_pixels_ * tile_pixels_ * 4, 0);
}

int TileCanvas::rows() const { return rows_; }
int TileCanvas::columns() const { return columns_; }
int TileCanvas::tile_pixels() const { return tile_pixels_; }
int TileCanvas::pixel_width() const { return columns_ * tile_pixels_; }
int TileCanvas::pixel_height() const { return rows_ * tile_pixels_; }
const std::vector<unsigned char>& TileCanvas::rgba() const { return rgba_; }

void TileCanvas::draw(int theme, const IntGrid& class_layer,
                      const IntGrid& tile_layer, const std::string& library_root)
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
        throw std::runtime_error("Could not create the tile canvas drawing context.");
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
                    throw std::invalid_argument("Tile class cannot be negative.");
                }
                const int tile_code = tile_layer[y][x];
                if (tile_code < 0)
                {
                    throw std::invalid_argument("Packed subclass/orientation code cannot be negative.");
                }
                const int subclass = subclass_from_code(tile_code);
                const int orientation = orientation_from_code(tile_code);

                const TileKey key{theme, tile_class, subclass, orientation};
                const auto entry = resolve_tile(catalog, key);
                if (entry == catalog.end())
                {
                    std::ostringstream error;
                    error << "No tile exists for theme=" << theme
                          << ", class=" << tile_class << ", subclass=" << subclass
                          << ", orientation=" << orientation
                          << " at grid {x=" << x << ", y=" << y << "}.";
                    throw std::out_of_range(error.str());
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

void TileCanvas::write_png(const std::string& output_png, int scale) const
{
    if (scale <= 0)
    {
        throw std::invalid_argument("PNG scale must be positive.");
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
    std::vector<unsigned char> scaled(
        static_cast<std::size_t>(output_width) * output_height * 4
    );
    for (int y = 0; y < source_height; ++y)
    {
        for (int x = 0; x < source_width; ++x)
        {
            const std::size_t source = (static_cast<std::size_t>(y) * source_width + x) * 4;
            for (int yy = 0; yy < scale; ++yy)
            {
                for (int xx = 0; xx < scale; ++xx)
                {
                    const std::size_t destination =
                        (static_cast<std::size_t>(y * scale + yy) * output_width + x * scale + xx) * 4;
                    std::copy_n(rgba_.begin() + source, 4, scaled.begin() + destination);
                }
            }
        }
    }
    write_png_rgba(output_png, scaled, output_width, output_height);
}

int subclass_count(int theme, int tile_class, const std::string& library_root)
{
    const TileCatalog catalog = load_catalog(library_root);
    std::vector<int> subclasses;
    for (const auto& [key, entry] : catalog)
    {
        if (std::get<0>(key) == theme && std::get<1>(key) == tile_class)
        {
            subclasses.push_back(std::get<2>(key));
        }
    }
    std::sort(subclasses.begin(), subclasses.end());
    subclasses.erase(std::unique(subclasses.begin(), subclasses.end()), subclasses.end());
    return static_cast<int>(subclasses.size());
}

int orientation_count(int theme, int tile_class, int subclass,
                      const std::string& library_root)
{
    const TileCatalog catalog = load_catalog(library_root);
    int count = 0;
    for (const auto& [key, entry] : catalog)
    {
        if (std::get<0>(key) == theme && std::get<1>(key) == tile_class &&
            std::get<2>(key) == subclass)
        {
            ++count;
        }
    }
    return count;
}

std::string describe_tile(int theme, int tile_class, int subclass, int orientation,
                          const std::string& library_root)
{
    const TileCatalog catalog = load_catalog(library_root);
    const auto entry = resolve_tile(catalog, TileKey{theme, tile_class, subclass, orientation});
    if (entry == catalog.end())
    {
        throw std::out_of_range("The requested theme/class/tile combination does not exist.");
    }
    return entry->second.theme_name + " / " + entry->second.class_name +
           " / subclass " + std::to_string(subclass) +
           " / orientation " + std::to_string(orientation);
}
}
