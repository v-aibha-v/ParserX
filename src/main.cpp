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

using namespace std;

static void showParse(const string& label, const ParseResult& r) {
    cout << "  " << label << ": "
         << (r.accepted ? "ACCEPTED" : "REJECTED")
         << " (" << r.steps.size() << " steps)\n";
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

int main() {
    utilBanner(cout, "LALR / CLR / SLR PARSER - CORE DEMONSTRATION");

    cout << "\n[1] Grammar\n";
    Grammar g;
    string err;
    if (!g.load("grammar/language.txt", err)) {
        cout << "Grammar FAILED to load: " << err << "\n";
        return 1;
    }
    cout << "Grammar loaded successfully from grammar/language.txt\n";
    cout << "Productions: " << g.productions().size()
         << ", Terminals: " << g.terminals().size()
         << ", Non-terminals: " << g.nonterminals().size()
         << ", Start: " << g.startSymbol() << "\n";

    cout << "\n[2] FIRST / FOLLOW (computed by the implementation)\n";
    FirstFollow ff(g);
    cout << "FIRST(E)  = " << utilFormatSet(ff.first("E")) << "\n";
    cout << "FIRST(F)  = " << utilFormatSet(ff.first("F")) << "\n";
    cout << "FOLLOW(E) = " << utilFormatSet(ff.follow("E")) << "\n";
    cout << "FOLLOW(S) = " << utilFormatSet(ff.follow("S")) << "\n";

    cout << "\n[3] Parser tables (built algorithmically)\n";
    LRMachine slrM = buildSLRMachine(g);
    ParsingTable slrT = buildParsingTable(g, ff, slrM);
    LRMachine clrM = buildCLRMachine(g, ff);
    ParsingTable clrT = buildParsingTable(g, ff, clrM);
    LRMachine lalrM = buildLALRMachine(g, clrM);
    ParsingTable lalrT = buildParsingTable(g, ff, lalrM);

    cout << "SLR states  = " << slrM.states.size() << "\n";
    cout << "CLR states  = " << clrM.states.size() << "\n";
    cout << "LALR states = " << lalrM.states.size() << "\n";

    cout << "\n[4] Conflicts (detected during table construction)\n";
    cout << "SLR : " << slrT.srConflicts() << " shift/reduce, "
         << slrT.rrConflicts() << " reduce/reduce\n";
    cout << "CLR : " << clrT.srConflicts() << " shift/reduce, "
         << clrT.rrConflicts() << " reduce/reduce\n";
    cout << "LALR: " << lalrT.srConflicts() << " shift/reduce, "
         << lalrT.rrConflicts() << " reduce/reduce\n";
    if (!lalrT.conflicts.empty())
        cout << "Note: " << lalrT.conflicts[0].resolution << "\n";

    cout << "\n[5] Valid program:  x = 5 ;\n";
    LexResult ok = Lexer::tokenize("x = 5 ;");
    Parser pSlr(g, slrT), pLalr(g, lalrT), pClr(g, clrT);
    showParse("SLR ", pSlr.parse(ok.tokens));
    showParse("LALR", pLalr.parse(ok.tokens));
    showParse("CLR ", pClr.parse(ok.tokens));

    cout << "\n[6] Invalid program:  x = a + ;\n";
    LexResult bad = Lexer::tokenize("x = a + ;");
    showParse("LALR", pLalr.parse(bad.tokens));

    cout << "\nDone.\n";
    return 0;
}
