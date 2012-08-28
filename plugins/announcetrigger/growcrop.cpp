#include "growcrop.h"
#include "announcetrigger.h"

#include "modules/Maps.h"
#include "df/building.h"
#include "df/building_actual.h"
#include "df/building_type.h"
#include "df/building_farmplotst.h"
#include "df/item.h"
#include "df/item_type.h"
#include "df/item_seedsst.h"

DFHack::command_result AnnounceTrigger::growcrop_trigger(std::string& str, DFHack::color_ostream& out) {
    //out.print("Growcrop.\n");
    for ( size_t a = 0; a < df::global::world->buildings.all.size(); a++ ) {
        df::building* building = df::global::world->buildings.all[a];
        if ( building->getType() != df::building_type::FarmPlot ) {
            continue;
        }
        df::building_farmplotst* farm = (df::building_farmplotst*)building;
        for ( size_t b = 0; b < farm->contained_items.size(); b++ ) {
            df::item* item = farm->contained_items[b]->item;
            if ( item->getType() != df::item_type::SEEDS ) {
                continue;
            }
            df::item_seedsst* seeds = (df::item_seedsst*)item;
            //out.print("    grow counter = %d\n", seeds->grow_counter);
            seeds->grow_counter = 1000000;
        }
    }
    
    return DFHack::CR_OK;
}

DFHack::command_result AnnounceTrigger::growcrop_init(DFHack::color_ostream& out) {
    registerTrigger("song of sustenance", &AnnounceTrigger::growcrop_trigger);
    return DFHack::CR_OK;
}

