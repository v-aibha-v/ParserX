#include "table.h"
#include "util.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace std;

string Action::toString(const Grammar& g) const {
    switch (type) {
        case ActionType::SHIFT:
            return "shift " + to_string(value);
        case ActionType::REDUCE:
            return "reduce " + to_string(value) + " (" +
                   g.production(value).toString() + ")";
        case ActionType::ACCEPT:
            return "accept";
    }
    return "?";
}

const Action* ParsingTable::findAction(int state, const string& symbol) const {
    map<int, map<string, Action> >::const_iterator s = action.find(state);
    if (s == action.end()) return 0;
    map<string, Action>::const_iterator a = s->second.find(symbol);
    if (a == s->second.end()) return 0;
    return &a->second;
}

int ParsingTable::srConflicts() const {
    int n = 0;
    for (size_t k = 0; k < conflicts.size(); ++k) {
        if (conflicts[k].kind == "shift/reduce") ++n;
    }
    return n;
}

int ParsingTable::rrConflicts() const {
    int n = 0;
    for (size_t k = 0; k < conflicts.size(); ++k) {
        if (conflicts[k].kind == "reduce/reduce") ++n;
    }
    return n;
}

int ParsingTable::actionEntries() const {
    int n = 0;
    for (map<int, map<string, Action> >::const_iterator it = action.begin();
         it != action.end(); ++it) {
        n += (int)it->second.size();
    }
    return n;
}

int ParsingTable::gotoEntries() const {
    int n = 0;
    for (map<int, map<string, int> >::const_iterator it = gotos.begin();
         it != gotos.end(); ++it) {
        n += (int)it->second.size();
    }
    return n;
}

ParsingTable buildParsingTable(const Grammar& g, const FirstFollow& ff,
                               const LRMachine& machine) {
    ParsingTable t;
    t.name = machine.name;
    t.kind = machine.kind;
    for (size_t k = 0; k < machine.states.size(); ++k) {
        t.stateItems.push_back(machine.states[k].items);
        t.stateNotes.push_back(machine.states[k].note);
    }

    // Conflict check
    struct Inserter {
        const Grammar& g;
        ParsingTable& t;
        Inserter(const Grammar& gg, ParsingTable& tt) : g(gg), t(tt) {}
        void put(int state, const string& sym, const Action& incoming) {
            map<string, Action>& row = t.action[state];
            map<string, Action>::iterator it = row.find(sym);
            if (it == row.end()) {
                row[sym] = incoming;
                return;
            }
            Action& existing = it->second;
            if (existing == incoming) return;

            Conflict c;
            c.state = state;
            c.symbol = sym;
            c.existing = existing.toString(g);
            c.incoming = incoming.toString(g);

            bool existingShift = existing.type == ActionType::SHIFT;
            bool incomingShift = incoming.type == ActionType::SHIFT;
            bool existingReduce = existing.type == ActionType::REDUCE;
            bool incomingReduce = incoming.type == ActionType::REDUCE;
            bool existingAccept = existing.type == ActionType::ACCEPT;
            bool incomingAccept = incoming.type == ActionType::ACCEPT;

            if ((existingShift && incomingReduce) || (existingReduce && incomingShift)) {
                c.kind = "shift/reduce";
                c.resolution = "resolved by SHIFT (shift preferred, dangling-else rule)";
                if (incomingShift) it->second = incoming;
                t.conflicts.push_back(c);
            } else if (existingReduce && incomingReduce) {
                c.kind = "reduce/reduce";
                int keep = min(existing.value, incoming.value);
                c.resolution = "resolved by keeping production " + to_string(keep) +
                               " (" + g.production(keep).toString() + ")";
                if (incoming.value == keep) it->second = incoming;
                t.conflicts.push_back(c);
            } else {
                c.kind = "other";
                if (existingAccept || incomingAccept) {
                    c.resolution = "accept kept";
                    if (incomingAccept) it->second = incoming;
                } else {
                    c.resolution = "kept the first action";
                }
                t.conflicts.push_back(c);
            }
        }
    };
    // Shift and goto
    Inserter inserter(g, t);
    Action a;
    for (size_t s = 0; s < machine.states.size(); ++s) {
        const State& st = machine.states[s];
        map<string, int>::const_iterator tr;
        for (tr = st.transitions.begin(); tr != st.transitions.end(); ++tr) {
            const string& sym = tr->first;
            int target = tr->second;
            if (g.isTerminal(sym)) {
                a.type = ActionType::SHIFT;
                a.value = target;
                inserter.put((int)s, sym, a);
            } else {
                t.gotos[(int)s][sym] = target;
            }
        }
    }
    // Reduce actions
    for (size_t s = 0; s < machine.states.size(); ++s) {
        const State& st = machine.states[s];
        for (size_t i = 0; i < st.items.size(); ++i) {
            const ItemRep& item = st.items[i];
            const Production& p = g.production(item.prod);
            if (item.dot != (int)p.rhs.size()) continue;
            if (p.lhs == g.augmentedStart()) {
                a.type = ActionType::ACCEPT;
                a.value = 0;
                inserter.put((int)s, "$", a);
                continue;
            }
            a.type = ActionType::REDUCE;
            a.value = p.id;
            if (machine.kind == MachineKind::SLR) {
                const set<string>& fl = ff.follow(p.lhs);
                set<string>::const_iterator fIt;
                for (fIt = fl.begin(); fIt != fl.end(); ++fIt)
                    inserter.put((int)s, *fIt, a);
            } else {
                set<string>::const_iterator lIt;
                for (lIt = item.lookaheads.begin();
                     lIt != item.lookaheads.end(); ++lIt)
                    inserter.put((int)s, *lIt, a);
            }
        }
    }
    return t;
}

