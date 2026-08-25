#include "stock_intmaps.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
using namespace std;

namespace
{
constexpr string_view kDictionaryName = "tile_dictionary.tsv";
constexpr string_view kIntMapDirectory = "stock_intmaps";
constexpr string_view kIntMapSuffix = ".intmap.tsv";
constexpr string_view kDictionaryHeader = "cell_code\trelative_png";

filesystem::path root_path(string_view data_root)
{
    if (data_root.empty())
    {
        throw invalid_argument("Stock intmap data root must not be empty.");
    }
    return filesystem::path(string(data_root));
}

vector<string> split_tab_line(const string& line)
{
    vector<string> fields;
    size_t start = 0;
    while (true)
    {
        const size_t end = line.find('\t', start);
        fields.push_back(line.substr(start, end - start));
        if (end == string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return fields;
}

stock_intmaps::TileCode parse_code(const string& text, const string& context)
{
    size_t consumed = 0;
    const auto value = static_cast<stock_intmaps::TileCode>(stoull(text, &consumed));
    if (consumed != text.size())
    {
        throw runtime_error("Invalid cell code in " + context + ": " + text);
    }
    return value;
}

void validate_address(stock_intmaps::TileAddress address)
{
    if (address.theme_id <= 0 || address.theme_id > 99 ||
        address.class_id < 0 || address.class_id > 99 ||
        address.subclass_id < 0 || address.subclass_id > 99 ||
        address.orientation_id < 0 || address.orientation_id > 999 ||
        address.terrain_id < 0 || address.terrain_id > 99)
    {
        throw runtime_error("Cell code contains an out-of-range field.");
    }
}

bool is_safe_map_name(string_view name)
{
    if (name.empty())
    {
        return false;
    }
    for (const char character : name)
    {
        if (!(character >= 'a' && character <= 'z') &&
            !(character >= '0' && character <= '9') &&
            character != '_')
        {
            return false;
        }
    }
    return true;
}

bool has_intmap_suffix(const string& filename)
{
    return filename.size() >= kIntMapSuffix.size() &&
        filename.compare(filename.size() - kIntMapSuffix.size(),
                         kIntMapSuffix.size(),
                         kIntMapSuffix) == 0;
}
}

namespace stock_intmaps
{
DecodedIntMap decode(const IntMap& tiles)
{
    DecodedIntMap result;
    result.theme_ids.reserve(tiles.size());
    result.class_ids.reserve(tiles.size());
    result.subclass_ids.reserve(tiles.size());
    result.orientation_ids.reserve(tiles.size());
    result.terrain_ids.reserve(tiles.size());

    for (const vector<TileCode>& row : tiles)
    {
        vector<int> theme_row;
        vector<int> class_row;
        vector<int> subclass_row;
        vector<int> orientation_row;
        vector<int> terrain_row;
        theme_row.reserve(row.size());
        class_row.reserve(row.size());
        subclass_row.reserve(row.size());
        orientation_row.reserve(row.size());
        terrain_row.reserve(row.size());

        for (const TileCode code : row)
        {
            const TileAddress address = decode(code);
            theme_row.push_back(address.theme_id);
            class_row.push_back(address.class_id);
            subclass_row.push_back(address.subclass_id);
            orientation_row.push_back(address.orientation_id);
            terrain_row.push_back(address.terrain_id);
        }

        result.theme_ids.push_back(std::move(theme_row));
        result.class_ids.push_back(std::move(class_row));
        result.subclass_ids.push_back(std::move(subclass_row));
        result.orientation_ids.push_back(std::move(orientation_row));
        result.terrain_ids.push_back(std::move(terrain_row));
    }

    return result;
}

vector<TileDefinition> dictionary(string_view data_root)
{
    const filesystem::path path = root_path(data_root) /
        string(kDictionaryName);
    ifstream file(path);
    if (!file)
    {
        throw runtime_error("Could not open tile dictionary: " + path.string());
    }

    string line;
    if (!getline(file, line) || line != kDictionaryHeader)
    {
        throw runtime_error("Tile dictionary has an unexpected header: " + path.string());
    }

    vector<TileDefinition> result;
    int line_number = 1;
    TileCode previous = 0;
    while (getline(file, line))
    {
        ++line_number;
        if (line.empty())
        {
            continue;
        }
        const vector<string> fields = split_tab_line(line);
        if (fields.size() != 2 || fields[1].empty())
        {
            throw runtime_error(
                "Tile dictionary line " + to_string(line_number) +
                " must contain a cell code and PNG path."
            );
        }

        const TileCode code = parse_code(fields[0], "tile dictionary");
        const TileAddress address = decode(code);
        validate_address(address);
        if (encode(address) != code)
        {
            throw runtime_error("Tile dictionary cell code is not canonical.");
        }
        if (code <= previous)
        {
            throw runtime_error("Tile dictionary cell codes must be unique and sorted.");
        }
        result.push_back({code, fields[1]});
        previous = code;
    }

    if (result.empty())
    {
        throw runtime_error("Tile dictionary must contain at least one tile.");
    }
    return result;
}

TileDefinition tile(TileCode code, string_view data_root)
{
    const vector<TileDefinition> entries = dictionary(data_root);
    const auto entry = lower_bound(
        entries.begin(), entries.end(), code,
        [](const TileDefinition& definition, TileCode target)
        {
            return definition.code < target;
        }
    );
    if (entry == entries.end() || entry->code != code)
    {
        throw out_of_range("Cell code is absent from the tile dictionary.");
    }
    return *entry;
}

vector<string> names(string_view data_root)
{
    const filesystem::path directory = root_path(data_root) /
        string(kIntMapDirectory);
    if (!filesystem::is_directory(directory))
    {
        throw runtime_error("Could not open stock intmap directory: " + directory.string());
    }

    vector<string> result;
    for (const filesystem::directory_entry& entry :
             filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const string filename = entry.path().filename().string();
        if (!has_intmap_suffix(filename))
        {
            continue;
        }
        result.push_back(filename.substr(0, filename.size() - kIntMapSuffix.size()));
    }
    sort(result.begin(), result.end());
    return result;
}

IntMap intmap(string_view name, string_view data_root)
{
    if (!is_safe_map_name(name))
    {
        throw invalid_argument("Stock intmap name may contain only lowercase letters, digits, and underscores.");
    }

    const filesystem::path path = root_path(data_root) /
        string(kIntMapDirectory) /
        (string(name) + string(kIntMapSuffix));
    ifstream file(path);
    if (!file)
    {
        throw runtime_error("Could not open stock intmap: " + path.string());
    }

    IntMap result;
    string line;
    size_t width = 0;
    int line_number = 0;
    while (getline(file, line))
    {
        ++line_number;
        if (line.empty())
        {
            continue;
        }
        const vector<string> fields = split_tab_line(line);
        vector<TileCode> row;
        row.reserve(fields.size());
        for (const string& field : fields)
        {
            const TileCode code = parse_code(field, "stock intmap");
            const TileAddress address = decode(code);
            validate_address(address);
            if (encode(address) != code)
            {
                throw runtime_error(
                    "Stock intmap line " + to_string(line_number) +
                    " contains a non-canonical cell code."
                );
            }
            row.push_back(code);
        }
        if (width == 0)
        {
            width = row.size();
        }
        else if (row.size() != width)
        {
            throw runtime_error("Stock intmap is not rectangular: " + path.string());
        }
        result.push_back(std::move(row));
    }

    if (result.empty() || width == 0)
    {
        throw runtime_error("Stock intmap is empty: " + path.string());
    }
    return result;
}

vector<NamedIntMap> all_intmaps(string_view data_root)
{
    vector<NamedIntMap> result;
    for (const string& name : names(data_root))
    {
        result.push_back({name, intmap(name, data_root)});
    }
    return result;
}
}
