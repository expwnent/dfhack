
#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>

#include "DataDefs.h"
#include "df/world.h"
#include "df/ui.h"
#include "modules/MapCache.h"
//#include "df/

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <map>
using namespace std;

using namespace DFHack;
using namespace df::enums;

DFhackCExport command_result plugin_init(color_ostream& out, vector<PluginCommand>& commands);
DFhackCExport command_result plugin_onupdate(color_ostream& out);

DFHACK_PLUGIN("noteMagic");

vector<string> magicPrefixes;

DFhackCExport command_result plugin_init(color_ostream& out, vector<PluginCommand>& commands) {
    //do nothing
    magicPrefixes.push_back(string("magma input "));
    magicPrefixes.push_back(string("magma output "));
    magicPrefixes.push_back(string("water input "));
    magicPrefixes.push_back(string("water output "));
    
    return CR_OK;
}

DFhackCExport command_result plugin_onupdate(color_ostream& out) {
    if (!df::global::world || !df::global::world->map.block_index)
        return CR_OK;
    
    CoreSuspender suspend;
    
    map<pair<string,string>, df::coord> magicPoints;
    for ( size_t a = 0; a < df::global::ui->waypoints.points.size(); a++ ) {
        df::ui::T_waypoints::T_points* point = df::global::ui->waypoints.points[a];
        for ( size_t b = 0; b < magicPrefixes.size(); b++ ) {
            if ( strncmp(point->comment.c_str(), magicPrefixes[b].c_str(), magicPrefixes[b].size()) == 0 ) {
                string suffix = point->comment.substr(magicPrefixes[b].size());
                magicPoints[pair<string,string>(magicPrefixes[b], suffix)] = point->pos;
            }
        }
        /*string prefix("magma output ");
        if ( point->comment.substr(0, prefix.size()) == prefix ) {
            //string name = point->comment.substr(prefix.size());
            magicPoints[point->comment] = point->pos;
            continue;
        }
        prefix = string("magma input ");
        if ( point->comment.substr(0, prefix.size()) == prefix ) {
            
        }*/
    }
    
    bool didSomething=false;
    MapExtras::MapCache cache;
    for ( map<pair<string,string>, df::coord>::iterator i = magicPoints.begin(); i != magicPoints.end(); i++ ) {
        pair<string, string> fullName = (*i).first;
        df::coord point = (*i).second;
        
        string type = fullName.first;
        string name = fullName.second;
        
        if ( type == "magma input " ) {
            //find the output
            map<pair<string,string>, df::coord>::iterator j = magicPoints.find(pair<string,string>(string("magma output "), name));
            if ( j == magicPoints.end() )
                continue;
            df::coord outputPoint = (*j).second;
            
            //now we figure out if we can transfer magma
            df::tile_designation inputDes = cache.designationAt(point);
            df::tile_designation outputDes = cache.designationAt(outputPoint);
            
            if ( outputDes.bits.flow_size != 0 && outputDes.bits.liquid_type != df::enums::tile_liquid::Magma || outputDes.bits.flow_size == 7 ) {
                continue; //cannot put magma on water
            }
            
            uint32_t oldOutputFlow = outputDes.bits.flow_size;
            uint32_t oldInputFlow = inputDes.bits.flow_size;
            if ( inputDes.bits.flow_size + outputDes.bits.flow_size <= 7 ) {
                outputDes.bits.flow_size += inputDes.bits.flow_size;
                inputDes.bits.flow_size = 0;
            } else {
                inputDes.bits.flow_size = 7 - outputDes.bits.flow_size;
                outputDes.bits.flow_size = 7;
            }
            
            outputDes.bits.liquid_type = df::enums::tile_liquid::Magma;
            if ( outputDes.bits.flow_size > 0 ) {
                cache.setTemp1At(outputPoint, 12000);
                cache.setTemp2At(outputPoint, 12000);
                outputDes.bits.flow_forbid = 1;
            } else {
                cache.setTemp1At(outputPoint, 10015);
                cache.setTemp2At(outputPoint, 10015);
                outputDes.bits.flow_forbid = 0;
            }
            
            cache.BlockAt(point/16)->enableBlockUpdates(inputDes.bits.flow_size != oldInputFlow, false);
            cache.BlockAt(outputPoint/16)->enableBlockUpdates(outputDes.bits.flow_size != oldOutputFlow, false);
            
            cache.setDesignationAt(point, inputDes);
            cache.setDesignationAt(outputPoint, outputDes);
            
            if ( outputDes.bits.flow_size != 0 && oldOutputFlow == 0 )
                didSomething = true;
        }
    }
    //if ( didSomething )
    //    df::global::world->reindex_pathfinding = true;
    cache.WriteAll();
    
    return CR_OK;
}


