#include "PluginManager.h"
#include "Export.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/unit.h"
#include "df/ui.h"
#include "df/world_history.h"
#include "df/historical_figure.h"
#include "df/histfig_hf_link.h"
#include "modules/Translation.h"

#include <map>
#include <algorithm>
#include <cstdlib>
#include <set>
#include <vector>
#include <string>

/*
TODO
    parts of speech
    consider making part of the script in LUA for easier modability
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
struct DwarfWrapper {
    bool historical;
    void* ptr;
    int32_t age; //actually birth time
    df::language_name* name;
    int32_t childCount;
    int32_t childIndex;
    int32_t parent1Index;
    int32_t parent2Index;
    bool alive;
    
    bool operator<(const DwarfWrapper a) const {
        return age < a.age;
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
            out->print("Doomed rhombus.\n"); ///////////////////////////////////////////////////test -1
        }*/
        int32_t founder1 = (*favorite)[a];
        int32_t founder2 = (*favorite)[b];
        return founder1 < founder2;
    }
};

void handleConflicts(df::language_name* name, set<string>& finalNames, vector<int32_t> &nameFrequency, int wordmax, color_ostream& out) {
    if (name->words[0] != name->words[1] && finalNames.find(DFHack::Translation::TranslateName(name)) == finalNames.end()) return;
    
#if 0
    //try a random unused name: unused name -> unused full name
    int32_t unused = -1;
    int32_t count = 0;
    for ( int32_t a = 0; a < wordmax; a++ ) {
        if ( a == name->words[0] ) continue;
        if ( nameFrequency[a] != 0 ) continue;
        
        count++;
        if ( (rand() / (1.0f+RAND_MAX)) < 1.0f / count ) {
            unused = a;
        }
    }
    if ( unused != -1 ) {
        name->words[1] = unused;
        //out.print("new: %d, %d\n", name->words[0], name->words[1]);
        if (finalNames.find(DFHack::Translation::TranslateName(name)) == finalNames.end()) return;
    }
    
    //try setting second name to something random
    for ( size_t a = 0; a < 10; a++ ) {
        name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
        if (name->words[0] == name->words[1]) continue;
        if (finalNames.find(DFHack::Translation::TranslateName(name)) == finalNames.end()) return;
    }
    
    //try all second names
    for ( int32_t a = 0; a < wordmax; a++ ) {
        name->words[1] = a;
        if (name->words[0] == name->words[1]) continue;
        if (finalNames.find(DFHack::Translation::TranslateName(name)) == finalNames.end()) return;
    }
#endif
    
    //try random names
    for ( size_t a = 0; a < 2*wordmax; a++ ) {
        name->words[0] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
        name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
        if (name->words[0] == name->words[1]) continue;
        if (finalNames.find(DFHack::Translation::TranslateName(name)) == finalNames.end()) return;
    }
    
    //give up on uniqueness: plain old random name
    name->words[0] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
    name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
    while(name->words[0] == name->words[1]) {
        name->words[1] = (int32_t)(rand() / (1.0f + RAND_MAX) * wordmax);
    }
}

