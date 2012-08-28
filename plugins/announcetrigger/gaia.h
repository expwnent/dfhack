#pragma once

#include "PluginManager.h"
#include <string>

namespace AnnounceTrigger {
    DFHack::command_result gaia_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result gaia_init(DFHack::color_ostream& out);
};

