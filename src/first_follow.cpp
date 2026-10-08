#include "first_follow.h"
#include "util.h"

using namespace std;

const string FirstFollow::EPS = "eps";

FirstFollow::FirstFollow(const Grammar& g) : g_(g) {
    for (size_t k = 0; k < g_.terminals().size(); ++k) {
        first_[g_.terminals()[k]].insert(g_.terminals()[k]);
    }
    computeNullable();
    computeFirst();
    computeFollow();
}

const set<string>& FirstFollow::first(const string& symbol) const {
    static const set<string> kEmpty;
    map<string, set<string> >::const_iterator it = first_.find(symbol);
    if (it == first_.end()) return kEmpty;
    return it->second;
}

const set<string>& FirstFollow::follow(const string& symbol) const {
    static const set<string> kEmpty;
    map<string, set<string> >::const_iterator it = follow_.find(symbol);
    if (it == follow_.end()) return kEmpty;
    return it->second;
}

bool FirstFollow::nullable(const string& symbol) const {
    return nullable_.count(symbol) > 0;
}

// Nullable set
void FirstFollow::computeNullable() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t k = 0; k < g_.productions().size(); ++k) {
            const Production& p = g_.productions()[k];
            if (nullable_.count(p.lhs)) continue;
            bool all = true;
            for (size_t i = 0; i < p.rhs.size(); ++i) {
                if (!g_.isNonterminal(p.rhs[i]) || !nullable_.count(p.rhs[i])) {
                    all = false;
                    break;
                }
            }
            if (all) {
                nullable_.insert(p.lhs);
                changed = true;
            }
        }
    }
}

set<string> FirstFollow::firstOfSequence(
    const vector<string>& sequence) const {
    set<string> result;
    for (size_t k = 0; k < sequence.size(); ++k) {
        const string& sym = sequence[k];
        if (g_.isTerminal(sym)) {
            result.insert(sym);
            return result;
        }
        const set<string>& f = first(sym);
        for (set<string>::const_iterator it = f.begin(); it != f.end(); ++it) {
            if (*it != EPS) result.insert(*it);
        }
        if (!nullable(sym)) return result;
    }
    result.insert(EPS);
    return result;
}

// FIRST sets
void FirstFollow::computeFirst() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t k = 0; k < g_.productions().size(); ++k) {
            const Production& p = g_.productions()[k];
            set<string>& target = first_[p.lhs];
            size_t before = target.size();
            if (nullable(p.lhs)) target.insert(EPS);
            if (!p.rhs.empty()) {
                for (size_t i = 0; i < p.rhs.size(); ++i) {
                    const string& sym = p.rhs[i];
                    if (g_.isTerminal(sym)) {
                        target.insert(sym);
                        break;
                    }
                    const set<string>& f = first(sym);
                    for (set<string>::const_iterator it = f.begin(); it != f.end(); ++it) {
                        if (*it != EPS) target.insert(*it);
                    }
                    if (!nullable(sym)) break;
                }
            }
            if (target.size() != before) changed = true;
        }
    }
}

// FOLLOW sets
void FirstFollow::computeFollow() {
    follow_[g_.startSymbol()].insert("$");
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t k = 0; k < g_.productions().size(); ++k) {
            const Production& p = g_.productions()[k];
            for (size_t i = 0; i < p.rhs.size(); ++i) {
                const string& B = p.rhs[i];
                if (!g_.isNonterminal(B)) continue;
                vector<string> rest(p.rhs.begin() + (i + 1), p.rhs.end());
                set<string> f = firstOfSequence(rest);
                set<string>& target = follow_[B];
                size_t before = target.size();
                for (set<string>::const_iterator it = f.begin(); it != f.end(); ++it) {
                    if (*it != EPS) target.insert(*it);
                }
                if (f.count(EPS) > 0) {
                    const set<string>& fl = follow(p.lhs);
                    for (set<string>::const_iterator it = fl.begin(); it != fl.end(); ++it) {
                        target.insert(*it);
                    }
                }
                if (target.size() != before) changed = true;
            }
        }
    }
}

void FirstFollow::printFirst(ostream& os) const {
    os << "FIRST sets (computed by fixed-point iteration):\n";
    for (size_t k = 0; k < g_.nonterminals().size(); ++k) {
        const string& nt = g_.nonterminals()[k];
        os << "  FIRST(" << nt << ") = " << utilFormatSet(first(nt)) << "\n";
    }
}

void FirstFollow::printFollow(ostream& os) const {
    os << "FOLLOW sets (computed by fixed-point iteration):\n";
    for (size_t k = 0; k < g_.nonterminals().size(); ++k) {
        const string& nt = g_.nonterminals()[k];
        os << "  FOLLOW(" << nt << ") = " << utilFormatSet(follow(nt)) << "\n";
    }
}
