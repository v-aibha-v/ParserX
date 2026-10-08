#!/usr/bin/env bash
# Runs every test case in tests/cases on the SLR, LALR and CLR parsers,
# saves the full output to tests/output/<case>.out and checks it.
#
# Each case has:
#   <case>.src     the source program given to the parser
#   <case>.args    (optional) extra options, e.g. --grammar FILE
#   <case>.expect  one check per line:
#                    text      the output must contain this text
#                    !text     the output must NOT contain this text
#                    re:REGEX  some output line must match this regex
#                    # ...     comment (the expected outcome from the document)
#
# Usage (from the project root):  bash tests/run_tests.sh [path-to-parserx]

cd "$(dirname "$0")/.." || exit 1
BIN="${1:-./parserx}"
[ -x "$BIN" ] || BIN="$BIN.exe"
if [ ! -x "$BIN" ]; then
    echo "parserx executable not found; build it first (make, or see README)" >&2
    exit 1
fi

mkdir -p tests/output
pass=0
fail=0
failed=()
for src in tests/cases/*.src; do
    name=$(basename "$src" .src)
    args=""
    [ -f "tests/cases/$name.args" ] && args=$(cat "tests/cases/$name.args")
    out="tests/output/$name.out"
    # shellcheck disable=SC2086
    "$BIN" $args --input "$src" > "$out" 2>&1
    ok=1
    while IFS= read -r check || [ -n "$check" ]; do
        check="${check%$'\r'}"
        [ -z "$check" ] && continue
        case "$check" in
            "#"*) continue ;;
            "!"*)
                if grep -qF -- "${check#!}" "$out"; then
                    echo "  [$name] unexpected: ${check#!}"
                    ok=0
                fi ;;
            "re:"*)
                if ! grep -qE -- "${check#re:}" "$out"; then
                    echo "  [$name] no line matches: ${check#re:}"
                    ok=0
                fi ;;
            *)
                if ! grep -qF -- "$check" "$out"; then
                    echo "  [$name] missing: $check"
                    ok=0
                fi ;;
        esac
    done < "tests/cases/$name.expect"
    if [ $ok -eq 1 ]; then
        echo "PASS  $name"
        pass=$((pass + 1))
    else
        echo "FAIL  $name   (see $out)"
        fail=$((fail + 1))
        failed+=("$name")
    fi
done

echo
echo "Passed $pass of $((pass + fail)) test cases. Outputs saved in tests/output/"
[ $fail -eq 0 ] || { echo "Failed: ${failed[*]}"; exit 1; }
