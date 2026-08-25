#include "mechanics_ascii.h"
#include "game_data.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
using namespace std;

namespace
{
constexpr int window_width = 66;

string equipped_name(const Entity& unit)
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
    catch (const invalid_argument&)
    {
        return "Item " + to_string(item.ID);
    }
}

string hp_bar(int hp, int max_hp, int width = 18)
{
    if (max_hp <= 0)
        return string(width, '-');

    int filled = clamp(hp * width / max_hp, 0, width);
    return string(filled, '#') + string(width - filled, '.');
}

void border()
{
    cout << '+' << string(window_width - 2, '=') << "+\n";
}

void line(const string& content = "")
{
    cout << "| " << left << setw(window_width - 4) << content << " |\n";
}

string forecast_line(const string& side, const Entity& unit, const CombatInfo& info)
{
    return side + " " + unit.name + "  HP " +
           to_string(unit.stats.HP) + "/" + to_string(unit.ogstats.HP) +
           "  MT " + to_string(info.MT) +
           "  HIT " + to_string(info.HIT) + "%" +
           "  CRT " + to_string(info.CRIT) + "%" +
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
         "] " + to_string(attacker.stats.HP) + "/" + to_string(attacker.ogstats.HP));
    line(defender.name + "  HP [" + hp_bar(defender.stats.HP, defender.ogstats.HP) +
         "] " + to_string(defender.stats.HP) + "/" + to_string(defender.ogstats.HP));
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
    line(target.name + "  HP " + to_string(hp_before) + " -> " +
         to_string(target.stats.HP) + "/" + to_string(target.ogstats.HP));
    line("HP [" + hp_bar(target.stats.HP, target.ogstats.HP) + "]");
    border();
}
*/
}
