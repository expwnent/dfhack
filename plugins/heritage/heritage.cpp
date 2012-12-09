#include "PluginManager.h"
#include "Export.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/unit.h"
#include "df/ui.h"
#include "df/world_history.h"
#include "df/historical_figure.h"
#include "df/histfig_hf_link.h"
#include "df/death_info.h"
#include "df/language_word.h"
#include "modules/Translation.h"

#include <ctime>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <set>
#include <vector>
#include <string>
#include <unordered_set>

#include "heritage_customization.h"
/*

Note that in the comments and output, it often says "dwarves" when it really means "the primary race of the current fort". It should work fine with modded main races.

TODO
    parts of speech
*/

using namespace std;
using namespace DFHack;

DFHACK_PLUGIN("heritage");

command_result heritage(color_ostream& out, vector<string>& parameters);

DFhackCExport command_result plugin_shutdown(color_ostream& out) {
    return CR_OK;
}

DFhackCExport command_result plugin_init(color_ostream& out, vector<PluginCommand> &commands) {
    commands.push_back(PluginCommand("heritage", "makes dwarves inherit last names",
        &heritage, false,
        "heritage: full description\n\n"
        ));
    return CR_OK;
}

//DFhackCExport command_result plugin_onupdate(color_ostream& out);

/*
 * Used to collect all necessary information about both historical and local dwarves.
 **/
struct DwarfWrapper {
    bool historical;
    bool local;
    void* ptr; //ptr to actual unit, or historical unit
    int32_t birthTime;
    df::language_name* name;
    int32_t childCount;
    int32_t childIndex;
    int32_t parent1Index;
    int32_t parent2Index;
    int32_t deathTime;
    
    bool operator<(const DwarfWrapper& a) const {
        //return birthTime < a.birthTime;
        if ( birthTime != a.birthTime ) return birthTime < a.birthTime;
        return ptr < a.ptr;
    }
};

static const int32_t ticksPerYear = 403200;

int32_t findRelation(df::historical_figure* unit, df::enums::histfig_hf_link_type::histfig_hf_link_type type) {
    for ( size_t b = 0; b < unit->histfig_links.size(); b++ ) {
        df::histfig_hf_link* link = unit->histfig_links[b];
        if ( link->getType() == type ) {
            return link->anon_1;
        }
    }
    return -1;
}

struct NameSorter {
    color_ostream* out;
    vector<int32_t> *favorite;
    
    NameSorter(vector<int32_t> *favoriteI) {
        favorite = favoriteI;
    }
    
    bool operator()(int32_t a, int32_t b) const {
        /*if ( favorite->find(a) == favorite->end() || favorite->find(b) == favorite->end() ) {
            out->print("Doomed rhombus.\n"); //test -1
        }*/
        int32_t founder1 = (*favorite)[a];
        int32_t founder2 = (*favorite)[b];
        return founder1 < founder2;
    }
};

typedef union {
    vector<string*> *strVector;
    vector<df::language_word*> *wordPtr;
} AlphabeticHelper;

struct AlphabeticSorter {
    color_ostream* out;
    AlphabeticHelper favorite;
    bool str;

    /*AlphabeticSorter(AlphabeticHelper favorite1) {
        favorite = favorite1;
    }*/

    bool operator()(int32_t a, int32_t b) const {
        if ( str ) {
            return (*(*favorite.strVector)[a]) < (*(*favorite.strVector)[b]);
        }
        
        df::language_word* word1 = (*favorite.wordPtr)[a];
        df::language_word* word2 = (*favorite.wordPtr)[b];
        std::string form1, form2;
        
        //return (*favorite)[a] < (*favorite)[b];
        return a < b;
    }
};

struct FullNameKey {
    hash<string> helper;
    size_t operator()(df::language_name* name) const {
        size_t result = 2;
        for ( int b = 0; b < 2; b++ )
            result = result*17 + name->words[b];
        result = result*17 + helper(name->first_name);
        return result;
    }
};

struct FullNameEq {
    bool operator()(df::language_name* name1, df::language_name* name2) const {
        if ( name1 == name2 ) return true;
        
        if ( name1->words[0] != name2->words[0] )
            return false;
        if ( name1->words[1] != name2->words[1] )
            return false;
        
        return name1->first_name == name2->first_name;
    }
};

typedef unordered_set<df::language_name*, FullNameKey, FullNameEq> FullNameSet;

