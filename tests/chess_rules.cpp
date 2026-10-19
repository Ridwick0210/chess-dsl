#include "chessscript.hpp"
#include <cstdlib>
#include <iostream>

namespace {
int square(char file, int rank) { return (rank - 1) * 8 + (file - 'a'); }

void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

cs::Board emptyBoard() {
    cs::Board board;
    board.cells.fill(cs::Piece::Empty);
    board.whiteKingCastle = board.whiteQueenCastle = false;
    board.blackKingCastle = board.blackQueenCastle = false;
    board.enPassant.reset();
    return board;
}

template <class Statement>
bool rejected(cs::Board board, Statement statement) {
    try { cs::applyStatement(board, cs::Statement{statement}); }
    catch (const cs::Diagnostic&) { return true; }
    return false;
}
}

int main() {
    {
        cs::Board board;
        require(rejected(board, cs::Move{"e7", "e5", {}}), "wrong turn is rejected");
        require(rejected(board, cs::Move{"f1", "b5", {}}), "blocked bishop path is rejected");
        require(rejected(board, cs::Castle{true, {}}), "castling through pieces is rejected");
    }
    {
        auto board = emptyBoard();
        board.cells[square('e', 1)] = cs::Piece::WK;
        board.cells[square('h', 1)] = cs::Piece::WR;
        board.cells[square('e', 8)] = cs::Piece::BK;
        board.whiteKingCastle = true;
        cs::applyStatement(board, cs::Statement{cs::Castle{true, {}}});
        require(board.cells[square('g', 1)] == cs::Piece::WK, "castling moves the king");
        require(board.cells[square('f', 1)] == cs::Piece::WR, "castling moves the rook");
        require(!board.whiteKingCastle && !board.whiteQueenCastle, "castling clears rights");
    }
    {
        auto board = emptyBoard();
        board.cells[square('e', 1)] = cs::Piece::WK;
        board.cells[square('h', 1)] = cs::Piece::WR;
        board.cells[square('e', 8)] = cs::Piece::BK;
        board.cells[square('f', 8)] = cs::Piece::BR;
        board.whiteKingCastle = true;
        require(rejected(board, cs::Castle{true, {}}), "castling through attack is rejected");
    }
    {
        auto board = emptyBoard();
        board.cells[square('e', 1)] = cs::Piece::WK;
        board.cells[square('e', 8)] = cs::Piece::BK;
        board.cells[square('e', 5)] = cs::Piece::WP;
        board.cells[square('d', 7)] = cs::Piece::BP;
        board.whiteToMove = false;
        cs::applyStatement(board, cs::Statement{cs::Move{"d7", "d5", {}}});
        require(board.enPassant == square('d', 6), "double pawn step records en passant target");
        cs::applyStatement(board, cs::Statement{cs::Capture{"e5", "d6", {}}});
        require(board.cells[square('d', 6)] == cs::Piece::WP, "en passant moves the pawn");
        require(board.cells[square('d', 5)] == cs::Piece::Empty, "en passant removes the captured pawn");
        require(!board.enPassant, "en passant target expires");
    }
    {
        auto board = emptyBoard();
        board.cells[square('e', 1)] = cs::Piece::WK;
        board.cells[square('a', 8)] = cs::Piece::BK;
        board.cells[square('e', 2)] = cs::Piece::WB;
        board.cells[square('e', 8)] = cs::Piece::BR;
        require(rejected(board, cs::Move{"e2", "f3", {}}), "pinned piece cannot expose its king");
    }
    {
        auto board = emptyBoard();
        board.cells[square('e', 1)] = cs::Piece::WK;
        board.cells[square('h', 8)] = cs::Piece::BK;
        board.cells[square('a', 7)] = cs::Piece::WP;
        board.cells[square('b', 8)] = cs::Piece::BR;
        cs::applyStatement(board, cs::Statement{cs::Promote{"a7", "b8", "knight", {}}});
        require(board.cells[square('b', 8)] == cs::Piece::WN, "promotion capture produces selected piece");
        require(board.cells[square('a', 7)] == cs::Piece::Empty, "promotion removes original pawn");
    }
    {
        auto board = emptyBoard();
        board.cells[square('e', 1)] = cs::Piece::WK;
        board.cells[square('h', 8)] = cs::Piece::BK;
        board.cells[square('a', 7)] = cs::Piece::WP;
        require(rejected(board, cs::Promote{"a7", "a8", "king", {}}), "invalid promotion piece is rejected");
    }
    {
        auto board = emptyBoard();
        board.cells[square('a', 8)] = cs::Piece::BK;
        board.cells[square('c', 6)] = cs::Piece::WK;
        board.cells[square('b', 6)] = cs::Piece::WQ;
        board.whiteToMove = false;
        require(cs::positionStatus(board) == "Stalemate", "stalemate is detected");
    }
    std::cout << "Chess rules passed: turn, blocked path, castling, king safety, en passant, promotion, stalemate.\n";
}
