#include "chessscript.hpp"
#include <iomanip>
#include <sstream>

namespace cs {
namespace {
std::string quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u" << std::hex << std::setw(4)
                                  << std::setfill('0') << static_cast<int>(c) << std::dec;
                else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}
std::string locationJson(Location where) {
    return "{\"line\":" + std::to_string(where.line) +
        ",\"column\":" + std::to_string(where.column) + "}";
}
std::string diagnosticJson(const Diagnostic& error) {
    std::ostringstream out;
    out << "{\"category\":" << quote(error.category)
        << ",\"line\":" << error.where.line << ",\"column\":" << error.where.column
        << ",\"message\":" << quote(error.detail)
        << ",\"opening\":" << quote(error.opening)
        << ",\"use\":" << (error.useWhere ? locationJson(*error.useWhere) : "null")
        << ",\"squares\":[";
    for (std::size_t i = 0; i < error.squares.size(); ++i) {
        if (i) out << ',';
        out << quote(error.squares[i]);
    }
    out << "]}";
    return out.str();
}
std::string castlingRights(const Board& board) {
    std::string rights;
    if (board.whiteKingCastle) rights += 'K';
    if (board.whiteQueenCastle) rights += 'Q';
    if (board.blackKingCastle) rights += 'k';
    if (board.blackQueenCastle) rights += 'q';
    return rights.empty() ? "-" : rights;
}
std::string squareName(int index) {
    return std::string(1, static_cast<char>('a' + index % 8))
        + static_cast<char>('1' + index / 8);
}
std::string astStatement(const Statement& statement) {
    if (const auto* move = std::get_if<Move>(&statement))
        return "Move(" + move->from + ", " + move->to + ")";
    if (const auto* capture = std::get_if<Capture>(&statement))
        return "Capture(" + capture->from + ", " + capture->to + ")";
    if (const auto* castle = std::get_if<Castle>(&statement))
        return std::string("Castle(") + (castle->kingSide ? "king_side" : "queen_side") + ")";
    if (const auto* promote = std::get_if<Promote>(&statement))
        return "Promote(" + promote->from + ", " + promote->to + ", " + promote->piece + ")";
    return "Use(" + std::get<Use>(statement).name + ")";
}
}

std::string renderJson(const Compilation& compilation, const std::string& source,
                       const std::vector<Board>& states) {
    std::ostringstream out;
    out << "{\"ok\":true,\"name\":" << quote(compilation.ast.game.name);
    out << ",\"symbols\":[";
    bool first = true;
    for (const auto& entry : compilation.symbols) {
        if (!first) out << ',';
        out << quote(entry.first);
        first = false;
    }
    out << "],\"ast\":[";
    bool firstNode = true;
    for (const auto& opening : compilation.ast.openings) {
        if (!firstNode) out << ',';
        out << quote("Opening(" + opening.name + ")");
        firstNode = false;
        for (const auto& statement : opening.statements)
            out << ',' << quote("  " + astStatement(statement));
    }
    if (!firstNode) out << ',';
    out << quote("Game(" + compilation.ast.game.name + ")");
    for (const auto& statement : compilation.ast.game.statements)
        out << ',' << quote("  " + astStatement(statement));
    out << "],\"tokens\":[";
    const auto tokens = lex(source);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i) out << ',';
        out << "{\"kind\":" << quote(tokenName(tokens[i].kind))
            << ",\"text\":" << quote(tokens[i].text)
            << ",\"line\":" << tokens[i].where.line
            << ",\"column\":" << tokens[i].where.column << '}';
    }
    out << "],\"moves\":[";
    for (std::size_t i = 0; i < compilation.moves.size(); ++i) {
        if (i) out << ',';
        const auto& move = compilation.moves[i];
        out << "{\"label\":" << quote(move.label)
            << ",\"from\":" << quote(move.from)
            << ",\"to\":" << quote(move.to)
            << ",\"line\":" << move.where.line
            << ",\"column\":" << move.where.column
            << ",\"opening\":" << quote(move.opening)
            << ",\"use\":" << (move.useWhere ? locationJson(*move.useWhere) : "null")
            << ",\"irBegin\":" << move.irBegin << ",\"irEnd\":" << move.irEnd << '}';
    }
    out << "],\"warnings\":[";
    for (std::size_t i = 0; i < compilation.warnings.size(); ++i) {
        if (i) out << ',';
        out << diagnosticJson(compilation.warnings[i]);
    }
    out << "],\"expansions\":[";
    for (std::size_t i = 0; i < compilation.expansions.size(); ++i) {
        if (i) out << ',';
        const auto& expansion = compilation.expansions[i];
        out << "{\"name\":" << quote(expansion.name)
            << ",\"definition\":" << locationJson(expansion.definition)
            << ",\"use\":" << locationJson(expansion.useWhere)
            << ",\"firstMove\":" << expansion.firstMove
            << ",\"moveCount\":" << expansion.moveCount << '}';
    }
    out << "],\"ir\":[";
    for (std::size_t i = 0; i < compilation.ir.size(); ++i) {
        if (i) out << ',';
        const auto& op = compilation.ir[i];
        out << "{\"opcode\":" << quote(op.opcode)
            << ",\"a\":" << quote(op.a)
            << ",\"b\":" << quote(op.b)
            << ",\"c\":" << quote(op.c)
            << ",\"line\":" << op.where.line << '}';
    }
    out << "],\"states\":[";
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (i) out << ',';
        out << "{\"board\":[";
        for (std::size_t square = 0; square < states[i].cells.size(); ++square) {
            if (square) out << ',';
            out << static_cast<int>(states[i].cells[square]);
        }
        out << "],\"turn\":" << quote(states[i].whiteToMove ? "White" : "Black")
            << ",\"status\":" << quote(positionStatus(states[i]))
            << ",\"castling\":" << quote(castlingRights(states[i]))
            << ",\"enPassant\":" << quote(states[i].enPassant ? squareName(*states[i].enPassant) : "-")
            << ",\"halfmove\":" << states[i].halfmove
            << ",\"fullmove\":" << states[i].fullmove << '}';
    }
    out << "]}";
    return out.str();
}

std::string renderErrorJson(const Diagnostic& error, const std::string& source) {
    std::ostringstream out;
    out << "{\"ok\":false,\"error\":" << diagnosticJson(error)
        << ",\"errors\":[" << diagnosticJson(error);
    for (const auto& additional : error.additional) out << ',' << diagnosticJson(additional);
    out << ']';
    if (error.partial) {
        out << ",\"preview\":" << renderJson(*error.partial, source, executeTrace(error.partial->ir));
    }
    out << '}';
    return out.str();
}
}
