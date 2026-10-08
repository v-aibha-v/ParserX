#include "comparison.h"
#include "clr.h"
#include "lalr.h"
#include "slr.h"
#include "util.h"

#include <iomanip>

using namespace std;

typedef chrono::steady_clock Clock;

static long microsSince(Clock::time_point start) {
    return (long)chrono::duration_cast<chrono::microseconds>(
        Clock::now() - start).count();
}

// Timed build
BuiltTables buildAllTables(const Grammar& g, const FirstFollow& ff) {
    BuiltTables b;
    Clock::time_point t0 = Clock::now();
    b.slrMachine = buildSLRMachine(g);
    b.slr = buildParsingTable(g, ff, b.slrMachine);
    b.slrMicroseconds = microsSince(t0);

    t0 = Clock::now();
    b.clrMachine = buildCLRMachine(g, ff);
    long clrMachineUs = microsSince(t0);
    Clock::time_point t1 = Clock::now();
    b.clr = buildParsingTable(g, ff, b.clrMachine);
    b.clrMicroseconds = clrMachineUs + microsSince(t1);

    t0 = Clock::now();
    b.lalrMachine = buildLALRMachine(g, b.clrMachine);
    b.lalr = buildParsingTable(g, ff, b.lalrMachine);
    b.lalrMicroseconds = clrMachineUs + microsSince(t0);
    return b;
}

// Run parsers
static ParserReport reportFor(const Grammar& g, const ParsingTable& t,
                              long buildUs, const vector<Token>& tokens) {
    ParserReport r;
    r.name = t.name;
    r.states = (int)t.stateItems.size();
    r.actionEntries = t.actionEntries();
    r.gotoEntries = t.gotoEntries();
    r.srConflicts = t.srConflicts();
    r.rrConflicts = t.rrConflicts();
    r.buildMicroseconds = buildUs;
    Parser p(g, t);
    Clock::time_point t0 = Clock::now();
    ParseResult pr = p.parse(tokens, false);
    r.parseMicroseconds = microsSince(t0);
    r.accepted = pr.accepted;
    r.steps = pr.stepCount;
    if (!pr.accepted && pr.error.hasError) {
        r.errorStep = pr.errorStep;
        r.reductionsBeforeError = pr.reductionsBeforeError;
        r.errorLine = pr.error.line;
        r.errorColumn = pr.error.column;
        r.errorToken = pr.error.unexpectedType;
        r.expected = pr.error.expected;
    }
    return r;
}

ComparisonResult compareParsers(const Grammar& g,
                                const vector<Token>& tokens,
                                const BuiltTables& tables) {
    ComparisonResult c;
    c.parsers.push_back(reportFor(g, tables.slr, tables.slrMicroseconds, tokens));
    c.parsers.push_back(reportFor(g, tables.lalr, tables.lalrMicroseconds, tokens));
    c.parsers.push_back(reportFor(g, tables.clr, tables.clrMicroseconds, tokens));
    return c;
}

// Comparison table
void printComparison(ostream& os, const ComparisonResult& c) {
    ios::fmtflags saved = os.flags();
    os << left
       << setw(8) << "Parser"
       << setw(8) << "States"
       << setw(8) << "ACTION"
       << setw(7) << "GOTO"
       << setw(7) << "TOTAL"
       << setw(6) << "S/R"
       << setw(6) << "R/R"
       << setw(10) << "Result"
       << setw(8) << "Steps"
       << setw(11) << "Error step"
       << setw(11) << "Build us"
       << "Parse us\n";
    os << utilRepeat('-', 100) << "\n";
    for (size_t k = 0; k < c.parsers.size(); ++k) {
        const ParserReport& p = c.parsers[k];
        os << left
           << setw(8) << p.name
           << setw(8) << p.states
           << setw(8) << p.actionEntries
           << setw(7) << p.gotoEntries
           << setw(7) << (p.actionEntries + p.gotoEntries)
           << setw(6) << p.srConflicts
           << setw(6) << p.rrConflicts
           << setw(10) << (p.accepted ? "ACCEPTED" : "rejected")
           << setw(8) << p.steps
           << setw(11) << (p.errorStep >= 0 ? to_string(p.errorStep) : "-")
           << setw(11) << p.buildMicroseconds
           << p.parseMicroseconds << "\n";
    }
    os.flags(saved);
    for (size_t k = 0; k < c.parsers.size(); ++k) {
        const ParserReport& p = c.parsers[k];
        if (!p.accepted && p.errorLine > 0) {
            os << "  " << p.name << " error at line " << p.errorLine
               << ", column " << p.errorColumn
               << " on token '" << p.errorToken << "' after "
               << p.reductionsBeforeError << " reduce(s) on that token; expected: "
               << utilJoin(p.expected, " ") << "\n";
        }
    }
}
