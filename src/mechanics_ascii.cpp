#include "mechanics_ascii.h"
#include "game_data.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{
constexpr int window_width = 66;

std::string equipped_name(const Entity& unit)
{
    int slot = unit.inventory.EquippedSlot;
    if (slot < 0 || slot >= 5)
        return "Unarmed";

    const ItemStack& item = unit.inventory.slot[slot];
    if (item.ID == NO_ITEM)
        return "Unarmed";

    try
    {
        return get_weapon(Armory, item.ID).NAME;
    }
    catch (const std::invalid_argument&)
    {
        return "Item " + std::to_string(item.ID);
    }
}

std::string hp_bar(int hp, int max_hp, int width = 18)
{
    if (max_hp <= 0)
        return std::string(width, '-');

    int filled = std::clamp(hp * width / max_hp, 0, width);
    return std::string(filled, '#') + std::string(width - filled, '.');
}

void border()
{
    std::cout << '+' << std::string(window_width - 2, '=') << "+\n";
}

void line(const std::string& content = "")
{
    std::cout << "| " << std::left << std::setw(window_width - 4) << content << " |\n";
}

std::string forecast_line(const std::string& side, const Entity& unit, const CombatInfo& info)
{
    return side + " " + unit.name + "  HP " +
           std::to_string(unit.stats.HP) + "/" + std::to_string(unit.ogstats.HP) +
           "  MT " + std::to_string(info.MT) +
           "  HIT " + std::to_string(info.HIT) + "%" +
           "  CRT " + std::to_string(info.CRIT) + "%" +
           (info.DB ? "  x2" : "");
}
}

namespace mechanics_ascii
{
void interact_window(
    const Entity& attacker,
    const Entity& defender,
    const CombatInfo& attacker_info,
    const CombatInfo& defender_info
)
{
    border();
    line("COMBAT FORECAST");
    border();
    if (attacker_info.WTA > 0)
    {
	line("ATTACKER: " + attacker.name + "  [" + equipped_name(attacker) + " 🡅]");
    }
    else if (attacker_info.WTA < 0)
    {
	line("ATTACKER: " + attacker.name + "  [" + equipped_name(attacker) + " 🡇]");
    }
    else if (attacker_info.WTA == 0)
    {
	line("ATTACKER: " + attacker.name + "  [" + equipped_name(attacker) + "]");
    }
    
    line(forecast_line("", attacker, attacker_info));
    line("HP [" + hp_bar(attacker.stats.HP, attacker.ogstats.HP) + "]");
    line();
    if (defender_info.WTA > 0)
    {
	line("DEFENDER: " + defender.name + "  [" + equipped_name(defender) + " 🡅]");
    }
    else if (defender_info.WTA < 0)
    {
	line("DEFENDER: " + defender.name + "  [" + equipped_name(defender) + " 🡇]");
    }
    else if (defender_info.WTA == 0)
    {
	line("DEFENDER: " + defender.name + "  [" + equipped_name(defender) + "]");
    }
    line(forecast_line("", defender, defender_info));
    line("HP [" + hp_bar(defender.stats.HP, defender.ogstats.HP) + "]");
    border();
}

void battle_window(const Entity& attacker, const Entity& defender)
{
    border();
    line("BATTLE RESULT");
    border();
    line(attacker.name + "  HP [" + hp_bar(attacker.stats.HP, attacker.ogstats.HP) +
         "] " + std::to_string(attacker.stats.HP) + "/" + std::to_string(attacker.ogstats.HP));
    line(defender.name + "  HP [" + hp_bar(defender.stats.HP, defender.ogstats.HP) +
         "] " + std::to_string(defender.stats.HP) + "/" + std::to_string(defender.ogstats.HP));
    if (attacker.stats.HP <= 0) line(attacker.name + " was killed.");
    if (defender.stats.HP <= 0) line(defender.name + " was killed.");
    border();
}

/*
void heal_window(const Entity& healer, const Entity& target, int hp_before)
{
    border();
    line("HEAL");
    border();
    line(healer.name + " restored " + target.name + ".");
    line(target.name + "  HP " + std::to_string(hp_before) + " -> " +
         std::to_string(target.stats.HP) + "/" + std::to_string(target.ogstats.HP));
    line("HP [" + hp_bar(target.stats.HP, target.ogstats.HP) + "]");
    border();
}
*/
}