void handleConflicts(df::language_name* name, FullNameSet& finalNames, vector<int32_t> &nameFrequency, vector<int32_t> &permutation, size_t wordmax, color_ostream& out, enum NameCollisionPolicy nameCollisionPolicy ) {
    if ( nameCollisionPolicy == NameCollisionPolicy::ignore )
        return;

    if (name->words[0] != name->words[1] && finalNames.find(name) == finalNames.end())
        return;
    
    if ( nameCollisionPolicy == NameCollisionPolicy::uniqueNames ) {
        //out.print("b1\n");
        //use unused name first if two exist
        int32_t b = 0;
        for ( size_t a = 0; a < wordmax; a++ ) {
            int i = permutation[a];
            if ( nameFrequency[i] != 0 ) continue;
            name->words[b] = i;
            if ( ++b >= 2 )
                break;
        }
        //out.print("b2\n");
        if ( b == 2 ) {
            if ( finalNames.find(name) == finalNames.end() )
                return;
        }
    }
    //out.print("b3\n");

    if ( nameCollisionPolicy >= NameCollisionPolicy::randomUnused ) {
        //try a random unused name: unused name -> unused full name
        int32_t unused1 = -1;
        int32_t count = 0;
        for ( size_t a = 0; a < wordmax; a++ ) {
            if ( nameFrequency[a] != 0 ) continue;
            
            count++;
            if ( (rand() / (1.0f+RAND_MAX)) < 1.0f / count ) {
                unused1 = a;
            }
        }
        count = 0;
        int32_t unused2 = -1;
        for ( size_t a = 0; a < wordmax; a++ ) {
            if ( a == unused1 ) continue;
            if ( nameFrequency[a] != 0 ) continue;
            
            count++;
            if ( (rand() / (1.0f+RAND_MAX)) < 1.0f / count ) {
                unused2 = a;
            }
        }
        if ( unused1 != -1 && unused2 != -1 ) {
            name->words[0] = unused1;
            name->words[1] = unused2;
            //out.print("new: %d, %d\n", name->words[0], name->words[1]);
            if (finalNames.find(name) == finalNames.end()) return;
        }
    }
    
    if ( nameCollisionPolicy >= NameCollisionPolicy::randomSecond ) {
        //try setting second name to something random
        for ( size_t a = 0; a < 10; a++ ) {
            name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
            if (name->words[0] == name->words[1]) continue;
            if (finalNames.find(name) == finalNames.end()) return;
        }
        
        //try all second names
        for ( size_t a = 0; a < wordmax; a++ ) {
            name->words[1] = a;
            if (name->words[0] == name->words[1]) continue;
            if (finalNames.find(name) == finalNames.end()) return;
        }
    }
    
    //try random names
    for ( size_t a = 0; a < 2*wordmax; a++ ) {
        name->words[0] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
        name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
        if (name->words[0] == name->words[1]) continue;
        if (finalNames.find(name) == finalNames.end()) return;
    }
    
    //give up on uniqueness: plain old random name
    name->words[0] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
    name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
    while(name->words[0] == name->words[1]) {
        name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
    }
}

command_result writeNameHistory(color_ostream& out, vector<DwarfWrapper>& dwarves);

int32_t nameDwarf(DwarfWrapper& dwarf, DwarfWrapper& parent1, DwarfWrapper& parent2, enum NameScheme nameScheme, vector<int32_t>& names, int32_t* newName) {
    int32_t index = dwarf.childIndex % 6;
    switch(nameScheme) {
    case NameScheme::normal:
        switch(index) {
            case 0:
                    newName[0] = names[0];
                    newName[1] = names[1];
                    break;
            case 1:
                    newName[0] = names[0];
                    newName[1] = names[2];
                    break;
            case 2:
                    newName[0] = names[0];
                    newName[1] = names[3];
                    break;
            case 3:
                    newName[0] = names[1];
                    newName[1] = names[2];
                    break;
            case 4:
                    newName[0] = names[1];
                    newName[1] = names[3];
                    break;
            case 5:
                    newName[0] = names[2];
                    newName[1] = names[3];
                    break;
            default: break;
        }
        break;
    case NameScheme::eldestParent:
        if ( parent1.birthTime <= parent2.birthTime ) {
            newName[0] = parent1.name->words[0];
            newName[1] = parent1.name->words[1];
        } else {
            newName[0] = parent2.name->words[0];
            newName[1] = parent2.name->words[1];
        }
        break;
    case NameScheme::fatherName:
        newName[0] = parent1.name->words[0];
        newName[1] = parent1.name->words[1];
        break;
    case NameScheme::motherName:
        newName[0] = parent2.name->words[0];
        newName[1] = parent2.name->words[1];
        break;
    default: return -1;
    }
    return 1;
}

