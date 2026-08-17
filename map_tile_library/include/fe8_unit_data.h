// Generated data API. Source records are exported by import_fe8_assets.py.
#pragma once

#include <string_view>
#include <vector>

namespace fe_tiles
{
struct Fe8StatBlock
{
    int hp = 0;
    int pow = 0;
    int skl = 0;
    int spd = 0;
    int def = 0;
    int res = 0;
    int lck = 0;
    int con = 0;
    int mov = 0;
};

// Character bases are FE8's personal modifiers. Combine them with the
// matching ClassRecord.base for the displayed initial statline.
struct Fe8CharacterRecord
{
    int id = 0;
    std::string_view symbol;
    std::string_view name;
    int default_class_id = 0;
    int portrait_id = 0;
    int base_level = 0;
    Fe8StatBlock personal_base;
    Fe8StatBlock growth;
    std::string_view affinity;
    std::string_view attributes;
    int visit_group = 0;
};

struct Fe8ClassRecord
{
    int id = 0;
    std::string_view symbol;
    std::string_view name;
    int promotion_id = 0;
    int sms_id = 0;
    bool slow_walking = false;
    Fe8StatBlock base;
    Fe8StatBlock maximum;
    Fe8StatBlock growth;
    Fe8StatBlock promotion;
    std::string_view attributes;
    std::string_view movement_cost_table;
};

const std::vector<Fe8CharacterRecord>& fe8_characters();
const std::vector<Fe8ClassRecord>& fe8_classes();
const Fe8CharacterRecord* fe8_character(int character_id);
const Fe8ClassRecord* fe8_class(int class_id);

// FE8's displayed initial stats: class base + character personal modifiers.
Fe8StatBlock resolved_base_stats(const Fe8CharacterRecord& character,
                                 const Fe8ClassRecord& unit_class);
}
