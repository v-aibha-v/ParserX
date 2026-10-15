# ParserX — SLR, CLR and LALR Parsers for an Advanced Grammar

SLR, Canonical LR(1) (CLR) and LALR parsers built from scratch in C++17
(no Yacc/Bison) for a small but realistic programming language with
declarations, integer and float literals, full operator precedence,
`if`/`else`, `while`, `for`, blocks, `break`/`continue`, functions with
parameters and return statements. All three parsers are generated
algorithmically from one external grammar file and compared on the same
inputs.

## Project objectives

* Build all three LR parser families from the same grammar and show
  exactly where they differ (state counts, conflicts, acceptance).
* Keep the real algorithms: LR(0)/LR(1) closure, GOTO, FOLLOW-based SLR
  reductions, LR(1) lookahead propagation, LALR core merging with
  transition remapping, and table-driven parsing with real error messages.
* Report conflicts honestly: every conflict found during table
  construction is printed with its state, terminal, competing actions and
  the resolution that was applied.

## Supported language features

* Variable declarations: `int x;`, `float y = 1.5;`
* Assignment as a statement: `x = a + b * 2;` (chained `a = b = c;` is
  rejected; assignment is not an expression operator)
* Integer literals (`42`) and float literals (`1.5`)
* Arithmetic `+ - * / %`, relational `< > <= >=`, equality `== !=`,
  logical `&& || !`, unary `-` and `!`
* Parenthesized and nested expressions
* `if` / `else`, `while`, `for` (any of the three clauses may be empty),
  nested conditionals, `{}` blocks, empty statements `;`
* `break` and `continue`
* Function declarations `int add(int a, int b) { ... }`, calls inside
  expressions (`x = add(1, 2) * 3;`) and as statements (`add(x, y);`),
  `return Expr;`
* `//` line comments and `/* ... */` block comments (may span lines)
* Lexical errors for unknown characters and malformed numbers
  (`12abc`, `1.`), with line/column positions

Grammar design: one non-terminal per precedence level, low to high:

```
Expr -> LogicalOr -> LogicalAnd -> Equality -> Relational
      -> Additive -> Multiplicative -> Unary -> Primary
```

All binary operators are left-associative through left recursion. The
dangling-else ambiguity is kept in the grammar on purpose (see
*Conflict handling*).

## The grammar file

`grammar/language.txt` is the main grammar (70 productions including the
augmented start production, 34 terminals). Format:

```
A -> B C          one production per line
A -> B | C        several alternatives on one line
A -> eps          epsilon production
# ...             comment to end of line
```

`||` is kept as a single terminal symbol; a single `|` separates
alternatives. Other grammar files:

| File | Purpose |
|------|---------|
| `grammar/language.txt` | The advanced language (default) |
| `grammar/simple_language.txt` | The original simple E/T/F grammar, kept for reference |
| `grammar/slr_vs_lalr.txt` | Classic grammar SLR cannot handle |
| `grammar/lalr_vs_clr.txt` | Classic grammar LALR cannot handle |
| `grammar/epsilon.txt`, `tests/grammars/epsilon_ab.txt` | Epsilon FIRST/FOLLOW demos |
| `tests/grammars/no_arrow.txt`, `tests/grammars/empty.txt` | Grammar-loader error demos |

Sample programs: the five `examples/0*.pgr` demo files (see *Example
programs* below), plus the legacy `examples/sample.pgr` (valid, exercises
every feature) and `examples/invalid.pgr` (several lexical errors).
## Source files

