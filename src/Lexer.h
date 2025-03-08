#pragma once
#include <vector>
#include <limits>
#include <tuple>

#include "Token.h"
#include "diagnostic.h"

namespace Lexer
{
	std::vector<Token> Lex(const std::string_view& source, const std::string_view& file) noexcept;

	namespace Util
	{
		constexpr size_t LexStringLiteral(const std::string_view& source, size_t i);
		constexpr size_t LexCharLiteral(const std::string_view& source, size_t i);
		std::tuple<size_t, size_t, size_t> SkipComment(std::string_view source);
		constexpr bool IsAlpha(char c) noexcept;
		constexpr bool IsDigit(char c) noexcept;
		constexpr bool IsAlnum(char c) noexcept;
		constexpr bool IsEscapeSequence(char c) noexcept;
		constexpr size_t GetEOFColumn(std::string_view source) noexcept;
	};

	class Trie
	{
		static constexpr size_t MAX_WORD_LENGTH = 256;

	private:
		Trie* children[MAX_WORD_LENGTH]{ nullptr };
		TokenKind kind{ TokenKind::TK_NONE };

	public:
		Trie() = default;
		Trie(TokenKind kind) :
			kind(kind) {}
		~Trie();

	public:
		static Trie InitializeTrie() noexcept;
		static void insert(Trie& root, std::string_view word, TokenKind kind) noexcept;
		static std::pair<TokenKind, size_t> find(const Trie& root, std::string_view word) noexcept;
	};

	class CommentBlockException : public std::exception
	{
	private:
		std::string message;
		size_t newlines;

	public:
		CommentBlockException(const std::string& msg, size_t newlines) :
			message(msg), newlines(newlines) {}

		const char* what() const noexcept override
		{
			return message.c_str();
		}

		size_t getNewlines() const noexcept
		{
			return newlines;
		}
	};

}	 // namespace Lexer
