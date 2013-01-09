#include "plan.h"

#include "Core.h"

#include "modules/Buildings.h"
#include "modules/Constructions.h"
#include "modules/Job.h"
#include "modules/MapCache.h"
#include "modules/Maps.h"

#include "df/building.h"
#include "df/construction.h"
#include "df/construction_flags.h"
#include "df/construction_type.h"
#include "df/item.h"
#include "df/item_boulderst.h"
#include "df/job.h"
#include "df/job_type.h"
#include "df/temperaturest.h"
#include "df/tiletype.h"
#include "df/tiletype_material.h"
#include "df/tiletype_shape.h"
#include "df/world.h"
#include "df/z_level_flags.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;
using namespace DFHack;

df::construction_type Plan::translate(df::tiletype_shape shape) {
    df::construction_type type = (df::construction_type)-1;
    switch(shape) {
        case df::enums::tiletype_shape::FORTIFICATION:
            type = df::enums::construction_type::Fortification;
            break;
        case df::enums::tiletype_shape::FLOOR:
            type = df::enums::construction_type::Floor;
            break;
        case df::enums::tiletype_shape::RAMP:
            type = df::enums::construction_type::Ramp;
            break;
        case df::enums::tiletype_shape::STAIR_UP:
            type = df::enums::construction_type::UpStair;
            break;
        case df::enums::tiletype_shape::STAIR_DOWN:
            type = df::enums::construction_type::DownStair;
            break;
        case df::enums::tiletype_shape::STAIR_UPDOWN:
            type = df::enums::construction_type::UpDownStair;
            break;
        case df::enums::tiletype_shape::WALL:
            type = df::enums::construction_type::Wall;
            break;
        default:
            Core::print("Error %s, line %d.\n", __FILE__, __LINE__);
            exit(1);
    }
    return type;
}

void Plan::Shape::execute(df::coord start, Plan::Shape* extras) {
    unordered_set<df::coord> points;
    for ( auto a = data.begin(); a != data.end(); a++ )
        points.insert((*a).first);
    for ( auto a = extras->data.begin(); a != extras->data.end(); a++ )
        points.insert((*a).first);

    for ( auto a = points.begin(); a != points.end(); a++ ) {
        //TODO: deal with queued constructions
        df::coord pos = (*a);
        Maps::ensureTileBlock(pos);

        //if ( needsChange(parentMap[pos]) )
            //continue;

        Plan::Tile tile;
        auto b = extras->data.find(pos);
        bool isExtra = b != extras->data.end();
        if ( isExtra ) {
            tile = (*b).second;
        } else {
            b = data.find(pos);
            if ( b == data.end() ) {
                Core::print("%s, line %d.\n", __FILE__, __LINE__);
                exit(1);
            }
            tile = (*b).second;
        }
        
        df::tiletype* tiletype = Maps::getTileType(pos);
        df::tiletype_shape shape = ENUM_ATTR(tiletype, shape, *tiletype);
        if ( tile.shape == shape ) {
            //TODO: check material
            continue;
        }

        if ( isExtra ) {
            bool needUp, needDown;
            if ( tile.shape == df::enums::tiletype_shape::STAIR_UP ) {
                needUp = true;
                needDown = false;
            } else if ( tile.shape == df::enums::tiletype_shape::STAIR_DOWN ) {
                needUp = false;
                needDown = true;
            } else if ( tile.shape == df::enums::tiletype_shape::STAIR_UPDOWN ) {
                needUp = true;
                needDown = true;
            } else if ( tile.shape == df::enums::tiletype_shape::FLOOR ) {
                needUp = false;
                needDown = false;
            } else {
                Core::print("%s, line %d\n", __FILE__, __LINE__);
                exit(1);
            }

            switch(shape) {
                case df::enums::tiletype_shape::STAIR_UP:
                    if ( !needDown )
                        continue;
                    break;
                case df::enums::tiletype_shape::STAIR_DOWN:
                    if ( !needUp )
                        continue;
                    break;
                case df::enums::tiletype_shape::STAIR_UPDOWN:
                    continue;
                case df::enums::tiletype_shape::FLOOR:
                    if ( needUp || needDown )
                        break;
                    continue;
                default: break;
            }
        } else {
            if ( !needsChange(pos) )
                continue;
        }
        
        if ( ENUM_ATTR(tiletype, material, *tiletype) == df::enums::tiletype_material::CONSTRUCTION ) {
            df::construction* construction = df::construction::find(pos);
            if ( construction == NULL ) {
                Core::print("Error %s, line %d.\n", __FILE__, __LINE__);
                exit(1);
            }
            if ( construction->flags.bits.top_of_wall == 0 ) {
                //bool immediate;
                Constructions::designateRemove(pos);
                //Core::print("%d: immediate = %d\n", __LINE__, immediate);
                //df::job* job = new df::job;
                //job->job_type = df::enums::job_type::RemoveConstruction;
                //job->pos = pos;
                //Job::linkIntoWorld(job);
                continue;
            } else {
            }
        }

        if ( tile.shape == df::enums::tiletype_shape::EMPTY ) {
            continue;
        }

        if ( false ) {
            //just instantly set it
            df::construction* constr = new df::construction;
            constr->pos = pos;
            constr->item_type = df::enums::item_type::BOULDER;
            constr->item_subtype = -1;
            constr->mat_type = 0;
            constr->mat_index = 61;
            constr->original_tile = df::enums::tiletype::OpenSpace;
            uint32_t index = df::construction::binsearch_index(df::construction::get_vector(), pos, false);
            auto iterator = df::construction::get_vector().begin() + index;
            df::global::world->constructions.insert(iterator, constr);
            continue;
        }

        df::construction_type type = Plan::translate(tile.shape);
        if ( isExtra ) {
            //type = df::enums::construction_type::UpDownStair;
        }
        //Constructions::designateNew(where, type, *item, 61);
        
        df::global::world->map.z_level_flags[pos.z].bits.update = true;
#if 0
        bool asdf = Constructions::designateNew(pos, type, df::enums::item_type::BOULDER, 61);
        if ( !asdf ) {
            Core::print("Failed to construct (%d,%d,%d), needsChange = %d.\n", pos.x-start.x,pos.y-start.y,pos.z-start.z, needsChange(pos));
        }
        
#else
        df::building* building = Buildings::allocInstance(pos, df::building_type::Construction, type);
        vector<df::item*> items;
        df::item* item = placeBoulder(start);
        items.push_back(item);
        Buildings::constructWithItems(building, items);
        
        /*df::map_block* block = Maps::getTileBlock(pos);
        for ( size_t c = 0; c < 16; c++ ) {
            for ( size_t d = 0; d < 16; d++ ) {
                block->temperature_1[c][d] = 10046;
                block->temperature_2[c][d] = 10046;
            }
        }
        block->flags.bits.designated = 1;
        block->flags.bits.update_temperature = 1;*/
#endif
    }
}

