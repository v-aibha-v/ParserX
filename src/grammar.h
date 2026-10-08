#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

struct Production {
    int id = -1;
    std::string lhs;
    std::vector<std::string> rhs;

    std::string toString() const;
};

class Grammar {
public:
    bool load(const std::string& path, std::string& errorMessage);

    const std::vector<Production>& productions() const { return productions_; }
    const Production& production(int id) const { return productions_[(size_t)id]; }

    const std::vector<std::string>& terminals() const { return terminals_; }
    const std::vector<std::string>& nonterminals() const { return nonterminals_; }

    int symbolId(const std::string& symbol) const;
    const std::vector<std::string>& symbols() const { return symbols_; }

    bool isTerminal(const std::string& symbol) const;
    bool isNonterminal(const std::string& symbol) const;

    const std::string& startSymbol() const { return startSymbol_; }
    const std::string& augmentedStart() const { return augmentedStart_; }

    const std::vector<int>& productionsOf(const std::string& lhs) const;

    bool isEpsilonProduction(int id) const { return productions_[(size_t)id].rhs.empty(); }

    void print(std::ostream& os) const;

private:
    std::vector<Production> productions_;
    std::vector<std::string> terminals_;
    std::vector<std::string> nonterminals_;
    std::set<std::string> terminalSet_;
    std::set<std::string> nonterminalSet_;
    std::string startSymbol_;
    std::string augmentedStart_;
    std::map<std::string, std::vector<int>> productionsOf_;
    std::vector<std::string> symbols_;
    std::map<std::string, int> symbolId_;
};

#endif
