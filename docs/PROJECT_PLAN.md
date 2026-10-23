# ChessScript Project Plan and Implementation Progress

The detailed follow-up plan is in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md), updated 23 October 2026. It records the five completed compiler-inspection improvements, acceptance checks, and remaining submission work.

## Abstract and problem

ChessScript is a domain-specific language for describing chess move sequences as executable programs. Chess notation alone does not expose a compiler pipeline or distinguish syntax errors from state-dependent chess errors. ChessScript provides a grammar, AST, symbol table, semantic analysis, Chess IR, and a virtual machine that updates the board. A local Studio makes the stages and board positions visible during demonstration.

## Motivation and objectives

Chess has clear movement and turn rules. The same syntactically valid command can be legal in one board state and illegal in another, which makes the language a useful demonstration of semantic analysis. The project objectives are to define a compact language; tokenize and parse it; resolve named openings; validate chess operations; generate and replay a domain-specific IR; and present source, diagnostics, internal stages, and board results clearly.

## Scope and requirements

One source file contains zero or more named `opening` definitions followed by one `game`. The game may `use` a named opening. Actions are `move`, `capture`, `castle`, and `promote`. The board begins in the standard position. The implementation checks movement, paths, captures, turn order, king safety, en passant, castling, and promotion. It reports check, checkmate, and stalemate. The browser Studio offers an editor, source-located errors, a chessboard, move controls, and token, AST, symbol, and IR views.

The full grammar and user instructions are in the repository README. Opening bodies contain actions only; nested opening references are outside the current grammar. A `promote` action can capture on the final rank. The code reports lexical, syntax, semantic, and server/runtime errors separately.

## Background and design

The lexer maps source characters to tokens with line and column positions. The recursive descent parser checks the grammar and builds typed AST nodes. The symbol table maps opening names to their definitions; `use` resolves and expands these definitions, with duplicate and undefined names rejected. The semantic analyzer simulates each action on a board and checks state-dependent rules before emitting IR. The IR represents validated board updates as `SET`, `STATE`, `CLOCK`, and `END_MOVE` operations. The VM replays IR without parsing source or repeating chess legality checks and returns a position trace.

```text
source
  → tokens
  → AST and opening symbol table
  → resolved, semantically validated actions
  → Chess IR
  → VM position trace
  → CLI, standalone viewer, or local Studio
```

The proposal listed Flex and Bison. The laboratory manual lists these tools as recommendations, not mandatory requirements. This implementation uses a handwritten lexer and recursive descent parser in C++17 so that token locations, grammar choices, AST construction, and errors are explicit and individually explainable. The parser does not use a third-party chess library.

## Technology stack

- C++17 standard library for compiler stages, chess rules, VM, JSON output, and standalone HTML generation.
- Make or CMake for building.
- Python 3 standard library for a local HTTP server that invokes the compiled binary.
- HTML, CSS, and JavaScript for the Studio. No external CDN or runtime package is required.

The Studio binds only to `127.0.0.1`, limits submitted source size, checks the request origin, and runs the compiler with a timeout. Its UI presents error locations in the editor and lets reviewers step through moves while inspecting the pipeline output.

## Phase 1 deliverables

- Title, abstract, problem, motivation, objectives, and scope: this document.
- Background and compiler concepts: the staged design above.
- Language specification: the README grammar and two example programs.
- Architecture and methodology: the pipeline above and documented modules.
- Initial prototype: the CLI running `examples/scholar_mate.chs`.

## Phase 2 implementation progress

| Module | Current result |
| --- | --- |
| Lexer and parser | Tokens, source locations, grammar diagnostics, and typed AST. |
| Symbol table | Named opening definitions, use resolution, duplicate and undefined name errors. |
| Semantic analysis | Validates board operations before IR is emitted; rejects play after the game ends. |
| Chess IR and VM | Lowers validated moves to board operations and replays them into a position trace. |
| Diagnostics | Multiple recoverable syntax errors; contextual semantic previews, relevant squares, definition/use locations, unused-opening warnings, and spelling hints. |
| Presentation | Synchronized source/IR/board stepping, state differences, opening expansion inspection, and clickable diagnostics. |

The sample now performs Scholar's Mate and reports `Checkmate — White wins`. `examples/opening_demo.chs` demonstrates symbol resolution. `make test` checks compiler output and diagnostics plus castling, en passant, promotion by capture, and stalemate in controlled board positions. The compiler inspection suite adds 12 checks for recovery, preview correctness, repeated-use mappings, warnings, and state metadata. The end-to-end suite adds full-game special-move checks, IR/VM trace agreement, input boundaries, and portable HTML generation. The standalone HTML viewer can be generated for a portable demonstration; the local Studio supports editing and rerunning programs.

## Review 2 demonstration

1. Run `make` and `make test`.
2. Run `make serve` and open `http://127.0.0.1:8765`.
3. Step through Scholar's Mate and show its checkmate status.
4. Open the Tokens, AST, Symbols, and Chess IR views; load `examples/opening_demo.chs` to demonstrate `use` resolution.
5. Change a move to `move e2 -> e5;` and show the semantic error at its source line. Remove an arrow to show a syntax error.

## Current limitations and Phase 3 direction

The game always starts from the standard initial position. The language does not yet accept FEN/PGN input, nested opening references, conditional or repeated blocks, or multiple games in one file. Checkmate and stalemate are reported, but repetition, fifty move, insufficient material, and other draw adjudication rules are not implemented. The IR is inspectable but has no optimization pass. Phase 3 can add those features, broader property and performance testing, final screenshots, and a formal report.

## Source documents

- Supplied Compiler Design Laboratory Project Instruction Manual, especially its Phase 1–3 deliverables, review criteria, and academic integrity guidance.
- Supplied ChessScript project proposal, including its proposed language, architecture, chess semantics, IR, and three-phase plan.
