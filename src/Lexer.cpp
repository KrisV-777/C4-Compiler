#include "Lexer.h"

#include <algorithm>
#include <array>

namespace Lexer
{
	std::vector<Token> Lex(const std::string_view& source, const std::string_view& file) noexcept
	{
		const auto root = Trie::InitializeTrie();
		std::vector<Token> tokens{};
		tokens.reserve(source.length() / 2);
		size_t i = 0, line = 1, column = 1;
		while (i < source.length()) {
			if (std::isblank(source[i])) {
				i++;
				column++;
				continue;
			}
			auto [kind, length] = Trie::find(root, source.substr(i));
			switch (kind) {
			case TokenKind::TK_NONE:
				errorloc(Locatable{ file, line, column }, "Unknown character: ", source[i]);
				return {};
			case TokenKind::TK_NEWLINE_SEQUENCE:
			case TokenKind::TK_NEWLINE_R:
			case TokenKind::TK_NEWLINE_N:
				i += length;
				line += 1;
				column = 1;
				continue;
			case TokenKind::TK_LINE_COMMENT:
				i += 2;	// Include the opening //
				column += 2;
				while (i < source.length() && source[i] != '\n' && source[i] != '\r') {
					i++;
					column++;
				}
				continue;
			case TokenKind::TK_BLOCK_COMMENT:
				try {
					const auto [len, lines, col] = Util::SkipComment(source.substr(i));
					if (lines > 0) {
						line += lines;
						column = col;
						i += len;
						continue;
					}
					length = len;
				} catch (const CommentBlockException& e) {
					const auto newlines = e.getNewlines();
					if (newlines > 0) {
						line += newlines;
						column = Util::GetEOFColumn(source.substr(i));
					}
					Locatable loc{ file, line, column };
					errorloc(loc, e.what());
					return {};
				}
				break;
			case TokenKind::TK_STRING:
				try {
					length = Util::LexStringLiteral(source, i);
					tokens.emplace_back( Locatable{ file, line, column }, TokenKind::TK_STRING, source.substr(i, length) );
				} catch (const std::runtime_error& e) {
					Locatable loc{ file, line, column };
					errorloc(loc, e.what());
					return {};
				}
				break;
			case TokenKind::TK_CHARACTER:
				try {
					length = Util::LexCharLiteral(source, i);
					tokens.emplace_back( Locatable{ file, line, column }, TokenKind::TK_CHARACTER, source.substr(i, length) );
				} catch (const std::runtime_error& e) {
					Locatable loc{ file, line, column };
					errorloc(loc, e.what());
					return {};
				}
				break;
			default:
				tokens.emplace_back(Locatable{ file, line, column }, kind, source.substr(i, length));
				break;
			}
			i += length;
			column += length;
		}
		tokens.emplace_back(Locatable{ file, line, column }, TokenKind::TK_EOI, "");

		return tokens;
	}
	
	constexpr size_t Util::LexStringLiteral(const std::string_view& source, size_t start)
	{
		size_t i = start + 1;	// Skip the opening quote
		while (i < source.length()) {
			const auto c = source[i];
			if (c == '"') {
				return i - start + 1;	 // Include the closing quote
			} else if (c == '\\') {
				if (i + 1 < source.length() && IsEscapeSequence(source[i + 1])) {
					i += 2;
				} else {
					throw std::runtime_error("Invalid Escape Sequence");
				}
			} else {
				i++;
			}
		}
		throw std::runtime_error("Unterminated string literal");
	}

	constexpr size_t Util::LexCharLiteral(const std::string_view& source, size_t start)
	{
		if (source.length() < start + 3) {
			throw std::runtime_error("Unterminated character literal");
		}
		const auto c = source[start + 1];
		if (c == '\\') {
			if (source.length() < start + 4 || source[start + 3] != '\'') {
				throw std::runtime_error("Unterminated escape character literal");
			} else if (!IsEscapeSequence(source[start + 2])) {
				throw std::runtime_error("Invalid Escape Sequence");
			}
			return 4;
		} else if (source[start + 2] == '\'') {
			return 3;
		} else {
			throw std::runtime_error("Unterminated character literal");
		}
	}

