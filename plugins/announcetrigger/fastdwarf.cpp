#include "announcetrigger.h"
#include "fastdwarf.h"

#include "Types.h"

#include "df/ui.h"
#include "df/unit.h"
#include "df/world.h"

#include <string>

static bool doFastDwarf = false;
static int32_t startYear = 0;
static int32_t startTick = 0;
static int32_t prevYear = 0;
static int32_t prevTick = 0;
static const int32_t ticksPerYear = 403200;
static const int32_t duration = 2400;

DFHack::command_result AnnounceTrigger::fastdwarf_periodic(DFHack::color_ostream& out) {
    if ( !doFastDwarf ) {
        return DFHack::CR_OK;
    }
    
    int32_t diff = ((*df::global::cur_year) - startYear)*ticksPerYear + ((*df::global::cur_year_tick) - startTick);
    if ( diff >= duration ) {
        doFastDwarf = false;
        //out.print("Fast disabled!\n");
        return DFHack::CR_OK;
    }
    
    if ( *df::global::cur_year_tick == prevTick && *df::global::cur_year == prevYear ) {
        return DFHack::CR_OK;
    }
    //out.print("diff = %d, duration = %d\n", diff, duration);
    prevYear = *df::global::cur_year;
    prevTick = *df::global::cur_year_tick;
    
    int32_t race = df::global::ui->race_id;
    int32_t civ = df::global::ui->civ_id;
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->race != race || unit->civ_id != civ || unit->flags1.bits.dead ) {
            continue;
        }
        
        if ( unit->counters.job_counter > 0 ) {
            //unit->counters.job_counter = 0;
            unit->counters.job_counter /= 2;
        }
    }
    
    return DFHack::CR_OK;
}

DFHack::command_result AnnounceTrigger::fastdwarf_trigger(std::string& str, DFHack::color_ostream& out) {
    startYear = *(df::global::cur_year);
    startTick = *(df::global::cur_year_tick);
    doFastDwarf = 1;
    //out.print("Fast enabled!\n");
    
    return DFHack::CR_OK;
}

DFHack::command_result AnnounceTrigger::fastdwarf_init(DFHack::color_ostream& out) {
    registerTrigger("blessing of speed", &AnnounceTrigger::fastdwarf_trigger, &AnnounceTrigger::fastdwarf_periodic);
    return DFHack::CR_OK;
}
