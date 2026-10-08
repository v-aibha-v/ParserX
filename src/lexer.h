#ifndef LEXER_H
#define LEXER_H

#include <ostream>
#include <string>
#include <vector>

struct Token {
    std::string type;
    std::string lexeme;
    int line = 0;
    int column = 0;
};

struct LexError {
    int line = 0;
    int column = 0;
    char character = 0;
};

struct LexResult {
    std::vector<Token> tokens;
    std::vector<LexError> errors;
};

class Lexer {
public:
    static LexResult tokenize(const std::string& source);
    static bool tokenizeFile(const std::string& path, LexResult& out, std::string& errorMessage);
};

void printTokens(std::ostream& os, const LexResult& result);
void printLexErrors(std::ostream& os, const LexResult& result);

#endif
