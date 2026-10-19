# ChessScript

ChessScript compiles a small chess language, checks each move against the current board, lowers valid moves to Chess IR, and replays that IR in a chess virtual machine. The local Studio lets you edit a game, run it, inspect compiler stages, and step through a visual board.

## Run the Studio

Requirements: a C++17 compiler, Make, and Python 3. The browser interface uses only local files and Python's standard library.

```sh
make
make serve
```

Open **http://127.0.0.1:8765** in a browser. The sample game loads automatically. Edit the source and click **Compile & run**, or press **Ctrl+Enter**. Compiler errors identify their category, line, and column and select the source location. The board shows chess pieces, the move sequence, and check, checkmate, or stalemate status. Use the move list, slider, or Previous/Next buttons to review each position.

Stop the server with **Ctrl+C**. It binds to `127.0.0.1` and does not require an internet connection.

## Command line

```sh
./build/chessscript examples/scholar_mate.chs
./build/chessscript --tokens --ast --ir examples/opening_demo.chs
./build/chessscript --html build/game.html examples/scholar_mate.chs
make test
```

Open `build/game.html` in a browser for a portable, standalone game viewer. `--no-run` suppresses board display but still performs semantic validation. `--json -` reads source from standard input and returns structured compiler output for the Studio.

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

The Makefile is the primary build route. CMake 3.16+ is also supported with `cmake -S . -B build && cmake --build build`.
