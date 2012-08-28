#pragma once

#include "PluginManager.h"
#include <string>

namespace AnnounceTrigger {
    DFHack::command_result growcrop_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result growcrop_init(DFHack::color_ostream& out);
};

