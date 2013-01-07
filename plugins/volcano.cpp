#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"

#include "df/world.h"
#include "df/world_data.h"

using namespace DFHack;

command_result volcano (color_ostream &out, std::vector <std::string> & parameters);

DFHACK_PLUGIN("volcano");

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "volcano", "Do nothing, look pretty.",
        volcano, false,
        "  This command does nothing at all.\n"
        "Example:\n"
        "  volcano\n"
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

command_result volcano (color_ostream &out, std::vector <std::string> & parameters)
{
    if (!parameters.empty())
        return CR_WRONG_USAGE;

    df::world* world = df::global::world;
    for ( size_t a = 0; a < world->world_data->underground_regions.size(); a++ ) {
    }

    return CR_OK;
}
