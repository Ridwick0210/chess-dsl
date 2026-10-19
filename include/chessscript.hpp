#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace cs {
struct Location { int line = 1, column = 1; };
struct Diagnostic : std::runtime_error {
    std::string category;
    std::string detail;
    Location where;
    Diagnostic(std::string category, Location where, const std::string& message);
};

enum class TokenKind { Game, Opening, Use, Move, Capture, Castle, Promote, As, KingSide, QueenSide,
                       Arrow, LBrace, RBrace, Semicolon, Square, Identifier, End };
struct Token { TokenKind kind; std::string text; Location where; };
std::vector<Token> lex(const std::string& source);

struct Move { std::string from, to; Location where; };
struct Capture { std::string from, to; Location where; };
struct Castle { bool kingSide; Location where; };
struct Promote { std::string from, to, piece; Location where; };
struct Use { std::string name; Location where; };
using Statement = std::variant<Move, Capture, Castle, Promote, Use>;
struct Game { std::string name; Location where; std::vector<Statement> statements; };
struct Opening { std::string name; Location where; std::vector<Statement> statements; };
struct Program { std::vector<Opening> openings; Game game; };
Program parse(const std::vector<Token>& tokens);

enum class Piece { Empty, WP, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK };
struct Board {
    std::array<Piece, 64> cells{};
    bool whiteToMove = true;
    bool whiteKingCastle = true, whiteQueenCastle = true;
    bool blackKingCastle = true, blackQueenCastle = true;
    std::optional<int> enPassant;
    int halfmove = 0, fullmove = 1;
    Board();
    std::string display() const;
};

struct Instruction { std::string opcode, a, b, c; Location where; };
struct MoveInfo { std::string label, from, to; Location where; };
struct Compilation { Program ast; std::map<std::string, Opening> symbols; std::vector<Instruction> ir; std::vector<MoveInfo> moves; };
Compilation compile(const std::string& source);
Board execute(const std::vector<Instruction>& ir, bool verbose = false);
std::vector<Board> executeTrace(const std::vector<Instruction>& ir);
std::string renderHtml(const Compilation& compilation, const std::string& source, const std::vector<Board>& states);
std::string renderJson(const Compilation& compilation, const std::string& source, const std::vector<Board>& states);
std::string renderErrorJson(const Diagnostic& error);
void applyStatement(Board& board, const Statement& statement);
std::string positionStatus(const Board& board);
std::string tokenName(TokenKind kind);
std::string pieceName(Piece piece);
}
