
//note the intentional lack of #pragma once

#ifdef X
#error "X defined!"
#endif

#ifndef ENUM_NAME
#error "You must #define ENUM_NAME."
#endif

#ifndef THE_FILE
#error "You must #define THE_FILE."
#endif

enum ENUM_NAME {
#define X(a) a,
#include THE_FILE
#undef X
};

//const char* const ENUM_NAME##_Names[] = {
const char* const Names[] = {
#define X(a) #a,
#include THE_FILE
#undef X
};

ENUM_NAME getEnumByString(const std::string str) {
    for ( size_t a = 0; a < sizeof(Names)/sizeof(ENUM_NAME); a++ ) {
        if ( str == Names[a] )
            return (ENUM_NAME)a;
    }
    return (ENUM_NAME)-1;
}

#undef ENUM_NAME
#undef THE_FILE

