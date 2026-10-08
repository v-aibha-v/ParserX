#include "lexer.h"
#include "util.h"

#include <cctype>
#include <iomanip>

using namespace std;

LexResult Lexer::tokenize(const string& source) {
    LexResult result;
    int line = 1;
    int col = 1;
    size_t i = 0;
    const size_t n = source.size();
    if (source.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;

    while (i < n) {
        char ch = source[i];
        if (ch == '\n') {
            ++line;
            col = 1;
            ++i;
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            ++col;
            ++i;
            continue;
        }
        int startLine = line;
        int startCol = col;

        // Identifiers and keywords
        if (isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            size_t j = i;
            while (j < n &&
                   (isalnum(static_cast<unsigned char>(source[j])) || source[j] == '_')) ++j;
            string text = source.substr(i, j - i);
            string type = "id";
            if (text == "if" || text == "else") type = text;
            Token t;
            t.type = type;
            t.lexeme = text;
            t.line = startLine;
            t.column = startCol;
            result.tokens.push_back(t);
            col += (int)(j - i);
            i = j;
            continue;
        }

        // Numbers
        if (isdigit(static_cast<unsigned char>(ch))) {
            size_t j = i;
            while (j < n && isdigit(static_cast<unsigned char>(source[j]))) ++j;
            Token t;
            t.type = "num";
            t.lexeme = source.substr(i, j - i);
            t.line = startLine;
            t.column = startCol;
            result.tokens.push_back(t);
            col += (int)(j - i);
            i = j;
            continue;
        }

        // Relational operators
        if (i + 1 < n) {
            string two = source.substr(i, 2);
            if (two == "<=" || two == ">=" || two == "==" || two == "!=") {
                Token t;
                t.type = "relop";
                t.lexeme = two;
                t.line = startLine;
                t.column = startCol;
                result.tokens.push_back(t);
                col += 2;
                i += 2;
                continue;
            }
        }

        string one(1, ch);
        if (ch == '<' || ch == '>') {
            Token t;
            t.type = "relop";
            t.lexeme = one;
            t.line = startLine;
            t.column = startCol;
            result.tokens.push_back(t);
            ++col;
            ++i;
            continue;
        }
        if (one == "+" || one == "-" || one == "*" || one == "/" ||
            one == "=" || one == ";" || one == "(" || one == ")" ||
            one == "{" || one == "}") {
            Token t;
            t.type = one;
            t.lexeme = one;
            t.line = startLine;
            t.column = startCol;
            result.tokens.push_back(t);
            ++col;
            ++i;
            continue;
        }

        // Unknown character
        LexError e;
        e.line = startLine;
        e.column = startCol;
        e.character = ch;
        result.errors.push_back(e);
        ++col;
        ++i;
    }

    Token end;
    end.type = "$";
    end.lexeme = "$";
    end.line = line;
    end.column = col;
    result.tokens.push_back(end);
    return result;
}

bool Lexer::tokenizeFile(const string& path, LexResult& out, string& errorMessage) {
    string source;
    if (!utilReadFile(path, source, errorMessage)) return false;
    out = tokenize(source);
    return true;
}

void printLexErrors(ostream& os, const LexResult& result) {
    if (result.errors.empty()) return;
    os << "Lexical errors (" << result.errors.size() << "):\n";
    for (size_t k = 0; k < result.errors.size(); ++k) {
        const LexError& e = result.errors[k];
        os << "  Line " << e.line << ", Column " << e.column
           << ": unknown character '" << e.character << "'\n";
    }
}

void printTokens(ostream& os, const LexResult& result) {
    ios::fmtflags saved = os.flags();
    os << left
       << setw(6) << "LINE"
       << setw(6) << "COL"
       << setw(8) << "TYPE"
       << "LEXEME\n";
    os << "------ ------ -------- --------\n";
    for (size_t k = 0; k < result.tokens.size(); ++k) {
        const Token& t = result.tokens[k];
        os << left
           << setw(6) << t.line
           << setw(6) << t.column
           << setw(8) << t.type
           << t.lexeme << "\n";
    }
    os.flags(saved);
    printLexErrors(os, result);
}
