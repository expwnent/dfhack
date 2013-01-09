#include "plan.h"

#include "Core.h"

#include "modules/Maps.h"

#include "df/tiletype.h"
#include "df/tiletype_material.h"
#include "df/tiletype_shape.h"
#include "df/tiletype_shape_basic.h"

#include <cstdlib>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace DFHack;
using namespace std;

//this is a complicated procedure, so it gets its own file

struct PointComp {
    unordered_map<df::coord, int32_t> *cost;
    PointComp(unordered_map<df::coord, int32_t> *costIn): cost(costIn) {

    }

    bool operator() (const df::coord p1, const df::coord p2) const {
        auto a = cost->find(p1);
        auto b = cost->find(p2);
        if ( a == cost->end() || b == cost->end() ) {
            Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
            exit(1);
        }
        int32_t c1 = (*a).second;
        int32_t c2 = (*b).second;
        if ( c1 < c2 ) {
            return true;
        } else if ( c1 > c2 ) {
            return false;
        }

        return p1 < p2;
    }
};

//int32_t getCost(Plan::Shape* shape, df::coord p1, df::coord p2);

Plan::Shape* Plan::Shape::getExtraPoints(df::coord start) {
    Plan::Shape* result = new Plan::Shape();
    //unordered_map<df::coord, df::coord> parentMap;
    unordered_map<df::coord, int32_t> cost;
    unordered_set<df::coord> closed;

    int32_t changesNeeded = 0;
    for ( auto a = data.begin(); a != data.end(); a++ ) {
        df::coord pos = (*a).first;
        if ( needsChange(pos) )
            changesNeeded++;
    }

    int32_t pointsReached = 0;
    PointComp comp(&cost);
    set<df::coord, PointComp> queue(comp);
    cost[start] = 0;
    queue.insert(start);

    vector<df::coord> offsets;
    for ( int32_t dx = -1; dx <= 1; dx++ ) {
        for ( int32_t dy = -1; dy <= 1; dy++ ) {
            for ( int32_t dz = -1; dz <= 1; dz++ ) {
                if ( dx == 0 && dy == 0 && dz == 0 )
                    continue;
                df::coord pt(dx, dy, dz);
                offsets.push_back(pt);
            }
        }
    }
    
    unordered_set<df::coord> needZPos;
    unordered_set<df::coord> needZNeg;
    
    while ( !queue.empty() ) {
        df::coord p1 = *queue.begin();
        int32_t cost1 = cost[p1];
        queue.erase(queue.begin());
        if ( closed.find(p1) != closed.end() ) {
            Core::print("%s,%d: (%d,%d,%d)\n", __FILE__, __LINE__, p1.x,p1.y,p1.z);
            exit(1);
        }
        closed.insert(p1);
        
        if ( /*(data.find(p1) != data.end() && data[p1].shape != df::enums::tiletype_shape::EMPTY) ||*/ needsChange(p1) ) {
            Core::print("%s,%d: (%2d,%2d,%2d) needs change.\n", __FILE__, __LINE__, p1.x-start.x,p1.y-start.y,p1.z-start.z);
            pointsReached++;
            df::coord parent = p1;
            df::coord prev = p1;
            while ( parentMap.find(parent) != parentMap.end() ) {
                df::coord next = parentMap[parent];
                if ( next.z < parent.z ) {
                    needZNeg.insert(parent);
                    needZPos.insert(next);
                } else if ( next.z > parent.z ) {
                    needZPos.insert(parent);
                    needZNeg.insert(next);
                }

                parent = parentMap[parent];
                df::tiletype* type = Maps::getTileType(parent);
                df::tiletype_shape shape = ENUM_ATTR(tiletype, shape, *type);
                //if ( !needsChange(parent) )
                    result->data[parent] = df::enums::tiletype_shape::STAIR_UPDOWN;
                if ( cost[parent] == 0 )
                    break;
            }
            if ( pointsReached >= changesNeeded ) {
                //Core::print("Found all of them!\n");
                //break;
            }
        }
        for ( size_t a = 0; a < offsets.size(); a++ ) {
            df::coord p2(p1.x+offsets[a].x, p1.y+offsets[a].y, p1.z+offsets[a].z);
            int32_t edgeCost = getCost(p1,p2);
            if ( edgeCost < 0 )
                continue;
            if ( cost.find(p2) == cost.end() ) {
                cost[p2] = cost1 + edgeCost;
                parentMap[p2] = p1;
                queue.insert(p2);
                continue;
            }
            if ( queue.find(p2) == queue.end() ) {
                //must be in the closed list
                continue;
            }
            int32_t cost2 = cost[p2];
            if ( cost1+edgeCost < cost2 || (cost1 + edgeCost == cost2 && p1 < parentMap[p2]) ) {
                queue.erase(p2);
                cost[p2] = cost1 + edgeCost;
                parentMap[p2] = p1;
                queue.insert(p2);
            }
        }
    }

    for ( auto a = result->data.begin(); a != result->data.end(); a++ ) {
        df::coord point = (*a).first;
        bool zPos = needZPos.find(point) != needZPos.end();
        bool zNeg = needZNeg.find(point) != needZNeg.end();

        //result->data[point] = df::enums::tiletype_shape::STAIR_UPDOWN;
        if ( zPos && zNeg ) {
            result->data[point] = df::enums::tiletype_shape::STAIR_UPDOWN;
        } else if ( zPos ) {
            result->data[point] = df::enums::tiletype_shape::STAIR_UP;
        } else if ( zNeg ) {
            result->data[point] = df::enums::tiletype_shape::STAIR_DOWN;
        } else {
            result->data[point] = df::enums::tiletype_shape::FLOOR;
        }
    }

    return result;
}

