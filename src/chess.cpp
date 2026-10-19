#include "chessscript.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <type_traits>

namespace cs {
namespace {
int sq(const std::string& s) { return (s[1]-'1')*8+(s[0]-'a'); }
bool white(Piece p) { return p>=Piece::WP && p<=Piece::WK; }
bool black(Piece p) { return p>=Piece::BP && p<=Piece::BK; }
bool same(Piece a, Piece b) { return (white(a)&&white(b))||(black(a)&&black(b)); }
bool isKing(Piece p) { return p==Piece::WK||p==Piece::BK; }
Piece promoted(bool w,const std::string& p) {
    std::string x=p; std::transform(x.begin(),x.end(),x.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(x=="queen")return w?Piece::WQ:Piece::BQ;
    if(x=="rook")return w?Piece::WR:Piece::BR;
    if(x=="bishop")return w?Piece::WB:Piece::BB;
    if(x=="knight")return w?Piece::WN:Piece::BN;
    throw std::invalid_argument("promotion piece must be queen, rook, bishop, or knight");
}
int findKing(const Board& board, bool forWhite) {
    const Piece king = forWhite ? Piece::WK : Piece::BK;
    for (int square = 0; square < 64; ++square)
        if (board.cells[square] == king) return square;
    return -1;
}

bool attacked(const Board& board, int target, bool byWhite) {
    const int targetRank = target / 8, targetFile = target % 8;
    for (int from = 0; from < 64; ++from) {
        const Piece piece = board.cells[from];
        if ((byWhite && !white(piece)) || (!byWhite && !black(piece))) continue;

        const int rank = from / 8, file = from % 8;
        const int rankDelta = targetRank - rank, fileDelta = targetFile - file;
        if (piece == Piece::WP || piece == Piece::BP) {
            if (rankDelta == (piece == Piece::WP ? 1 : -1) && std::abs(fileDelta) == 1)
                return true;
            continue;
        }
        if (piece == Piece::WN || piece == Piece::BN) {
            if ((std::abs(rankDelta) == 2 && std::abs(fileDelta) == 1) ||
                (std::abs(rankDelta) == 1 && std::abs(fileDelta) == 2)) return true;
            continue;
        }
        if (piece == Piece::WK || piece == Piece::BK) {
            if (std::max(std::abs(rankDelta), std::abs(fileDelta)) == 1) return true;
            continue;
        }

        const bool diagonal = std::abs(rankDelta) == std::abs(fileDelta) && rankDelta != 0;
        const bool straight = (rankDelta == 0) != (fileDelta == 0);
        const bool bishopLike = piece == Piece::WB || piece == Piece::BB ||
                                piece == Piece::WQ || piece == Piece::BQ;
        const bool rookLike = piece == Piece::WR || piece == Piece::BR ||
                              piece == Piece::WQ || piece == Piece::BQ;
        if (!(diagonal && bishopLike) && !(straight && rookLike)) continue;

        const int rankStep = (rankDelta > 0) - (rankDelta < 0);
        const int fileStep = (fileDelta > 0) - (fileDelta < 0);
        int currentRank = rank + rankStep, currentFile = file + fileStep;
        bool clear = true;
        while (currentRank != targetRank || currentFile != targetFile) {
            if (board.cells[currentRank * 8 + currentFile] != Piece::Empty) {
                clear = false;
                break;
            }
            currentRank += rankStep;
            currentFile += fileStep;
        }
        if (clear) return true;
    }
    return false;
}
void updateRights(Board& board, Piece moving, int from, int to) {
    if (moving == Piece::WK) board.whiteKingCastle = board.whiteQueenCastle = false;
    if (moving == Piece::BK) board.blackKingCastle = board.blackQueenCastle = false;
    if (from == 0 || to == 0) board.whiteQueenCastle = false;
    if (from == 7 || to == 7) board.whiteKingCastle = false;
    if (from == 56 || to == 56) board.blackQueenCastle = false;
    if (from == 63 || to == 63) board.blackKingCastle = false;
}

void applyMove(Board& board, const Instruction& instruction) {
    const int from = sq(instruction.a), to = sq(instruction.b);
    const Piece moving = board.cells[from], target = board.cells[to];
    const bool whiteTurn = board.whiteToMove;
    const bool capture = instruction.opcode == "CAPTURE";
    const auto fail = [&](const std::string& reason) {
        throw Diagnostic("SEMANTIC", instruction.where, reason);
    };

    if (moving == Piece::Empty) fail("no piece occupies " + instruction.a);
    if (white(moving) != whiteTurn)
        fail(std::string("wrong turn: ") + (whiteTurn ? "White" : "Black") + " to move");
    if (same(moving, target)) fail("destination is occupied by a friendly piece");
    if (isKing(target)) fail("a king cannot be captured; checkmate ends the game");

    const bool pawn = moving == Piece::WP || moving == Piece::BP;
    const bool enPassantTarget = pawn && board.enPassant && to == *board.enPassant;
    if (enPassantTarget && !capture) fail("en passant must be written as a capture");
    if (capture && target == Piece::Empty && !enPassantTarget)
        fail("capture requires an opposing piece on the destination");
    if (!capture && target != Piece::Empty)
        fail("move destination is occupied; use capture");

    const int fromRank = from / 8, fromFile = from % 8;
    const int toRank = to / 8, toFile = to % 8;
    const int rankDelta = toRank - fromRank, fileDelta = toFile - fromFile;
    bool legalShape = false, enPassantCapture = false;

    if (pawn) {
        const int direction = whiteTurn ? 1 : -1;
        const int startRank = whiteTurn ? 1 : 6;
        if (fileDelta == 0 && target == Piece::Empty) {
            legalShape = rankDelta == direction ||
                (rankDelta == 2 * direction && fromRank == startRank &&
                 board.cells[(fromRank + direction) * 8 + fromFile] == Piece::Empty);
        } else if (std::abs(fileDelta) == 1 && rankDelta == direction) {
            legalShape = target != Piece::Empty || enPassantTarget;
            enPassantCapture = target == Piece::Empty && enPassantTarget;
        }
        if (capture && fileDelta == 0) legalShape = false;
    } else if (moving == Piece::WN || moving == Piece::BN) {
        legalShape = (std::abs(rankDelta) == 2 && std::abs(fileDelta) == 1) ||
                     (std::abs(rankDelta) == 1 && std::abs(fileDelta) == 2);
    } else if (isKing(moving)) {
        legalShape = std::max(std::abs(rankDelta), std::abs(fileDelta)) == 1;
    } else {
        const bool diagonal = std::abs(rankDelta) == std::abs(fileDelta) && rankDelta != 0;
        const bool straight = (rankDelta == 0) != (fileDelta == 0);
        const bool bishopLike = moving == Piece::WB || moving == Piece::BB ||
                                moving == Piece::WQ || moving == Piece::BQ;
        const bool rookLike = moving == Piece::WR || moving == Piece::BR ||
                              moving == Piece::WQ || moving == Piece::BQ;
        if ((diagonal && bishopLike) || (straight && rookLike)) {
            const int rankStep = (rankDelta > 0) - (rankDelta < 0);
            const int fileStep = (fileDelta > 0) - (fileDelta < 0);
            int rank = fromRank + rankStep, file = fromFile + fileStep;
            legalShape = true;
            while (rank != toRank || file != toFile) {
                if (board.cells[rank * 8 + file] != Piece::Empty) {
                    legalShape = false;
                    break;
                }
                rank += rankStep;
                file += fileStep;
            }
        }
    }
    if (!legalShape) fail("piece cannot make that move under chess movement rules");

    Board next = board;
    next.cells[to] = moving;
    next.cells[from] = Piece::Empty;
    if (enPassantCapture) next.cells[to + (whiteTurn ? -8 : 8)] = Piece::Empty;
    next.enPassant.reset();
    if (pawn && std::abs(rankDelta) == 2) next.enPassant = from + (whiteTurn ? 8 : -8);
    updateRights(next, moving, from, to);
    if (pawn || target != Piece::Empty || enPassantCapture) next.halfmove = 0;
    else ++next.halfmove;
    if (!whiteTurn) ++next.fullmove;
    next.whiteToMove = !whiteTurn;
    const int king = findKing(next, whiteTurn);
    if (king >= 0 && attacked(next, king, !whiteTurn))
        fail("move leaves the moving side's king in check");
    board = next;
}

void castle(Board& board, const Instruction& instruction) {
    const bool whiteTurn = board.whiteToMove;
    const bool kingSide = instruction.a == "king_side";
    const int rank = whiteTurn ? 0 : 7;
    const int king = rank * 8 + 4;
    const int rook = rank * 8 + (kingSide ? 7 : 0);
    const int transit = king + (kingSide ? 1 : -1);
    const int destination = rank * 8 + (kingSide ? 6 : 2);
    const int rookDestination = rank * 8 + (kingSide ? 5 : 3);
    const auto fail = [&](const std::string& reason) {
        throw Diagnostic("SEMANTIC", instruction.where, reason);
    };
    const bool right = whiteTurn
        ? (kingSide ? board.whiteKingCastle : board.whiteQueenCastle)
        : (kingSide ? board.blackKingCastle : board.blackQueenCastle);
    if (!right) fail("castling right is unavailable");
    if (board.cells[king] != (whiteTurn ? Piece::WK : Piece::BK) ||
        board.cells[rook] != (whiteTurn ? Piece::WR : Piece::BR))
        fail("king or rook is not on its castling square");

    for (int square = std::min(king, rook) + 1; square < std::max(king, rook); ++square)
        if (board.cells[square] != Piece::Empty) fail("pieces block castling");
    if (attacked(board, king, !whiteTurn))
        fail("king cannot castle out of check");

    Board through = board;
    through.cells[transit] = through.cells[king];
    through.cells[king] = Piece::Empty;
    if (attacked(through, transit, !whiteTurn))
        fail("king cannot castle through check");

    Board next = board;
    next.cells[destination] = next.cells[king];
    next.cells[king] = Piece::Empty;
    next.cells[rookDestination] = next.cells[rook];
    next.cells[rook] = Piece::Empty;
    if (attacked(next, destination, !whiteTurn))
        fail("king cannot castle into check");
    if (whiteTurn) next.whiteKingCastle = next.whiteQueenCastle = false;
    else next.blackKingCastle = next.blackQueenCastle = false;
    next.enPassant.reset();
    ++next.halfmove;
    if (!whiteTurn) ++next.fullmove;
    next.whiteToMove = !whiteTurn;
    board = next;
}
}
Board::Board() {
    cells.fill(Piece::Empty);
    const Piece whiteBack[] = {Piece::WR, Piece::WN, Piece::WB, Piece::WQ,
                               Piece::WK, Piece::WB, Piece::WN, Piece::WR};
    const Piece blackBack[] = {Piece::BR, Piece::BN, Piece::BB, Piece::BQ,
                               Piece::BK, Piece::BB, Piece::BN, Piece::BR};
    for (int file = 0; file < 8; ++file) {
        cells[file] = whiteBack[file];
        cells[8 + file] = Piece::WP;
        cells[48 + file] = Piece::BP;
        cells[56 + file] = blackBack[file];
    }
}
std::string pieceName(Piece piece) {
    switch (piece) {
        case Piece::Empty: return ".";
        case Piece::WP: return "P"; case Piece::WN: return "N";
        case Piece::WB: return "B"; case Piece::WR: return "R";
        case Piece::WQ: return "Q"; case Piece::WK: return "K";
        case Piece::BP: return "p"; case Piece::BN: return "n";
        case Piece::BB: return "b"; case Piece::BR: return "r";
        case Piece::BQ: return "q"; case Piece::BK: return "k";
    }
    return "?";
}
namespace {
const char* pieceGlyph(Piece p) {
    switch(p) {
        case Piece::Empty:return "·";
        case Piece::WP:return "♙"; case Piece::WN:return "♘"; case Piece::WB:return "♗";
        case Piece::WR:return "♖"; case Piece::WQ:return "♕"; case Piece::WK:return "♔";
        case Piece::BP:return "♟"; case Piece::BN:return "♞"; case Piece::BB:return "♝";
        case Piece::BR:return "♜"; case Piece::BQ:return "♛"; case Piece::BK:return "♚";
    }
    return "?";
}
}
std::string Board::display() const {
    std::ostringstream output;
    output << "    a b c d e f g h\n  +-----------------+\n";
    for (int rank = 7; rank >= 0; --rank) {
        output << rank + 1 << " | ";
        for (int file = 0; file < 8; ++file)
            output << pieceGlyph(cells[rank * 8 + file]) << ' ';
        output << "| " << rank + 1 << '\n';
    }
    output << "  +-----------------+\n    a b c d e f g h\nTurn: "
           << (whiteToMove ? "White" : "Black");
    return output.str();
}
void applyStatement(Board& board, const Statement& statement) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, Move> || std::is_same_v<T, Capture>) {
            Piece moving = board.cells[sq(node.from)];
            int lastRank = moving == Piece::WP ? 7 : moving == Piece::BP ? 0 : -1;
            if (lastRank >= 0 && sq(node.to) / 8 == lastRank)
                throw Diagnostic("SEMANTIC", node.where, "a pawn reaching the final rank must use a promote statement");
            applyMove(board, Instruction{std::is_same_v<T, Move> ? "MOVE" : "CAPTURE", node.from, node.to, "", node.where});
        } else if constexpr (std::is_same_v<T, Castle>) {
            castle(board, Instruction{"CASTLE", node.kingSide ? "king_side" : "queen_side", "", "", node.where});
        } else if constexpr (std::is_same_v<T, Promote>) {
            const int from = sq(node.from), to = sq(node.to);
            const bool whiteTurn = board.whiteToMove;
            if (board.cells[from] != (whiteTurn ? Piece::WP : Piece::BP))
                throw Diagnostic("SEMANTIC", node.where, "only the side-to-move pawn can be promoted");
            if (to / 8 != (whiteTurn ? 7 : 0))
                throw Diagnostic("SEMANTIC", node.where, "promotion destination must be the final rank");
            Piece replacement;
            try { replacement = promoted(whiteTurn, node.piece); }
            catch (const std::invalid_argument& e) { throw Diagnostic("SEMANTIC", node.where, e.what()); }
            const bool capturing = board.cells[to] != Piece::Empty;
            applyMove(board, Instruction{capturing ? "CAPTURE" : "MOVE", node.from, node.to, "", node.where});
            board.cells[to] = replacement;
            board.halfmove = 0;
        } else {
            throw Diagnostic("SEMANTIC", node.where, "opening references must be resolved before validation");
        }
    }, statement);
}

