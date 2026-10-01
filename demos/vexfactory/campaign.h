#pragma once
#include "simulation.h"

namespace vexfactory
{
constexpr int MissionCount = 10;
struct Mission
{
    const char* title;
    const char* place;
    const char* speaker;
    const char* briefing;
    const char* hint;
    const char* completion;
    int credits, power, deadline, parTime, parCost;
    std::array<uint32_t, ItemCount> goals;
    std::array<uint32_t, 3> deposits;
};
inline const std::array<Mission, MissionCount>& missions()
{
    static const std::array<Mission, MissionCount> content = {{
        { "A Light in the Yard", "South signal yard", "IONA / dispatch",
          "Engineer, if this channel still reaches you: the storm severed the north line. The settlements are running on batteries. I found one miner that still turns. Twelve crates of ore will power the signal relay. Give me a route, and I can tell the others we are here.",
          "Connect the miner to the top freight dock. Belts follow their arrows. R rotates your brush. Planning is free: press Launch only when the route is ready.",
          "The relay answers. Three wards report survivors. Vale is already on the next platform with a box of smelter parts. For the first time tonight, the line has somewhere to go.",
          240, 2, 90, 30, 150, { 12, 0, 0, 0 }, { 36, 0, 0 } },
        { "Shelter Before Dawn", "East ward roofing depot", "VALE / maintenance",
          "The east ward lost its roof. We can patch it before the next rain band if you send sixteen plates. The smelter is slow, but it is honest: one ore in, one plate out. The warehouse only wants finished metal. Keep the ore out of its queue.",
          "Build a miner on the top deposit, then a smelter between conveyors. Machines reserve power even while idle. You have exactly enough for one miner and one smelter.",
          "The first roof panels arrive as the rain begins. Someone paints a little sun on one of them. Iona tapes a photograph of it above the dispatch board.",
          420, 5, 120, 50, 340, { 0, 16, 0, 0 }, { 60, 0, 0 } },
        { "The Stalled Lift", "Lower freight elevator", "VALE / maintenance",
          "The lift is stuck below the flooded platform. Its transmission ate every spare gear we had. The press can make replacements, but each gear takes two plates. Twelve gears will bring the lift and its passengers back to the surface.",
          "This deposit and dock are on the bottom lane. A gear press needs two plates per gear. Budget for all three powered machines before spending on a long route.",
          "The lift comes up slowly. There are forty people inside, and a dog that refuses to leave the operator's cabin. Our next train will have one more passenger than the manifest says.",
          590, 9, 150, 65, 470, { 0, 0, 12, 0 }, { 0, 0, 75 } },
        { "One Furnace, Three Wards", "Central distribution junction", "IONA / dispatch",
          "Every ward needs a different repair, and we only have one functioning furnace supply. Send twenty plates to the middle dock and twelve gears to the upper dock. You cannot afford a second powered line. Divide the metal you already make.",
          "A splitter sends items forward and left, alternating when both exits are open. One plate branch can ship directly; the other feeds the gear press. If an exit jams, the splitter tries the other.",
          "The wards stop calling over one another. We mark each delivery on the board, then notice that the first column is completely green. Vale opens the workshop: there is enough breathing room to improve the machinery.",
          720, 9, 170, 100, 620, { 0, 20, 12, 0 }, { 0, 90, 0 } },
        { "A Heart for the Pumps", "River pumping station", "VALE / maintenance",
          "The river pumps need eight new engines. An engine takes two gears and one plate. This is not another straight line: the assembler needs two kinds of input, and a single furnace must feed both. Leave room for the return route before you build the press.",
          "Split the smelter's plates. Send one branch through the press and the other straight to the assembler. Inputs can arrive from any side. The sorter sends its selected cargo forward and all other cargo left; F changes its filter.",
          "Eight engines start together. The waterline finally moves down instead of up. Through the cleared windows we can see the old bridge. It is still standing, but the approaches are gone.",
          1100, 14, 210, 130, 950, { 0, 0, 0, 8 }, { 0, 125, 0 } },
        { "Across the Flood", "Broken bridge approaches", "IONA / dispatch",
          "The bridge crew needs plates, gears, and engines before the relief train can cross. The flood left narrow openings in the floor. The old supply cannot power the whole factory; generators are now available. Their footprint matters as much as their price.",
          "Generators supply twelve power. Separate freight docks request separate products. A splitter on the plate line can feed the engine assembler from below while also serving the plate dock.",
          "The relief train crosses at walking speed. People follow its lanterns across the repaired approach. The driver hands Iona a dispatch from the coast: the storm is turning back toward us.",
          1650, 8, 140, 55, 1550, { 0, 18, 12, 8 }, { 100, 70, 65 } },
        { "The Night Shift", "North repair works", "VALE / maintenance",
          "We have one night to prepare the north train. The order is larger than anything this yard has handled: thirty-five plates, twenty gears, twelve engines. A beautiful layout is not enough if one furnace throttles every branch. Think in rates, not just connections.",
          "A smelter supplies about fifty-four plates per minute. A gear consumes two of them; an engine consumes five ore in total. Use parallel lines and leave space for generators before launching.",
          "The workshop lights stay on until morning. Vale falls asleep beside a conveyor, wearing hearing protectors and holding a wrench. The north train leaves with every compartment stocked.",
          1900, 8, 160, 70, 1550, { 0, 35, 20, 12 }, { 150, 105, 85 } },
        { "The Last Convoy", "Coastal dispatch siding", "IONA / dispatch",
          "The coast is closing its gates. Twenty engines, twenty-eight plates, and eighteen gears must reach this convoy before its departure. The storm clock does not care that a machine is blocked. Pause, inspect your buffers, and fix the slowest part rather than adding random capacity.",
          "Your deadline measures simulated production time, not planning time. The upper and lower passages need detours. A dock that has filled its quota rejects surplus; use that backpressure to stop wasting a deposit.",
          "We make the departure by a breath. On the radio, the coastal driver keeps saying thank you to nobody in particular. Iona draws a line through the remaining stations. Only ours is still exposed.",
          2100, 12, 160, 90, 1550, { 0, 28, 18, 20 }, { 180, 100, 80 } },
        { "Keep the Lights On", "Lumen emergency yard", "VALE / maintenance",
          "The storm took the last grid feed. There is no borrowed power now. Build your own supply and make twenty-five engines, forty plates, and twenty-five gears for the shelters. Recover idle machinery if you must, but remember that its buffers contain real ore.",
          "Start with generators: each costs 130 credits and supplies twelve power. Three complete production lanes reserve twenty-eight power. Demolition refunds seventy percent before workshop upgrades, and scraps every item inside.",
          "Every shelter reports power. Then the evacuation order arrives. The repaired line is our only way out, and the train needs one final load. Vale asks whether we are leaving the factory. Iona says we are taking what it was for.",
          2400, 0, 185, 110, 1700, { 0, 40, 25, 25 }, { 190, 140, 110 } },
        { "Homebound", "Final departure platform", "IONA / dispatch",
          "This is the last manifest: thirty engines to drive the convoy, fifty plates for its repairs, thirty gears for its spares. Every person we reached is waiting at the platform. Build a factory that can finish the whole order, not just the first crate. When the signal turns green, we all go home.",
          "Plan all three quotas and their ore costs before launching. Engines need a separate plate input. The upper and lower passages bend around damaged floor. You can finish with standard equipment; workshop upgrades are optional.",
          "The last crate rolls aboard. Vale closes the factory gate, and Iona finally leaves the radio on the desk. The train passes the painted sun on the east ward roof. Behind us the storm reaches an empty platform. Ahead, the lights of the coast are real.",
          2600, 0, 200, 130, 1750, { 0, 50, 30, 30 }, { 210, 160, 120 } }
    }};
    return content;
}
inline Scenario scenarioFor( int id, const std::array<int, 3>& upgrades = {} )
{
    const Mission& m = missions()[id]; Scenario s; s.campaign = true;
    s.credits = m.credits; s.power = m.power; s.deadline = m.deadline; s.parTime = m.parTime; s.parCost = m.parCost; s.goals = m.goals;
    s.beltTier = upgrades[0]; s.processingTier = upgrades[1]; s.recoveryTier = upgrades[2];
    s.unlocked = toolBit( Tool::Belt ) | toolBit( Tool::Miner ) | toolBit( Tool::Erase );
    if ( id >= 1 ) s.unlocked |= toolBit( Tool::Smelter );
    if ( id >= 2 ) s.unlocked |= toolBit( Tool::Press );
    if ( id >= 3 ) s.unlocked |= toolBit( Tool::Splitter );
    if ( id >= 4 ) s.unlocked |= toolBit( Tool::Assembler ) | toolBit( Tool::Sorter );
    if ( id >= 5 ) s.unlocked |= toolBit( Tool::Generator );
    for ( int row = 0; row < 3; ++row ) s.ore[Simulation::index( 1, 3 + row * 5 )] = m.deposits[row];
    uint32_t upper = itemBit( Item::Engine ), middle = itemBit( Item::Plate ), lower = itemBit( Item::Gear );
    if ( id == 0 ) upper = itemBit( Item::Ore );
    if ( id == 1 ) upper = itemBit( Item::Plate );
    if ( id == 2 ) lower = itemBit( Item::Gear );
    if ( id == 3 ) upper = itemBit( Item::Gear );
    for ( int row = 0; row < 3; ++row )
    {
        const uint32_t mask = row == 0 ? upper : row == 1 ? middle : lower;
        uint32_t active = 0; for ( int i = 0; i < ItemCount; ++i ) if ( m.goals[i] && ( mask & ( 1u << i ) ) ) active |= 1u << i;
        s.docks.push_back( { 22, 3 + row * 5, active } );
    }
    // Ruined floor creates choke points but always leaves tested freight routes.
    for ( int y = 0; y < Height; ++y ) if ( y != 3 && y != 8 && y != 13 ) s.blocked[Simulation::index( 12, y )] = true;
    if ( id >= 5 )
    {
        s.blocked[Simulation::index( 12, 12 )] = false;
        s.blocked[Simulation::index( 12, 13 )] = true;
        for ( int x = 4; x < 9; ++x ) s.blocked[Simulation::index( x, 5 )] = true;
    }
    if ( id >= 6 ) s.blocked[Simulation::index( 8, 8 )] = true;
    if ( id >= 7 ) { s.blocked[Simulation::index( 17, 3 )] = true; s.blocked[Simulation::index( 18, 3 )] = true; }
    if ( id >= 8 ) for ( int y = 3; y <= 5; ++y ) s.blocked[Simulation::index( 3, y )] = true;
    if ( id == 9 )
    {
        s.blocked[Simulation::index( 20, 8 )] = true;
        s.blocked[Simulation::index( 12, 2 )] = false;
        s.blocked[Simulation::index( 12, 3 )] = true;
    }
    return s;
}
inline void beginMission( Simulation& sim, int id, const std::array<int, 3>& upgrades = {} )
{
    sim.configure( scenarioFor( id, upgrades ) );
    if ( id == 0 ) { sim.seed( 1, 3, Tool::Miner, 0 ); sim.seed( 2, 3, Tool::Belt, 0 ); sim.seed( 3, 3, Tool::Belt, 0 ); }
}
struct Progress
{
    std::array<int, MissionCount> stars{};
    std::array<double, MissionCount> bestTime{};
    std::array<int, 3> upgrades{};
    int tokens = 0;
    int unlocked() const { int id = 0; while ( id < MissionCount - 1 && stars[id] ) ++id; return id; }
    int totalStars() const { int sum = 0; for ( int s : stars ) sum += s; return sum; }
    bool complete() const { return stars[MissionCount - 1] > 0; }
    int reward( int id, int earned, double seconds )
    {
        if ( id < 0 || id >= MissionCount || id > unlocked() || earned < 1 || earned > 3 || !std::isfinite( seconds ) || seconds <= 0 ) return 0;
        const int gained = ( stars[id] == 0 ? 2 : 0 ) + std::max( 0, earned - stars[id] );
        tokens += gained; stars[id] = std::max( stars[id], earned );
        if ( bestTime[id] == 0 || seconds < bestTime[id] ) bestTime[id] = seconds;
        return gained;
    }
    int upgradeCost( int track ) const { constexpr int costs[] = { 2, 3, 5 }; return track >= 0 && track < 3 && upgrades[track] < 3 ? costs[upgrades[track]] : 0; }
    bool purchase( int track )
    {
        const int cost = upgradeCost( track ); if ( !cost || tokens < cost ) return false;
        tokens -= cost; ++upgrades[track]; return true;
    }
};
}