int32_t Plan::Shape::getCost(df::coord p1, df::coord p2) {
    int32_t dx = p1.x-p2.x;
    int32_t dy = p1.y-p2.y;
    int32_t dz = p1.z-p2.z;
    //for now
    if ( dx*dx + dy*dy + dz*dz > 1 )
        return -1;

    if ( !Maps::isValidTilePos(p2) || !Maps::isValidTilePos(p1) ) {
        return -1;
    }

    //if ( needsChange(p1) )
    //    return -1;
    
    if ( range1 != df::coord() && range2 != df::coord() ) {
        if ( p2.x < range1.x || p2.y < range1.y || p2.z < range1.z || p2.x > range2.x || p2.y > range2.y || p2.z > range2.z ) {
            return -1;
        }
    }

    auto i1 = data.find(p1);
    auto i2 = data.find(p2);
    if ( i1 == data.end() && i2 == data.end() ) {
        //if ( Maps::canStepBetween(p1, p2) )
        //    return 0; //TODO: this is wrong
    }

    Maps::ensureTileBlock(p1);
    Maps::ensureTileBlock(p2);
    
    df::tiletype *type1 = Maps::getTileType(p1);
    df::tiletype *type2 = Maps::getTileType(p2);
    df::tiletype_shape shape1 = ENUM_ATTR(tiletype, shape, *type1);
    df::tiletype_shape shape1New = shape1;
    df::tiletype_shape shape2 = ENUM_ATTR(tiletype, shape, *type2);
    df::tiletype_shape shape2New = shape2;
    //bool construction1 = ENUM_ATTR(tiletype, material, type1) == df::enums::tiletype_material::CONSTRUCTION;
    //bool construction2 = ENUM_ATTR(tiletype, material, type2) == df::enums::tiletype_material::CONSTRUCTION;
    
    if ( i1 != data.end() ) {
        Plan::Tile tile = (*i1).second;
        shape1New = tile.shape;
        //construction1 = tile.shape != df::enums::tiletype_shape::EMPTY;
    } else {
        shape1New = df::enums::tiletype_shape::EMPTY;
    }
    if ( i2 != data.end() ) {
        Plan::Tile tile = (*i2).second;
        shape2New = tile.shape;
        //construction2 = tile.shape != df::enums::tiletype_shape::EMPTY;
    } else {
        shape2New = df::enums::tiletype_shape::EMPTY;
    }

    /*if ( dz != 0 ) {
        if ( shape1 != df::enums::tiletype_shape::STAIR_UPDOWN && shape1 != df::enums::tiletype_shape::EMPTY )
            return -1;
    }*/

    df::tiletype_shape_basic basic1 = ENUM_ATTR(tiletype_shape, basic_shape, shape1);
    df::tiletype_shape_basic basic1New = ENUM_ATTR(tiletype_shape, basic_shape, shape1New);
    df::tiletype_shape_basic basic2 = ENUM_ATTR(tiletype_shape, basic_shape, shape2);
    df::tiletype_shape_basic basic2New = ENUM_ATTR(tiletype_shape, basic_shape, shape2New);

    //if ( basic2 == df::enums::tiletype_shape_basic::Wall )
    if ( p1.z == p2.z ) {
        switch(basic2) {
            case df::enums::tiletype_shape_basic::Open:
                //TODO: if we don't care, we don't need to remove
                switch(basic2New) {
                    case df::enums::tiletype_shape_basic::Open:
                        return 2;
                    case df::enums::tiletype_shape_basic::Floor:
                        return 2;
                    case df::enums::tiletype_shape_basic::Wall:
                        return 2;
                    case df::enums::tiletype_shape_basic::Stair:
                        return 0;
                    case df::enums::tiletype_shape_basic::Ramp:
                        return 2;
                    default:
                        Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                        exit(1);
                }
                //return 2;
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(2);
            case df::enums::tiletype_shape_basic::Floor:
                switch(basic2New) {
                    case df::enums::tiletype_shape_basic::Open:
                        return 2;
                    case df::enums::tiletype_shape_basic::Floor:
                        return 4;
                    case df::enums::tiletype_shape_basic::Wall:
                        return 2;
                    case df::enums::tiletype_shape_basic::Stair:
                        return 0;
                    case df::enums::tiletype_shape_basic::Ramp:
                        return 2;
                    default:
                        Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                        exit(1);
                }
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(1);
            case df::enums::tiletype_shape_basic::Ramp:
                switch(basic2New) {
                    case df::enums::tiletype_shape_basic::Open:
                        return 2;
                    case df::enums::tiletype_shape_basic::Floor:
                        return 2;
                    case df::enums::tiletype_shape_basic::Wall:
                        return 2;
                    case df::enums::tiletype_shape_basic::Stair:
                        return 0;
                    case df::enums::tiletype_shape_basic::Ramp:
                        return 4;
                    default:
                        Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                        exit(1);
                }
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(1);
            case df::enums::tiletype_shape_basic::Wall:
                switch(basic2New) {
                    case df::enums::tiletype_shape_basic::Open:
                        return 2;
                    case df::enums::tiletype_shape_basic::Floor:
                        return 2;
                    case df::enums::tiletype_shape_basic::Wall:
                        return 4;
                    case df::enums::tiletype_shape_basic::Stair:
                        return 0;
                    case df::enums::tiletype_shape_basic::Ramp:
                        return 2;
                    default:
                        Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                        exit(1);
                }
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(1);
            case df::enums::tiletype_shape_basic::Stair:
                return 0;
            default:
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(1);
                break;
        }
    }

    //dz != 0
    if ( p1.z < p2.z ) {
        switch(basic2) {
            case df::enums::tiletype_shape_basic::Open:
                switch(basic2New) {
                    case df::enums::tiletype_shape_basic::Open:
                        return 2;
                    case df::enums::tiletype_shape_basic::Floor:
                        return -1;
                    case df::enums::tiletype_shape_basic::Ramp:
                        return -1;
                    case df::enums::tiletype_shape_basic::Wall:
                        return -1;
                    case df::enums::tiletype_shape_basic::Stair:
                        return 0;
                    default:
                        Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                        exit(1);
                }
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(1);
            case df::enums::tiletype_shape_basic::Floor:
                return -1;
            case df::enums::tiletype_shape_basic::Ramp:
                return -1;
            case df::enums::tiletype_shape_basic::Wall:
                return -1;
            case df::enums::tiletype_shape_basic::Stair:
                //TODO: what kind of stair?
                return 0;
            default:
                Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                exit(1);
                break;
        }
    }
    
    //p1.z > p2.z
    switch(basic2) {
        case df::enums::tiletype_shape_basic::Open:
            switch(basic2New) {
                case df::enums::tiletype_shape_basic::Open:
                    return 2;
                case df::enums::tiletype_shape_basic::Floor:
                    return -1;
                case df::enums::tiletype_shape_basic::Ramp:
                    return -1;
                case df::enums::tiletype_shape_basic::Wall:
                    return -1;
                case df::enums::tiletype_shape_basic::Stair:
                    return 0;
                default:
                    Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
                    exit(1);
            }
            Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
            exit(1);
        case df::enums::tiletype_shape_basic::Floor:
            return -1;
        case df::enums::tiletype_shape_basic::Ramp:
            return -1;
        case df::enums::tiletype_shape_basic::Wall:
            return -1;
        case df::enums::tiletype_shape_basic::Stair:
            //TODO: what kind of stair?
            return 0;
        default:
            Core::print("%s, line %d. Error.\n", __FILE__, __LINE__);
            exit(1);
            break;
    }
    
    return 0;
}
