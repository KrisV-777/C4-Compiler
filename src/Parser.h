#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "SyntaxTree.h"
#include "Token.h"

namespace Parser
{
	struct Iterator
	{
	private:
		std::vector<Token>::const_iterator it;
		const std::vector<Token>::const_iterator end;

	public:
		Iterator(const std::vector<Token>& tokens) :
			it(tokens.begin()), end(tokens.end() - 1) {}

		// Check if the next token is of a certain kind, and if so, consume it, otherwise return nullptr
		template <typename... TokenKind>
		const Token* CheckNext(TokenKind... kind)
		{
			if (it == end)
				throw std::runtime_error("Unexpected end of file");
			return ((it->Kind != kind) && ...) ? nullptr : &*(it++);
		}
		TokenKind PeekNext(ptrdiff_t n = 0) const { return std::distance(it, end) > n ? (it + n)->Kind : TokenKind::TK_NONE; }
		const Token* GetNext(ptrdiff_t n = 0) const { return std::distance(it, end) > n ? &*(it + n) : nullptr; }
		bool HasNext() const { return it != end; }
		void Error(const std::string& message) const { errorloc(*it, message); }
	};

	AST::TranslationUnit Parse(const std::vector<Token>& tokens);
	AST::ExternPtr ParseExternalDeclaration(Iterator& it);
	// Expressions
	std::optional<AST::ExpressionPtr> ParseExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParsePrimaryExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParsePostFixExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseUnaryExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseCastExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseMultiplicativeExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseAdditiveExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseShiftExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseRelationalExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseEqualityExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseLogicalAndExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseLogicalOrExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseConditionalExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseAssignmentExpression(Iterator& it);
	std::optional<AST::ExpressionPtr> ParseConstantExpression(Iterator& it);

	// Declarations
	enum class DeclaratorType
	{
		kDefault,
		kAbstract,
		kEither,
	};

	std::optional<AST::NodePtr> ParseDeclaration(Iterator& it);																		// int a = 3, b, *c;
	std::optional<AST::TypeSpecifierPtr> ParseTypeSpecifier(Iterator& it);												// void, int, char, struct
	std::optional<AST::StructMemberListPtr> ParseStructDeclarationList(Iterator& it);							// Struct: int a, b, c; bool d, e, f;
	std::optional<AST::DeclarationPtr> ParseStructDeclaration(Iterator& it);											// Struct: int a, b, c;
	std::optional<AST::PointerDeclaratorPtr> ParseInitDeclarator(Iterator& it);										// a = b
	std::optional<AST::PointerDeclaratorPtr> ParseDeclarator(Iterator& it, DeclaratorType type);	// a, *b, **c, ... or *, **, ...
	std::optional<AST::PointerDeclaratorPtr> ParseDeclaratorRec(Iterator& it, DeclaratorType& type);
	std::optional<AST::DirectDeclaratorPtr> ParseDirectDeclarator(Iterator& it, DeclaratorType& type);	// a, b, c,
	AST::ParameterListPtr ParseParameterTypeList(const Token* start, Iterator& it);											// int a, int b, int c
	std::optional<AST::TypeNamePtr> ParseTypeName(Iterator& it);																				// int, char, ...

	// Statements
	std::optional<AST::StatementPtr> ParseStatement(Iterator& it);
	std::optional<AST::StatementPtr> ParseLabeledStatement(Iterator& it);
	std::optional<AST::StatementPtr> ParseCompoundStatement(Iterator& it);
	std::optional<AST::StatementPtr> ParseExpressionStatement(Iterator& it);
	std::optional<AST::StatementPtr> ParseSelectionStatement(Iterator& it);
	std::optional<AST::StatementPtr> ParseIterationStatement(Iterator& it);
	std::optional<AST::StatementPtr> ParseJumpStatement(Iterator& it);

	namespace Util
	{
		inline constexpr std::array TokenKindArray{
			TokenKind::TK_INT,
			TokenKind::TK_CHAR,
			TokenKind::TK_VOID,
			TokenKind::TK_STRUCT,
		};
		inline bool IsTypeName(TokenKind kind) { return std::find(TokenKindArray.begin(), TokenKindArray.end(), kind) != TokenKindArray.end(); }
	}	 // namespace Util

}	 // namespace Parser
