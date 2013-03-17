
#include "Core.h"
#include "Console.h"

#include "modules/IDCache.h"

#include "df/unit.h"
#include "df/world.h"

#include <unordered_map>

using namespace std;

unordered_map<int,int> unitCache;
unordered_map<int,int> buildingCache;

df::unit* DFHack::IDCache::findUnit(int32_t id) {
    int index = unitCache[id];
    df::world* world = df::global::world;
    if ( index >= 0 && index < world->units.all.size() ) {
        if ( world->units.all[index]->id == id )
            return world->units.all[index];
    }
    //wrong or absent
    index = df::unit::binsearch_index(world->units.all, id);
    if ( index < 0 )
        return NULL;
    unitCache[id] = index;
    return world->units.all[index];
}

df::building* DFHack::IDCache::findBuilding(int32_t id) {
    int index = buildingCache[id];
    df::world* world = df::global::world;
    if ( index >= 0 && index < world->buildings.all.size() ) {
        if ( world->buildings.all[index]->id == id )
            return world->buildings.all[index];
    }
    //wrong or absent
    index = df::building::binsearch_index(world->buildings.all, id);
    if ( index < 0 ) {
        buildingCache.erase(id);
        return NULL;
    }
    buildingCache[id] = index;
    return world->buildings.all[index];
}

