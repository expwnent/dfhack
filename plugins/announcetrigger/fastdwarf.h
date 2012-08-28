#pragma once

#include "ColorText.h"
#include "PluginManager.h"
#include <string>

namespace AnnounceTrigger {
    DFHack::command_result fastdwarf_periodic(DFHack::color_ostream& out);
    DFHack::command_result fastdwarf_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result fastdwarf_init(DFHack::color_ostream& out);
};

