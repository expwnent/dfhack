#pragma once

#include "ColorText.h"
#include "PluginManager.h"
#include <string>

namespace AnnounceTrigger {
    DFHack::command_result immolate_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result immolate_init(DFHack::color_ostream& out);
};