	std::tuple<size_t, size_t, size_t> Util::SkipComment(std::string_view source)
	{
		size_t i = 2;
		size_t lines = 0, colums = 1;
		while (i + 1 < source.length() && !(source[i] == '*' && source[i + 1] == '/')) {
			if (source[i] == '\n') {
				colums = 1;
				lines++;
			} else if (source[i] == '\r') {
				if (i + 1 < source.length() && source[i + 1] == '\n') {
					i++;
				}
				colums = 1;
				lines++;
			} else {
				colums++;
			}
			i++;
		}
		if (i + 1 >= source.length()) {
			throw CommentBlockException("Unterminated block comment", lines);
		}
		return std::make_tuple(i + 2, lines, colums + 2);	 // Skip the closing */
	}

	constexpr bool Util::IsAlpha(char c) noexcept
	{
		return std::isalpha(c) || c == '_';
	}

	constexpr bool Util::IsDigit(char c) noexcept
	{
		return std::isdigit(c);
	}

	constexpr bool Util::IsAlnum(char c) noexcept
	{
		return std::isalnum(c) || c == '_';
	}

	constexpr bool Util::IsEscapeSequence(char c) noexcept
	{
		return c == '\\' || c == '\"' || c == '\'' || c == '?' || c == 'a' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v' || c == '0';
	}

	constexpr size_t Util::GetEOFColumn(std::string_view source) noexcept
	{
		const auto where = source.find_last_of("\n\r");
		if (where != std::string::npos) {
			return source.length() - where;
		} else {
			return source.length();
		}
	}

	// Trie

	Trie::~Trie()
	{
		for (auto& child : children) {
			if (child) {
				delete child;
			}
		}
	}

	Trie Trie::InitializeTrie() noexcept
	{
		Trie root{};

#define KIND_ACTION(KD, STR, CAT) \
	if (*STR)                       \
		Trie::insert(root, STR, TokenKind::KD);
#include "tokenkinds.def"
#undef KIND_ACTION

		return root;
	}

	void Trie::insert(Trie& root, std::string_view word, TokenKind kind) noexcept
	{
		Trie* node = &root;
		for (char c : word) {
			const auto idx = static_cast<unsigned char>(c);
			if (node->children[idx] == nullptr) {
				node = node->children[idx] = new Trie{};
			} else {
				node = node->children[idx];
			}
		}
		node->kind = kind;
	}

	std::pair<TokenKind, size_t> Trie::find(const Trie& root, std::string_view word) noexcept
	{
		const Trie* node = &root;
		bool mightBeIdentifier = Util::IsAlpha(word[0]);
		bool mightBeNumeric = Util::IsDigit(word[0]);
		std::pair<TokenKind, size_t> result{ TokenKind::TK_NONE, 0 };
		size_t i = 0;
		for (; i < word.length(); i++) {
			const auto c = word[i];
			const auto next = node->children[static_cast<unsigned char>(c)];
			if (next == nullptr) {
				break;
			} else if (next->kind != TokenKind::TK_NONE) {
				result = { next->kind, i + 1 };
			}
			mightBeIdentifier = mightBeIdentifier && Util::IsAlnum(c);
			mightBeNumeric = mightBeNumeric && Util::IsDigit(c);
			node = next;
		}
		const auto c = i < word.length() ? word[i] : '\0';
		if (node->kind != TokenKind::TK_NONE && ((mightBeIdentifier && !Util::IsAlnum(c)) || (mightBeNumeric && !Util::IsDigit(c)))) {
			return result;
		}
		if (mightBeIdentifier) {
			while (i < word.length() && Util::IsAlnum(word[i])) {
				i++;
			}
			return { TokenKind::TK_IDENTIFIER, i };
		} else if (mightBeNumeric) {
			while (i < word.length() && Util::IsDigit(word[i])) {
				i++;
			}
			return { TokenKind::TK_NUMBER, i };
		}
		return result;
	}

}	 // namespace Lexer
