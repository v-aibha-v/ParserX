#include <iostream>
#include <string>

#include "util.h"
#include "grammar.h"
#include "first_follow.h"
#include "lexer.h"
#include "slr.h"
#include "clr.h"
#include "lalr.h"
#include "table.h"
#include "parser.h"
#include "comparison.h"

using namespace std;

// Small fixed demonstration of the advanced language grammar on the
// SLR, CLR and LALR parsers. parserx (repo root) is the full driver.

static void showParse(const string& label, const ParseResult& r) {
    cout << "  " << label << ": "
         << (r.accepted ? "ACCEPTED" : "REJECTED")
         << " (" << r.stepCount << " steps)\n";
    if (!r.accepted && r.error.hasError) {
        cout << "    parser state : " << r.error.state << "\n"
             << "    line/col     : " << r.error.line
             << " / " << r.error.column << "\n"
             << "    unexpected   : '" << r.error.unexpectedLexeme
             << "' (token " << r.error.unexpectedType << ")\n";
        if (!r.error.expected.empty())
            cout << "    expected     : "
                 << utilJoin(r.error.expected, ", ") << "\n";
        if (!r.error.reason.empty())
            cout << "    reason       : " << r.error.reason << "\n";
    }
}

static const char* const kValidProgram =
    "int calculate(int a, int b) {\n"
    "    int result = a + b * 2;\n"
    "    if (result >= 10 && a != 0) {\n"
    "        result = result % b;\n"
    "    } else {\n"
    "        result = result - 1;\n"
    "    }\n"
    "    while (result > 0) {\n"
    "        result = result - 1;\n"
    "    }\n"
    "    for (int i = 0; i < 3; i = i + 1) {\n"
    "        result = result + calculate(a, b);\n"
    "    }\n"
    "    return result;\n"
    "}\n";

static const char* const kInvalidProgram =
    "int f(int a) {\n"
    "    int x = ;\n"
    "    return x;\n"
    "}\n";

int main() {
    utilBanner(cout, "ADVANCED LANGUAGE - SLR / CLR / LALR DEMONSTRATION");
    bool ok = true;

    cout << "\n[1] Grammar\n";
    Grammar g;
    string err;
    if (!g.load("grammar/language.txt", err)) {
        cout << "Grammar FAILED to load: " << err << "\n";
        return 1;
    }
    cout << "Grammar loaded from grammar/language.txt\n";
    g.print(cout);

    cout << "\n[2] FIRST / FOLLOW (selected sets, computed by the implementation)\n";
    FirstFollow ff(g);
    cout << "FIRST(Stmt)       = " << utilFormatSet(ff.first("Stmt")) << "\n";
    cout << "FIRST(Expr)       = " << utilFormatSet(ff.first("Expr")) << "\n";
    cout << "FIRST(Unary)      = " << utilFormatSet(ff.first("Unary")) << "\n";
    cout << "FOLLOW(Stmt)      = " << utilFormatSet(ff.follow("Stmt")) << "\n";
    cout << "FOLLOW(Expr)      = " << utilFormatSet(ff.follow("Expr")) << "\n";
    cout << "FOLLOW(ParamList) = " << utilFormatSet(ff.follow("ParamList")) << "\n";

    cout << "\n[3] Parser tables (built algorithmically from the grammar)\n";
    BuiltTables bt = buildAllTables(g, ff);
    const ParsingTable* tables[3] = {&bt.slr, &bt.lalr, &bt.clr};
    for (int k = 0; k < 3; ++k) {
        cout << "  " << left << setw(5) << tables[k]->name << right
             << setw(4) << tables[k]->stateItems.size() << " states, "
             << tables[k]->actionEntries() << " ACTION + "
             << tables[k]->gotoEntries() << " GOTO entries\n";
    }

    cout << "\n[4] Conflicts (detected during table construction)\n";
    for (int k = 0; k < 3; ++k) printConflicts(cout, *tables[k]);

    cout << "\n[5] Valid program (declarations, if/else, while, for, call)\n";
    LexResult okLex = Lexer::tokenize(kValidProgram);
    if (!okLex.errors.empty()) {
        printLexErrors(cout, okLex);
        cout << "Unexpected lexical error in the built-in valid example!\n";
        return 1;
    }
    Parser pSlr(g, bt.slr), pLalr(g, bt.lalr), pClr(g, bt.clr);
    ParseResult rSlr = pSlr.parse(okLex.tokens, false);
    ParseResult rLalr = pLalr.parse(okLex.tokens, false);
    ParseResult rClr = pClr.parse(okLex.tokens, false);
    showParse("SLR ", rSlr);
    showParse("LALR", rLalr);
    showParse("CLR ", rClr);
    if (!rSlr.accepted || !rLalr.accepted || !rClr.accepted) ok = false;

    cout << "\n[6] Invalid program:  int x = ;  (missing initializer)\n";
    LexResult badLex = Lexer::tokenize(kInvalidProgram);
    ParseResult rBad = pLalr.parse(badLex.tokens, false);
    showParse("LALR", rBad);
    if (rBad.accepted || !rBad.error.hasError) ok = false;

    cout << "\n[7] Comparison on the valid program\n";
    ComparisonResult cmp = compareParsers(g, okLex.tokens, bt);
    printComparison(cout, cmp);

    cout << "\n" << utilRepeat('-', 60) << "\n";
    cout << (ok ? "DEMO RESULT: OK\n" : "DEMO RESULT: FAILED\n");
    return ok ? 0 : 1;
}