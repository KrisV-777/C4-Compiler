#include "Token.h"

Token::Token(const Locatable& loc, TokenKind kind, std::string_view text) :
	Locatable(loc), Kind(kind), Text(text) {}

std::ostream& operator<<(std::ostream& stream, const Token& tok)
{
	stream << static_cast<const Locatable&>(tok) << ' ';

#define KIND_ACTION(KD, STR, CAT) \
	case TokenKind::KD:             \
		stream << CAT;                \
		break;

	switch (tok.Kind) {
#include "tokenkinds.def"
	}

#undef KIND_ACTION

	stream << ' ' << tok.Text;

	return stream;
}
