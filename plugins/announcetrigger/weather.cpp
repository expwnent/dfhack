#include "weather.h"
#include "announcetrigger.h"

#include "Core.h"
#include "modules/World.h"
#include "df/weather_type.h"

#include <vector>
#include <string>


DFHack::command_result AnnounceTrigger::weather_rain_trigger(std::string& str, DFHack::color_ostream& out) {
    return AnnounceTrigger::weather_trigger(str, out, df::weather_type::Rain);
}

DFHack::command_result AnnounceTrigger::weather_snow_trigger(std::string& str, DFHack::color_ostream& out) {
    return AnnounceTrigger::weather_trigger(str, out, df::weather_type::Snow);
}

DFHack::command_result AnnounceTrigger::weather_clear_trigger(std::string& str, DFHack::color_ostream& out) {
    return AnnounceTrigger::weather_trigger(str, out, df::weather_type::None);
}

DFHack::command_result AnnounceTrigger::weather_trigger(std::string& str, DFHack::color_ostream& out, df::weather_type type) {
    DFHack::Core::getInstance().getWorld()->SetCurrentWeather(type);
    
    return DFHack::CR_OK;
}

DFHack::command_result AnnounceTrigger::weather_init(DFHack::color_ostream& out) {
    registerTrigger("summon a rainstorm", &AnnounceTrigger::weather_rain_trigger);
    registerTrigger("summon a blizzard", &AnnounceTrigger::weather_snow_trigger);
    registerTrigger("clear the sky", &AnnounceTrigger::weather_clear_trigger);
    return DFHack::CR_OK;
}

