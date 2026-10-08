#include "parser.h"
#include "util.h"

#include <iomanip>
#include <sstream>

using namespace std;

Parser::Parser(const Grammar& g, const ParsingTable& table)
    : g_(g), table_(table) {}

// Parse loop
ParseResult Parser::parse(const vector<Token>& tokens, bool recordTrace) const {
    ParseResult r;
    vector<int> states;
    vector<string> symbols;
    states.push_back(0);
    symbols.push_back("$");

    size_t pos = 0;
    int stepNo = 0;
    int reducesOnToken = 0;
    const size_t kGuard = 100000 + tokens.size() * 64;
    while (stepNo < (int)kGuard) {
        ParseStep step;
        step.number = stepNo;
        if (recordTrace) {
            step.stateStack = states;
            step.symbolStack = symbols;
            for (size_t k = pos; k < tokens.size(); ++k) step.remaining.push_back(tokens[k]);
        }

        int state = states.back();
        const Token& tok = (pos < tokens.size()) ? tokens[pos] : tokens.back();
        const Action* act = table_.findAction(state, tok.type);
        if (!act) {
            step.action = "ERROR";
            if (recordTrace) r.steps.push_back(step);
            r.stepCount = stepNo + 1;
            r.errorStep = stepNo;
            r.reductionsBeforeError = reducesOnToken;
            ParseError e;
            e.hasError = true;
            e.parserName = table_.name;
            e.state = state;
            e.unexpectedType = tok.type;
            e.unexpectedLexeme = tok.lexeme;
            e.line = tok.line;
            e.column = tok.column;
            map<int, map<string, Action> >::const_iterator row =
                table_.action.find(state);
            if (row != table_.action.end()) {
                map<string, Action>::const_iterator aIt;
                for (aIt = row->second.begin(); aIt != row->second.end(); ++aIt)
                    e.expected.push_back(aIt->first);
            }
            e.reason = "no " + table_.name + " action for state " +
                       to_string(state) + " on '" + tok.type + "'";
            r.error = e;
            r.accepted = false;
            return r;
        }

        if (act->type == ActionType::SHIFT) {
            step.action = "shift " + to_string(act->value);
            if (recordTrace) r.steps.push_back(step);
            states.push_back(act->value);
            symbols.push_back(tok.type);
            if (pos < tokens.size()) ++pos;
            reducesOnToken = 0;
            ++stepNo;
        } else if (act->type == ActionType::REDUCE) {
            const Production& p = g_.production(act->value);
            if (recordTrace) {
                ostringstream desc;
                desc << "reduce " << p.id << " (" << p.toString() << ")";
                step.action = desc.str();
                step.reduceProd = p.id;
                r.steps.push_back(step);
            }
            for (size_t k = 0; k < p.rhs.size(); ++k) {
                states.pop_back();
                symbols.pop_back();
            }
            int top = states.back();
            map<int, map<string, int> >::const_iterator grow =
                table_.gotos.find(top);
            if (grow == table_.gotos.end() ||
                grow->second.find(p.lhs) == grow->second.end()) {
                ParseError e;
                e.hasError = true;
                e.parserName = table_.name;
                e.state = top;
                e.unexpectedType = tok.type;
                e.unexpectedLexeme = tok.lexeme;
                e.line = tok.line;
                e.column = tok.column;
                e.reason = "missing GOTO[" + to_string(top) + "][" +
                           p.lhs + "] after reducing " + p.toString();
                r.stepCount = stepNo + 1;
                r.errorStep = stepNo;
                r.reductionsBeforeError = reducesOnToken;
                r.error = e;
                r.accepted = false;
                return r;
            }
            int target = grow->second.find(p.lhs)->second;
            states.push_back(target);
            symbols.push_back(p.lhs);
            ++reducesOnToken;
            ++stepNo;
        } else {
            step.action = "ACCEPT";
            if (recordTrace) r.steps.push_back(step);
            r.stepCount = stepNo + 1;
            r.accepted = true;
            return r;
        }
    }
    ParseError e;
    e.hasError = true;
    e.parserName = table_.name;
    e.reason = "parse step limit exceeded (possible grammar loop)";
    r.stepCount = stepNo;
    r.errorStep = stepNo;
    r.error = e;
    r.accepted = false;
    return r;
}

static string stackToString(const vector<int>& states) {
    ostringstream ss;
    for (size_t k = 0; k < states.size(); ++k) {
        if (k) ss << " ";
        ss << states[k];
    }
    return ss.str();
}

static string inputToString(const vector<Token>& toks) {
    ostringstream ss;
    for (size_t k = 0; k < toks.size(); ++k) {
        if (k) ss << " ";
        ss << toks[k].type;
    }
    return ss.str();
}

// Trace output
void printTrace(ostream& os, const Grammar& g, const ParseResult& r, bool full) {
    (void)g;
    if (r.steps.empty()) {
        os << "(empty trace)\n";
        return;
    }
    vector<ParseStep> steps = r.steps;
    bool truncated = false;
    if (!full && steps.size() > 24) {
        vector<ParseStep> shown;
        for (size_t k = 0; k < 12; ++k) shown.push_back(steps[k]);
        for (size_t k = steps.size() - 12; k < steps.size(); ++k) shown.push_back(steps[k]);
        steps = shown;
        truncated = true;
    }
    ios::fmtflags saved = os.flags();
    os << left
       << setw(5) << "STEP"
       << setw(26) << "STATE STACK"
       << setw(30) << "SYMBOL STACK"
       << setw(30) << "INPUT"
       << "ACTION\n";
    os << utilRepeat('-', 110) << "\n";
    size_t skippedAt = 12;
    for (size_t k = 0; k < steps.size(); ++k) {
        if (truncated && k == skippedAt) {
            os << "  ... (" << (r.steps.size() - 24)
               << " middle steps omitted; use --trace full) ...\n";
        }
        const ParseStep& s = steps[k];
        os << left
           << setw(5) << s.number
           << setw(26) << stackToString(s.stateStack).substr(0, 25)
           << setw(30) << utilJoin(s.symbolStack, " ").substr(0, 29)
           << setw(30) << inputToString(s.remaining).substr(0, 29)
           << s.action << "\n";
    }
    os.flags(saved);
}

// Error report
void printParseError(ostream& os, const string& source,
                     const ParseResult& r) {
    const ParseError& e = r.error;
    os << "Syntax Error\n";
    os << "  Parser:          " << e.parserName << "\n";
    os << "  Line:            " << e.line << "\n";
    os << "  Column:          " << e.column << "\n";
    if (e.unexpectedType == "$")
        os << "  Unexpected:      end of input ($)\n";
    else
        os << "  Unexpected:      '" << e.unexpectedLexeme << "' (token " << e.unexpectedType
        << ")\n";
    os << "  Parser state:    " << e.state << "\n";
    if (!e.expected.empty())
        os << "  Expected one of: " << utilJoin(e.expected, ", ") << "\n";
    if (!e.reason.empty()) os << "  Reason:          " << e.reason << "\n";
    if (!source.empty()) {
        os << "  Input:\n";
        istringstream lines(source);
        string ln;
        int n = 1;
        while (getline(lines, ln)) {
            os << "    " << n << ": " << ln << "\n";
            ++n;
        }
        if (e.line > 0) {
            os << "    " << utilRepeat(' ', (int)(to_string(e.line).size() + 2 + e.column - 1))
               << "^\n";
        }
    }
}
