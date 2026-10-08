#ifndef COMPARISON_H
#define COMPARISON_H

#include <chrono>
#include <ostream>
#include <string>
#include <vector>

#include "first_follow.h"
#include "lexer.h"
#include "parser.h"
#include "table.h"

struct ParserReport {
    std::string name;
    int states = 0;
    int actionEntries = 0;
    int gotoEntries = 0;
    int srConflicts = 0;
    int rrConflicts = 0;
    bool accepted = false;
    int steps = 0;
    long buildMicroseconds = 0;
    long parseMicroseconds = 0;
    int errorStep = -1;
    int reductionsBeforeError = 0;
    int errorLine = 0;
    int errorColumn = 0;
    std::string errorToken;
    std::vector<std::string> expected;
};

struct ComparisonResult {
    std::vector<ParserReport> parsers;
};

struct BuiltTables {
    LRMachine slrMachine, clrMachine, lalrMachine;
    ParsingTable slr, clr, lalr;
    long slrMicroseconds = 0;
    long clrMicroseconds = 0;
    long lalrMicroseconds = 0;
};

BuiltTables buildAllTables(const Grammar& g, const FirstFollow& ff);

ComparisonResult compareParsers(const Grammar& g,
                                const std::vector<Token>& tokens,
                                const BuiltTables& tables);

void printComparison(std::ostream& os, const ComparisonResult& c);

#endif
