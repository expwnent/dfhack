
// some headers required for a plugin. Nothing special, just the basics.
#include "PluginManager.h"
#include "Export.h"
#include "DataDefs.h"
#include "Core.h"

#include "modules/Buildings.h"
#include "modules/Gui.h"
#include "modules/Items.h"
#include "modules/MapCache.h"

// DF data structure definition headers
#include "df/building.h"
#include "df/construction_type.h"
#include "df/global_objects.h"
#include "df/item.h"
#include "df/item_boulderst.h"
#include "df/job.h"
#include "df/world.h"

#include "plan.h"

#include <unordered_set>
#include <vector>


using namespace DFHack;
using namespace std;

command_result architect (color_ostream &out, std::vector <std::string> & parameters);

DFHACK_PLUGIN("architect");

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "architect", "Do nothing, look pretty.",
        architect, false, /* true means that the command can't be used from non-interactive user interface */
        // Extended help string. Used by CR_WRONG_USAGE and the help command:
        "  This command does nothing at all.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}

/*
DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    switch (event) {
    case SC_GAME_LOADED:
        // initialize from the world just loaded
        break;
    case SC_GAME_UNLOADED:
        // cleanup
        break;
    default:
        break;
    }
    return CR_OK;
}
*/

/*
DFhackCExport command_result plugin_onupdate ( color_ostream &out )
{
    // whetever. You don't need to suspend DF execution here.
    return CR_OK;
}
*/

void doArchitecture(df::coord start, int32_t scale, int32_t zScale) {
    Plan::Shape shape;
    //shape.addAll(Plan::Shape::getRectangle(scale+2, scale+2, zScale, df::enums::tiletype_shape::EMPTY));
    //shape.addAll(Plan::Shape::getRectangle(scale+2, scale+2, 1, df::enums::tiletype_shape::FLOOR));
    //shape.addAll(Plan::Shape::getRectangle(scale+0, scale+0, zScale, df::enums::tiletype_shape::STAIR_UPDOWN)->translate(1,1,0));
    shape.addAll(Plan::Shape::getRectangle(scale+0, scale+0, zScale, df::enums::tiletype_shape::STAIR_UPDOWN));
    //shape.addAll(Plan::Shape::getRectangle(scale-2, scale-2, zScale, df::enums::tiletype_shape::EMPTY)->translate(2,2,0));
    //shape.addAll(Plan::Shape::getCircle(5, df::enums::tiletype_shape::WALL));
    shape = *shape.translate(start.x+1, start.y, start.z);
    shape.computeRange();
//    shape.range1 = df::coord(start.x-5, start.y-5, start.z);
    //shape.range2 = df::coord(start.x+11, start.y+11, start.z);
#if 0
    for ( int32_t x = start.x-1; x <= start.x+6; x++ ) {
        for ( int32_t y = start.y-1; y <= start.y+1; y++ ) {
            for ( int32_t z = start.z; z <= start.z+2; z++ ) {
                if ( z == start.z ) {
                    shape.data[df::coord(x,y,z)] = Plan::Tile(df::enums::tiletype_shape::FLOOR);
                } else {
                    shape.data[df::coord(x,y,z)] = Plan::Tile(df::enums::tiletype_shape::EMPTY);
                }
            }
        }
    }
#endif

#if 0
    //shape.data[df::coord(start.x+1,start.y,start.z)] = Plan::Tile(df::enums::tiletype_shape::WALL);
    //shape.data[df::coord(start.x+1,start.y,start.z+1)] = Plan::Tile(df::enums::tiletype_shape::FLOOR);
    //shape.data[df::coord(start.x,start.y+1,start.z)] = Plan::Tile(df::enums::tiletype_shape::WALL);
    //shape.data[df::coord(start.x,start.y+1,start.z+1)] = Plan::Tile(df::enums::tiletype_shape::FLOOR);
    //shape.data[df::coord(start.x+5,start.y,start.z+0)] = Plan::Tile(df::enums::tiletype_shape::WALL);
    //shape.data[df::coord(start.x+5,start.y,start.z+1)] = Plan::Tile(df::enums::tiletype_shape::WALL);
    //shape.data[df::coord(start.x+5,start.y,start.z+2)] = Plan::Tile(df::enums::tiletype_shape::WALL);
    //shape.range1 = df::coord(start.x-3, start.y-3, start.z-3);
    //shape.range2 = df::coord(start.x+3, start.y+3, start.z+3);
    Plan::Shape* extras = shape.getExtraPoints(start);

    set<df::coord> extraPoints;
    for ( auto a = extras->data.begin(); a != extras->data.end(); a++ ) {
        extraPoints.insert((*a).first);
    }

    Core::print("Important points:\n");
    for ( auto a = extraPoints.begin(); a != extraPoints.end(); a++ ) {
        df::coord p = *a;
        Core::print("  (%3d,%3d,%3d)\n", p.x-start.x,p.y-start.y,p.z-start.z);
    }
#endif

    shape.execute(start, new Plan::Shape);
#if 0
    MapExtras::MapCache cache;

    //create a boulder
    df::item_boulderst* item = new df::item_boulderst;
    item->mat_type = 0;
    item->mat_index = 61;
    item->flags.bits.on_ground = true;
    item->id = (*df::global::item_next_id)++;
    item->categorize(true);
    item->pos = Gui::getCursorPos();
    df::global::world->items.all.push_back(item);
    if ( !cache.addItemOnGround(item) ) {
        out << "Error.\n";
    }

    cache.WriteAll();
    
    //place a construction
    out.print("%d\n", placeConstruction(Gui::getCursorPos(), df::construction_type::Wall, item));
#endif
    
}

command_result architect (color_ostream &out, std::vector <std::string> & parameters)
{
    int32_t scale = 2;
    int32_t zScale = 1;
    if (parameters.size() > 0)
        scale = atoi(parameters[0].c_str());
    if (parameters.size() > 1)
        zScale = atoi(parameters[1].c_str());

    if ( scale == -1 ) {
        unordered_set<df::coord> locations;
        for ( size_t a = 0; a < df::global::world->constructions.size(); a++ ) {
            df::construction* con = df::global::world->constructions[a];
            if ( locations.find(con->pos) != locations.end() ) {
                Core::print("NOOOOOO!\n");
                return CR_FAILURE;
            }
            locations.insert(con->pos);
        }
        Core::print("AOK\n");
        return CR_OK;
    }

    df::coord start = Gui::getCursorPos();
    doArchitecture(start, scale, zScale);
    return CR_OK;
}
