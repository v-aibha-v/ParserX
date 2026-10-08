#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "src/clr.h"
#include "src/comparison.h"
#include "src/first_follow.h"
#include "src/grammar.h"
#include "src/lalr.h"
#include "src/lexer.h"
#include "src/parser.h"
#include "src/slr.h"
#include "src/table.h"
#include "src/util.h"

using namespace std;

namespace {

const size_t kMaxTracedTokens = 2000;
const size_t kMaxPrintedTokens = 200;

struct Options {
    string grammarPath = "grammar/language.txt";
    string inputPath;
    string code;
    bool haveCode = false;
    string parser = "all";
    string trace = "brief";
    bool tokens = false;
    bool states = false;
    bool tables = false;
};

void usage(ostream& os) {
    os << "Usage: parserx [--grammar FILE] [--input FILE | --code TEXT]\n"
          "               [--parser slr|lalr|clr|all] [--trace brief|full|none]\n"
          "               [--tokens] [--states] [--tables] [input-file]\n"
          "Without --input or --code the program is read from standard input.\n";
}

bool parseArgs(int argc, char** argv, Options& o, string& err) {
    for (int k = 1; k < argc; ++k) {
        string a = argv[k];
        bool hasValue = k + 1 < argc;
        if (a == "--help" || a == "-h") {
            usage(cout);
            exit(0);
        } else if (a == "--grammar" && hasValue) {
            o.grammarPath = argv[++k];
        } else if (a == "--input" && hasValue) {
            o.inputPath = argv[++k];
        } else if (a == "--code" && hasValue) {
            o.code = argv[++k];
            o.haveCode = true;
        } else if (a == "--parser" && hasValue) {
            o.parser = argv[++k];
            if (o.parser != "slr" && o.parser != "lalr" && o.parser != "clr" &&
                o.parser != "all") {
                err = "unknown parser '" + o.parser + "' (use slr, lalr, clr or all)";
                return false;
            }
        } else if (a == "--trace" && hasValue) {
            o.trace = argv[++k];
            if (o.trace != "brief" && o.trace != "full" && o.trace != "none") {
                err = "unknown trace mode '" + o.trace + "' (use brief, full or none)";
                return false;
            }
        } else if (a == "--tokens") {
            o.tokens = true;
        } else if (a == "--states") {
            o.states = true;
        } else if (a == "--tables") {
            o.tables = true;
        } else if (!a.empty() && a[0] != '-' && o.inputPath.empty()) {
            o.inputPath = a;
        } else {
            err = "unknown or incomplete option '" + a + "'";
            return false;
        }
    }
    return true;
}

void step(const string& title) {
    cout << "\n" << utilRepeat('=', 70) << "\n" << title << "\n"
         << utilRepeat('=', 70) << "\n";
}

int retypeForGrammar(vector<Token>& tokens, const Grammar& g) {
    int changed = 0;
    for (size_t k = 0; k < tokens.size(); ++k) {
        Token& t = tokens[k];
        if (!g.isTerminal(t.type) && g.isTerminal(t.lexeme)) {
            t.type = t.lexeme;
            ++changed;
        }
    }
    return changed;
}

string compactTree(const Grammar& g, const vector<Token>& tokens,
                   const ParseResult& r) {
    vector<string> stack;
    size_t pos = 0;
    for (size_t k = 0; k < r.steps.size(); ++k) {
        const ParseStep& s = r.steps[k];
        if (s.action.compare(0, 5, "shift") == 0) {
            stack.push_back(pos < tokens.size() ? tokens[pos].lexeme : "?");
            ++pos;
        } else if (s.reduceProd >= 0) {
            const Production& p = g.production(s.reduceProd);
            size_t n = p.rhs.size();
            if (n > stack.size()) return "(tree unavailable)";
            vector<string> kids(stack.end() - (long)n, stack.end());
            stack.resize(stack.size() - n);
            if (n == 1) stack.push_back(kids[0]);
            else if (n == 0) stack.push_back("[" + p.lhs + "]");
            else stack.push_back("[" + p.lhs + " " + utilJoin(kids, " ") + "]");
        }
    }
    return stack.size() == 1 ? stack[0] : "(tree unavailable)";
}

bool selected(const Options& o, const string& tableName) {
    if (o.parser == "all") return true;
    string lower;
    for (size_t k = 0; k < tableName.size(); ++k)
        lower += (char)tolower(static_cast<unsigned char>(tableName[k]));
    return lower == o.parser;
}

string tokenDesc(const string& type, const string& lexeme) {
    if (type == "$") return "end of input";
    if (type == lexeme) return "'" + lexeme + "'";
    return "'" + lexeme + "' (" + type + ")";
}

}

