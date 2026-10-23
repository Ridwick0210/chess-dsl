# ChessScript: compiler inspection and diagnostics plan

Updated: 23 October 2026.

## Goal and scope

Make the existing compiler pipeline easier to inspect and explain while retaining the documented ChessScript language: zero or more action-only openings, exactly one game, standard initial position, and the existing move/capture/castle/promote/use statements.

This release implements five improvements: synchronized source/IR/board stepping, contextual semantic diagnostics, multiple syntax diagnostics, opening expansion inspection, and compiler warnings with spelling hints. It retains the C++17 compiler, independent IR replay, local Python server, and dependency-free browser interface.

The repository's `PROJECT_PLAN.md`, README, and the supplied laboratory instruction manual are the requirements baseline. The manual lists Flex/Bison as recommended tools, not mandatory ones. The handwritten parser remains in place. The separate original project proposal is not in this checkout, so any commitments specific to it should still be checked before submission.

## Requirements mapping and current status

| Requirement/objective | Delivered behavior | Evidence |
| --- | --- | --- |
| Make compiler stages visible | Selecting a move highlights its source line and exact IR range; selecting an IR instruction selects its move | Studio, `MoveInfo.irBegin/irEnd` |
| Explain state-dependent semantics | Failed actions retain the validated prefix, board before failure, and relevant error squares | JSON `preview`, semantic regression cases |
| Provide useful parser diagnostics | Recoverable statement errors are collected in one compile; invalid syntax never proceeds to semantics or VM execution | Parser recovery tests |
| Demonstrate symbol resolution | Separate expansion records identify the definition, use site, and resulting moves, including repeated uses | Expansions tab and repeated-use tests |
| Improve compiler feedback | Unused opening warnings are non-fatal; undefined names can receive an unambiguous spelling hint | CLI and JSON tests |
| Retain the language boundary | Nested `use`, multiple games, and malformed programs remain rejected | Grammar regression tests |

## Step 1 — Establish the baseline and requirement boundary

Status: implementation baseline and laboratory manual reviewed; original proposal comparison remains open.

1. Read the current grammar, scope, compiler stages, and test commands.
2. Trace source locations through lexer, parser, semantic analysis, lowering, JSON, and Studio.
3. Keep the existing chess-rule tests and sample-game behavior as regression gates.
4. Record the tooling choice: the proposal mentions Flex/Bison, the manual recommends them, and the implementation uses a handwritten lexer and recursive descent parser.
5. Before academic submission, compare the original proposal against this plan for any additional commitments.

Acceptance: features fit the laboratory manual without changing accepted language syntax; proposal-specific commitments remain to be checked.

## Step 2 — Add provenance and a structured diagnostic contract

Status: implemented.

1. Extend each validated move with a half-open IR range: `irBegin` is inclusive and `irEnd` is exclusive.
2. Preserve its original statement line and column.
3. For expanded opening actions, also preserve the opening name and the `use` line and column.
4. Record each opening expansion independently, including its definition, use site, first move index, and validated move count. Record empty expansions too.
5. Add diagnostic fields for related use locations, relevant board squares, additional syntax errors, and an optional validated-prefix compilation.
6. Serialize state metadata: side to move, castling rights, en passant target, halfmove clock, and fullmove number.
7. Preserve the existing top-level `ok` and `error` JSON fields; add `errors` and optional `preview` on failure, and `warnings`/`expansions` on results.

Files: `include/chessscript.hpp`, `src/compiler.cpp`, `src/json.cpp`, `src/main.cpp`.

Acceptance: repeated uses of the same opening have distinct IR ranges and use locations. Every move's range ends at its own `END_MOVE`, and ranges cover the generated IR in order.

## Step 3 — Implement parser recovery

Status: implemented for statement-level syntax errors.