command_result heritage(color_ostream& out, vector<string>& parameters) {
    
    enum NameScheme nameScheme = NameScheme::default_;
    enum NameCollisionPolicy nameCollisionPolicy = NameCollisionPolicy::default_;
    enum OutputType outputType = OutputType::default_;
    enum OutputSortType outputSortType = OutputSortType::default_;
    enum RepeatPolicy repeatPolicy = RepeatPolicy::default_;
    enum InfluencePolicy influencePolicy = InfluencePolicy::default_;
    bool reverse = false;
    
    size_t a;
    for (a = 0; a < parameters.size(); a++) {
        if ( parameters[a] == "-nameScheme" ) {
           if ( a+1 >= parameters.size() )
               return CR_WRONG_USAGE;
           a++;
           enum NameScheme t = NameScheme::getEnumByString(parameters[a]);
           if ( t == -1 )
               return CR_WRONG_USAGE;
           nameScheme = t;
        } else if ( parameters[a] == "-nameCollisionPolicy" ) {
           if ( a+1 >= parameters.size() )
               return CR_WRONG_USAGE; 

           a++;
           enum NameCollisionPolicy t = NameCollisionPolicy::getEnumByString(parameters[a]);
           if ( t == -1 )
               return CR_WRONG_USAGE;
           nameCollisionPolicy = t;
        } else if ( parameters[a] == "-outputType" ) {
           if ( a+1 >= parameters.size() )
              return CR_WRONG_USAGE; 

           a++;
           enum OutputType t = OutputType::getEnumByString(parameters[a]);
           if ( t == -1 )
               return CR_WRONG_USAGE;
           outputType = t;
        } else if ( parameters[a] == "-outputSortType" ) {
           if ( a+1 >= parameters.size() )
              return CR_WRONG_USAGE; 

           a++;
           enum OutputSortType t = OutputSortType::getEnumByString(parameters[a]);
           if ( t == -1 )
               return CR_WRONG_USAGE;
           outputSortType = t;
        } else if ( parameters[a] == "-repeatPolicy" ) {
           if ( a+1 >= parameters.size() )
              return CR_WRONG_USAGE; 

           a++;
           enum RepeatPolicy t = RepeatPolicy::getEnumByString(parameters[a]);
           if ( t == -1 )
               return CR_WRONG_USAGE;
           repeatPolicy = t;
        } else if ( parameters[a] == "-influencePolicy" ) {
           if ( a+1 >= parameters.size() )
              return CR_WRONG_USAGE; 

           a++;
           enum InfluencePolicy t = InfluencePolicy::getEnumByString(parameters[a]);
           if ( t == -1 )
               return CR_WRONG_USAGE;
           influencePolicy = t;
        } else if ( parameters[a] == "-reverse" ) {
            reverse = !reverse;
        } else {
            return CR_WRONG_USAGE;
        }
    }
    if ( a < parameters.size() )
        return CR_WRONG_USAGE;

    if ( outputType != OutputType::none ) {
        out.print("Heritage Parameters:\n"
                "  Name Scheme: %s\n"
                "  Name Collision Policy: %s\n"
                "  Output Type: %s\n"
                "  Output Sort Type: %s%s\n"
                "  Repeat Policy: %s\n"
                "  Influence Policy: %s\n",
                NameScheme::Names[nameScheme],
                NameCollisionPolicy::Names[nameCollisionPolicy],
                OutputType::Names[outputType],
                OutputSortType::Names[outputSortType],
                reverse ? " reversed" : "",
                RepeatPolicy::Names[repeatPolicy],
                InfluencePolicy::Names[influencePolicy]
        );
    }
    
    clock_t start = clock();
    int32_t civ_id = df::global::ui->civ_id;
    int32_t race_id = df::global::ui->race_id;
    int32_t language = -1;
    vector<DwarfWrapper> dwarves;
    for ( size_t a = 0; a < df::global::world->history.figures.size(); a++ ) {
        df::historical_figure* figure = df::global::world->history.figures[a];
        if ( figure->race != race_id )
            continue;
        if ( figure->flags.is_set(df::enums::histfig_flags::deity) )
            continue;
        DwarfWrapper wrapper;
        wrapper.historical = true;
        wrapper.local = false;
        wrapper.ptr = figure;
        wrapper.birthTime = figure->born_year*ticksPerYear + figure->born_seconds;
        wrapper.name = &figure->name;
        wrapper.childCount = 0;
        wrapper.childIndex = -1;
        wrapper.parent1Index = -1;
        wrapper.parent2Index = -1;
        wrapper.deathTime = figure->died_year == -1 ? -1 : figure->died_year*ticksPerYear + figure->died_seconds;
        dwarves.push_back(wrapper);
        if ( language == -1 )
            language = wrapper.name->language;
    }
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->race != race_id )
            continue;
        DwarfWrapper wrapper;
        wrapper.historical = false;
        wrapper.local = true;
        wrapper.ptr = unit;
        wrapper.birthTime = unit->relations.birth_year*ticksPerYear + unit->relations.birth_time;
        wrapper.name = &unit->name;
        wrapper.childCount = 0;
        wrapper.childIndex = -1;
        wrapper.parent1Index = -1;
        wrapper.parent2Index = -1;
        if ( unit->flags1.bits.dead == 0 )
            wrapper.deathTime = -1;
        else {
            int32_t deathId = unit->counters.death_id;
            int32_t deathIndex = df::death_info::binsearch_index(df::global::world->deaths.all, deathId);
            if ( deathIndex == -1 ) {
                out.print("Couldn't find death index.\n");
                return CR_FAILURE;
            }
            df::death_info* info = df::global::world->deaths.all[deathIndex];
            if ( info->event_year == -1 ) {
                out.print("Death event doesn't know when it happened?\n");
                return CR_FAILURE;
            }
            wrapper.deathTime = info->event_year*ticksPerYear + info->event_time;
        }
        dwarves.push_back(wrapper);
    }
    
    if ( outputType != OutputType::none )
        out.print("Sorting...\n");
    sort(dwarves.begin(), dwarves.end());
    if ( outputType != OutputType::none )
        out.print("...done sorting.\n");
    
    map<int32_t, int32_t> localIdToWrapper;
    map<int32_t, int32_t> historicalIdToWrapper;
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        if ( dwarves[a].historical ) {
            df::historical_figure* figure = (df::historical_figure*)dwarves[a].ptr;
            int32_t id = figure->id;
            historicalIdToWrapper[id] = a;
        } else {
            df::unit* unit = (df::unit*)dwarves[a].ptr;
            int32_t id = unit->id;
            localIdToWrapper[id] = a;
        }
    }
    
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        if ( !dwarves[a].historical ) {
            df::unit* unit = (df::unit*)dwarves[a].ptr;
            int32_t id = unit->id;
            if ( unit->hist_figure_id != -1 ) {
                int32_t index = df::historical_figure::binsearch_index(df::global::world->history.figures, unit->hist_figure_id);
                if ( index == -1 ) {
                    out.print("Error: couldn't find hist fig: %d -> %d\n", unit->hist_figure_id, index);
                    continue;
                }
                df::historical_figure* figure = df::global::world->history.figures[index];
                //historicalIdToWrapper[figure->id] = a;
                if ( historicalIdToWrapper.find(figure->id) == historicalIdToWrapper.end() ) {
                    out.print("Error monkopotomus: %s, %s\n", DFHack::Translation::TranslateName(dwarves[a].name, false).c_str(), DFHack::Translation::TranslateName(&figure->name, false).c_str());
                    continue;
                }
                dwarves[historicalIdToWrapper[figure->id]].local = true;
                localIdToWrapper[id] = historicalIdToWrapper[figure->id];
            }
        }
    }
    
    //compute parents
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        DwarfWrapper& wrapper = dwarves[a];
        
        if (wrapper.historical) {
            df::historical_figure* unit = (df::historical_figure*)wrapper.ptr;
            
            int32_t father = findRelation(unit, df::enums::histfig_hf_link_type::FATHER);
            if ( historicalIdToWrapper.find(father) != historicalIdToWrapper.end() ) {
                wrapper.parent1Index = historicalIdToWrapper[father];
            }
            
            int32_t mother = findRelation(unit, df::enums::histfig_hf_link_type::MOTHER);
            if ( historicalIdToWrapper.find(mother) != historicalIdToWrapper.end() ) {
                wrapper.parent2Index = historicalIdToWrapper[mother];
            }
            
            //out.print("%d historical: father %d, mother %d, parent1Index %d, parent2Index %d\n", a, father, mother, wrapper.parent1Index, wrapper.parent2Index );
        } else {
            df::unit* unit = (df::unit*)wrapper.ptr;
            
            if ( unit->hist_figure_id != -1 ) {
                int32_t index = df::historical_figure::binsearch_index(df::global::world->history.figures, unit->hist_figure_id);
                if ( index == -1 ) {
                    out.print("Error: couldn't find hist fig: %d -> %d\n", unit->hist_figure_id, index);
                    continue;
                }
                df::historical_figure* figure = df::global::world->history.figures[index];
                
                int32_t father = findRelation(figure, df::enums::histfig_hf_link_type::FATHER);
                if ( historicalIdToWrapper.find(father) != historicalIdToWrapper.end() ) {
                    wrapper.parent1Index = historicalIdToWrapper[father];
                }
                
                int32_t mother = findRelation(figure, df::enums::histfig_hf_link_type::MOTHER);
                if ( historicalIdToWrapper.find(mother) != historicalIdToWrapper.end() ) {
                    wrapper.parent2Index = historicalIdToWrapper[mother];
                }
                
                //out.print("%d: %s: histIndex=%d, father = %d, parent1Index = %d,"
                //    "mother = %d, parent2Index = %d\n", a, DFHack::Translation::TranslateName(wrapper.name, false).c_str(), index, father, wrapper.parent1Index,
                //        mother, wrapper.parent2Index);
            }
            
            if ( wrapper.parent1Index == -1 ) {
                wrapper.parent1Index = unit->relations.father_id;
                if ( wrapper.parent1Index != -1 ) {
                    if (localIdToWrapper.find(wrapper.parent1Index) == localIdToWrapper.end()) {
                        out.print("%s\n", DFHack::Translation::TranslateName(wrapper.name, false).c_str());
                        out.print("Error 3\n");
                        return CR_FAILURE;
                    } else
                        wrapper.parent1Index = localIdToWrapper[wrapper.parent1Index];
                }
            }
            
            if ( wrapper.parent2Index == -1 ) {
                wrapper.parent2Index = unit->relations.mother_id;
                if ( wrapper.parent2Index != -1 ) {
                    if (localIdToWrapper.find(wrapper.parent2Index) == localIdToWrapper.end()) {
                        out.print("Error 4\n");
                        return CR_FAILURE;
                    } else
                        wrapper.parent2Index = localIdToWrapper[wrapper.parent2Index];
                }
            }
            
            if ( wrapper.parent1Index != -1 && wrapper.parent2Index != -1 ) {
                //out.print("%d: parent1=%d, parent2=%d\n", a, wrapper.parent1Index, wrapper.parent2Index);
            }
        }
    }
    //now, all the information we need is collected into the wrappers
    
    string& seedStr = df::global::world->worldgen.worldgen_parms.name_seed;
    int32_t seed = 3;
    for ( size_t a = 0; a < seedStr.length(); a++ ) {
        seed *= 65537;
        seed += seedStr[a];
    }
    srand(seed);
    
    size_t wordmax = df::global::world->raws.language.words.size();
    vector<int32_t> nameFrequency(wordmax);
    vector<int32_t> nameFrequencyFort(wordmax);
    vector<int32_t> nameAliveFrequency(wordmax);
    vector<int32_t> nameAliveFrequencyFort(wordmax);
    vector<int32_t> nameFounder(wordmax, -1);
    vector<int32_t> nameFounderFort(wordmax, -1);
    vector<int32_t> nameLeader(wordmax, -1);
    vector<int32_t> nameLeaderFort(wordmax, -1);
    vector<int32_t> influence(dwarves.size(), 0);
    FullNameSet finalNames((int)(2.0f * dwarves.size()));
    
    NameSorter sorter(&nameFounder);
    sorter.out = &out;
    
    vector<int32_t> permutation;
    for ( size_t a = 0; a < wordmax; a++ ) {
        permutation.push_back(a);
    }
    for ( size_t a = 0; a < wordmax; a++ ) {
        int32_t index = (int32_t)((wordmax-a)*(rand() / (1.0f+RAND_MAX))) + a;
        int32_t temp = permutation[a];
        permutation[a] = permutation[index];
        permutation[index] = temp;
    }
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        if ( a % 10000 == 0 ) {
            if ( outputType != OutputType::none )
                out.print("a == %d / %d\n", a, dwarves.size());
        }
        int32_t newName[2];
        if ( dwarves[a].name == NULL ) continue;
        if ( dwarves[a].name->words[0] == -1 || dwarves[a].name->words[1] == -1 ) continue;
        
        if ( !dwarves[a].historical ) {
            df::unit* unit = (df::unit*)dwarves[a].ptr;
            if ( unit->hist_figure_id != -1 ) {
                continue;
            }
        }
        
        newName[0] = dwarves[a].name->words[0];
        newName[1] = dwarves[a].name->words[1];
        if ( dwarves[a].parent1Index == -1 || dwarves[a].parent2Index == -1 ) {
            //no parents (or missing one): new name
            for ( size_t b = 0; b < 2; b++ ) {
                newName[b] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
            }
        } else {
            vector<int32_t> names;
            
            //out.print("found both parents! parent1Index = %d, name = 0x%X\n", dwarves[a].parent1Index, dwarves[dwarves[a].parent1Index].name);
            DwarfWrapper& parent1 = dwarves[dwarves[a].parent1Index];
            DwarfWrapper& parent2 = dwarves[dwarves[a].parent2Index];
            for ( size_t b = 0; b < 2; b++ )
                names.push_back(parent1.name->words[b]);
            for ( size_t b = 0; b < 2; b++ )
                names.push_back(parent2.name->words[b]);
            
            sort(names.begin(), names.end(), sorter);
            
            dwarves[a].childIndex = parent1.childCount;
            if ( parent1.childCount != parent2.childCount ) {
                out.print("%d Octopus in a jar of almonds.\n", a);
                continue;
            }
            dwarves[a].childIndex = parent1.childCount;
            parent1.childCount++;
            parent2.childCount++;

            int32_t err = nameDwarf(dwarves[a], parent1, parent2, nameScheme, names, newName);
            if ( err < 0 ) {
                out.print("Error naming dwarf.\n");
                return CR_FAILURE;
            }
        }
        
        //handleConflicts(df::language_name& name, set<string>& finalNames, map<int32_t, int32_t> &nameFrequency, int wordmax)
        dwarves[a].name->words[0] = newName[0];
        dwarves[a].name->words[1] = newName[1];
        
        int32_t before[2] = {newName[0], newName[1]};
        handleConflicts(dwarves[a].name, finalNames, nameFrequency, permutation, wordmax, out, nameCollisionPolicy);
        newName[0] = dwarves[a].name->words[0];
        newName[1] = dwarves[a].name->words[1];
        if ( nameCollisionPolicy != NameCollisionPolicy::ignore && newName[0] == newName[1] ) {
            out.print("Repeated name! %s\n", DFHack::Translation::TranslateName(dwarves[a].name, false).c_str());
            out.print("    (%d, %d) -> (%d, %d)\n", before[0], before[1], newName[0], newName[1]);
        }
        
        //sort name
        if ( (nameFounder[newName[0]] != -1 && nameFounder[newName[1]] != -1 && nameFounder[newName[0]] > nameFounder[newName[1]]) || (nameFounder[newName[0]] == -1 && nameFounder[newName[1]] != -1) ) {
            int32_t temp = newName[0];
            newName[0] = newName[1];
            newName[1] = temp;
        }
        dwarves[a].name->words[0] = newName[0];
        dwarves[a].name->words[1] = newName[1];
        
        if ( dwarves[a].historical ) {
            df::historical_figure* hist = (df::historical_figure*)dwarves[a].ptr;
            if ( hist->unit_id != -1 ) {
                int unitIndex = df::unit::binsearch_index(df::global::world->units.all, hist->unit_id);
                if ( unitIndex != -1 ) {
                    df::unit* unit = df::global::world->units.all[unitIndex];
                    unit->name.words[0] = newName[0];
                    unit->name.words[1] = newName[1];
                }
            }
        }
        
        for ( size_t b = 0; b < 2; b++ ) {
            nameFrequency[newName[b]]++;
            if ( dwarves[a].local )
                nameFrequencyFort[newName[b]]++;
            if ( dwarves[a].deathTime == -1 ) {
                nameAliveFrequency[newName[b]]++;
                if ( dwarves[a].local )
                    nameAliveFrequencyFort[newName[b]]++;
                if ( nameLeader[newName[b]] == -1 )
                    nameLeader[newName[b]]  = a;
                if ( dwarves[a].local && nameLeaderFort[newName[b]] == -1 )
                    nameLeaderFort[newName[b]] = a;
            }
            if ( nameFounder[newName[b]] == -1 )
                nameFounder[newName[b]] = 2*a+b;
            if ( dwarves[a].local && nameFounderFort[newName[b]] == -1 )
                nameFounderFort[newName[b]] = 2*a+b;
        }

        if ( nameCollisionPolicy != NameCollisionPolicy::ignore )
            finalNames.insert(dwarves[a].name);
        
        //int32_t asdf1 = dwarves[a].name->words[0];
        //out.print("%d\n", asdf1);
    }
    
    if ( outputType == OutputType::none || outputType == OutputType::progress )
        return CR_OK;
    
    out.print("Computing influence.\n");
    //compute influence
    for ( int32_t a = dwarves.size()-1; a >= 0; a-- ) {
        if ( dwarves[a].deathTime != -1 )
            continue;
        if ( !dwarves[a].historical ) {
            df::unit* unit = (df::unit*)dwarves[a].ptr;
            if ( unit->hist_figure_id != -1 ) {
                continue;
            }
        }
        if ( dwarves[a].name == NULL ) continue;
        int32_t name = dwarves[a].name->words[0];
        if ( nameLeader[name] == -1 ) {
            out.print("Error: no leader for %d\n", name);
            continue;
        }
        influence[a]++;
        
        int32_t leaderIndex = nameLeader[name];
        if ( leaderIndex != a ) {
            influence[leaderIndex] += influence[a];
            influence[a] = 0;
        }
    }
    
    vector<int32_t> nameInfluence;
    for ( size_t a = 0; a < wordmax; a++ ) {
        if ( nameLeader[a] != -1 ) {
            nameInfluence.push_back(influence[nameLeader[a]]);
        } else
            nameInfluence.push_back(0);
    }
    
    vector<int32_t> sorted;
    for ( size_t a = 0; a < wordmax; a++ ) {
        sorted.push_back(a);
    }
    if ( outputSortType == OutputSortType::aliveUses )
        sorter.favorite = &nameAliveFrequency;
    else if ( outputSortType == OutputSortType::fortAliveUses )
        sorter.favorite = &nameAliveFrequencyFort;
    else if ( outputSortType == OutputSortType::fortLeaderAge )
        sorter.favorite = &nameLeaderFort;
    else if ( outputSortType == OutputSortType::fortUses )
        sorter.favorite = &nameFrequencyFort;
    else if ( outputSortType == OutputSortType::influence )
        sorter.favorite == &influence;
    else if ( outputSortType == OutputSortType::leaderAge )
        sorter.favorite = &nameLeader;
    else if ( outputSortType == OutputSortType::nameAge )
        sorter.favorite = &nameFounder;
    else if ( outputSortType == OutputSortType::totalUses )
        sorter.favorite = &nameFrequency;

    AlphabeticSorter alphaSort;
    if ( outputSortType == OutputSortType::alphabeticTranslated ) {
        //df::global::world->raws.language. 
        alphaSort.favorite.wordPtr = &df::global::world->raws.language.words;
        alphaSort.str = false;
        sort(sorted.begin(), sorted.end(), alphaSort);
    } else if ( outputSortType == OutputSortType::alphabeticUntranslated ) {
        alphaSort.favorite.strVector = &df::global::world->raws.language.translations[language]->words;
        alphaSort.str = true;
        sort(sorted.begin(), sorted.end(), alphaSort);
    } else if ( outputSortType == OutputSortType::none ) {

    } else {
        sort(sorted.begin(), sorted.end(), sorter);
    }
    
    clock_t end = clock();

    out.print("printing data\n");
    //for ( int32_t a = wordmax-1; a >= 0; a-- ) {
    for ( size_t a = 0; a < wordmax; a++ ) {
        int32_t i = sorted[a];
        if ( reverse )
            i = sorted[wordmax-1-a];
        if ( nameFounder[i] == -1 )
            continue;
        if ( outputType != OutputType::allNames && nameLeader[i] == -1 )
            continue;
        if ( outputType == OutputType::fortNames && nameFrequencyFort[i] == -1 )
            continue;
        if ( outputType == OutputType::fortAliveNames && nameAliveFrequencyFort[i] == 0 )
            continue;
        if ( outputType == OutputType::aliveNames && nameAliveFrequency[i] == 0 )
            continue;
        //if ( nameInfluence[i] == 0 )
        //    continue;
        //if ( a != 0 && nameLeader[sorted[a-1]] == nameLeader[i] )
        //    continue;
        //out.print("%d, %d\n", a, i);
        out.print("%s\n", df::global::world->raws.language.translations[language]->words[i]->c_str());
        out.print("    Founded in %d by %s\n", dwarves[nameFounder[i]/2].birthTime/ticksPerYear, DFHack::Translation::TranslateName(dwarves[nameFounder[i]/2].name, false).c_str());
        if ( nameLeader[i] != -1 ) {
            out.print("    Led by %s (born %d)\n"
                    "      influence: %d\n",
                    DFHack::Translation::TranslateName(dwarves[nameLeader[i]].name, false).c_str(),
                    dwarves[nameLeader[i]].birthTime/ticksPerYear,
                    nameInfluence[i]
                    );
        }
        if ( nameLeaderFort[i] != -1 ) {
            out.print("    Locally led by %s (born %d)\n",
                    DFHack::Translation::TranslateName(dwarves[nameLeaderFort[i]].name, false).c_str());
        }
        out.print("    Living members: %d\n", nameAliveFrequency[i]);
        out.print("    Living fort members: %d\n", nameAliveFrequencyFort[i]);
        out.print("    Total members: %d\n", nameFrequency[i]);
        out.print("    Total fort members: %d\n", nameFrequencyFort[i]);

    }
    
    float time = (float)(end - start)/CLOCKS_PER_SEC;
    out.print("Total time: %f\nTotal dwarves: %d\nDwarves per second: %f\n", time, dwarves.size(), dwarves.size() / time);
    
    //return writeNameHistory(out, dwarves);
    return CR_OK;
}


