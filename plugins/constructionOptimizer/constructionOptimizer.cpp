
#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>

#include "DataDefs.h"
#include "df/world.h"

using namespace DFHack;

command_result constructionOptimizer (color_ostream &out, std::vector <std::string> & parameters);

DFHACK_PLUGIN("constructionOptimizer");

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    // Fill the command list with your commands.
    commands.push_back(PluginCommand(
        "constructionOptimizer", "Do nothing, look pretty.",
        constructionOptimizer, false, 
        "  This command does nothing at all.\n"
        "Example:\n"
        "  skeleton\n"
        "    Does nothing.\n"
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

command_result constructionOptimizer (color_ostream &out, std::vector <std::string> & parameters)
{
    vector<df::job*> constructionJobs;
    vector<df::item*> constructionItems;
    for ( df::job_list_link* link = &df::global::world->job_list; link != NULL; link = link->next ) {
        df::job* job = link->item;
        if ( job == NULL )
            continue;
        if ( job->job_type != df::job_type::ConstructBuilding )
            continue;
        
        if ( job->items.size() != 1 )
            continue;

        df::item* item = job->items[0]->item;
        constructionJobs.push_back(job);
        constructionItems.push_back(item);
    }

    
}
