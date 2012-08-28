#include "gaia.h"
#include "announcetrigger.h"

#include "modules/Maps.h"
#include "modules/MapCache.h"

#include "df/plant.h"

#include <vector>
#include <string>

DFHack::command_result AnnounceTrigger::gaia_trigger(std::string& str, DFHack::color_ostream& out) {
    //grow plants
    
    MapExtras::MapCache map;
    for ( size_t a = 0; a < df::global::world->plants.all.size(); a++ ) {
        df::plant* plant = df::global::world->plants.all[a];
        
        df::tiletype ttype = map.tiletypeAt(plant->pos);
        if(tileShape(ttype) == tiletype_shape::SAPLING &&
           tileSpecial(ttype) != tiletype_special::DEAD) {
            plant->grow_counter = Vegetation::sapling_to_tree_threshold;
        }
    }
    
    //regrass
    std::vector<std::string> arguments;
    Core::getInstance().getPluginManager()->InvokeCommand(out, std::string("regrass"), arguments);
    
    return CR_OK;
}

DFHack::command_result AnnounceTrigger::gaia_init(DFHack::color_ostream& out) {
    registerTrigger("song of gaia", &AnnounceTrigger::gaia_trigger);
    return DFHack::CR_OK;
}

