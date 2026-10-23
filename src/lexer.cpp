#include "chessscript.hpp"
#include <cctype>
#include <sstream>

namespace cs {
Diagnostic::Diagnostic(std::string c, Location l, const std::string& m)
    : std::runtime_error(c + " error at " + std::to_string(l.line) + ":" + std::to_string(l.column) + ": " + m),
      category(std::move(c)), detail(m), where(l) {}

std::string tokenName(TokenKind k) {
    switch (k) {
        case TokenKind::Game:return "GAME"; case TokenKind::Opening:return "OPENING";
        case TokenKind::Use:return "USE"; case TokenKind::Move:return "MOVE";
        case TokenKind::Capture:return "CAPTURE"; case TokenKind::Castle:return "CASTLE";
        case TokenKind::Promote:return "PROMOTE"; case TokenKind::As:return "AS";
        case TokenKind::KingSide:return "KING_SIDE"; case TokenKind::QueenSide:return "QUEEN_SIDE";
        case TokenKind::Arrow:return "ARROW"; case TokenKind::LBrace:return "LBRACE";
        case TokenKind::RBrace:return "RBRACE"; case TokenKind::Semicolon:return "SEMICOLON";
        case TokenKind::Square:return "SQUARE"; case TokenKind::Identifier:return "IDENTIFIER";
        case TokenKind::End:return "EOF";
    } return "UNKNOWN";
}

std::vector<Token> lex(const std::string& s) {
    std::vector<Token> out; std::size_t i=0; int line=1,col=1;
    auto loc=[&]{ return Location{line,col}; };
    auto advance=[&]{ if (s[i++]=='\n') { ++line; col=1; } else ++col; };
    while (i<s.size()) {
        if (std::isspace(static_cast<unsigned char>(s[i]))) { advance(); continue; }
        if (s[i]=='#') { while (i<s.size() && s[i]!='\n') advance(); continue; }
        Location at=loc(); char c=s[i];
        if (c=='-' && i+1<s.size() && s[i+1]=='>') { advance(); advance(); out.push_back({TokenKind::Arrow,"->",at}); continue; }
        if (c=='{') { advance(); out.push_back({TokenKind::LBrace,"{",at}); continue; }
        if (c=='}') { advance(); out.push_back({TokenKind::RBrace,"}",at}); continue; }
        if (c==';') { advance(); out.push_back({TokenKind::Semicolon,";",at}); continue; }
        if (c>='a'&&c<='h'&&i+1<s.size()&&s[i+1]>='1'&&s[i+1]<='8') {
            std::string val; val+=c; advance(); val+=s[i]; advance(); out.push_back({TokenKind::Square,val,at}); continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c=='_') {
            std::string word; while (i<s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i]=='_')) { word+=s[i]; advance(); }
            if(word.size()==2 && word[0]>='a' && word[0]<='h' && std::isdigit(static_cast<unsigned char>(word[1])))
                throw Diagnostic("LEXICAL",at,"invalid square '"+word+"' (ranks are 1 through 8)");
            TokenKind k=TokenKind::Identifier;
            if(word=="game")k=TokenKind::Game; else if(word=="opening")k=TokenKind::Opening;
            else if(word=="use")k=TokenKind::Use; else if(word=="move")k=TokenKind::Move;
            else if(word=="capture")k=TokenKind::Capture; else if(word=="castle")k=TokenKind::Castle;
            else if(word=="promote")k=TokenKind::Promote; else if(word=="as")k=TokenKind::As;
            else if(word=="king_side")k=TokenKind::KingSide; else if(word=="queen_side")k=TokenKind::QueenSide;
            out.push_back({k,word,at}); continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string val; while(i<s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { val+=s[i]; advance(); }
            throw Diagnostic("LEXICAL",at,"malformed square or unexpected number '"+val+"'");
        }
        // The language grammar is ASCII. Avoid placing a raw non-ASCII byte in
        // a diagnostic, since the JSON interface must remain valid UTF-8.
        if (static_cast<unsigned char>(c) >= 0x80)
            throw Diagnostic("LEXICAL", at, "non-ASCII character outside a comment");
        throw Diagnostic("LEXICAL",at,"unexpected character '"+std::string(1,c)+"'");
    }
    out.push_back({TokenKind::End,"",{line,col}}); return out;
}
}
