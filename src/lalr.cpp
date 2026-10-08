#include "lalr.h"

#include <algorithm>
#include <map>
#include <sstream>

using namespace std;

typedef pair<int, int> CoreKey;

static vector<CoreKey> coreOf(const State& st) {
    vector<CoreKey> core;
    for (size_t k = 0; k < st.items.size(); ++k)
        core.push_back(CoreKey(st.items[k].prod, st.items[k].dot));
    sort(core.begin(), core.end());
    return core;
}

LRMachine buildLALRMachine(const Grammar& g, const LRMachine& clrMachine) {
    (void)g;
    LRMachine m;
    m.name = "LALR";
    m.kind = MachineKind::LALR;

    const size_t n = clrMachine.states.size();
    // Group by core
    map<vector<CoreKey>, int> groupOf;
    vector<vector<int> > groups;
    vector<int> stateGroup(n, -1);
    for (size_t s = 0; s < n; ++s) {
        vector<CoreKey> core = coreOf(clrMachine.states[s]);
        map<vector<CoreKey>, int>::const_iterator found = groupOf.find(core);
        int gid;
        if (found == groupOf.end()) {
            gid = (int)groups.size();
            groupOf[core] = gid;
            groups.push_back(vector<int>());
        } else {
            gid = found->second;
        }
        groups[(size_t)gid].push_back((int)s);
        stateGroup[s] = gid;
    }

    // Merge lookaheads
    for (size_t gid = 0; gid < groups.size(); ++gid) {
        const vector<int>& members = groups[gid];
        State st;
        st.id = (int)gid;
        map<CoreKey, set<string> > merged;
        for (size_t q = 0; q < members.size(); ++q) {
            const State& src = clrMachine.states[(size_t)members[q]];
            for (size_t k = 0; k < src.items.size(); ++k) {
                CoreKey key(src.items[k].prod, src.items[k].dot);
                set<string>::const_iterator lIt;
                for (lIt = src.items[k].lookaheads.begin();
                     lIt != src.items[k].lookaheads.end(); ++lIt)
                    merged[key].insert(*lIt);
            }
        }
        map<CoreKey, set<string> >::const_iterator mIt;
        for (mIt = merged.begin(); mIt != merged.end(); ++mIt) {
            ItemRep item;
            item.prod = mIt->first.first;
            item.dot = mIt->first.second;
            item.lookaheads = mIt->second;
            st.items.push_back(item);
        }
        if (members.size() > 1) {
            ostringstream note;
            note << "merged CLR states { ";
            for (size_t q = 0; q < members.size(); ++q) {
                if (q) note << ", ";
                note << members[q];
            }
            note << " }";
            st.note = note.str();
        } else {
            ostringstream note;
            note << "from CLR state " << members[0];
            st.note = note.str();
        }
        m.states.push_back(st);
    }

    // Remap transitions
    for (size_t gid = 0; gid < groups.size(); ++gid) {
        const vector<int>& members = groups[gid];
        map<string, int> remapped;
        for (size_t q = 0; q < members.size(); ++q) {
            const State& src = clrMachine.states[(size_t)members[q]];
            map<string, int>::const_iterator tr;
            for (tr = src.transitions.begin(); tr != src.transitions.end(); ++tr) {
                int targetGroup = stateGroup[(size_t)tr->second];
                map<string, int>::const_iterator have =
                    remapped.find(tr->first);
                if (have == remapped.end()) remapped[tr->first] = targetGroup;
            }
        }
        m.states[gid].transitions = remapped;
    }
    return m;
}
