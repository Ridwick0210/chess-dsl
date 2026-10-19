#include "chessscript.hpp"

namespace cs {
namespace {
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

    Program program() {
        Program result;
        while (peek().kind == TokenKind::Opening) {
            take();
            Token name = expect(TokenKind::Identifier, "an opening name");
            expect(TokenKind::LBrace, "'{' after opening name");
            Opening opening{name.text, name.where, {}};
            while (peek().kind != TokenKind::RBrace) {
                requireBlockEnd();
                opening.statements.push_back(statement(false));
            }
            take();
            result.openings.push_back(std::move(opening));
        }

        expect(TokenKind::Game, "'game'");
        Token name = expect(TokenKind::Identifier, "a game name");
        expect(TokenKind::LBrace, "'{' after game name");
        result.game = Game{name.text, name.where, {}};
        while (peek().kind != TokenKind::RBrace) {
            requireBlockEnd();
            result.game.statements.push_back(statement(true));
        }
        take();
        expect(TokenKind::End, "end of file");
        return result;
    }

private:
    const std::vector<Token>& tokens_;
    std::size_t cursor_ = 0;

    const Token& peek() const { return tokens_[cursor_]; }
    Token take() { return tokens_[cursor_++]; }

    Token expect(TokenKind kind, const std::string& wanted) {
        if (peek().kind != kind) {
            const std::string found = peek().text.empty() ? "end of file" : peek().text;
            throw Diagnostic("SYNTAX", peek().where, "expected " + wanted + ", found '" + found + "'");
        }
        return take();
    }

    void requireBlockEnd() const {
        if (peek().kind == TokenKind::End)
            throw Diagnostic("SYNTAX", peek().where, "expected '}' to close block");
    }

    Statement statement(bool allowUse) {
        Token op = take();
        if (op.kind == TokenKind::Move || op.kind == TokenKind::Capture) {
            Token from = expect(TokenKind::Square, "a source square");
            expect(TokenKind::Arrow, "'->'");
            Token to = expect(TokenKind::Square, "a destination square");
            expect(TokenKind::Semicolon, "';'");
            if (op.kind == TokenKind::Move) return Move{from.text, to.text, op.where};
            return Capture{from.text, to.text, op.where};
        }
        if (op.kind == TokenKind::Castle) {
            bool kingSide;
            if (peek().kind == TokenKind::KingSide) { take(); kingSide = true; }
            else if (peek().kind == TokenKind::QueenSide) { take(); kingSide = false; }
            else throw Diagnostic("SYNTAX", peek().where, "expected 'king_side' or 'queen_side'");
            expect(TokenKind::Semicolon, "';'");
            return Castle{kingSide, op.where};
        }
        if (op.kind == TokenKind::Promote) {
            Token from = expect(TokenKind::Square, "a source square");
            expect(TokenKind::Arrow, "'->'");
            Token to = expect(TokenKind::Square, "a destination square");
            expect(TokenKind::As, "'as'");
            Token piece = expect(TokenKind::Identifier, "a promotion piece");
            expect(TokenKind::Semicolon, "';'");
            return Promote{from.text, to.text, piece.text, op.where};
        }
        if (allowUse && op.kind == TokenKind::Use) {
            Token name = expect(TokenKind::Identifier, "an opening name");
            expect(TokenKind::Semicolon, "';'");
            return Use{name.text, op.where};
        }
        throw Diagnostic("SYNTAX", op.where, "expected move, capture, castle, promote"
                         + std::string(allowUse ? ", use" : "") + ", or '}'");
    }
};
}

Program parse(const std::vector<Token>& tokens) { return Parser(tokens).program(); }
}
