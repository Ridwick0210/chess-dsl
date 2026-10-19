#!/bin/sh
set -eu
bin=${1:-./build/chessscript}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$bin" --ir examples/scholar_mate.chs >"$tmp/mate.out"
grep -q 'Checkmate' "$tmp/mate.out"
grep -q 'SET e2 0' "$tmp/mate.out"
grep -q 'END_MOVE' "$tmp/mate.out"

"$bin" examples/opening_demo.chs >"$tmp/opening.out"
grep -q '4 moves' "$tmp/opening.out"

printf 'game Bad { move e2 -> e5; }\n' >"$tmp/semantic.chs"
if "$bin" --no-run "$tmp/semantic.chs" >"$tmp/semantic.out" 2>&1; then exit 1; fi
grep -q 'SEMANTIC error' "$tmp/semantic.out"

printf 'game Bad { move e2 e4; }\n' >"$tmp/syntax.chs"
if "$bin" "$tmp/syntax.chs" >"$tmp/syntax.out" 2>&1; then exit 1; fi
grep -q 'SYNTAX error' "$tmp/syntax.out"

printf 'game Bad { move 2e -> e4; }\n' >"$tmp/lexical.chs"
if "$bin" "$tmp/lexical.chs" >"$tmp/lexical.out" 2>&1; then exit 1; fi
grep -q 'LEXICAL error' "$tmp/lexical.out"

printf 'game Bad { use Missing; }\n' >"$tmp/undefined.chs"
if "$bin" "$tmp/undefined.chs" >"$tmp/undefined.out" 2>&1; then exit 1; fi
grep -q "undefined opening 'Missing'" "$tmp/undefined.out"

printf 'opening A {} opening A {} game B {}\n' >"$tmp/duplicate.chs"
if "$bin" "$tmp/duplicate.chs" >"$tmp/duplicate.out" 2>&1; then exit 1; fi
grep -q "duplicate definition 'A'" "$tmp/duplicate.out"

"$bin" --json - <examples/scholar_mate.chs >"$tmp/result.json"
python3 -c 'import json,sys; result=json.load(open(sys.argv[1])); assert result["ok"] and result["states"][-1]["status"].startswith("Checkmate") and len(result["states"])==8' "$tmp/result.json"

printf 'Compiler cases passed: mate, IR, symbols, validation, diagnostics, and JSON.\n'
