#ifndef TABLE_H
#define TABLE_H

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "first_follow.h"
#include "grammar.h"

enum class ActionType { SHIFT, REDUCE, ACCEPT };

struct Action {
    ActionType type = ActionType::SHIFT;
    int value = 0;
    bool operator==(const Action& o) const { return type == o.type && value == o.value; }
    bool operator!=(const Action& o) const { return !(*this == o); }
    std::string toString(const Grammar& g) const;
};

struct Conflict {
    int state = 0;
    std::string symbol;
    std::string kind;
    std::string existing;
    std::string incoming;
    std::string resolution;
};

enum class MachineKind { SLR, CLR, LALR };

struct ItemRep {
    int prod = 0;
    int dot = 0;
    std::set<std::string> lookaheads;
};

struct State {
    int id = 0;
    std::vector<ItemRep> items;
    std::map<std::string, int> transitions;
    std::string note;
};

struct LRMachine {
    std::string name;
    MachineKind kind = MachineKind::SLR;
    std::vector<State> states;
};

struct ParsingTable {
    std::string name;
    MachineKind kind = MachineKind::SLR;
    std::vector<std::vector<ItemRep> > stateItems;
    std::vector<std::string> stateNotes;
    std::map<int, std::map<std::string, Action> > action;
    std::map<int, std::map<std::string, int> > gotos;
    std::vector<Conflict> conflicts;

    const Action* findAction(int state, const std::string& symbol) const;
    int srConflicts() const;
    int rrConflicts() const;
    int actionEntries() const;
    int gotoEntries() const;
    int totalEntries() const { return actionEntries() + gotoEntries(); }
};

ParsingTable buildParsingTable(const Grammar& g, const FirstFollow& ff,
                               const LRMachine& machine);

std::string itemToString(const Grammar& g, const ItemRep& item, MachineKind kind);
void printMachineStates(std::ostream& os, const Grammar& g, const LRMachine& m);
void printActionTable(std::ostream& os, const Grammar& g, const ParsingTable& t, bool brief);
void printGotoTable(std::ostream& os, const Grammar& g, const ParsingTable& t, bool brief);
void printConflicts(std::ostream& os, const ParsingTable& t);

#endif
