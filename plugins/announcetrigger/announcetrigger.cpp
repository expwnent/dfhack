
#include "announcetrigger.h"
#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>

// DF data structure definition headers
#include "DataDefs.h"
#include "modules/Gui.h"
#include "modules/Maps.h"
#include "modules/MapCache.h"

#include "df/flow_info.h"
#include "df/map_block.h"
#include "df/report.h"
#include "df/unit.h"
#include "df/ui.h"
#include "df/unit_thought.h"
#include "df/world.h"

//local include
#include "fastdwarf.h"
#include "gaia.h"
#include "growcrop.h"
#include "immolate.h"
#include "slay.h"
#include "weather.h"

#include <iostream>
#include <string>
using namespace std;

using namespace DFHack;
using namespace df::enums;

static uint32_t nextReportToCheck = 0;
static vector<string> triggerConditions;
static vector<command_result(*)(string&, color_ostream&)> triggerFunctions;
static vector<command_result(*)(color_ostream&)> triggerPeriodicFunctions;

//command_result commandtrigger (color_ostream &out, std::vector <std::string> & parameters);

DFHACK_PLUGIN("announcetrigger");

void registerTrigger(const char* condition, command_result (*function)(string&, color_ostream&), command_result (*periodic)(color_ostream&)) {
    triggerConditions.push_back(string(condition));
    triggerFunctions.push_back(function);
    triggerPeriodicFunctions.push_back(periodic);
}

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    AnnounceTrigger::fastdwarf_init(out);
    AnnounceTrigger::gaia_init(out);
    AnnounceTrigger::growcrop_init(out);
    AnnounceTrigger::immolate_init(out);
    AnnounceTrigger::slay_init(out);
    AnnounceTrigger::weather_init(out);
    
    return CR_OK;
}

// This is called right before the plugin library is removed from memory.
DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    // You *MUST* kill all threads you created before this returns.
    // If everything fails, just return CR_FAILURE. Your plugin will be
    // in a zombie state, but things won't crash.
    return CR_OK;
}

/*DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    out.print("plugin_onstatechange\n");
    return CR_OK;
}*/

static void callClean(color_ostream& out) {
    vector<string> arguments;
    arguments.push_back(string("all"));
    Core::getInstance().getPluginManager()->InvokeCommand(out, string("clean"), arguments);
}

DFhackCExport command_result plugin_onupdate ( color_ostream &out )
{
    static bool doOnce = false; //TODO: rename
    //out.print("plugin_onupdate\n");
    
    // check run conditions
    if(!df::global::world || !df::global::world->map.block_index)
    {
        // give up if we shouldn't be running'
        doOnce = false;
        nextReportToCheck = 0;
        return CR_OK;
    }
    
    if ( !doOnce ) {
        nextReportToCheck = df::global::world->status.next_report_id;
        doOnce = true;
    }
    
    CoreSuspender susp;
    /*for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->path.dest.x == -30000 )
            continue;
        unit->pos.x = unit->path.dest.x;
        unit->pos.y = unit->path.dest.y;
        unit->pos.z = unit->path.dest.z;
    }*/
    
    for ( size_t a = 0; a < triggerConditions.size(); a++ ) {
        if ( triggerPeriodicFunctions[a] ) {
            triggerPeriodicFunctions[a](out);
        }
    }
    
    uint32_t totalReportCount = df::global::world->status.next_report_id;
    if ( totalReportCount <= nextReportToCheck )
        return CR_OK;
    
    //out.print("report size = %d\n", df::global::world->status.reports.size());
    
    for ( size_t a = 0; a < df::global::world->status.reports.size(); a++ ) {
        df::report* report = df::global::world->status.reports[a];
        int id = report->id;
        if ( id < nextReportToCheck )
            continue;
        //out.print("report %5d: \"%s\"\n", id, report->text.c_str());
        string& text = report->text;
        for ( size_t b = 0; b < triggerConditions.size(); b++ ) {
            if ( text.find(triggerConditions[b]) != std::string::npos ) {
                triggerFunctions[b](text, out);
            }
        }
    }
    
    nextReportToCheck = totalReportCount;
    
    //out.print("next report id = %d\n", totalReportCount);
    return CR_OK;
}