| File | What it does |
|------|--------------|
| `src/lexer.cpp` | Tokens with line/column; keywords, ids, int/float literals, all operators, comments, lexical error messages |
| `src/grammar.cpp` | Loads the grammar file, classifies symbols, adds `Program' -> Program`, ids for every symbol |
| `src/first_follow.cpp` | Nullable set, then FIRST and FOLLOW by fixed-point iteration |
| `src/slr.cpp` | LR(0) item sets (closure and GOTO) for SLR |
| `src/clr.cpp` | Canonical LR(1) item sets with exact lookaheads |
| `src/lalr.cpp` | Merges CLR states with the same LR(0) core and remaps transitions |
| `src/table.cpp` | ACTION/GOTO construction and conflict detection/resolution |
| `src/parser.cpp` | One table-driven parser for all three tables (shift/reduce/accept/error), trace, expected-token error reports |
| `src/comparison.cpp` | Timed builds and SLR/LALR/CLR comparison on one input |
| `src/main.cpp` | Fixed demo of the advanced grammar (build, conflicts, valid + invalid program) |
| `parserx.cpp` | Full CLI driver over the whole pipeline |

## Architecture

```mermaid
flowchart TD
    A[/Source program/] --> B[lexer.cpp<br/>tokens]
    G[/Grammar file/] --> C[grammar.cpp<br/>productions and symbols]
    C --> D[first_follow.cpp<br/>FIRST and FOLLOW]
    C --> E1[slr.cpp<br/>LR(0) states]
    D --> E2[clr.cpp<br/>LR(1) states]
    E2 --> E3[lalr.cpp<br/>merged states]
    E1 --> T[table.cpp<br/>ACTION / GOTO + conflicts]
    E2 --> T
    E3 --> T
    D --> T
    B --> P[parser.cpp<br/>shift / reduce / accept]
    T --> P
    P --> R[comparison.cpp<br/>SLR vs LALR vs CLR]
    R --> O[/Output: trace, errors, comparison/]
```

**Lexer.** Keywords (`int float if else while for return break continue`)
get a token type equal to the word; identifiers are `id`; literals are
`intconst`/`floatconst`; `<= >= == != && ||` are single two-character
tokens; everything else is a one-character token. `//` and `/* */`
comments are skipped. Errors carry a message, line and column.

**FIRST/FOLLOW.** `nullable` is computed first, then FIRST by iterating
productions to a fixed point, then FOLLOW with the standard two rules
(`FIRST(beta)` after a symbol, `FOLLOW(A)` when `beta` is nullable, plus
`FOLLOW(start)` contains `$`). `firstOfSequence` is used by both FOLLOW
and the LR(1) closure.

**SLR.** LR(0) closure: for every item with a dot before a non-terminal
`X`, add `X -> . gamma` for all `X` productions. GOTO moves the dot over
one symbol and closes. Reduce actions are added on `FOLLOW(LHS)`.

**CLR.** LR(1) items `[A -> alpha . beta, a]`. Closure computes
`FIRST(beta a)` and adds `[B -> . gamma, b]` for every `b` in that set
(`$`-lookahead included, EPS skipped). GOTO propagates lookaheads
unchanged; items are deduplicated by `(production, dot, lookahead)`.

**LALR.** CLR states are grouped by their LR(0) core
(`(production, dot)` pairs); lookahead sets of states in a group are
unioned; transitions of all members are remapped to the group ids. Every
merged group is annotated (`merged CLR states { i, j, ... }`), so any
conflict introduced by merging can be traced to its origin states.

