#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
using namespace std;

namespace stock_intmaps
{
// Fixed-field decimal cell code: TT CC SS OOO RR.
//
// TT  exact global theme ID       (two digits)
// CC  visual class ID             (two digits)
// SS  raw subclass ID             (two digits)
// OOO raw orientation/literal ID  (three digits)
// RR  native FE8 TerrainId        (two digits)
//
// It deliberately contains neither a mutable visual category nor a guessed
// direction/pattern role. Leading zeroes are written to disk, but callers use
// this 64-bit integer type in memory.
using TileCode = uint64_t;
using IntMap = vector<vector<TileCode>>;
using ComponentGrid = vector<vector<int>>;

struct TileAddress
{
    int theme_id = 0;
    int class_id = 0;
    int subclass_id = 0;
    int orientation_id = 0;
    int terrain_id = 0;
};

// The sole runtime dictionary row: a complete cell code and its exact source
// image. All numeric fields are decoded from `code` rather than duplicated.
struct TileDefinition
{
    TileCode code = 0;
    string relative_png;
};

struct NamedIntMap
{
    string name;
    IntMap tiles;
};

struct DecodedIntMap
{
    ComponentGrid theme_ids;
    ComponentGrid class_ids;
    ComponentGrid subclass_ids;
    ComponentGrid orientation_ids;
    ComponentGrid terrain_ids;
};

inline constexpr string_view DEFAULT_DATA_ROOT = "map_tile_library/data";

constexpr TileCode encode(TileAddress address)
{
    return static_cast<TileCode>(address.theme_id) * 1000000000ULL +
        static_cast<TileCode>(address.class_id) * 10000000ULL +
        static_cast<TileCode>(address.subclass_id) * 100000ULL +
        static_cast<TileCode>(address.orientation_id) * 100ULL +
        static_cast<TileCode>(address.terrain_id);
}

constexpr TileAddress decode(TileCode code)
{
    return {
        static_cast<int>(code / 1000000000ULL),
        static_cast<int>((code / 10000000ULL) % 100ULL),
        static_cast<int>((code / 100000ULL) % 100ULL),
        static_cast<int>((code / 100ULL) % 1000ULL),
        static_cast<int>(code % 100ULL),
    };
}

// Decodes every cell into parallel [y][x] component grids.
DecodedIntMap decode(const IntMap& tiles);

// The one canonical dictionary used by every exported stock intmap.
vector<TileDefinition> dictionary(
    string_view data_root = DEFAULT_DATA_ROOT
);

TileDefinition tile(
    TileCode code,
    string_view data_root = DEFAULT_DATA_ROOT
);

// Each returned name can be passed to intmap(). Names are discovered from the
// exported .intmap.tsv files; no second map dictionary is maintained.
vector<string> names(
    string_view data_root = DEFAULT_DATA_ROOT
);

IntMap intmap(
    string_view name,
    string_view data_root = DEFAULT_DATA_ROOT
);

vector<NamedIntMap> all_intmaps(
    string_view data_root = DEFAULT_DATA_ROOT
);
}
