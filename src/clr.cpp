#include "clr.h"

#include <algorithm>
#include <map>

using namespace std;

struct LR1Triple {
    int prod;
    int dot;
    string look;
    bool operator<(const LR1Triple& o) const {
        if (prod != o.prod) return prod < o.prod;
        if (dot != o.dot) return dot < o.dot;
        return look < o.look;
    }
    bool operator==(const LR1Triple& o) const {
        return prod == o.prod && dot == o.dot && look == o.look;
    }
};

// LR(1) closure
static vector<LR1Triple> closureLR1(const Grammar& g, const FirstFollow& ff,
                                    const vector<LR1Triple>& kernel) {
    vector<LR1Triple> items = kernel;
    set<LR1Triple> seen(kernel.begin(), kernel.end());
    size_t head = 0;
    while (head < items.size()) {
        LR1Triple cur = items[head++];
        const Production& p = g.production(cur.prod);
        if (cur.dot >= (int)p.rhs.size()) continue;
        const string& next = p.rhs[(size_t)cur.dot];
        if (!g.isNonterminal(next)) continue;
        vector<string> beta;
        for (size_t k = (size_t)cur.dot + 1; k < p.rhs.size(); ++k)
            beta.push_back(p.rhs[k]);
        beta.push_back(cur.look);
        set<string> firsts = ff.firstOfSequence(beta);
        const vector<int>& prods = g.productionsOf(next);
        for (size_t q = 0; q < prods.size(); ++q) {
            set<string>::const_iterator fIt;
            for (fIt = firsts.begin(); fIt != firsts.end(); ++fIt) {
                if (*fIt == FirstFollow::EPS) continue;
                LR1Triple fresh;
                fresh.prod = prods[q];
                fresh.dot = 0;
                fresh.look = *fIt;
                if (seen.insert(fresh).second) items.push_back(fresh);
            }
        }
    }
    return items;
}

// LR(1) goto
static vector<LR1Triple> gotoLR1(const Grammar& g, const FirstFollow& ff,
                                 const vector<LR1Triple>& items,
                                 const string& symbol) {
    vector<LR1Triple> moved;
    for (size_t k = 0; k < items.size(); ++k) {
        const Production& p = g.production(items[k].prod);
        if (items[k].dot < (int)p.rhs.size() &&
            p.rhs[(size_t)items[k].dot] == symbol) {
            LR1Triple t = items[k];
            t.dot += 1;
            moved.push_back(t);
        }
    }
    if (moved.empty()) return moved;
    return closureLR1(g, ff, moved);
}

// Build CLR states
LRMachine buildCLRMachine(const Grammar& g, const FirstFollow& ff) {
    LRMachine m;
    m.name = "CLR";
    m.kind = MachineKind::CLR;

    LR1Triple seed;
    seed.prod = 0;
    seed.dot = 0;
    seed.look = "$";
    vector<LR1Triple> first;
    first.push_back(seed);

    vector<vector<LR1Triple> > kernels;
    kernels.push_back(closureLR1(g, ff, first));

    map<vector<LR1Triple>, int> stateOf;
    for (size_t s = 0; s < kernels.size(); ++s) {
        sort(kernels[s].begin(), kernels[s].end());
        kernels[s].erase(unique(kernels[s].begin(), kernels[s].end()),
                         kernels[s].end());
    }
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
        vector<LR1Triple> cur = kernels[head];
        for (size_t s = 0; s < symbols.size(); ++s) {
            vector<LR1Triple> target = gotoLR1(g, ff, cur, symbols[s]);
            if (target.empty()) continue;
            sort(target.begin(), target.end());
            target.erase(unique(target.begin(), target.end()), target.end());
            map<vector<LR1Triple>, int>::const_iterator found =
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
        map<pair<int, int>, set<string> > merged;
        for (size_t k = 0; k < kernels[s].size(); ++k) {
            merged[make_pair(kernels[s][k].prod, kernels[s][k].dot)]
                .insert(kernels[s][k].look);
        }
        map<pair<int, int>, set<string> >::const_iterator mIt;
        for (mIt = merged.begin(); mIt != merged.end(); ++mIt) {
            ItemRep item;
            item.prod = mIt->first.first;
            item.dot = mIt->first.second;
            item.lookaheads = mIt->second;
            st.items.push_back(item);
        }
        st.transitions = transitions[s];
        m.states.push_back(st);
    }
    return m;
}
