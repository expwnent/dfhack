

#include "PluginManager.h"
#include "Export.h"
#include "TileTypes.h"
#include "modules/MapCache.h"
#include "modules/Gui.h"

using namespace DFHack;

DFHACK_PLUGIN("embarkflattener");

command_result embarkflattener (color_ostream &out, std::vector <std::string> & parameters);

// Mandatory init function. If you have some global state, create it here.
DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    // Fill the command list with your commands.
    commands.push_back(PluginCommand(
        "embarkflattener", "Digs out all the stuff above the designated z-level.",
        embarkflattener, false,
        ""
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
   return CR_OK;
}

command_result embarkflattener (color_ostream &out, std::vector <std::string> & parameters) {
    CoreSuspender suspend;
    
    int32_t cx, cy, cz;
    uint32_t xMax,yMax,zMax;
    Maps::getSize(xMax,yMax,zMax);
    uint32_t tileXMax = xMax * 16;
    uint32_t tileYMax = yMax * 16;
    Gui::getCursorCoords(cx,cy,cz);
    if (cx == -30000)
    {
        out.printerr("Cursor is not active. Point the cursor at a vein.\n");
        return CR_FAILURE;
    }
    DFHack::DFCoord xy ((uint32_t)cx,(uint32_t)cy,cz);
    MapExtras::MapCache * mCache = new MapExtras::MapCache;
    
    for( uint32_t z = cz; z < zMax; z++ )
    {
        for( uint32_t x = 0; x < tileXMax-0; x++ )
        {
            for( uint32_t y = 0; y < tileYMax-0; y++ )
            {
                DFHack::DFCoord current(x,y,z);
                df::tile_designation des = mCache->designationAt(current);
                des.bits.hidden = 0;
                des.bits.light = 1;
                des.bits.subterranean = 0;
                des.bits.outside = 1;
                mCache->setDesignationAt(current, des);
                df::tiletype tt = mCache->tiletypeAt(current);
                if (DFHack::isOpenTerrain(tt))
                    continue;
                if (z == cz && !(DFHack::isWallTerrain(tt))) {
                    continue;
                }
                
                bool wallBelow = false;
                DFHack::DFCoord below(x,y,z-1);
                df::tiletype belowType;
                if ( mCache->testCoord(below) ) {
                    belowType = mCache->tiletypeAt(below);
                    if ( DFHack::isWallTerrain(belowType) )
                        wallBelow = true;
                }
                
                //set it to be open
                if ( wallBelow ) {
                    df::tiletype_material belowMat = DFHack::tileMaterial(belowType);
                    switch(belowMat) {
                        case df::enums::tiletype_material::SOIL: 
                            mCache->setTiletypeAt(current, df::tiletype::/*GrassLightFloor1*/SoilFloor1, false);
                            break;
                        case df::enums::tiletype_material::STONE:
                        case df::enums::tiletype_material::MINERAL:
                            mCache->setTiletypeAt(current, df::tiletype::StoneFloor1, false);
                            break;
                        default:
                            out.printerr("Error: belowMat == %d\n", (int32_t)belowMat);
                            break;
                    }
                    //mCache->setTiletypeAt(current, df::tiletype::StoneFloor1, false);
                    
                }
                else
                    mCache->setTiletypeAt(current, df::tiletype::OpenSpace, false);
            }
        }
    }
    
    mCache->WriteAll();
    delete mCache;
    return CR_OK;
}