**Tables.** For every state: terminal transitions become shift actions,
non-terminal transitions become GOTO entries, completed items become
reduce actions (SLR: on FOLLOW; CLR/LALR: on the item's lookaheads), and
the augmented start item becomes accept on `$`.

**Parser.** One table-driven loop for all three tables: on the top
state and current token, shift pushes, reduce pops `|rhs|` states and
pushes `GOTO`, accept stops, and a missing entry is a syntax error that
reports state, unexpected token, line/column and the set of expected
terminals.
## SLR vs CLR vs LALR on this grammar

Actual numbers produced by the implementation (see *Demo output*):

| | SLR | LALR | CLR |
|---|---|---|---|
| States | 130 | 130 | 359 |
| ACTION entries | 1112 | 1110 | 3152 |
| GOTO entries | 269 | 269 | 778 |
| Shift/reduce conflicts | 1 | 1 | 2 |
| Reduce/reduce conflicts | 0 | 0 | 0 |

SLR and LALR have the same state count here because LALR merging of the
CLR states reconstructs exactly the LR(0) automaton for this grammar;
they differ in two ACTION entries where CLR keeps finer lookaheads.
CLR has the most states but the finest lookaheads.

The theoretical differences are demonstrated with the classic example
grammars (`make test` cases 23/24):

* `grammar/slr_vs_lalr.txt` (`S -> L = R | R; L -> * R | id; R -> L`):
  SLR reports a shift/reduce conflict on `=`; LALR and CLR are
  conflict-free and accept `* id = id`.
* `grammar/lalr_vs_clr.txt` (`S -> a A d | b B d | a B e | b A e;
  A -> c; B -> c`): LALR merging creates reduce/reduce conflicts and
  rejects `b c d`; CLR (14 states, no conflicts) accepts it.

## Conflict handling

Every conflict found while filling the ACTION table is recorded with
parser name, state, terminal, the existing action, the incoming action
and the resolution:

* **Shift/reduce** — resolved by **shift** (shift wins). This is the
  documented dangling-else policy: `else` binds to the nearest `if`.
  The language grammar has exactly this conflict (SLR 1 state, LALR 1
  state, CLR 2 states — all on `else`); the shift choice is what makes
  `if (a) if (b) x = 1; else x = 2;` bind the `else` to the inner `if`,
  which the tests verify from the parse trees.
* **Reduce/reduce** — resolved by keeping the **lower-numbered
  production**; the choice is reported. The language grammar has no
  reduce/reduce conflicts; the demonstration grammars show real ones.

No conflict is hidden: the summary lines print `sr=`/`rr=` counts per
table, and test case 60 asserts `rr=0` and the `else` conflicts.

## Build and run

Requires a C++17 compiler. On Windows use MSYS2 UCRT64 `g++`
(`mingw32-make`), on Linux/macOS any recent `g++` or `clang++`.

```
make                # builds parserx (CLI driver) and parser (fixed demo)
make test           # builds parserx and runs the whole test suite
make demo           # builds and runs the fixed demo (src/main.cpp)
make clean
```

Without make:

```
g++ -std=c++17 -O2 -Isrc parserx.cpp src/grammar.cpp src/lexer.cpp \
    src/first_follow.cpp src/slr.cpp src/clr.cpp src/lalr.cpp \
    src/table.cpp src/parser.cpp src/comparison.cpp -o parserx
```

On Windows with MinGW, add `-static` so the exe does not depend on a
libstdc++ DLL.

Run the parser:

```
parserx --input examples/sample.pgr        # parse a file
parserx --code "x = a + b * 2 ;"           # parse one-liner code
parserx                                    # type program, end with line END
parserx --grammar grammar/slr_vs_lalr.txt --code "* id = id"
```

Useful flags: `--parser slr|lalr|clr|all` (which parsers to run),
`--trace brief|full|none` (detailed parse steps, off by default),
`--tokens` (token table), `--tree` (full tree-like parse structure),
`--states`/`--tables` (item sets and ACTION/GOTO tables), `--verbose`
(grammar dump, FIRST/FOLLOW sets and full syntax-error reports).

## Example programs

Five short demo files live in `examples/`:

| File | Purpose | Expected result |
|------|---------|-----------------|
| `01_arithmetic.pgr` | `int`/`float` declarations, `+ - * / %`, parentheses, unary `-`, precedence (`b * 2` inside an addition) | ACCEPTED |
| `02_conditions.pgr` | `if`, `if-else`, `< == != >= <=`, `&&`, `\|\|`, `!`, nested conditionals | ACCEPTED |
| `03_loops.pgr` | `while`, `for` (init/condition/step), nested `if`, `break`, `continue` | ACCEPTED |
| `04_functions.pgr` | `int`/`float` function definitions, parameters, calls in expressions and as statements, `return`, locals | ACCEPTED |
| `05_invalid_syntax.pgr` | Missing `;` after an initializer (a genuine syntax error) | REJECTED with line/column |

Build, then run each example on Windows PowerShell:

```
g++ -std=c++17 -O2 -Wall -Wextra -Isrc parserx.cpp src/grammar.cpp src/lexer.cpp src/first_follow.cpp src/slr.cpp src/clr.cpp src/lalr.cpp src/table.cpp src/parser.cpp src/comparison.cpp -o parserx.exe
.\parserx.exe --input examples\01_arithmetic.pgr
.\parserx.exe --input examples\02_conditions.pgr
.\parserx.exe --input examples\03_loops.pgr
.\parserx.exe --input examples\04_functions.pgr
.\parserx.exe --input examples\05_invalid_syntax.pgr
```

(`make` also builds `parserx`; `make test` runs the full suite.)

## Command-line output

The default output is a concise faculty-demo summary:

```
========================================================
                       PARSEX
             SLR(1) | CLR(1) | LALR(1)
========================================================

Input File : examples/01_arithmetic.pgr
Grammar    : grammar/language.txt

PARSING RESULTS
--------------------------------------------------------
Parser       Result       States       Steps
--------------------------------------------------------
SLR(1)       ACCEPTED     ...
LALR(1)      ACCEPTED     ...
CLR(1)       ACCEPTED     ...
--------------------------------------------------------

PARSER COMPARISON
--------------------------------------------------------
Parser       ACTION Entries   GOTO Entries   Conflicts
--------------------------------------------------------
SLR(1)       ...              ...             ...
LALR(1)      ...              ...             ...
CLR(1)       ...              ...             ...
--------------------------------------------------------

OVERALL RESULT: ALL PARSERS ACCEPTED THE INPUT
```

State counts, step counts, ACTION/GOTO entry counts, conflict counts and
the ACCEPTED/REJECTED verdicts all come from the real parser tables and
parse runs — nothing is hardcoded. Conflict details (state, symbol,
competing actions, SHIFT resolution) follow the comparison table. The
final line is one of: all accepted, all rejected, parsers disagreed, or
execution failed before parsing. Rejected inputs also show the first
error with line/column and the expected tokens. Optional detail flags:

```
.\parserx.exe --input examples\01_arithmetic.pgr --tree     # full tree-like parse structure
.\parserx.exe --input examples\01_arithmetic.pgr --tokens   # token stream
.\parserx.exe --input examples\01_arithmetic.pgr --trace brief   # detailed parse steps
.\parserx.exe --input examples\01_arithmetic.pgr --tables   # ACTION/GOTO statistics/tables
.\parserx.exe --input examples\01_arithmetic.pgr --parser all    # run SLR + LALR + CLR
```

The "parse structure" text is a compact bracket rendering rebuilt from
the recorded shift/reduce steps (it is not a separately constructed
AST). Machine-readable `TABLE`/`CONFLICT`/`RESULT`/`TREE` lines are also
printed for scripts and the test suite; very long default `TREE` lines
are truncated (use `--tree` for the full text).

The invalid example is expected to fail: `05_invalid_syntax.pgr` is
REJECTED by all three parsers with a syntax error (line/column
included) and a nonzero exit status. Rejection is the correct outcome,
not a test failure.

## Demo output

`parser` (fixed demo of `src/main.cpp`, abbreviated):

```
[3] Parser tables (built algorithmically from the grammar)
  SLR   130 states, 1112 ACTION + 269 GOTO entries
  LALR  130 states, 1110 ACTION + 269 GOTO entries
  CLR   359 states, 3152 ACTION + 778 GOTO entries

[4] Conflicts (detected during table construction)
SLR conflicts (1): 1 shift/reduce, 0 reduce/reduce.
  [1] state 108 on 'else' (shift/reduce)
      existing: shift 118
      incoming: reduce 19 (Stmt -> if ( Expr ) Stmt)
      resolved by SHIFT (shift preferred, dangling-else rule)
LALR conflicts (1): 1 shift/reduce, 0 reduce/reduce.   (same resolution)
CLR conflicts (2): 2 shift/reduce, 0 reduce/reduce.    (both on 'else')

[5] Valid program (declarations, if/else, while, for, call)
  SLR : ACCEPTED (320 steps)
  LALR: ACCEPTED (320 steps)
  CLR : ACCEPTED (320 steps)

[6] Invalid program:  int x = ;  (missing initializer)
  LALR: REJECTED (18 steps)
    line/col     : 2 / 13
    unexpected   : ';' (token ;)
    expected     : !, (, -, floatconst, id, intconst

DEMO RESULT: OK
```

The demo exits with status 0 only if all three parsers accept the valid
program and reject the invalid one.

## Tests

`make test` runs `tests/run_tests.sh`: every `tests/cases/*.src` file is
parsed by `parserx` (optionally with a different grammar via a `.args`
file), and each line of the matching `.expect` file must appear in the
output (`text` = must contain, `!text` = must not contain,
`re:REGEX` = some line matches). Current suite: **62 cases, all passing**
on Windows (MinGW g++ 16.1, via `bash tests/run_tests.sh`):

* 01-22: valid programs — declarations, precedence trees, parens,
  relational/logical expressions, unary operators, if/else, dangling
  else, while, for (full, empty clauses, declaring init), nested
  blocks, functions (int/float/no params), calls in expressions and as
  statements, break/continue, the full sample program, empty blocks,
  empty statements, comments, space-free input, floats, deep nesting,
  many statements, mixed functions and statements.
* 30-52: invalid programs — missing semicolons, unmatched parens and
  braces, incomplete expressions, invalid operator sequences, malformed
  function headers, missing condition parens, `return;`, chained
  assignment, `else` without `if`, keywords as identifiers, empty
  input, unterminated comments, lexical errors (`@`, `12abc`, `1.`,
  single `&`), error position on a later line.
* 60-63: table summary and conflict counts, FIRST/FOLLOW sets, token
  table, expected-token reporting.
* 23/24: SLR-vs-LALR and LALR-vs-CLR demonstration grammars.
* 27a-27d: epsilon grammar, FIRST/FOLLOW, grammar-loader errors.

Example result lines (from `tests/output/`, regenerated by the suite):

```
TABLE SLR states=130 action=1112 goto=269 sr=1 rr=0
TABLE LALR states=130 action=1110 goto=269 sr=1 rr=0
TABLE CLR states=359 action=3152 goto=778 sr=2 rr=0
RESULT SLR REJECTED token=; line=1 col=9 step=8 reduces=0 expected=! ( - floatconst id intconst
```

## Known limitations

* Syntax analysis only: no symbol table, no type checking, no runtime
  execution; `break`/`continue` are accepted anywhere syntactically
  (no loop-context check).
* Assignment is a statement, not an expression, so `a = b = c;` and
  assignments inside conditions are rejected by design.
* `return` requires an expression (`return;` is invalid).
* Errors are reported at the first failure; no error recovery.
* The dangling-else conflict is resolved by shift; this is a deliberate
  documented policy, not a conflict-free grammar.
* No `++`/`--`, arrays, or types other than `int`/`float`.

## Future work

* Error recovery (panic-mode with synchronizing tokens).
* LALR construction by lookahead propagation (DeRemer/Pennello or
  burst mode) instead of CLR-then-merge.
* Semantic analysis: symbol table, type checking, loop-context checks
  for `break`/`continue`.
* Pretty-printed parse trees instead of the compact bracket notation.