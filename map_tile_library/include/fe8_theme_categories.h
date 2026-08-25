#pragma once

#include <array>
#include <string_view>

#include "fe8_theme_ids.h"
using namespace std;

namespace fe_tiles
{
// A broad visual setting used to choose an exact theme. A category is not an
// art palette: its numbered variants can still have different FE8 artwork.
enum class ThemeCategory
{
    NATURE,
    CASTLE,
    SHIP,
    DESERT,
    DUNGEON,
    UNKNOWN,
};

// Variant numbers are zero-based, like subclass and orientation IDs. These
// arrays stay in original theme-ID order, so their numbering is stable.
inline constexpr array<ThemeId, 33> NATURE_THEMES = {
    THEME_CHAPTERS_01, THEME_CHAPTERS_02, THEME_CHAPTERS_04,
    THEME_CHAPTERS_05, THEME_CHAPTERS_06, THEME_CHAPTERS_07,
    THEME_CHAPTERS_09EIRIKA, THEME_CHAPTERS_10EIRIKA,
    THEME_CHAPTERS_10EPHRAIM, THEME_CHAPTERS_12EIRIKA,
    THEME_CHAPTERS_12EPHRAIM, THEME_CHAPTERS_13EIRIKA,
    THEME_CHAPTERS_13EPHRAIM, THEME_CHAPTERS_17, THEME_CHAPTERS_18,
    THEME_CHAPTERS_20, THEME_CHAPTERS_PROLOGUE,
    THEME_CUTSCENES_CAER_PELYN, THEME_CUTSCENES_CHAPTER02,
    THEME_CUTSCENES_GRADO_KEEP, THEME_CUTSCENES_GRADO_OUTSKIRTS,
    THEME_CUTSCENES_PORT_KIRIS, THEME_CUTSCENES_SERAFEW,
    THEME_CUTSCENES_ZA_ALBUL_MARSH, THEME_SKIRMISHES_ADLAS_PLAINS,
    THEME_SKIRMISHES_BETHROEN, THEME_SKIRMISHES_HAMILL_CANYON,
    THEME_SKIRMISHES_MELKAEN_COAST, THEME_SKIRMISHES_NARUBE_RIVER,
    THEME_SKIRMISHES_NELERAS_PEAK, THEME_SKIRMISHES_TERAZ_PLATEAU,
    THEME_SKIRMISHES_ZA_ALBUL_MARSH, THEME_SKIRMISHES_ZA_HA_WOODS,
};

inline constexpr array<ThemeId, 17> CASTLE_THEMES = {
    THEME_CHAPTERS_03, THEME_CHAPTERS_05X, THEME_CHAPTERS_08,
    THEME_CHAPTERS_09EPHRAIM, THEME_CHAPTERS_11EIRIKA,
    THEME_CHAPTERS_14EIRIKA, THEME_CHAPTERS_14EPHRAIM,
    THEME_CHAPTERS_16, THEME_CHAPTERS_19,
    THEME_CUTSCENES_CHAPTER08, THEME_CUTSCENES_GRADO_CHAMBER,
    THEME_CUTSCENES_GRADO_PRISON, THEME_CUTSCENES_JEHANNA_HALL,
    THEME_CUTSCENES_RAUSTEN_CHAMBER, THEME_CUTSCENES_RENAIS_CASTLE,
    THEME_CUTSCENES_RENAIS_CHAMBER, THEME_CUTSCENES_RENVALL,
};

inline constexpr array<ThemeId, 1> SHIP_THEMES = {
    THEME_CHAPTERS_11EPHRAIM,
};

inline constexpr array<ThemeId, 1> DESERT_THEMES = {
    THEME_CHAPTERS_15,
};

inline constexpr array<ThemeId, 20> DUNGEON_THEMES = {
    THEME_CHAPTERS_FINAL1, THEME_CHAPTERS_FINAL2,
    THEME_TOWER_OF_VALNI_1, THEME_TOWER_OF_VALNI_2,
    THEME_TOWER_OF_VALNI_3, THEME_TOWER_OF_VALNI_4,
    THEME_TOWER_OF_VALNI_5, THEME_TOWER_OF_VALNI_6,
    THEME_TOWER_OF_VALNI_7, THEME_TOWER_OF_VALNI_8,
    THEME_LAGDOU_RUINS_01, THEME_LAGDOU_RUINS_02,
    THEME_LAGDOU_RUINS_03, THEME_LAGDOU_RUINS_04,
    THEME_LAGDOU_RUINS_05, THEME_LAGDOU_RUINS_06,
    THEME_LAGDOU_RUINS_07, THEME_LAGDOU_RUINS_08,
    THEME_LAGDOU_RUINS_09, THEME_LAGDOU_RUINS_10,
};

template <size_t Count>
constexpr ThemeId theme_at(const array<ThemeId, Count>& themes, int variant)
{
    return variant >= 0 && variant < static_cast<int>(Count) ? themes[variant] : 0;
}

template <size_t Count>
constexpr bool contains_theme(const array<ThemeId, Count>& themes, ThemeId theme_id)
{
    for (const ThemeId candidate : themes)
    {
        if (candidate == theme_id)
        {
            return true;
        }
    }
    return false;
}

// Resolves the public {category, variant} address to the renderer's exact
// theme ID. It returns 0 if the category or variant is invalid.
constexpr ThemeId theme_id(ThemeCategory category, int variant)
{
    switch (category)
    {
    case ThemeCategory::NATURE:  return theme_at(NATURE_THEMES, variant);
    case ThemeCategory::CASTLE:  return theme_at(CASTLE_THEMES, variant);
    case ThemeCategory::SHIP:    return theme_at(SHIP_THEMES, variant);
    case ThemeCategory::DESERT:  return theme_at(DESERT_THEMES, variant);
    case ThemeCategory::DUNGEON: return theme_at(DUNGEON_THEMES, variant);
    case ThemeCategory::UNKNOWN: return 0;
    }

    return 0;
}

constexpr int theme_variant_count(ThemeCategory category)
{
    switch (category)
    {
    case ThemeCategory::NATURE:  return static_cast<int>(NATURE_THEMES.size());
    case ThemeCategory::CASTLE:  return static_cast<int>(CASTLE_THEMES.size());
    case ThemeCategory::SHIP:    return static_cast<int>(SHIP_THEMES.size());
    case ThemeCategory::DESERT:  return static_cast<int>(DESERT_THEMES.size());
    case ThemeCategory::DUNGEON: return static_cast<int>(DUNGEON_THEMES.size());
    case ThemeCategory::UNKNOWN: return 0;
    }

    return 0;
}

constexpr ThemeCategory theme_category(ThemeId theme_id)
{
    if (contains_theme(NATURE_THEMES, theme_id))  return ThemeCategory::NATURE;
    if (contains_theme(CASTLE_THEMES, theme_id))  return ThemeCategory::CASTLE;
    if (contains_theme(SHIP_THEMES, theme_id))    return ThemeCategory::SHIP;
    if (contains_theme(DESERT_THEMES, theme_id))  return ThemeCategory::DESERT;
    if (contains_theme(DUNGEON_THEMES, theme_id)) return ThemeCategory::DUNGEON;
    return ThemeCategory::UNKNOWN;
}

constexpr string_view theme_category_name(ThemeCategory category)
{
    switch (category)
    {
    case ThemeCategory::NATURE:  return "Nature";
    case ThemeCategory::CASTLE:  return "Castle";
    case ThemeCategory::SHIP:    return "Ship";
    case ThemeCategory::DESERT:  return "Desert";
    case ThemeCategory::DUNGEON: return "Dungeon";
    case ThemeCategory::UNKNOWN: return "Unknown";
    }

    return "Unknown";
}
}