namespace {
std::string squareName(int index) {
    return std::string(1, static_cast<char>('a' + index % 8))
        + static_cast<char>('1' + index / 8);
}

bool hasLegalMove(const Board& board) {
    for (int from = 0; from < 64; ++from) {
        const Piece moving = board.cells[from];
        if (moving == Piece::Empty || white(moving) != board.whiteToMove) continue;
        for (int to = 0; to < 64; ++to) {
            if (from == to) continue;
            const std::string source = squareName(from), destination = squareName(to);
            const bool promotion = (moving == Piece::WP && to / 8 == 7)
                                || (moving == Piece::BP && to / 8 == 0);
            const bool capture = board.cells[to] != Piece::Empty
                || ((moving == Piece::WP || moving == Piece::BP)
                    && board.enPassant && to == *board.enPassant && from % 8 != to % 8);
            Statement candidate = promotion
                ? Statement{Promote{source, destination, "queen", {}}}
                : capture ? Statement{Capture{source, destination, {}}}
                          : Statement{Move{source, destination, {}}};
            Board next = board;
            try { applyStatement(next, candidate); return true; }
            catch (const Diagnostic&) {}
        }
    }
    for (bool kingSide : {true, false}) {
        Board next = board;
        try { applyStatement(next, Statement{Castle{kingSide, {}}}); return true; }
        catch (const Diagnostic&) {}
    }
    return false;
}
}