1. Collect syntax diagnostics while parsing opening and game bodies.
2. On an invalid statement, synchronize at a semicolon, closing brace, end of input, a top-level declaration, or a new statement keyword.
3. Retain the next statement keyword when the previous statement is missing its semicolon.
4. Report an unclosed block without consuming the following top-level declaration.
5. Bound recovery to 50 collected body errors; a final structural error can produce a 51st diagnostic.
6. Throw the collected diagnostics before semantic analysis when any syntax error exists.
7. Print all collected errors in the CLI and provide clickable entries in the Studio.

Files: `src/parser.cpp`, `src/main.cpp`, `src/json.cpp`, `web/app.js`.

Acceptance: two malformed statements produce two diagnostics in one invocation, the process exits with failure, and no preview/execution trace is returned for syntax errors. Missing braces and noisy input terminate promptly.

Limit: lexical failures and unrecoverable declaration/header errors still stop further collection. Recovery does not invent valid statements or attempt semantic execution after a syntax error.

## Step 4 — Explain semantic failures in context

Status: implemented.

1. Validate each action before appending its IR.
2. On failure, retain only earlier validated actions and their IR.
3. Replay that prefix to reconstruct the board immediately before the failing statement.
4. Attach attempted source/destination squares to the diagnostic.
5. Identify blocked paths, blocked pawn double advances, king safety violations, and castling obstructions/check squares by coordinate.
6. Replace generic movement-shape errors with piece-specific explanations.
7. Carry definition and use locations when an expanded action fails.
8. Display the prefix as a clearly labeled diagnostic preview, with red error-square outlines; show valid board changes separately in gold.
9. Keep the compilation status failed. Never lower or execute the rejected action or later statements.

Files: `src/chess.cpp`, `src/compiler.cpp`, `src/json.cpp`, `src/main.cpp`, `web/app.js`, `web/styles.css`.

Acceptance: a failed second move leaves the first move on the board; a blocked bishop identifies its blocker; a move exposing its king identifies that king's square. CLI exit status remains nonzero.

Contract note: the nested `preview` uses the ordinary trace JSON shape, including `ok: true` for that valid prefix. The outer `ok: false` is the authoritative status of the submitted program. The Studio always labels this nested data as a preview.

## Step 5 — Connect source, IR, board, and expansion inspection

Status: implemented in the local Studio.

1. Selecting a move updates the board, source gutter highlight, and active IR range.
2. Selecting an IR row selects its owning move.
3. Highlight every changed board square, including the rook in castling and the captured pawn square in en passant.
4. Show before/after values for changed turn, castling rights, en passant target, and clocks, plus a textual square diff.
5. Provide a source-link button to select the source statement line.
6. Add an Expansions tab with definition/use links and selectable expanded moves.
7. Clicking a compiled `use` statement in the editor selects its expansion. Source columns distinguish statements sharing a line; repeated definition occurrences retain the current occurrence when possible.
8. Clear source-linked results immediately on edits, including Tab indentation, to prevent stale mappings.
9. Ignore an in-flight compiler response when it no longer matches the editor contents.
10. Scroll the IR panel internally without moving the entire page during stepping.

Files: `web/index.html`, `web/app.js`, `web/styles.css`.

Acceptance: move selection, IR selection, and expansion selection agree on the current board position. A source edit immediately invalidates old results. Buttons remain keyboard accessible, and long labels/pipeline content remain contained.

Limit: navigation advances whole moves, not individual VM instructions. The standalone HTML report retains its existing replay interface; these new interactive inspection controls are in the Studio.

## Step 6 — Add warnings and spelling hints

Status: implemented.

1. Collect all opening references from the game.
2. Warn at the definition of each unused opening.
3. Display warnings separately from errors and retain successful execution/exit status.
4. For an undefined name, suggest a unique nearest opening within two insertions, deletions, or substitutions.
5. Suppress a hint when equally close candidates exist.
6. Use a bounded edit-distance calculation so very long identifiers do not cause quadratic comparison work.
7. Keep misspellings as semantic errors; a suggestion never silently rewrites the program.

Files: `src/compiler.cpp`, `src/main.cpp`, `src/json.cpp`, `web/app.js`.

