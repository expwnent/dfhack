#include "announcetrigger.h"
#include "immolate.h"
#include "modules/Maps.h"
#include "df/plant.h"
//#include "modules/MapCache.h"

DFHack::command_result AnnounceTrigger::immolate_trigger(std::string& str, DFHack::color_ostream& out) {
    //set everything on fire
    if ( !DFHack::Maps::IsValid() ) {
        out.print("Error in AnnounceTrigger::trigger_immolate: map not valid.\n");
        return DFHack::CR_FAILURE;
    }
    
    for ( size_t a = 0; a < df::global::world->plants.all.size(); a++ ) {
        df::plant* plant = df::global::world->plants.all[a];
        plant->is_burning = true;
        plant->hitpoints = 0;
    }
    
    return DFHack::CR_OK;
}

DFHack::command_result AnnounceTrigger::immolate_init(DFHack::color_ostream& out) {
    registerTrigger("world on fire", &AnnounceTrigger::immolate_trigger);
    return DFHack::CR_OK;
}
