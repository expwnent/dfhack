

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>

// DF data structure definition headers
#include "DataDefs.h"
#include "df/world.h"
#include "df/report.h"
#include "df/unit.h"
#include "df/ui.h"
#include "df/unit_thought.h"

#include <iostream>
#include <string>
using namespace std;

using namespace DFHack;
using namespace df::enums;

static uint32_t nextReportToCheck = 0;
static vector<string> triggerConditions;
static vector<void(*)(color_ostream&)> triggerFunctions;

//command_result commandtrigger (color_ostream &out, std::vector <std::string> & parameters);

DFHACK_PLUGIN("announcetrigger");

static void killFirstThing(color_ostream&);
static void callClean(color_ostream&);
static void removeBadThoughts(color_ostream&);
static void dwarfsplosion(color_ostream&);

void registerTrigger(const char* condition, void (*function)(color_ostream&)) {
    triggerConditions.push_back(string(condition));
    triggerFunctions.push_back(function);
}

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    //registerTrigger("catch", &killFirstThing);
    //registerTrigger("death", &callClean);
    registerTrigger("catch", &dwarfsplosion);
    //out.print("Testing................\n");
    //command_result InvokeCommand(color_ostream &out, const std::string & command, std::vector <std::string> & parameters);
    
    /*PersistentDataItem nextReportToCheckFromPreviousRun = Core::getInstance().getWorld()->GetPersistentData("announcetrigger");
    
    if ( nextReportToCheckFromPreviousRun.isValid() ) {
        nextReportToCheck = nextReportToCheckFromPreviousRun.ival(0);
    }*/
    
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

static void killFirstThing(color_ostream& out) {
    //find the first living thing and kill it
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->flags1.bits.dead )
            continue;
        unit->body.blood_count = 0;
        break;
    }
    vector<string> arguments;
    Core::getInstance().getPluginManager()->InvokeCommand(out, string("revtoggle"), arguments);
}

static void callClean(color_ostream& out) {
    vector<string> arguments;
    arguments.push_back(string("all"));
    //out.print("Calling clean.\n");
    Core::getInstance().getPluginManager()->InvokeCommand(out, string("clean"), arguments);
}

static void removeBadThoughts(color_ostream& out) {
    int32_t race = df::global::ui->race_id;
    int32_t civ = df::global::ui->civ_id;
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->flags1.bits.dead || unit->race != race || unit->civ_id != civ )
            continue;
        
        //vector<df::unit_thought*> v = unit->status.recent_events;
        for ( size_t b = 0; b < unit->status.recent_events.size(); b++ ) {
            int32_t value = unit->status.recent_events[b]->type.value;
            if ( value < 0 )
                continue;
            unit->status.recent_events[b]->age = 1000000;
        }
    }
}

static void dwarfsplosion(color_ostream& out) {
    //clones all female dwarves
    int32_t race = df::global::ui->race_id;
    int32_t civ = df::global::ui->civ_id;
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->flags1.bits.dead || unit->race != race || unit->civ_id != civ )
            continue;
        
        if ( unit->sex ) //0 is female, 1 is male
            continue;
        
        if ( unit->relations.pregnancy_ptr )
            continue;
        
        df::unit_genes *genes = new df::unit_genes;
        genes->appearance = unit->appearance.genes.appearance;
        genes->colors = unit->appearance.genes.colors;
        unit->relations.pregnancy_ptr = genes;
        unit->relations.pregnancy_timer = 100;
        unit->relations.pregnancy_mystery = 1;
    }
}

DFhackCExport command_result plugin_onupdate ( color_ostream &out )
{
    static bool doOnce = false;
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
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->path.dest.x == -30000 )
            continue;
        unit->pos.x = unit->path.dest.x;
        unit->pos.y = unit->path.dest.y;
        unit->pos.z = unit->path.dest.z;
    }
    
    uint32_t totalReportCount = df::global::world->status.next_report_id;
    if ( totalReportCount <= nextReportToCheck )
        return CR_OK;
    
    //out.print("on state change1\n");
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
                triggerFunctions[b](out);
            }
        }
        /*if ( report->text.find("catch") != std::string::npos ) {
            //out.print("report %5d: \"%s\" contains \"%s\"\n", id, report->text.c_str(), "catch");
            doThing();
        }*/
    }
    
    //out.print("on state change5\n");
    nextReportToCheck = totalReportCount;
    //lastReportChecked = bob-1;
    //out.print("next report id = %d\n", totalReportCount);
    //out.print("on state change6\n");
    return CR_OK;
}