Acceptance: an unused opening warns without blocking a valid game; `Strat` suggests `Start`; an ambiguous spelling produces no arbitrary suggestion.

## Step 7 — Validate and prepare a demonstration

Status: verified with `make test` and `node --check web/app.js`. Browser checks confirmed move/IR stepping, expansion links, warnings, multiple syntax diagnostics, semantic previews, and invalidation on edits. The expanded suite includes full-game castling, en passant, promotion, an empty program, near-limit and long programs, IR/VM trace agreement, invalid input handling, and portable HTML generation. CMake/CTest verification could not run because CMake is not installed in this environment; the configuration includes the new tests but remains unverified here. Run the commands below for reproducible verification.

1. Run `make test`: compiler smoke checks, chess-rule checks, 12 compiler inspection tests, and 8 end-to-end tests.
2. Run `node --check web/app.js` when Node is available. Node is a development check only, not a Studio runtime dependency.
3. Verify the alternative build with CMake/CTest in a separate build directory.
4. Open the Studio and exercise move/IR selection, opening expansion links, warnings, syntax diagnostics, and semantic previews.
5. Confirm that editing the source clears stale board/pipeline results.
6. Keep the original sample and portable HTML output working.
7. Use the following demo sequence for the final review.

```sh
make test
node --check web/app.js
cmake -S . -B /tmp/chessscript-cmake
cmake --build /tmp/chessscript-cmake
ctest --test-dir /tmp/chessscript-cmake --output-on-failure
make serve
```

### Review demonstration, approximately 7–10 minutes

1. Load Scholar's Mate and confirm the final checkmate result.
2. Select the first move and open Chess IR. Explain its two square updates, state, clock, and move boundary.
3. Step to the second move. Point out the turn and en passant changes.
4. Paste `examples/inspection_demo.chs` into the editor and compile. Explain why the unused opening is a warning.
5. Open Expansions, select an expanded move, and navigate to its use and definition.
6. Change `use KingsPawn;` to `use KingsPaw;`. Show the undefined-name error and suggested spelling.
7. Restore the name and change the first opening action to `move e2 -> e5;`. Show the failed definition location, use location, board preview, and pawn rule explanation.
8. Restore the valid program, remove the arrows from two statements, and compile once. Show both syntax errors and the absence of an execution trace.
9. Explain why an invalid action prevents later semantic validation: subsequent board state cannot be trusted.
10. Show automated test results and state the remaining limitations accurately.

## Remaining submission work

1. Compare the original project proposal with the finished scope for any unfulfilled proposal-specific commitments. The laboratory manual does not mandate Flex/Bison.
2. Capture final screenshots appropriate for the submission format: valid trace, expansion inspector, multiple syntax errors, and semantic preview.
3. Assemble the formal report with grammar, architecture, compiler stages, test evidence, limitations, and references.
4. Prepare the presentation, rehearse the demonstration, and retain a working offline build plus the standalone HTML viewer.

## Optional later extensions — outside this release

These require an explicit update to the current scope; they are not implemented by this plan's completed steps.

### Nested opening references

1. Permit `use` inside opening bodies in the grammar.
2. Build a dependency graph and resolve references.
3. Detect cycles with an expansion stack and report the complete cycle with locations.
4. Preserve the full chain of definition/use locations for each expanded action.
5. Limit expansion size and validate moves in the caller's actual board state.
6. Test direct/indirect cycles, missing names, repeated references, and context-dependent illegality.

### FEN starting positions

1. Define an explicit initial-position language/API construct.
2. Parse and validate all FEN state fields, not only piece placement.
3. Make the same initial state available to semantic analysis and the VM.
4. Update replay, JSON, report generation, and examples.
5. Test malformed positions, castling rights, turn order, en passant state, and clocks.

Draw adjudication, PGN import, control flow, and IR optimization remain separate proposals. Do not add an optimizer without identifying a useful transformation and proving that each observable replay position is preserved. AI opponents, multiplayer, accounts, and move-quality scoring are outside the compiler-focused roadmap.
