#include "Core.h"
#include "Console.h"
#include "modules/EventManager.h"
#include "modules/Job.h"
#include "modules/World.h"

#include "df/global_objects.h"
#include "df/item.h"
#include "df/job.h"
#include "df/job_list_link.h"
#include "df/unit.h"
#include "df/world.h"

//#include <list>
#include <map>
//#include <vector>
using namespace std;
using namespace DFHack;
using namespace EventManager;

/*
 * TODO:
 *  error checking
 **/

//map<uint32_t, vector<DFHack::EventManager::EventHandler> > tickQueue;
multimap<uint32_t, EventHandler> tickQueue;

multimap<Plugin*, EventHandler> handlers[EventType::EVENT_MAX];

const uint32_t ticksPerYear = 403200;

void DFHack::EventManager::registerListener(EventType::EventType e, EventHandler handler, Plugin* plugin) {
    handlers[e].insert(pair<Plugin*, EventHandler>(plugin, handler));
}

void DFHack::EventManager::registerTick(EventHandler handler, int32_t when, Plugin* plugin, bool absolute) {
    uint32_t tick = DFHack::World::ReadCurrentYear()*ticksPerYear
        + DFHack::World::ReadCurrentTick();
    if ( !Core::getInstance().isWorldLoaded() ) {
        tick = 0;
        if ( absolute ) {
            Core::getInstance().getConsole().print("Warning: absolute flag will not be honored.\n");
        }
    }
    if ( absolute ) {
        tick = 0;
    }
    
    tickQueue.insert(pair<uint32_t, EventHandler>(tick+(uint32_t)when, handler));
    handlers[EventType::TICK].insert(pair<Plugin*,EventHandler>(plugin,handler));
    return;
}

void DFHack::EventManager::unregister(EventType::EventType e, EventHandler handler, Plugin* plugin) {
    for ( multimap<Plugin*, EventHandler>::iterator i = handlers[e].find(plugin); i != handlers[e].end(); i++ ) {
        if ( (*i).first != plugin )
            break;
        EventHandler handle = (*i).second;
        if ( handle == handler ) {
            handlers[e].erase(i);
            break;
        }
    }
    return;
}

void DFHack::EventManager::unregisterAll(Plugin* plugin) {
    for ( auto i = handlers[EventType::TICK].find(plugin); i != handlers[EventType::TICK].end(); i++ ) {
        if ( (*i).first != plugin )
            break;
        
        //shenanigans to avoid concurrent modification
        EventHandler getRidOf = (*i).second;
        bool didSomething;
        do {
            didSomething = false;
            for ( auto j = tickQueue.begin(); j != tickQueue.end(); j++ ) {
                EventHandler candidate = (*j).second;
                if ( getRidOf != candidate )
                    continue;
                tickQueue.erase(j);
                didSomething = true;
                break;
            }
        } while(didSomething);
    }
    for ( size_t a = 0; a < (size_t)EventType::EVENT_MAX; a++ ) {
        handlers[a].erase(plugin);
    }
    return;
}

static void manageTickEvent(color_ostream& out);
static void manageJobInitiatedEvent(color_ostream& out);
static void manageJobCompletedEvent(color_ostream& out);
static void manageUnitDeathEvent(color_ostream& out);
static void manageItemCreationEvent(color_ostream& out);

static uint32_t lastTick = 0;
static int32_t lastJobId = -1;
static map<int32_t, df::job*> prevJobs;
static set<int32_t> livingUnits;
static int32_t nextItem;

void DFHack::EventManager::onStateChange(color_ostream& out, state_change_event event) {
    if ( event == DFHack::SC_MAP_UNLOADED ) {
        lastTick = 0;
        lastJobId = -1;
        for ( auto i = prevJobs.begin(); i != prevJobs.end(); i++ ) {
            Job::deleteJobStruct((*i).second);
        }
        prevJobs.clear();
        tickQueue.clear();
        livingUnits.clear();
        nextItem = -1;
    } else if ( event == DFHack::SC_MAP_LOADED ) {
        uint32_t tick = DFHack::World::ReadCurrentYear()*ticksPerYear
            + DFHack::World::ReadCurrentTick();
        multimap<uint32_t,EventHandler> newTickQueue;
        for ( auto i = tickQueue.begin(); i != tickQueue.end(); i++ ) {
            newTickQueue.insert(pair<uint32_t,EventHandler>(tick + (*i).first, (*i).second));
        }
        tickQueue.clear();

        tickQueue.insert(newTickQueue.begin(), newTickQueue.end());

        nextItem = *df::global::item_next_id;
    }
}

void DFHack::EventManager::manageEvents(color_ostream& out) {
    if ( !Core::getInstance().isWorldLoaded() ) {
        return;
    }
    uint32_t tick = DFHack::World::ReadCurrentYear()*ticksPerYear
        + DFHack::World::ReadCurrentTick();
    if ( tick <= lastTick )
        return;
    lastTick = tick;
    
    manageTickEvent(out);
    manageJobInitiatedEvent(out);
    manageJobCompletedEvent(out);
    manageUnitDeathEvent(out);
    manageItemCreationEvent(out);

    return;
}