// Printers
string itemToString(const Grammar& g, const ItemRep& item, MachineKind kind) {
    const Production& p = g.production(item.prod);
    string s = p.lhs + " ->";
    for (size_t k = 0; k < p.rhs.size(); ++k) {
        s += ((int)k == item.dot) ? " . " + p.rhs[k] : " " + p.rhs[k];
    }
    if (item.dot == (int)p.rhs.size()) s += " .";
    if (p.rhs.empty()) s += " .";
    if (kind != MachineKind::SLR) {
        s += "  ,  ";
        if (item.lookaheads.empty()) s += "{}";
        else {
            s += "{ ";
            bool first = true;
            set<string>::const_iterator it;
            for (it = item.lookaheads.begin();
                 it != item.lookaheads.end(); ++it) {
                if (!first) s += ", ";
                first = false;
                s += *it;
            }
            s += " }";
        }
    }
    return s;
}

void printMachineStates(ostream& os, const Grammar& g, const LRMachine& m) {
    os << m.name << " automaton: " << m.states.size() << " states\n";
    for (size_t s = 0; s < m.states.size(); ++s) {
        const State& st = m.states[s];
        os << "\nState " << st.id;
        if (!st.note.empty()) os << "  [" << st.note << "]";
        os << "  (" << st.items.size() << " items)\n";
        for (size_t i = 0; i < st.items.size(); ++i)
            os << "    " << itemToString(g, st.items[i], m.kind) << "\n";
        if (!st.transitions.empty()) {
            os << "    transitions:";
            map<string, int>::const_iterator tr;
            for (tr = st.transitions.begin(); tr != st.transitions.end(); ++tr)
                os << "  --" << tr->first << "--> " << tr->second;
            os << "\n";
        }
    }
}

static string shortAction(const Action& a) {
    switch (a.type) {
        case ActionType::SHIFT: return "s" + to_string(a.value);
        case ActionType::REDUCE: return "r" + to_string(a.value);
        case ActionType::ACCEPT: return "acc";
    }
    return "?";
}

void printActionTable(ostream& os, const Grammar& g, const ParsingTable& t, bool brief) {
    (void)g;
    if (brief) {
        os << t.name << " ACTION: " << t.actionEntries() << " entries in "
           << t.stateItems.size() << " states.\n";
        return;
    }
    const vector<string>& terms = g.terminals();
    ios::fmtflags saved = os.flags();
    os << left << setw(7) << "STATE";
    for (size_t k = 0; k < terms.size(); ++k) os << setw(10) << terms[k];
    os << "\n" << utilRepeat('-', (int)(7 + terms.size() * 10)) << "\n";
    for (size_t s = 0; s < t.stateItems.size(); ++s) {
        os << left << setw(7) << s;
        for (size_t k = 0; k < terms.size(); ++k) {
            const Action* a = t.findAction((int)s, terms[k]);
            os << setw(10) << (a ? shortAction(*a) : ".");
        }
        os << "\n";
    }
    os.flags(saved);
}

void printGotoTable(ostream& os, const Grammar& g, const ParsingTable& t, bool brief) {
    if (brief) {
        os << t.name << " GOTO: " << t.gotoEntries() << " entries.\n";
        return;
    }
    const vector<string>& nts = g.nonterminals();
    vector<string> cols;
    for (size_t k = 0; k < nts.size(); ++k)
        if (nts[k] != g.augmentedStart()) cols.push_back(nts[k]);
    ios::fmtflags saved = os.flags();
    os << left << setw(7) << "STATE";
    for (size_t k = 0; k < cols.size(); ++k) os << setw(8) << cols[k];
    os << "\n" << utilRepeat('-', (int)(7 + cols.size() * 8)) << "\n";
    for (size_t s = 0; s < t.stateItems.size(); ++s) {
        os << left << setw(7) << s;
        map<int, map<string, int> >::const_iterator row =
            t.gotos.find((int)s);
        for (size_t k = 0; k < cols.size(); ++k) {
            string cell = ".";
            if (row != t.gotos.end()) {
                map<string, int>::const_iterator gt =
                    row->second.find(cols[k]);
                if (gt != row->second.end()) cell = to_string(gt->second);
            }
            os << setw(8) << cell;
        }
        os << "\n";
    }
    os.flags(saved);
}

void printConflicts(ostream& os, const ParsingTable& t) {
    if (t.conflicts.empty()) {
        os << t.name << ": no conflicts detected.\n";
        return;
    }
    os << t.name << " conflicts (" << t.conflicts.size() << "): "
       << t.srConflicts() << " shift/reduce, "
       << t.rrConflicts() << " reduce/reduce.\n";
    for (size_t k = 0; k < t.conflicts.size(); ++k) {
        const Conflict& c = t.conflicts[k];
        os << "  [" << (k + 1) << "] state " << c.state << " on '" << c.symbol
           << "' (" << c.kind << ")\n"
           << "      existing: " << c.existing << "\n"
           << "      incoming: " << c.incoming << "\n"
           << "      " << c.resolution << "\n";
    }
}
