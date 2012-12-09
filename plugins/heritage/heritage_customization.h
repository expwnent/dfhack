#pragma once

//enums used for arguments
////////////////////////////////////////////////////////////////////////

#if 0
#define MakeEnum(name, filename, default_value) /
namespace name { /
#define ENUM_NAME name /
#define THE_FILE filename /
#include "enumMaker.h" /
    name default_ = default_value; /
};
#endif

//MakeEnum(NameScheme, "NameScheme.h", normal);


#if 1
namespace NameScheme {
#define ENUM_NAME NameScheme
#define THE_FILE "NameScheme.h"
#include "enumMaker.h"
    NameScheme default_ = normal;
};
#endif

namespace NameCollisionPolicy {
#define ENUM_NAME NameCollisionPolicy
#define THE_FILE "NameCollisionPolicy.h"
#include "enumMaker.h"
    NameCollisionPolicy default_ = uniqueNames;
};

namespace OutputType {
#define ENUM_NAME OutputType
#define THE_FILE "OutputType.h"
#include "enumMaker.h"
    OutputType default_ = allNames;
};

namespace OutputSortType {
#define ENUM_NAME OutputSortType
#define THE_FILE "OutputSortType.h"
#include "enumMaker.h"
    OutputSortType default_ = totalUses;
};

namespace RepeatPolicy {
#define ENUM_NAME RepeatPolicy
#define THE_FILE "RepeatPolicy.h"
#include "enumMaker.h"
    RepeatPolicy default_ = none;
};

namespace InfluencePolicy {
#define ENUM_NAME InfluencePolicy
#define THE_FILE "InfluencePolicy.h"
#include "enumMaker.h"
    InfluencePolicy default_ = oldestName;
};

//////////////////////////////////////////////////////////////////////

using NameScheme::NameScheme;
using NameCollisionPolicy::NameCollisionPolicy;
using OutputType::OutputType;
using OutputSortType::OutputSortType;
using RepeatPolicy::RepeatPolicy;
using InfluencePolicy::InfluencePolicy;


