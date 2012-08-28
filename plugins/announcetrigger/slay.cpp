#include "slay.h"
#include "announcetrigger.h"

#include <vector>
#include <string>
#include <sstream>
#include <cctype>

#include "Types.h"
#include "df/creature_raw.h"
#include "df/ui.h"
#include "df/unit.h"
#include "df/world.h"

#include <algorithm>

DFHack::command_result AnnounceTrigger::slay_trigger(std::string& str, DFHack::color_ostream& out) {
    //find the name of the target species
    size_t i = str.find(std::string("slay ")) + 5;
    std::stringstream helper;
    for ( size_t a = i; a < str.length(); a++ ) {
        if ( !isalpha(str[a]) )
            break;
        helper << str[a];
    }
    std::string target = helper.str();
    //convert to lower case
    std::transform(target.begin(), target.end(), target.begin(), ::tolower);
    
    out.print("slay target = \"%s\".\n", target.c_str());
    
    int32_t civ_id = df::global::ui->civ_id;
    int32_t race_id = df::global::ui->race_id;
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->civ_id == civ_id && unit->race == race_id )
            continue;
        if ( unit->flags1.bits.dead )
            continue;
        df::creature_raw* raw = df::global::world->raws.creatures.all[unit->race];
        for ( size_t b = 0; b < 3; b++ ) {
            std::string name = raw->name[b];
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if ( name == target ) {
                //best attempt to kill them: take away their blood
                //unit->body.blood_count = 0;
                unit->flags3.bits.scuttle=1;
                break;
            }
        }
    }
    
    return DFHack::CR_OK;
}

DFHack::command_result AnnounceTrigger::slay_init(DFHack::color_ostream& out) {
    registerTrigger("slay ", &AnnounceTrigger::slay_trigger);
    return DFHack::CR_OK;
}

