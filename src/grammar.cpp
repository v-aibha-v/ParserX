#include "grammar.h"
#include "util.h"

#include <fstream>

using namespace std;

string Production::toString() const {
    string s = lhs + " -> ";
    if (rhs.empty()) return s + "eps";
    for (size_t i = 0; i < rhs.size(); ++i) {
        if (i) s += " ";
        s += rhs[i];
    }
    return s;
}

static bool isEpsilonToken(const string& t) {
    return t == "eps" || t == "epsilon" || t == "empty" || t == "\xCE\xB5";
}

int Grammar::symbolId(const string& symbol) const {
    map<string, int>::const_iterator it = symbolId_.find(symbol);
    return it == symbolId_.end() ? -1 : it->second;
}

bool Grammar::isTerminal(const string& symbol) const {
    return terminalSet_.count(symbol) > 0;
}

bool Grammar::isNonterminal(const string& symbol) const {
    return nonterminalSet_.count(symbol) > 0;
}

const vector<int>& Grammar::productionsOf(const string& lhs) const {
    static const vector<int> kEmpty;
    map<string, vector<int> >::const_iterator it = productionsOf_.find(lhs);
    if (it == productionsOf_.end()) return kEmpty;
    return it->second;
}

// Load grammar
bool Grammar::load(const string& path, string& errorMessage) {
    ifstream in(path.c_str());
    if (!in) {
        errorMessage = "cannot open grammar file: " + path;
        return false;
    }

    productions_.clear();
    terminals_.clear();
    nonterminals_.clear();
    terminalSet_.clear();
    nonterminalSet_.clear();
    productionsOf_.clear();
    startSymbol_.clear();
    augmentedStart_.clear();
    symbols_.clear();
    symbolId_.clear();

    struct Raw {
        string lhs;
        vector<string> rhs;
    };
    vector<Raw> raw;
    string line;
    int lineNo = 0;
    while (getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        size_t hash = line.find('#');
        if (hash != string::npos) line = line.substr(0, hash);
        line = utilTrim(line);
        if (line.empty()) continue;

        size_t arrow = line.find("->");
        size_t arrowLen = 2;
        if (arrow == string::npos) {
            arrow = line.find("\xE2\x86\x92");
            arrowLen = 3;
        }
        if (arrow == string::npos) {
            errorMessage = "line " + to_string(lineNo) + ": expected '->' in production";
            return false;
        }
        string lhs = utilTrim(line.substr(0, arrow));
        string rhs = utilTrim(line.substr(arrow + arrowLen));
        if (lhs.empty() || lhs.find_first_of(" \t") != string::npos) {
            errorMessage = "line " + to_string(lineNo) +
                           ": left-hand side must be a single symbol";
            return false;
        }
        vector<string> alternatives;
        if (rhs.empty()) {
            alternatives.push_back("");
        } else {
            string cur;
            for (size_t k = 0; k < rhs.size(); ++k) {
                if (rhs[k] == '|') {
                    alternatives.push_back(cur);
                    cur.clear();
                } else {
                    cur += rhs[k];
                }
            }
            alternatives.push_back(cur);
        }

        for (size_t a = 0; a < alternatives.size(); ++a) {
            vector<string> symbols = utilSplitWhitespace(utilTrim(alternatives[a]));
            vector<string> cleaned;
            bool sawEps = false;
            for (size_t k = 0; k < symbols.size(); ++k) {
                if (isEpsilonToken(symbols[k])) sawEps = true;
                else cleaned.push_back(symbols[k]);
            }
            if (sawEps && !cleaned.empty()) {
                errorMessage = "line " + to_string(lineNo) +
                               ": 'eps' cannot be combined with other symbols";
                return false;
            }
            if (sawEps) cleaned.clear();
            Raw r;
            r.lhs = lhs;
            r.rhs = cleaned;
            raw.push_back(r);
        }
    }
    if (raw.empty()) {
        errorMessage = "grammar file contains no productions: " + path;
        return false;
    }

    // Classify symbols
    for (size_t k = 0; k < raw.size(); ++k) {
        if (nonterminalSet_.insert(raw[k].lhs).second) nonterminals_.push_back(raw[k].lhs);
    }
    set<string> seen;
    for (size_t k = 0; k < raw.size(); ++k) {
        for (size_t j = 0; j < raw[k].rhs.size(); ++j) {
            const string& s = raw[k].rhs[j];
            if (nonterminalSet_.count(s)) continue;
            if (seen.insert(s).second) {
                terminalSet_.insert(s);
                terminals_.push_back(s);
            }
        }
    }

    startSymbol_ = raw.front().lhs;

    // Augment grammar
    augmentedStart_ = startSymbol_ + "'";
    Production aug;
    aug.id = 0;
    aug.lhs = augmentedStart_;
    aug.rhs.push_back(startSymbol_);
    productions_.push_back(aug);
    int nextId = 1;
    for (size_t k = 0; k < raw.size(); ++k) {
        Production p;
        p.id = nextId++;
        p.lhs = raw[k].lhs;
        p.rhs = raw[k].rhs;
        productions_.push_back(p);
    }

    nonterminalSet_.insert(augmentedStart_);
    nonterminals_.insert(nonterminals_.begin(), augmentedStart_);

    terminalSet_.insert("$");
    terminals_.push_back("$");

    for (size_t k = 0; k < productions_.size(); ++k) {
        productionsOf_[productions_[k].lhs].push_back(productions_[k].id);
    }

    // Symbol ids
    for (size_t k = 0; k < nonterminals_.size(); ++k) symbols_.push_back(nonterminals_[k]);
    for (size_t k = 0; k < terminals_.size(); ++k) symbols_.push_back(terminals_[k]);
    for (size_t k = 0; k < symbols_.size(); ++k) symbolId_[symbols_[k]] = (int)k;
    return true;
}

void Grammar::print(ostream& os) const {
    os << "Start symbol: " << startSymbol_ << "\n";
    os << "Augmented start production (0): " << augmentedStart_
       << " -> " << startSymbol_ << "\n";
    os << "Non-terminals (" << nonterminals_.size() << "): "
       << utilJoin(nonterminals_, ", ") << "\n";
    os << "Terminals     (" << terminals_.size() << "): "
       << utilJoin(terminals_, ", ") << "\n";
    os << "Symbol ids    :";
    for (size_t k = 0; k < symbols_.size(); ++k) os << " " << symbols_[k] << "=" << k;
    os << "\n";
    os << "Productions (" << productions_.size() << "):\n";
    for (size_t k = 0; k < productions_.size(); ++k) {
        os << "  (" << productions_[k].id << ") "
           << productions_[k].toString() << "\n";
    }
}
