#include "slr.h"

#include <algorithm>
#include <map>

using namespace std;

typedef pair<int, int> LR0Key;

// LR(0) closure
static vector<LR0Key> closureLR0(const Grammar& g, const vector<LR0Key>& kernel) {
    vector<LR0Key> items = kernel;
    set<LR0Key> seen(kernel.begin(), kernel.end());
    size_t head = 0;
    while (head < items.size()) {
        LR0Key cur = items[head++];
        const Production& p = g.production(cur.first);
        if (cur.second >= (int)p.rhs.size()) continue;
        const string& next = p.rhs[(size_t)cur.second];
        if (!g.isNonterminal(next)) continue;
        const vector<int>& prods = g.productionsOf(next);
        for (size_t k = 0; k < prods.size(); ++k) {
            LR0Key fresh(prods[k], 0);
            if (seen.insert(fresh).second) items.push_back(fresh);
        }
    }
    sort(items.begin(), items.end());
    return items;
}

// LR(0) goto
static vector<LR0Key> gotoLR0(const Grammar& g, const vector<LR0Key>& items,
                              const string& symbol) {
    vector<LR0Key> moved;
    for (size_t k = 0; k < items.size(); ++k) {
        const Production& p = g.production(items[k].first);
        if (items[k].second < (int)p.rhs.size() &&
            p.rhs[(size_t)items[k].second] == symbol) {
            moved.push_back(LR0Key(items[k].first, items[k].second + 1));
        }
    }
    if (moved.empty()) return moved;
    return closureLR0(g, moved);
}

// Build SLR states
LRMachine buildSLRMachine(const Grammar& g) {
    LRMachine m;
    m.name = "SLR";
    m.kind = MachineKind::SLR;

    vector<LR0Key> start;
    start.push_back(LR0Key(0, 0));
    vector<vector<LR0Key> > kernels;
    kernels.push_back(closureLR0(g, start));

    map<vector<LR0Key>, int> stateOf;
    stateOf[kernels[0]] = 0;

    vector<string> symbols = g.terminals();
    const vector<string>& nts = g.nonterminals();
    for (size_t k = 0; k < nts.size(); ++k) {
        if (nts[k] != "$") symbols.push_back(nts[k]);
    }

    size_t head = 0;
    vector<map<string, int> > transitions;
    transitions.push_back(map<string, int>());
    while (head < kernels.size()) {
        vector<LR0Key> cur = kernels[head];
        for (size_t s = 0; s < symbols.size(); ++s) {
            vector<LR0Key> target = gotoLR0(g, cur, symbols[s]);
            if (target.empty()) continue;
            map<vector<LR0Key>, int>::const_iterator found =
                stateOf.find(target);
            int targetId;
            if (found == stateOf.end()) {
                targetId = (int)kernels.size();
                stateOf[target] = targetId;
                kernels.push_back(target);
                transitions.push_back(map<string, int>());
            } else {
                targetId = found->second;
            }
            transitions[head][symbols[s]] = targetId;
        }
        ++head;
    }

    for (size_t s = 0; s < kernels.size(); ++s) {
        State st;
        st.id = (int)s;
        for (size_t k = 0; k < kernels[s].size(); ++k) {
            ItemRep item;
            item.prod = kernels[s][k].first;
            item.dot = kernels[s][k].second;
            st.items.push_back(item);
        }
        st.transitions = transitions[s];
        m.states.push_back(st);
    }
    return m;
}
