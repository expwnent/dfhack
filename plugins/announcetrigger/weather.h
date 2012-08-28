#pragma once

#include "PluginManager.h"
#include "Types.h"
    #include "df/weather_type.h"
        //TODO: this doesn't compile without Types.h
#include <string>

namespace AnnounceTrigger {
    DFHack::command_result weather_trigger(std::string& str, DFHack::color_ostream& out, df::weather_type type);
    DFHack::command_result weather_rain_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result weather_snow_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result weather_clear_trigger(std::string& str, DFHack::color_ostream& out);
    DFHack::command_result weather_init(DFHack::color_ostream& out);
};

