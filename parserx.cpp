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

struct Options {
    string grammarPath = "grammar/language.txt";
    string inputPath;
    string code;
    bool haveCode = false;
    string parser = "all";
    string trace = "none";
    bool tokens = false;
    bool tree = false;
    bool states = false;
    bool tables = false;
    bool verbose = false;
};

void usage(ostream& os) {
    os << "Usage: parserx [--grammar FILE] [--input FILE | --code TEXT]\n"
          "               [--parser slr|lalr|clr|all] [--trace brief|full|none]\n"
          "               [--tokens] [--tree] [--states] [--tables] [--verbose]\n"
          "               [input-file]\n"
          "Without --input or --code the program is typed in; end it with a line END.\n"
          "Default output is a concise summary. Use --tree for the parse\n"
          "structure, --tokens for the token table, --trace for parse steps,\n"
          "--states/--tables for item sets and ACTION/GOTO tables, and\n"
          "--verbose for grammar plus FIRST/FOLLOW details.\n";
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
        } else if (a == "--tree") {
            o.tree = true;
        } else if (a == "--states") {
            o.states = true;
        } else if (a == "--tables") {
            o.tables = true;
        } else if (a == "--verbose" || a == "--detail" || a == "--details") {
            o.verbose = true;
        } else if (!a.empty() && a[0] != '-' && o.inputPath.empty()) {
            o.inputPath = a;
        } else {
            err = "unknown or incomplete option '" + a + "'";
            return false;
        }
    }
    return true;
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
        cerr << "Enter the program. Type END on a new line to finish:\n";
        string line;
        while (getline(cin, line)) {
            size_t eof = line.find_first_of("\x1A\x04");
            if (eof != string::npos) {
                source += line.substr(0, eof) + "\n";
                break;
            }
            if (utilTrim(line) == "END") break;
            source += line + "\n";
        }
        sourceName = "(standard input)";
    }

    cout << "========================================================\n"
         << "                       PARSEX\n"
         << "             SLR(1) | CLR(1) | LALR(1)\n"
         << "========================================================\n\n";
    cout << "Input File : " << sourceName << "\n";
    cout << "Grammar    : " << opt.grammarPath << "\n\n";

    // Lexer (concise by default; full token table only with --tokens).
    LexResult lex = Lexer::tokenize(source);
    cout << "Tokens: " << lex.tokens.size() << " (including the end marker $)\n";
    if (opt.tokens) printTokens(cout, lex);
    if (!lex.errors.empty()) {
        if (!opt.tokens) printLexErrors(cout, lex);
        cout << "\nParsing skipped because of lexical errors.\n";
        cout << "\n" << utilRepeat('-', 56) << "\n";
        for (size_t k = 0; k < lex.errors.size(); ++k) {
            cout << "RESULT LEXICAL-ERROR symbol='" << lex.errors[k].character
                 << "' line=" << lex.errors[k].line
                 << " col=" << lex.errors[k].column << "\n";
        }
        return 3;
    }

    // Grammar loader.
    Grammar g;
    if (!g.load(opt.grammarPath, err)) {
        cout << "Grammar error: " << err << "\n";
        cout << "\n" << utilRepeat('-', 56) << "\n";
        cout << "RESULT GRAMMAR-ERROR " << err << "\n";
        return 2;
    }
    if (opt.verbose) g.print(cout);
    int retyped = retypeForGrammar(lex.tokens, g);
    if (retyped > 0 && opt.verbose) {
        cout << "Note: " << retyped << " token(s) retyped to this grammar's terminals: ";
        vector<string> types;
        for (size_t k = 0; k < lex.tokens.size(); ++k) types.push_back(lex.tokens[k].type);
        cout << utilJoin(types, " ") << "\n";
    }

    // FIRST/FOLLOW: one summary line always (needed for epsilon tests);
    // full sets only with --verbose.
    FirstFollow ff(g);
    vector<string> nullables;
    for (size_t k = 0; k < g.nonterminals().size(); ++k)
        if (ff.nullable(g.nonterminals()[k])) nullables.push_back(g.nonterminals()[k]);
    cout << "Nullable non-terminals: "
         << (nullables.empty() ? "none" : utilJoin(nullables, ", ")) << "\n";
    if (opt.verbose) {
        ff.printFirst(cout);
        ff.printFollow(cout);
    }

    // Parser tables (built algorithmically from the grammar).
    BuiltTables bt = buildAllTables(g, ff);
    const ParsingTable* tables[3] = {&bt.slr, &bt.lalr, &bt.clr};
    const LRMachine* machines[3] = {&bt.slrMachine, &bt.lalrMachine, &bt.clrMachine};
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
    if (opt.verbose) {
        cout << "\nConflicts (detected during table construction):\n";
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
    } else {
        for (int k = 0; k < 3; ++k) {
            if (tables[k]->kind == MachineKind::LALR) {
                for (size_t c = 0; c < tables[k]->conflicts.size(); ++c) {
                    int st = tables[k]->conflicts[c].state;
                    cout << "      LALR state " << st << " is "
                         << tables[k]->stateNotes[(size_t)st] << "\n";
                }
            }
        }
    }

    // Parse every selected parser. Traces are opt-in (--trace). The
    // tree-like parse structure text is always recorded for the
    // machine-readable TREE line; --tree additionally prints the full
    // readable copy, and long default TREE lines are truncated.
    const bool wantTrace = opt.trace != "none";
    const size_t tooBig =
        lex.tokens.size() > kMaxTracedTokens ? lex.tokens.size() : 0;
    vector<ParseResult> results(3);
    vector<bool> ran(3, false);
    for (int k = 0; k < 3; ++k) {
        if (!selected(opt, tables[k]->name)) continue;
        Parser p(g, *tables[k]);
        ParseResult r = p.parse(lex.tokens,
                                lex.tokens.size() <= kMaxTracedTokens);
        results[(size_t)k] = r;
        ran[(size_t)k] = true;
        if (wantTrace) {
            cout << "\n--- " << tables[k]->name << " parse trace ---\n";
            if (r.steps.empty() && tooBig > 0)
                cout << "(trace not recorded: input has " << tooBig
                     << " tokens, limit is " << kMaxTracedTokens << ")\n";
            else
                printTrace(cout, g, r, opt.trace == "full");
        }
        if (opt.tree && r.accepted && !r.steps.empty()) {
            string tree = compactTree(g, lex.tokens, r);
            cout << "\n" << tables[k]->name
                 << " parse structure (tree-like, not a full AST):\n"
                 << tree << "\n";
        }
        if (!r.accepted && opt.verbose) {
            cout << "\n" << tables[k]->name << ": REJECTED at step " << r.errorStep
                 << " on " << tokenDesc(r.error.unexpectedType, r.error.unexpectedLexeme)
                 << " after " << r.reductionsBeforeError
                 << " reduce(s) on that token\n";
            printParseError(cout, source, r);
        }
    }

    // Concise professional summary.
    ComparisonResult cmp = compareParsers(g, lex.tokens, bt);
    cout << "\nPARSING RESULTS\n";
    cout << utilRepeat('-', 56) << "\n";
    cout << left << setw(10) << "Parser" << setw(12) << "Result"
         << setw(10) << "States" << "Steps\n";
    cout << utilRepeat('-', 56) << "\n";
    for (size_t k = 0; k < cmp.parsers.size(); ++k) {
        const ParserReport& pr = cmp.parsers[k];
        if (!selected(opt, pr.name)) continue;
        cout << left << setw(10) << pr.name
             << setw(12) << (pr.accepted ? "ACCEPTED" : "REJECTED")
             << setw(10) << pr.states << pr.steps << "\n";
    }
    cout << utilRepeat('-', 56) << "\n";

    cout << "\nPARSER COMPARISON\n";
    cout << utilRepeat('-', 56) << "\n";
    cout << left << setw(10) << "Parser" << setw(15) << "ACTION Entries"
         << setw(13) << "GOTO Entries" << "Conflicts\n";
    cout << utilRepeat('-', 56) << "\n";
    for (int k = 0; k < 3; ++k) {
        if (!selected(opt, tables[k]->name)) continue;
        cout << left << setw(10) << tables[k]->name
             << setw(15) << tables[k]->actionEntries()
             << setw(13) << tables[k]->gotoEntries()
             << (tables[k]->srConflicts() + tables[k]->rrConflicts()) << "\n";
    }
    cout << utilRepeat('-', 56) << "\n";
    bool anyConflict = false;
    for (int k = 0; k < 3; ++k) {
        if (!selected(opt, tables[k]->name)) continue;
        for (size_t c = 0; c < tables[k]->conflicts.size(); ++c) {
            const Conflict& cf = tables[k]->conflicts[c];
            if (!anyConflict) {
                cout << "Conflict details (resolved by documented SHIFT policy):\n";
                anyConflict = true;
            }
            cout << "  " << tables[k]->name << " state " << cf.state << " on '"
                 << cf.symbol << "' (" << cf.kind << "): " << cf.existing
                 << " vs " << cf.incoming << "\n";
        }
    }

    // Overall verdict.
    int accepted = 0, rejected = 0;
    for (int k = 0; k < 3; ++k) {
        if (!ran[(size_t)k]) continue;
        if (results[(size_t)k].accepted) ++accepted;
        else ++rejected;
    }
    cout << "\n";
    if (accepted > 0 && rejected == 0)
        cout << "OVERALL RESULT: ALL PARSERS ACCEPTED THE INPUT\n";
    else if (rejected > 0 && accepted == 0)
        cout << "OVERALL RESULT: ALL PARSERS REJECTED THE INPUT\n";
    else if (accepted > 0 && rejected > 0)
        cout << "OVERALL RESULT: PARSERS DISAGREED (" << accepted
             << " accepted, " << rejected << " rejected)\n";
    else
        cout << "OVERALL RESULT: NO PARSER RAN\n";
    if (rejected > 0) {
        cout << "\nFirst error (with line/column where available):\n";
        for (int k = 0; k < 3; ++k) {
            if (!ran[(size_t)k] || results[(size_t)k].accepted) continue;
            const ParseError& e = results[(size_t)k].error;
            cout << "  " << tables[k]->name << ": line " << e.line << ", column "
                 << e.column << " on ";
            if (e.unexpectedType == "$") cout << "end of input ($)";
            else cout << "'" << e.unexpectedLexeme << "' (token " << e.unexpectedType << ")";
            if (!e.expected.empty())
                cout << "; expected one of: " << utilJoin(e.expected, ", ");
            cout << "\n";
            break;
        }
    }

    // Machine-readable summary lines (kept for scripts and the test suite).
    // TREE lines are the compact one-line parse structure; very long trees
    // are truncated in the default output so a faculty demo is not flooded.
    const size_t kMaxTreeChars = 4000;
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
        if (!selected(opt, pr.name)) continue;
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
    for (int k = 0; k < 3; ++k) {
        if (!ran[(size_t)k] || !results[(size_t)k].accepted) continue;
        if (results[(size_t)k].steps.empty()) continue;
        string tree = compactTree(g, lex.tokens, results[(size_t)k]);
        if (!opt.tree && tree.size() > kMaxTreeChars)
            tree = tree.substr(0, kMaxTreeChars) + " ... (truncated; use --tree)";
        cout << "TREE " << tables[k]->name << ": " << tree << "\n";
    }
    bool allAccepted = (rejected == 0 && accepted > 0);
    return allAccepted ? 0 : 1;
}