command_result heritage(color_ostream& out, vector<string>& parameters) {
    //out.print("Hello!\n");
    
    int32_t civ_id = df::global::ui->civ_id;
    int32_t race_id = df::global::ui->race_id;
    vector<DwarfWrapper> dwarves;
    for ( size_t a = 0; a < df::global::world->history.figures.size(); a++ ) {
        df::historical_figure* figure = df::global::world->history.figures[a];
        if ( figure->race != race_id )
            continue;
        if ( figure->flags.is_set(df::enums::histfig_flags::deity) )
            continue;
        DwarfWrapper wrapper;
        wrapper.historical = true;
        wrapper.ptr = figure;
        wrapper.age = figure->born_year*ticksPerYear + figure->born_seconds;
        wrapper.name = &figure->name;
        wrapper.childCount = 0;
        wrapper.childIndex = -1;
        wrapper.parent1Index = -1;
        wrapper.parent2Index = -1;
        wrapper.alive = figure->died_year == -1;
        dwarves.push_back(wrapper);
    }
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->race != race_id )
            continue;
        DwarfWrapper wrapper;
        wrapper.historical = false;
        wrapper.ptr = unit;
        wrapper.age = unit->relations.birth_year*ticksPerYear + unit->relations.birth_time;
        wrapper.name = &unit->name;
        wrapper.childCount = 0;
        wrapper.childIndex = -1;
        wrapper.parent1Index = -1;
        wrapper.parent2Index = -1;
        wrapper.alive = unit->flags1.bits.dead == 0;
        dwarves.push_back(wrapper);
    }
    
    out.print("Sorting...\n");
    sort(dwarves.begin(), dwarves.end());
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
                size_t index = df::historical_figure::binsearch_index(df::global::world->history.figures, unit->hist_figure_id);
                if ( index == -1 ) {
                    out.print("Error: couldn't find hist fig: %d -> %d\n", unit->hist_figure_id, index);
                    continue;
                }
                df::historical_figure* figure = df::global::world->history.figures[index];
                //historicalIdToWrapper[figure->id] = a;
                if ( historicalIdToWrapper.find(figure->id) == historicalIdToWrapper.end() ) {
                    out.print("Error monkopotomus: %s, %s\n", DFHack::Translation::TranslateName(dwarves[a].name).c_str(), DFHack::Translation::TranslateName(&figure->name).c_str());
                    continue;
                }
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
                size_t index = df::historical_figure::binsearch_index(df::global::world->history.figures, unit->hist_figure_id);
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
                //    "mother = %d, parent2Index = %d\n", a, DFHack::Translation::TranslateName(wrapper.name).c_str(), index, father, wrapper.parent1Index,
                //        mother, wrapper.parent2Index);
            }
            
            if ( wrapper.parent1Index == -1 ) {
                wrapper.parent1Index = unit->relations.father_id;
                if ( wrapper.parent1Index != -1 ) {
                    if (localIdToWrapper.find(wrapper.parent1Index) == localIdToWrapper.end()) {
                        out.print("%s\n", DFHack::Translation::TranslateName(wrapper.name).c_str());
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
    
    srand(57);
    
    out.print("Final loop.\n");
    //now, all the information we need is collected into the wrappers
    set<string> finalNames;
    vector<int32_t> nameFrequency;
    vector<int32_t> nameAliveFrequency;
    vector<int32_t> nameFounder;
    NameSorter sorter(&nameFounder);
    sorter.out = &out;
    size_t wordmax = df::global::world->raws.language.words.size();
    for ( int32_t a = 0; a < wordmax; a++ ) {
        nameFrequency.push_back(0);
        nameAliveFrequency.push_back(0);
        nameFounder.push_back(-1);
    }
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        if ( a % 1000 == 0 ) {
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
            //TODO: nameAge
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
            
            //TODO: sort names by name age
            dwarves[a].childIndex = parent1.childCount;
            if ( parent1.childCount != parent2.childCount ) {
                out.print("%d Octopus in a jar of almonds.\n", a);
                continue;
            }
            dwarves[a].childIndex = parent1.childCount;
            parent1.childCount++;
            parent2.childCount++;
            
            int32_t index = dwarves[a].childIndex % 6;
            switch(index) {
                case 0: newName[0] = names[0];
                        newName[1] = names[1];
                        break;
                case 1: newName[0] = names[0];
                        newName[1] = names[2];
                        break;
                case 2: newName[0] = names[0];
                        newName[1] = names[3];
                        break;
                case 3: newName[0] = names[1];
                        newName[1] = names[2];
                        break;
                case 4: newName[0] = names[1];
                        newName[1] = names[3];
                        break;
                case 5: newName[0] = names[2];
                        newName[1] = names[3];
                        break;
            }
        }
        
        //handleConflicts(df::language_name& name, set<string>& finalNames, map<int32_t, int32_t> &nameFrequency, int wordmax)
        dwarves[a].name->words[0] = newName[0];
        dwarves[a].name->words[1] = newName[1];
        
        int32_t before[2] = {newName[0], newName[1]};
        handleConflicts(dwarves[a].name, finalNames, nameFrequency, wordmax, out);
        newName[0] = dwarves[a].name->words[0];
        newName[1] = dwarves[a].name->words[1];
        if ( newName[0] == newName[1] ) {
            out.print("Repeated name! %s\n", DFHack::Translation::TranslateName(dwarves[a].name).c_str());
            out.print("    (%d, %d) -> (%d, %d)\n", before[0], before[1], newName[0], newName[1]);
        }
        
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
            if ( dwarves[a].alive )
                nameAliveFrequency[newName[b]]++;
            if ( nameFounder[newName[b]] != -1) {
                continue;
            }
            nameFounder[newName[b]] = 2*a+b;
        }
        finalNames.insert(DFHack::Translation::TranslateName(dwarves[a].name));
    }
    
    vector<int32_t> sorted;
    for ( int32_t a = 0; a < wordmax; a++ ) {
        sorted.push_back(a);
    }
    sorter.favorite = &nameAliveFrequency;
    sort(sorted.begin(), sorted.end(), sorter);
    //for ( int32_t a = wordmax-1; a >= 0; a-- ) {
    for ( int32_t a = 0; a < wordmax; a++ ) {
        int32_t i = sorted[a];
        if ( nameFounder[i] == -1 )
            continue;
        out.print("%15s: Founded by %30s in %4d: %4d had it, %4d have it.\n", 
            df::global::world->raws.language.translations[0]->words[i]->c_str(),
            DFHack::Translation::TranslateName(dwarves[nameFounder[i]/2].name).c_str(),
            dwarves[nameFounder[i]/2].age/ticksPerYear,
            nameFrequency[i],
            nameAliveFrequency[i]);
    }
    
    return CR_OK;
}