static void manageTickEvent(color_ostream& out) {
    uint32_t tick = DFHack::World::ReadCurrentYear()*ticksPerYear
        + DFHack::World::ReadCurrentTick();
    while ( !tickQueue.empty() ) {
        if ( tick < (*tickQueue.begin()).first )
            break;
        EventHandler handle = (*tickQueue.begin()).second;
        tickQueue.erase(tickQueue.begin());
        handle.eventHandler(out, (void*)tick);
    }
    
}

static void manageJobInitiatedEvent(color_ostream& out) {
    if ( handlers[EventType::JOB_INITIATED].empty() )
        return;
    
    if ( lastJobId == -1 ) {
        lastJobId = *df::global::job_next_id - 1;
        return;
    }

    if ( lastJobId+1 == *df::global::job_next_id ) {
        return; //no new jobs
    }
    
    for ( df::job_list_link* link = &df::global::world->job_list; link != NULL; link = link->next ) {
        if ( link->item == NULL )
            continue;
        if ( link->item->id <= lastJobId )
            continue;
        for ( multimap<Plugin*, EventHandler>::iterator i = handlers[EventType::JOB_INITIATED].begin(); i != handlers[EventType::JOB_INITIATED].end(); i++ ) {
            (*i).second.eventHandler(out, (void*)link->item);
        }
    }

    lastJobId = *df::global::job_next_id - 1;
}


static void manageJobCompletedEvent(color_ostream& out) {
    if ( handlers[EventType::JOB_COMPLETED].empty() ) {
        return;
    }
    
    map<int32_t, df::job*> nowJobs;
    for ( df::job_list_link* link = &df::global::world->job_list; link != NULL; link = link->next ) {
        if ( link->item == NULL )
            continue;
        nowJobs[link->item->id] = link->item;
    }

    for ( map<int32_t, df::job*>::iterator i = prevJobs.begin(); i != prevJobs.end(); i++ ) {
        if ( nowJobs.find((*i).first) != nowJobs.end() )
            continue;

        //recently finished or cancelled job!
        for ( multimap<Plugin*, EventHandler>::iterator j = handlers[EventType::JOB_COMPLETED].begin(); j != handlers[EventType::JOB_COMPLETED].end(); j++ ) {
            (*j).second.eventHandler(out, (void*)(*i).second);
        }
    }

    //erase old jobs, copy over possibly altered jobs
    for ( map<int32_t, df::job*>::iterator i = prevJobs.begin(); i != prevJobs.end(); i++ ) {
        Job::deleteJobStruct((*i).second);
    }
    prevJobs.clear();
    
    //create new jobs
    for ( map<int32_t, df::job*>::iterator j = nowJobs.begin(); j != nowJobs.end(); j++ ) {
        /*map<int32_t, df::job*>::iterator i = prevJobs.find((*j).first);
        if ( i != prevJobs.end() ) {
            continue;
        }*/

        df::job* newJob = Job::cloneJobStruct((*j).second, true);
        prevJobs[newJob->id] = newJob;
    }

    /*//get rid of old pointers to deallocated jobs
    for ( size_t a = 0; a < toDelete.size(); a++ ) {
        prevJobs.erase(a);
    }*/
}

static void manageUnitDeathEvent(color_ostream& out) {
    if ( handlers[EventType::UNIT_DEATH].empty() ) {
        return;
    }
    
    for ( size_t a = 0; a < df::global::world->units.active.size(); a++ ) {
        df::unit* unit = df::global::world->units.active[a];
        if ( unit->counters.death_id == -1 ) {
            livingUnits.insert(unit->id);
            continue;
        }
        //dead: if dead since last check, trigger events
        if ( livingUnits.find(unit->id) == livingUnits.end() )
            continue;

        for ( auto i = handlers[EventType::UNIT_DEATH].begin(); i != handlers[EventType::UNIT_DEATH].end(); i++ ) {
            (*i).second.eventHandler(out, (void*)unit->id);
        }
        livingUnits.erase(unit->id);
    }
}

static void manageItemCreationEvent(color_ostream& out) {
    if ( handlers[EventType::ITEM_CREATED].empty() ) {
        return;
    }

    if ( nextItem >= *df::global::item_next_id ) {
        return;
    }

    size_t index = df::item::binsearch_index(df::global::world->items.all, nextItem, false);
    for ( size_t a = index; a < df::global::world->items.all.size(); a++ ) {
        df::item* item = df::global::world->items.all[a];
        //invaders
        if ( item->flags.bits.foreign )
            continue;
        //traders who bring back your items?
        if ( item->flags.bits.trader )
            continue;
        //migrants
        if ( item->flags.bits.owned )
            continue;
        //spider webs don't count
        if ( item->flags.bits.spider_web )
            continue;
        for ( auto i = handlers[EventType::ITEM_CREATED].begin(); i != handlers[EventType::ITEM_CREATED].end(); i++ ) {
            (*i).second.eventHandler(out, (void*)item->id);
        }
    }
    nextItem = *df::global::item_next_id;
}