df::item_boulderst* Plan::placeBoulder(df::coord where) {
    MapExtras::MapCache cache;
    df::item_boulderst* item = new df::item_boulderst;
    item->mat_type = 0;
    item->mat_index = 61;
    item->flags.bits.on_ground = true;
    item->id = (*df::global::item_next_id)++;
    item->categorize(true);
    item->pos = where;
    item->temperature.whole = 10067;
    df::global::world->items.all.push_back(item);
    if ( !cache.addItemOnGround(item) ) {
        Core::print("Error %s, line %d.\n", __FILE__, __LINE__);
    }

    cache.WriteAll();
    return item;
}

int32_t Plan::placeConstruction(df::coord where, df::construction_type type, df::item* item) {
    //Constructions::designateNew(where, type, *item, 61);
    /*df::building* building = Buildings::allocInstance(where, df::building_type::Construction, type);
    if ( building == NULL )
        return -1;

    vector<df::item*> items;
    items.push_back(item);
    Maps::ensureTileBlock(where);

    if ( !Buildings::constructWithItems(building, items) ) {
        delete building;
        return -2;
    }*/

    return 0;
}

bool Plan::Shape::needsChange(df::coord point) {
    if ( data.find(point) == data.end() )
        return false;
    
    Maps::ensureTileBlock(point);
    df::tiletype* tiletype = Maps::getTileType(point);
    df::tiletype_shape shape = ENUM_ATTR(tiletype, shape, *tiletype);
    if ( shape == data[point].shape ) {
        return false;
    }
    if ( ENUM_ATTR(tiletype_shape, basic_shape, shape) == df::enums::tiletype_shape_basic::Floor ) {
        shape = df::enums::tiletype_shape::FLOOR;
    }
    if ( shape == data[point].shape )
        return false;
    return true;
}

void Plan::Shape::addAll(Plan::Shape* shape) {
    for ( auto a = shape->data.begin(); a != shape->data.end(); a++ ) {
        data[(*a).first] = (*a).second;
    }
}

Plan::Shape* Plan::Shape::getRectangle(int32_t x, int32_t y, int32_t z, Plan::Tile tile) {
    Plan::Shape* result = new Plan::Shape;
    for ( int32_t a = 0; a < x; a++ ) {
        for ( int32_t b = 0; b < y; b++ ) {
            for ( int32_t c = 0; c < z; c++ ) {
                result->data[df::coord(a,b,c)] = tile;
            }
        }
    }
    return result;
}

Plan::Shape* Plan::Shape::translate(int32_t x, int32_t y, int32_t z) {
    Plan::Shape* result = new Plan::Shape;
    for ( auto a = data.begin(); a != data.end(); a++ ) {
        df::coord pos = (*a).first;
        Plan::Tile tile = (*a).second;
        result->data[df::coord(pos.x+x,pos.y+y,pos.z+z)] = tile;
    }
    return result;
}

Plan::Shape* Plan::Shape::getCircle(int32_t r, Plan::Tile tile) {
    Plan::Shape* result = new Plan::Shape;
    for ( int32_t a = -r; a <= r; a++ ) {
        for ( int32_t b = -r; b <= r; b++ ) {
            if ( a*a+b*b > r*r )
                continue;
            result->data[df::coord(a,b,0)] = tile;
        }
    }
    return result;
}

void Plan::Shape::computeRange() {
    int16_t xMin=-30000, xMax=-30000;
    int16_t yMin=-30000, yMax=-30000;
    int16_t zMin=-30000, zMax=-30000;

    for ( auto a = data.begin(); a != data.end(); a++ ) {
        df::coord bob = (*a).first;
        if ( bob.x < xMin || xMin == -30000 )
            xMin = bob.x;
        if ( bob.x > xMax || xMax == -30000 )
            xMax = bob.x;
        if ( bob.y < yMin || yMin == -30000 )
            yMin = bob.y;
        if ( bob.y > yMax || yMax == -30000 )
            yMax = bob.y;
        if ( bob.z < zMin || zMin == -30000 )
            zMin = bob.z;
        if ( bob.z > zMax || zMax == -30000 )
            zMax = bob.z;
    }

    range1 = df::coord(xMin-1,yMin-1,zMin-1);
    range2 = df::coord(xMax+1,yMax+1,zMax+1);
}
