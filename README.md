# ChessScript

ChessScript compiles a small chess language, checks each move against the current board, lowers valid moves to Chess IR, and replays that IR in a chess virtual machine. The local Studio lets you edit a game, run it, inspect compiler stages, and step through a visual board.

## Run the Studio

Requirements: a C++17 compiler, Make, and Python 3. The browser interface uses only local files and Python's standard library.

```sh
make
make serve
```

Open **http://127.0.0.1:8765** in a browser. The sample game loads automatically. Edit the source and click **Compile & run**, or press **Ctrl+Enter**. Compiler errors identify their category, line, and column and select the source location. The board shows chess pieces, the move sequence, and check, checkmate, or stalemate status. Use the move list, slider, or Previous/Next buttons to review each position.

Stop the server with **Ctrl+C**. It binds to `127.0.0.1` and does not require an internet connection. The Studio accepts UTF-8 source up to 64 KiB and reports an input error for invalid encoding or oversized submissions.

## Inspecting compilation and errors

- Selecting a move highlights its source line, changed board squares, and exact instruction range in **Chess IR**. Click an IR row to select its move. State details show turn, castling rights, en passant, and clock changes.
- **Expansions** shows each opening use, its definition, and its validated actions. Definition/use links select source locations; clicking a compiled `use` statement in the editor opens its expansion. Repeated uses remain separate.
- Recoverable statement syntax errors are reported together. Click any diagnostic to select its source line. Lexical errors and unrecoverable header errors still stop collection; invalid syntax never runs.
- An illegal move displays the board immediately before failure as a **diagnostic preview** of the valid prefix. Red outlines identify attempted/relevant squares; error text explains blocked paths, piece movement, or king safety. Errors within an opening include its use location. Compilation still fails and later actions are not executed.
- Unused openings produce non-fatal warnings. Undefined opening names may include an unambiguous spelling suggestion, but remain errors.
- Editing the source clears old results so source links never point into a changed program. Recompile to inspect the updated source.

Paste `examples/inspection_demo.chs` into the Studio for a compact demonstration of expansions and warnings. The standalone HTML viewer keeps its existing replay controls; these new inspection controls are in the Studio.

See [the detailed implementation plan](docs/IMPLEMENTATION_PLAN.md) for implementation steps, acceptance criteria, verification commands, a review demonstration, and remaining submission work.

## Command line

```sh
./build/chessscript examples/scholar_mate.chs
./build/chessscript --tokens --ast --ir examples/opening_demo.chs
./build/chessscript --html build/game.html examples/scholar_mate.chs
make test
```

Open `build/game.html` in a browser for a portable, standalone game viewer. `--no-run` suppresses board display but still performs semantic validation. `--json -` reads source from standard input and returns structured compiler output for the Studio. `make test` runs smoke checks, direct chess-rule tests, compiler inspection checks, and end-to-end games covering castling, en passant, promotion, long programs, and the portable viewer.

## Language

```chessscript
opening KingsPawn {
    move e2 -> e4;
    move e7 -> e5;
}

game Example {
    use KingsPawn;
    move g1 -> f3;
    move b8 -> c6;
}
```

```ebnf
program          = { opening }, game ;
opening          = "opening", identifier, "{", { action }, "}" ;
game             = "game", identifier, "{", { action | use }, "}" ;
use              = "use", identifier, ";" ;
action           = move | capture | castle | promotion ;
move             = "move", square, "->", square, ";" ;
capture          = "capture", square, "->", square, ";" ;
castle           = "castle", ("king_side" | "queen_side"), ";" ;
promotion        = "promote", square, "->", square, "as", piece, ";" ;
square           = file, rank ;
piece            = "queen" | "rook" | "bishop" | "knight" ;
```

Squares use file `a`–`h` followed by rank `1`–`8`. `capture` is required when a piece is captured. `promote` supports both quiet moves and captures. Opening names are declared before the game and resolved through the symbol table; an undefined or duplicate name is a semantic error. Opening bodies contain actions, while `use` appears in the game block.

## Compiler pipeline

```text
source → lexer → parser/AST → symbol resolution → board-state semantic analysis
       → Chess IR → virtual machine → position trace and game status
```

The semantic analyzer validates turn order, piece movement, blocked paths, occupancy, king safety, en passant, castling rights and attacked transit squares, and promotion. It rejects commands after checkmate or stalemate. Validated moves are lowered into board operations (`SET`, `STATE`, `CLOCK`, `END_MOVE`). The VM replays those operations and records a board after every move. `--ir` and the Studio expose the intermediate operations for inspection.

The implementation is intentionally a handwritten lexer and recursive descent parser in C++17. The proposal mentioned Flex and Bison; the handwritten approach keeps token locations, AST construction, and grammar decisions visible in this individual project. The language has one game per file and always starts from the standard initial position. Draw rules such as threefold repetition and the fifty move rule, PGN/FEN import, nested opening references, optimization, and chess AI are future work.

## Project files

- `include/chessscript.hpp`: shared AST, board, IR, and compiler interfaces.
- `src/lexer.cpp` and `src/parser.cpp`: tokenization, grammar checks, and AST.
- `src/compiler.cpp`: symbol resolution, semantic pass, and IR lowering.
- `src/chess.cpp`: board rules, VM execution, and game status.
- `src/json.cpp` and `src/report.cpp`: structured Studio output and standalone HTML viewer.
- `web/`: local Studio interface and HTTP server.
- `examples/`: working ChessScript programs.
- `tests/`: compiler and chess-rule checks.
- `docs/PROJECT_PLAN.md`: project objectives, architecture, progress, and limitations.
- `docs/IMPLEMENTATION_PLAN.md`: detailed feature implementation, verification, and submission plan.
- `tests/compiler_inspection.py`: diagnostic recovery, valid-prefix previews, source mapping, expansion, and warning regressions.

The Makefile is the primary build route. CMake 3.16+ is also supported with `cmake -S . -B build && cmake --build build`.
