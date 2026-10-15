#include "lexer.h"
#include "util.h"

#include <cctype>
#include <iomanip>

using namespace std;

namespace {

bool isIdentStart(char c) {
    return isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentChar(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Reserved words. Each keyword becomes a token whose type is the word
// itself, so the grammar file can use it directly as a terminal.
bool keywordType(const string& text, string& type) {
    static const char* const kKeywords[] = {
        "int", "float", "if", "else", "while", "for",
        "return", "break", "continue"
    };
    for (size_t k = 0; k < sizeof(kKeywords) / sizeof(kKeywords[0]); ++k) {
        if (text == kKeywords[k]) {
            type = text;
            return true;
        }
    }
    return false;
}

Token makeToken(const string& type, const string& lexeme, int line, int column) {
    Token t;
    t.type = type;
    t.lexeme = lexeme;
    t.line = line;
    t.column = column;
    return t;
}

}  // namespace

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

        // Line comment: // ... (to end of line)
        if (ch == '/' && i + 1 < n && source[i + 1] == '/') {
            while (i < n && source[i] != '\n') {
                ++col;
                ++i;
            }
            continue;
        }

        // Block comment: /* ... */ (may span lines)
        if (ch == '/' && i + 1 < n && source[i + 1] == '*') {
            i += 2;
            col += 2;
            bool closed = false;
            while (i < n) {
                if (source[i] == '\n') {
                    ++line;
                    col = 1;
                    ++i;
                    continue;
                }
                if (source[i] == '*' && i + 1 < n && source[i + 1] == '/') {
                    i += 2;
                    col += 2;
                    closed = true;
                    break;
                }
                ++col;
                ++i;
            }
            if (!closed) {
                LexError e;
                e.line = startLine;
                e.column = startCol;
                e.character = '/';
                e.message = "unterminated block comment";
                result.errors.push_back(e);
            }
            continue;
        }

        // Identifiers and keywords
        if (isIdentStart(ch)) {
            size_t j = i;
            while (j < n && isIdentChar(source[j])) ++j;
            string text = source.substr(i, j - i);
            string type = "id";
            keywordType(text, type);
            result.tokens.push_back(makeToken(type, text, startLine, startCol));
            col += (int)(j - i);
            i = j;
            continue;
        }

        // Integer and floating point literals: digits [. digits]
        if (isdigit(static_cast<unsigned char>(ch))) {
            size_t j = i;
            while (j < n && isdigit(static_cast<unsigned char>(source[j]))) ++j;
            bool isFloat = false;
            if (j < n && source[j] == '.') {
                if (j + 1 < n && isdigit(static_cast<unsigned char>(source[j + 1]))) {
                    isFloat = true;
                    ++j;
                    while (j < n && isdigit(static_cast<unsigned char>(source[j]))) ++j;
                } else {
                    LexError e;
                    e.line = startLine;
                    e.column = startCol;
                    e.character = '.';
                    e.message = "malformed numeric literal: '.' must be followed by a digit";
                    result.errors.push_back(e);
                    col += (int)(j - i) + 1;  // digits plus the stray dot
                    i = j + 1;
                    continue;
                }
            }
            if (j < n && isIdentChar(source[j])) {
                LexError e;
                e.line = startLine;
                e.column = startCol;
                e.character = source[j];
                e.message = "malformed numeric literal: digits must not be followed by '"
                            + string(1, source[j]) + "'";
                result.errors.push_back(e);
                while (j < n && isIdentChar(source[j])) ++j;
                col += (int)(j - i);
                i = j;
                continue;
            }
            string text = source.substr(i, j - i);
            result.tokens.push_back(
                makeToken(isFloat ? "floatconst" : "intconst", text, startLine, startCol));
            col += (int)(j - i);
            i = j;
            continue;
        }

        // Two-character operators: <= >= == != && ||
        if (i + 1 < n) {
            string two = source.substr(i, 2);
            if (two == "<=" || two == ">=" || two == "==" || two == "!=" ||
                two == "&&" || two == "||") {
                result.tokens.push_back(makeToken(two, two, startLine, startCol));
                col += 2;
                i += 2;
                continue;
            }
        }

        // Single-character operators and punctuation
        string one(1, ch);
        if (one == "+" || one == "-" || one == "*" || one == "/" || one == "%" ||
            one == "<" || one == ">" || one == "!" || one == "=" ||
            one == ";" || one == "," || one == "(" || one == ")" ||
            one == "{" || one == "}") {
            result.tokens.push_back(makeToken(one, one, startLine, startCol));
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

    result.tokens.push_back(makeToken("$", "$", line, col));
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
        os << "  Line " << e.line << ", Column " << e.column << ": ";
        if (e.message.empty())
            os << "unknown character '" << e.character << "'\n";
        else
            os << e.message << " ('" << e.character << "')\n";
    }
}

void printTokens(ostream& os, const LexResult& result) {
    ios::fmtflags saved = os.flags();
    os << left
       << setw(6) << "LINE"
       << setw(6) << "COL"
       << setw(12) << "TYPE"
       << "LEXEME\n";
    os << "------ ------ ------------ --------\n";
    for (size_t k = 0; k < result.tokens.size(); ++k) {
        const Token& t = result.tokens[k];
        os << left
           << setw(6) << t.line
           << setw(6) << t.column
           << setw(12) << t.type
           << t.lexeme << "\n";
    }
    os.flags(saved);
    printLexErrors(os, result);
}
