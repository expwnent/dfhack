#pragma once

#include "Core.h"
#include "Console.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"

#include "df/building.h"
#include "df/unit.h"

namespace DFHack {
    namespace IDCache {
        DFHACK_EXPORT df::unit* findUnit(int32_t id);
        DFHACK_EXPORT df::building* findBuilding(int32_t id);
    }
}

