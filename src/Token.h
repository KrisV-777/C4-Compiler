#pragma once

#include "diagnostic.h"

#define KIND_ACTION(KD, STR, CAT) KD,

enum class TokenKind {
#include "tokenkinds.def"
};

#undef KIND_ACTION

struct Token : public Locatable {
   public:
    TokenKind Kind;
    std::string Text;

    Token(const Locatable& loc, TokenKind kind, std::string_view text);

    friend std::ostream& operator<<(std::ostream& stream, const Token& tok);

    template <typename... TokenKind>
    bool Is(TokenKind... kinds) const { return ((Kind == kinds) || ...); }
    template <typename... TokenKind>
    bool IsNot(TokenKind... kinds) const { return ((Kind != kinds) && ...); }

    bool operator==(Token rhs) const { return Kind == rhs.Kind && Text == rhs.Text; }
    bool operator!=(Token rhs) const { return !(*this == rhs); }
};
