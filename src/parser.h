#ifndef PARSER_H
#define PARSER_H

#include <ostream>
#include <string>
#include <vector>

#include "grammar.h"
#include "lexer.h"
#include "table.h"

struct ParseStep {
    int number = 0;
    std::vector<int> stateStack;
    std::vector<std::string> symbolStack;
    std::vector<Token> remaining;
    std::string action;
    int reduceProd = -1;
};

struct ParseError {
    bool hasError = false;
    std::string parserName;
    int state = 0;
    std::string unexpectedType;
    std::string unexpectedLexeme;
    int line = 0;
    int column = 0;
    std::vector<std::string> expected;
    std::string reason;
};

struct ParseResult {
    bool accepted = false;
    std::vector<ParseStep> steps;
    int stepCount = 0;
    int errorStep = -1;
    int reductionsBeforeError = 0;
    ParseError error;
};

class Parser {
public:
    Parser(const Grammar& g, const ParsingTable& table);
    ParseResult parse(const std::vector<Token>& tokens, bool recordTrace = true) const;
    const ParsingTable& table() const { return table_; }

private:
    const Grammar& g_;
    const ParsingTable& table_;
};

void printTrace(std::ostream& os, const Grammar& g, const ParseResult& r,
                bool full);
void printParseError(std::ostream& os, const std::string& source,
                     const ParseResult& r);

#endif