struct Event {
    int32_t time;
    bool birth;
    size_t who;
    
    bool operator<(const Event& e) const {
        return time < e.time;
    }
};


command_result writeNameHistory(color_ostream& out, vector<DwarfWrapper>& dwarves) {
    vector<Event> events;
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        if ( !dwarves[a].historical ) {
            df::unit* unit = (df::unit*)dwarves[a].ptr;
            if ( unit->hist_figure_id != -1 ) {
                continue;
            }
        }
        
        Event e;
        e.time = dwarves[a].birthTime;
        e.birth = true;
        e.who = a;
        events.push_back(e);
        
        if ( dwarves[a].deathTime != -1 ) {
            e.time = dwarves[a].deathTime;
            e.birth = false;
            e.who = a;
            events.push_back(e);
        }
    }
    
    sort(events.begin(), events.end());
    
    size_t wordmax = df::global::world->raws.language.words.size();
    vector<vector<int32_t> > nameFrequencyHistory(wordmax);
    vector<int32_t> nameFrequency(wordmax);
    vector<int32_t> years;
    
    //NameSorter sorter(&nameFounder);
    //sorter.out = &out;
    
    vector<set<DwarfWrapper> > nameHolderAges(wordmax);
    
    int32_t year = 0;
    for ( size_t a = 0; a < events.size(); a++ ) {
        DwarfWrapper& wrap = dwarves[events[a].who];
        if ( events[a].birth ) {
            for ( int b = 0; b < 2; b++ ) {
                nameFrequency[wrap.name->words[b]]++;
                nameHolderAges[wrap.name->words[b]].insert(wrap);
            }
        } else {
            for ( int b = 0; b < 2; b++ ) {
                nameFrequency[wrap.name->words[b]]--;
                nameHolderAges[wrap.name->words[b]].erase(wrap);
            }
        }
        
        if ( a+1 >= events.size() || events[a+1].time/ticksPerYear > year ) {
            for ( size_t b = 0; b < wordmax; b++ ) {
                nameFrequencyHistory[b].push_back(nameFrequency[b]);
            }
            years.push_back(year);
            if ( a+1 < events.size() ) {
                year = events[a+1].time/ticksPerYear;
            }
        }
    }
    
    return CR_OK;
}