#if 0
command_result heritage2(color_ostream& out, vector<string>& parameters) {
    /*map<void*, DwarfWrapper> parent1;
    map<void*, DwarfWrapper> parent2;
    for ( size_t a = 0; a < df::global::world->history.figures.size(); a++ ) {
        df::historical_figure* figure = df::global::world->history.figures[a];
        
    }*/
    
    //TODO: does race matter, or is civ sufficient?
    int32_t civ_id = df::global::ui->civ_id;
    vector<DwarfWrapper> dwarves;
    for ( size_t a = 0; a < df::global::world->history.figures.size(); a++ ) {
        df::historical_figure* figure = df::global::world->history.figures[a];
        //if ( figure->unit_id != -1 )
        //    continue; //don't put historical fort dwarves
        if ( figure->civ_id != civ_id )
            continue;
        DwarfWrapper wrapper;
        wrapper.historical = true;
        wrapper.ptr = figure;
        wrapper.age = figure->born_year*ticksPerYear + figure->born_seconds;
        wrapper.name = &figure->name;
        dwarves.push_back(wrapper);
    }
    
    for ( size_t a = 0; a < df::global::world->units.all.size(); a++ ) {
        df::unit* unit = df::global::world->units.all[a];
        if ( unit->civ_id != civ_id )
            continue;
        if ( unit->historical_id != -1 )
            continue;
        DwarfWrapper wrapper;
        wrapper.historical = false;
        wrapper.ptr = unit;
        wrapper.age = unit->relations.birth_year*ticksPerYear + unit->relations.birth_seconds;
        wrapper.name = &unit->name;
        dwarves.push_back(wrapper);
    }
    
    sort(dwarves);
    
    map<int32_t, int32_t> localIdToWrapper;
    map<int32_t, int32_t> historicalIdToWrapper;
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        if ( dwarves[a].historical ) {
            int32_t id = ((df::historical_figure*)dwarves[a].ptr)->id;
            historicalIdToWrapper[id] = a;
        } else {
            int32_t id = ((df::unit*)dwarves[a]->ptr)->id;
            localIdToWrapper[id] = a;
        }
    }
    
    vector<int32_t> nameAge;
    vector<int32_t> childCount;
    vector<int32_t> childIndex;
    vector<int32_t> parent1Table;
    vector<int32_t> parent2Table;
    for ( size_t a = 0; a < dwarves.size(); a++ ) {
        DwarfWrapper wrapper = dwarves[a];
        for ( size_t b = 0; b < 2; b++ ) {
            int32_t name = wrapper.name->words[b];
            while(nameAge.size() <= name)
                nameAge.push_back(-1);
            if ( nameAge[name] == -1 )
                nameAge[name] = a;
        }
        
        childCount.push_back(0);
        childIndex.push_back(-1);
        
        int32_t parent1Index=-1;
        int32_t parent2Index=-1;
        if (wrapper.isHistorical()) {
            df::historical_figure* unit = (df::historical_figure*)wrapper.ptr;
            for ( size_t b = 0; b < unit->histfig_links.size(); b++ ) {
                df::histfig_hf_link* link = unit->histfig_links[b];
                if ( link->getType() == df::enums::histfig_hf_link_type::MOTHER ) {
                    assert(historicalIdToWrapper.find(link->anon_1) != historicalIdToWrapper.end());
                    parent1Index = historicalIdToWrapper[link->anon_1];
                } else if ( link->getType() == df::enums::histfig_hf_link_type::FATHER ) {
                    assert(historicalIdToWrapper.find(link->anon_1) != historicalIdToWrapper.end());
                    parent2Index = historicalIdToWrapper[link->anon_1];
                }
            }
        } else {
            df::unit* unit = (df::unit*)wrapper.ptr;
            parent1Index = unit->relations.father_id;
            parent2Index = unit->relations.mother_id;
            if ( parent1Index != -1 ) {
                assert(localIdToWrapper.find(parent1Index) != localIdToWrapper.end());
                parent1Index = localIdToWrapper[parent1Index];
            }
            if ( parent2Index != -1 ) {
                assert(localIdToWrapper.find(parent2Index) != localIdToWrapper.end());
                parent2Index = localIdToWrapper[parent2Index];
            }
        }
        
        if ( parent1Index == -1 && parent2Index == -1 )
            continue;
        
        if ( parent1Index != -1 && parent2Index != -1 )
            assert(childCount[parent1Index] != childCount[parent2Index]);
        
        int index = -1;
        if ( parent1Index != -1 )
            index = childCount[parent1Index]++;
        if ( parent2Index != -1 )
            index = childCount[parent2Index]++;
        childIndex[a] = index;
    }
    
    
    
    //find name age, parents, child index
    /*
    for each dwarf {
        int32_t birthTime = blah;
        for each name {
            if (nameExists[name] == false || nameAge[name] > birthTime) {
                nameExists[name] = true;
                nameAge[name] = birthTime;
            }
        }
        
        parent1[dwarf] = dwarf.father;
        parent2[dwarf] = dwarf.mother;
        childIndex[dwarf] = childCount[parent1[dwarf]]++;
    }
    */
    
    /*
    //set the new names
    for each dwarf {
        auto parent1 = parent1[dwarf];
        auto parent2 = parent2[dwarf];
        if ( parent1 == null || parent2 == null )
            continue;
        int childIndex = getChildIndex(dwarf);
        assert(childIndex != -1);
        childIndex %= 6;
        
        int32_t names[4] = {parent1.name[0], parent1.name[1], parent2.name[0], parent2.name[1]};
        sort(names, nameAge);
        
        int32_t newName[2];
        switch(childIndex) {
        case 0: newName = {names[0],names[1]}; break;
        case 1: newName = {names[0],names[2]}; break;
        case 2: newName = {names[0],names[3]}; break;
        case 3: newName = {names[1],names[2]}; break;
        case 4: newName = {names[1],names[3]}; break;
        case 5: newName = {names[2],names[3]}; break;
        default: assert(false);
        }
        
        dwarf.name.words[0] = newName[0];
        dwarf.name.words[1] = newName[1];
    }
    */
    
    return CR_OK;
}
#endif