std::string positionStatus(const Board& board) {
    const int king = findKing(board, board.whiteToMove);
    const bool check = king >= 0 && attacked(board, king, !board.whiteToMove);
    if (hasLegalMove(board)) return check ? "Check" : "In progress";
    if (check) return board.whiteToMove ? "Checkmate — Black wins" : "Checkmate — White wins";
    return "Stalemate";
}

std::vector<Board> executeTrace(const std::vector<Instruction>& ir) {
    Board board;
    std::vector<Board> states{board};
    for (const auto& instruction : ir) {
        if (instruction.opcode == "SET") {
            const int value = std::stoi(instruction.b);
            if (value < static_cast<int>(Piece::Empty) || value > static_cast<int>(Piece::BK))
                throw std::runtime_error("invalid piece in Chess IR");
            board.cells[sq(instruction.a)] = static_cast<Piece>(value);
        } else if (instruction.opcode == "STATE") {
            board.whiteToMove = instruction.a == "WHITE";
            const std::string& rights = instruction.b;
            if (rights.size() != 4) throw std::runtime_error("invalid castling flags in Chess IR");
            board.whiteKingCastle = rights[0] == '1';
            board.whiteQueenCastle = rights[1] == '1';
            board.blackKingCastle = rights[2] == '1';
            board.blackQueenCastle = rights[3] == '1';
            board.enPassant = instruction.c == "-" ? std::nullopt : std::optional<int>{sq(instruction.c)};
        } else if (instruction.opcode == "CLOCK") {
            board.halfmove = std::stoi(instruction.a);
            board.fullmove = std::stoi(instruction.b);
        } else if (instruction.opcode == "END_MOVE") {
            states.push_back(board);
        } else {
            throw std::runtime_error("unknown Chess IR operation: " + instruction.opcode);
        }
    }
    return states;
}

Board execute(const std::vector<Instruction>& ir, bool verbose) {
    (void)verbose;
    return executeTrace(ir).back();
}
}
