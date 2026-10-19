#include "chessscript.hpp"
#include <type_traits>

namespace cs {
namespace {
std::string squareName(int index) {
    return std::string(1, static_cast<char>('a' + index % 8))
        + static_cast<char>('1' + index / 8);
}

std::string castlingFlags(const Board& board) {
    std::string flags;
    flags += board.whiteKingCastle ? '1' : '0';
    flags += board.whiteQueenCastle ? '1' : '0';
    flags += board.blackKingCastle ? '1' : '0';
    flags += board.blackQueenCastle ? '1' : '0';
    return flags;
}

MoveInfo describe(const Statement& statement, const Board& before, const std::string& prefix) {
    return std::visit([&](const auto& node) -> MoveInfo {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, Move>)
            return {prefix + "Move " + node.from + " → " + node.to, node.from, node.to, node.where};
        else if constexpr (std::is_same_v<T, Capture>)
            return {prefix + "Capture " + node.from + " → " + node.to, node.from, node.to, node.where};
        else if constexpr (std::is_same_v<T, Promote>)
            return {prefix + "Promote " + node.from + " → " + node.to + " to " + node.piece,
                    node.from, node.to, node.where};
        else if constexpr (std::is_same_v<T, Castle>) {
            const std::string rank = before.whiteToMove ? "1" : "8";
            return {prefix + (node.kingSide ? "Castle kingside" : "Castle queenside"),
                    "e" + rank, (node.kingSide ? "g" : "c") + rank, node.where};
        } else return {};
    }, statement);
}

void lowerStep(Compilation& compilation, Board& board, const Statement& statement,
               const std::string& prefix) {
    const Board before = board;
    const MoveInfo move = describe(statement, before, prefix);
    const std::string status = positionStatus(before);
    if (status == "Stalemate" || status.find("Checkmate") == 0)
        throw Diagnostic("SEMANTIC", move.where, "the game has already ended: " + status);

    applyStatement(board, statement);
    for (int square = 0; square < 64; ++square) {
        if (before.cells[square] != board.cells[square])
            compilation.ir.push_back({"SET", squareName(square),
                std::to_string(static_cast<int>(board.cells[square])), "", move.where});
    }
    compilation.ir.push_back({"STATE", board.whiteToMove ? "WHITE" : "BLACK",
        castlingFlags(board), board.enPassant ? squareName(*board.enPassant) : "-", move.where});
    compilation.ir.push_back({"CLOCK", std::to_string(board.halfmove),
        std::to_string(board.fullmove), "", move.where});
    compilation.ir.push_back({"END_MOVE", "", "", "", move.where});
    compilation.moves.push_back(move);
}
}

Compilation compile(const std::string& source) {
    Compilation compilation{parse(lex(source)), {}, {}, {}};
    for (const Opening& opening : compilation.ast.openings) {
        if (opening.name == compilation.ast.game.name ||
            !compilation.symbols.emplace(opening.name, opening).second)
            throw Diagnostic("SEMANTIC", opening.where, "duplicate definition '" + opening.name + "'");
    }

    Board board;
    for (const Statement& statement : compilation.ast.game.statements) {
        if (const auto* use = std::get_if<Use>(&statement)) {
            auto found = compilation.symbols.find(use->name);
            if (found == compilation.symbols.end())
                throw Diagnostic("SEMANTIC", use->where, "undefined opening '" + use->name + "'");
            for (const Statement& expanded : found->second.statements)
                lowerStep(compilation, board, expanded, use->name + " · ");
        } else {
            lowerStep(compilation, board, statement, "");
        }
    }
    return compilation;
}
}
