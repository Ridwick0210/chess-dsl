#include "chessscript.hpp"
#include <type_traits>
#include <algorithm>
#include <set>

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
            return {prefix + "Move " + node.from + " → " + node.to, node.from, node.to, node.where, "", std::nullopt, 0, 0};
        else if constexpr (std::is_same_v<T, Capture>)
            return {prefix + "Capture " + node.from + " → " + node.to, node.from, node.to, node.where, "", std::nullopt, 0, 0};
        else if constexpr (std::is_same_v<T, Promote>)
            return {prefix + "Promote " + node.from + " → " + node.to + " to " + node.piece,
                    node.from, node.to, node.where, "", std::nullopt, 0, 0};
        else if constexpr (std::is_same_v<T, Castle>) {
            const std::string rank = before.whiteToMove ? "1" : "8";
            return {prefix + (node.kingSide ? "Castle kingside" : "Castle queenside"),
                    "e" + rank, (node.kingSide ? "g" : "c") + rank, node.where, "", std::nullopt, 0, 0};
        } else return {};
    }, statement);
}

void lowerStep(Compilation& compilation, Board& board, const Statement& statement,
               const std::string& prefix, const Use* use = nullptr) {
    const Board before = board;
    MoveInfo move = describe(statement, before, prefix);
    move.irBegin = compilation.ir.size();
    if (use) { move.opening = use->name; move.useWhere = use->where; }
    try {
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
        move.irEnd = compilation.ir.size();
        compilation.moves.push_back(move);
    } catch (Diagnostic& error) {
        if (use) { error.opening = use->name; error.useWhere = use->where; }
        error.squares.push_back(move.from);
        error.squares.push_back(move.to);
        throw;
    }
}

// Only distances up to two are useful for a spelling hint. A narrow band
// keeps very long names from making diagnostics quadratic in source size.
std::size_t editDistance(const std::string& a, const std::string& b) {
    if (a.size() > b.size() + 2 || b.size() > a.size() + 2) return 3;
    std::vector<std::size_t> previous(b.size() + 1, 3), current(b.size() + 1, 3);
    for (std::size_t j = 0; j <= std::min<std::size_t>(2, b.size()); ++j) previous[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        current[0] = std::min<std::size_t>(i, 3);
        const auto first = i > 2 ? i - 2 : 1;
        const auto last = std::min(b.size(), i + 2);
        if (first > 1) current[first - 1] = 3;
        std::size_t minimum = current[0];
        for (std::size_t j = first; j <= last; ++j) {
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1,
                                  previous[j - 1] + (a[i - 1] != b[j - 1])});
            minimum = std::min(minimum, current[j]);
        }
        if (last < b.size()) current[last + 1] = 3;
        if (minimum > 2) return 3;
        previous.swap(current);
    }
    return previous.back();
}

std::string suggestion(const std::string& name, const std::map<std::string, Opening>& symbols) {
    std::size_t best = 3;
    std::string candidate;
    bool tied = false;
    for (const auto& entry : symbols) {
        const auto distance = editDistance(name, entry.first);
        if (distance < best) { best = distance; candidate = entry.first; tied = false; }
        else if (distance == best) tied = true;
    }
    return !candidate.empty() && !tied ? "; did you mean '" + candidate + "'?" : "";
}
}

Compilation compile(const std::string& source) {
    Compilation compilation;
    compilation.ast = parse(lex(source));
    for (const Opening& opening : compilation.ast.openings) {
        if (opening.name == compilation.ast.game.name ||
            !compilation.symbols.emplace(opening.name, opening).second)
            throw Diagnostic("SEMANTIC", opening.where, "duplicate definition '" + opening.name + "'");
    }

    std::set<std::string> used;
    for (const auto& statement : compilation.ast.game.statements)
        if (const auto* use = std::get_if<Use>(&statement)) used.insert(use->name);
    for (const auto& opening : compilation.ast.openings)
        if (!used.count(opening.name))
            compilation.warnings.emplace_back("WARNING", opening.where,
                "opening '" + opening.name + "' is never used");

    Board board;
    try {
        for (const Statement& statement : compilation.ast.game.statements) {
            if (const auto* use = std::get_if<Use>(&statement)) {
                auto found = compilation.symbols.find(use->name);
                if (found == compilation.symbols.end())
                    throw Diagnostic("SEMANTIC", use->where, "undefined opening '" + use->name + "'"
                        + suggestion(use->name, compilation.symbols));
                compilation.expansions.push_back({use->name, found->second.where, use->where,
                                                  compilation.moves.size(), 0});
                for (const Statement& expanded : found->second.statements) {
                    lowerStep(compilation, board, expanded, use->name + " · ", use);
                    ++compilation.expansions.back().moveCount;
                }
            } else {
                lowerStep(compilation, board, statement, "");
            }
        }
    } catch (Diagnostic& error) {
        error.partial = std::make_shared<Compilation>(compilation);
        throw;
    }
    return compilation;
}
}
