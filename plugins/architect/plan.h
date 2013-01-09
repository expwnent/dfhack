#pragma once

#include "DataDefs.h"
#include "Export.h"

#include "df/coord.h"
#include "df/construction_type.h"
#include "df/item.h"
#include "df/item_boulderst.h"
#include "df/tiletype.h"
#include "df/tiletype_shape.h"

#include <vector>
#include <unordered_map>

namespace std {
    template<>
    class hash<df::coord> {
        public:
        int32_t operator()(const df::coord c) const {
            int32_t result = 3;
            result = result*65537 + c.x;
            result = result*65537 + c.y;
            result = result*65537 + c.z;
            return result;
        }
    };
}

namespace Plan {
    df::item_boulderst* placeBoulder(df::coord where);
    int32_t placeConstruction(df::coord where, df::construction_type type, df::item* item);
    df::construction_type translate(df::tiletype_shape shape);

    struct Tile {
        df::tiletype_shape shape;
        std::string material;
        int32_t count;

        Tile(): count(0) {

        }

        Tile(df::tiletype_shape shapeIn, std::string materialIn): shape(shapeIn), material(materialIn) {

        }

        Tile(df::tiletype_shape shapeIn): shape(shapeIn) {

        }

        bool operator==(const Tile& t) const {
            return this->shape == t.shape && this->material == t.material;
        }

        bool operator!=(const Tile& t) const {
            return ! ( (*this) == t);
        }
    };

    struct Shape {
        std::unordered_map<df::coord, Tile> data;
        std::unordered_map<df::coord, df::coord> parentMap;
        df::coord range1;
        df::coord range2;

        void execute(df::coord start, Shape* extras);

        Shape* getExtraPoints(df::coord start);
        int32_t getCost(df::coord p1, df::coord p2);
        bool needsChange(df::coord point);
        void computeRange();

        void addAll(Shape* shape);
        Shape* translate(int32_t x, int32_t y, int32_t z);
        static Shape* getRectangle(int32_t x, int32_t y, int32_t z, Tile tile);
        static Shape* getCircle(int32_t r, Tile tile);
    };
}