int main(int argc, char** argv) {
    Options opt;
    string err;
    if (!parseArgs(argc, argv, opt, err)) {
        cerr << "Error: " << err << "\n";
        usage(cerr);
        return 4;
    }

    // Read input
    string source;
    string sourceName;
    if (opt.haveCode) {
        source = opt.code;
        sourceName = "(command line)";
    } else if (!opt.inputPath.empty()) {
        if (!utilReadFile(opt.inputPath, source, err)) {
            cerr << "Error: " << err << "\n";
            return 4;
        }
        sourceName = opt.inputPath;
    } else {
        cerr << "Enter the program, then press Ctrl+Z and Enter (Windows) "
                "or Ctrl+D (Linux/macOS):\n";
        ostringstream ss;
        ss << cin.rdbuf();
        source = ss.str();
        sourceName = "(standard input)";
    }

    utilBanner(cout, "PARSERX - SLR / LALR / CLR PARSER");
    cout << "Grammar file : " << opt.grammarPath << "\n";
    cout << "Input        : " << sourceName << " (" << source.size() << " bytes)\n";

    // Step 1: Lexer
    step("STEP 1  LEXER");
    LexResult lex = Lexer::tokenize(source);
    cout << "Tokens: " << lex.tokens.size() << " (including the end marker $)\n";
    if (opt.tokens || lex.tokens.size() <= kMaxPrintedTokens) printTokens(cout, lex);
    else cout << "(token table not printed for large input; use --tokens)\n";
    if (!lex.errors.empty()) {
        if (!opt.tokens && lex.tokens.size() > kMaxPrintedTokens) printLexErrors(cout, lex);
        cout << "\nParsing skipped because of lexical errors.\n";
        cout << "\n" << utilRepeat('-', 70) << "\n";
        for (size_t k = 0; k < lex.errors.size(); ++k) {
            cout << "RESULT LEXICAL-ERROR symbol='" << lex.errors[k].character
                 << "' line=" << lex.errors[k].line
                 << " col=" << lex.errors[k].column << "\n";
        }
        return 3;
    }

    // Step 2: Grammar
    step("STEP 2  GRAMMAR LOADER");
    Grammar g;
    if (!g.load(opt.grammarPath, err)) {
        cout << "Grammar error: " << err << "\n";
        cout << "\n" << utilRepeat('-', 70) << "\n";
        cout << "RESULT GRAMMAR-ERROR " << err << "\n";
        return 2;
    }
    g.print(cout);
    int retyped = retypeForGrammar(lex.tokens, g);
    if (retyped > 0) {
        cout << "Note: " << retyped << " token(s) retyped to this grammar's terminals: ";
        vector<string> types;
        for (size_t k = 0; k < lex.tokens.size(); ++k) types.push_back(lex.tokens[k].type);
        cout << utilJoin(types, " ") << "\n";
    }

    // Steps 3-4: FIRST/FOLLOW
    FirstFollow ff(g);
    step("STEP 3  FIRST SETS");
    vector<string> nullables;
    for (size_t k = 0; k < g.nonterminals().size(); ++k)
        if (ff.nullable(g.nonterminals()[k])) nullables.push_back(g.nonterminals()[k]);
    cout << "Nullable non-terminals: "
         << (nullables.empty() ? "none" : utilJoin(nullables, ", ")) << "\n";
    ff.printFirst(cout);
    step("STEP 4  FOLLOW SETS");
    ff.printFollow(cout);

    // Steps 5-7: Tables
    step("STEPS 5-7  SLR (LR(0)), CLR (LR(1)) AND LALR (merged CLR) TABLES");
    BuiltTables bt = buildAllTables(g, ff);
    const ParsingTable* tables[3] = {&bt.slr, &bt.lalr, &bt.clr};
    const LRMachine* machines[3] = {&bt.slrMachine, &bt.lalrMachine, &bt.clrMachine};
    const long buildUs[3] = {bt.slrMicroseconds, bt.lalrMicroseconds, bt.clrMicroseconds};
    for (int k = 0; k < 3; ++k) {
        cout << left << setw(5) << tables[k]->name << ": "
             << setw(4) << tables[k]->stateItems.size() << " states, "
             << tables[k]->actionEntries() << " ACTION + " << tables[k]->gotoEntries()
             << " GOTO entries, built in " << buildUs[k] << " us\n";
    }
    int mergedGroups = 0;
    for (size_t s = 0; s < bt.lalrMachine.states.size(); ++s)
        if (bt.lalrMachine.states[s].note.compare(0, 6, "merged") == 0) ++mergedGroups;
    cout << "LALR merged " << bt.clrMachine.states.size() << " CLR states into "
         << bt.lalrMachine.states.size() << " states (" << mergedGroups
         << " states came from two or more CLR states with the same core)\n";
    for (int k = 0; k < 3; ++k) {
        if (!selected(opt, tables[k]->name)) continue;
        if (opt.states) {
            cout << "\n";
            printMachineStates(cout, g, *machines[k]);
        }
        if (opt.tables) {
            cout << "\n" << tables[k]->name << " ACTION table\n";
            printActionTable(cout, g, *tables[k], false);
            cout << "\n" << tables[k]->name << " GOTO table\n";
            printGotoTable(cout, g, *tables[k], false);
        }
    }
    if (!opt.states && !opt.tables)
        cout << "(use --states to print item sets and --tables for ACTION/GOTO)\n";

    // Step 8: Conflicts
    step("STEP 8  CONFLICT CHECK");
    for (int k = 0; k < 3; ++k) {
        printConflicts(cout, *tables[k]);
        if (tables[k]->kind == MachineKind::LALR) {
            for (size_t c = 0; c < tables[k]->conflicts.size(); ++c) {
                int st = tables[k]->conflicts[c].state;
                cout << "      LALR state " << st << " is "
                     << tables[k]->stateNotes[(size_t)st] << "\n";
            }
        }
    }

    // Steps 9-10: Parse
    bool record = opt.trace != "none" && lex.tokens.size() <= kMaxTracedTokens;
    bool allAccepted = true;
    vector<string> treeLines;
    for (int k = 0; k < 3; ++k) {
        if (!selected(opt, tables[k]->name)) continue;
        step("STEPS 9-10  PARSE WITH " + tables[k]->name);
        Parser p(g, *tables[k]);
        ParseResult r = p.parse(lex.tokens, record);
        if (record) printTrace(cout, g, r, opt.trace == "full");
        else if (opt.trace != "none")
            cout << "(trace not recorded: input has more than " << kMaxTracedTokens
            << " tokens)\n";
        if (r.accepted) {
            cout << "\n" << tables[k]->name << ": ACCEPTED in " << r.stepCount
                 << " steps\n";
            if (record) {
                string tree = compactTree(g, lex.tokens, r);
                cout << "Structure: " << tree << "\n";
                treeLines.push_back("TREE " + tables[k]->name + ": " + tree);
            }
        } else {
            allAccepted = false;
            cout << "\n" << tables[k]->name << ": REJECTED at step " << r.errorStep
                 << " on " << tokenDesc(r.error.unexpectedType, r.error.unexpectedLexeme)
                 << " after " << r.reductionsBeforeError
                 << " reduce(s) on that token\n";
            printParseError(cout, source, r);
        }
    }

    // Step 11: Comparison
    step("STEP 11  COMPARISON OF SLR, LALR AND CLR ON THIS INPUT");
    ComparisonResult cmp = compareParsers(g, lex.tokens, bt);
    printComparison(cout, cmp);

    // Summary lines
    cout << "\n" << utilRepeat('-', 70) << "\n";
    for (int k = 0; k < 3; ++k) {
        cout << "TABLE " << tables[k]->name
             << " states=" << tables[k]->stateItems.size()
             << " action=" << tables[k]->actionEntries()
             << " goto=" << tables[k]->gotoEntries()
             << " sr=" << tables[k]->srConflicts()
             << " rr=" << tables[k]->rrConflicts() << "\n";
    }
    for (int k = 0; k < 3; ++k) {
        for (size_t c = 0; c < tables[k]->conflicts.size(); ++c) {
            const Conflict& cf = tables[k]->conflicts[c];
            cout << "CONFLICT " << tables[k]->name << " " << cf.kind << " on '"
                 << cf.symbol << "' state=" << cf.state << " -> " << cf.resolution
                 << "\n";
        }
    }
    for (size_t k = 0; k < cmp.parsers.size(); ++k) {
        const ParserReport& pr = cmp.parsers[k];
        cout << "RESULT " << pr.name << " ";
        if (pr.accepted) {
            cout << "ACCEPTED steps=" << pr.steps << "\n";
        } else {
            cout << "REJECTED token=" << pr.errorToken << " line=" << pr.errorLine
                 << " col=" << pr.errorColumn << " step=" << pr.errorStep
                 << " reduces=" << pr.reductionsBeforeError
                 << " expected=" << utilJoin(pr.expected, " ") << "\n";
        }
    }
    for (size_t k = 0; k < treeLines.size(); ++k) cout << treeLines[k] << "\n";
    return allAccepted ? 0 : 1;
}
