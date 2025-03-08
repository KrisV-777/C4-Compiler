#include "Parser.h"

#include <cassert>

AST::TranslationUnit Parser::Parse(const std::vector<Token>& tokens)
{
	std::vector<AST::ExternPtr> nodes{};
	if (tokens.front().Kind == TokenKind::TK_EOI) {
		errorloc(tokens.front(), "Empty file");
		return { nodes };
	}
	Iterator it(tokens);
	try {
		while (it.HasNext()) {
			auto node = ParseExternalDeclaration(it);
			if (!node) {
				it.Error("Expected external declaration");
				return { {} };
			}
			nodes.push_back(node);
		}
	} catch (const std::exception& e) {
		it.Error(e.what());
		return { {} };
	}
	return { nodes };
}

AST::ExternPtr Parser::ParseExternalDeclaration(Iterator& it)
{
	const auto specifiers = ParseTypeSpecifier(it);
	if (!specifiers) {
		return nullptr;
	}
	const auto declarator = ParseInitDeclarator(it);
	if (it.CheckNext(TokenKind::TK_SEMICOLON) != nullptr) {
		return std::make_shared<AST::ExternalDeclaration>(specifiers.value(), declarator.value_or(nullptr));
	} else if (!declarator) {
		throw std::runtime_error("Expected declarator");
	} else {
		const auto& decl = *declarator;
		if (decl->Type() == AST::NodeType::kInitDeclarator) {
			throw std::runtime_error("Invalid initializer");
		}
		const auto compound = ParseCompoundStatement(it);
		if (!compound) {
			throw std::runtime_error("Expected compound statement");
		}
		return std::make_shared<AST::FunctionDefinition>(specifiers.value(), decl, compound.value());
	}
}

std::optional<AST::ExpressionPtr> Parser::ParseExpression(Iterator& it)
{
	std::vector<AST::ExpressionPtr> expressions{};
	do {
		auto expr = ParseAssignmentExpression(it);
		if (!expr) {
			if (expressions.size() <= 1) {
				break;
			} else {
				throw std::runtime_error("Expected expression");
			}
		}
		expressions.emplace_back(expr.value());
	} while (it.CheckNext(TokenKind::TK_COMMA) != nullptr);
	if (expressions.empty()) {
		return std::nullopt;
	} else if (expressions.size() == 1) {
		return expressions.front();
	}
	return std::make_shared<AST::CommaExpression>(expressions);
}

std::optional<AST::ExpressionPtr> Parser::ParsePrimaryExpression(Iterator& it)
{
	if (it.PeekNext() == TokenKind::TK_NUMBER) {
		const auto numberToken = it.GetNext();
		if (numberToken->Text.empty() || (numberToken->Text.front() == '0' && numberToken->Text.size() > 1)) {
			throw std::runtime_error("Invalid number");
		}
	}
	const auto token = it.CheckNext(TokenKind::TK_IDENTIFIER, TokenKind::TK_NUMBER, TokenKind::TK_CHARACTER, TokenKind::TK_STRING, TokenKind::TK_LPAREN);
	if (!token) {
		return std::nullopt;
	}
	switch (token->Kind) {
	case TokenKind::TK_IDENTIFIER:
		return std::make_shared<AST::Identifier>(token);
	case TokenKind::TK_NUMBER:
	case TokenKind::TK_CHARACTER:
		return std::make_shared<AST::Constant>(token);
	case TokenKind::TK_STRING:
		return std::make_shared<AST::StringLiteral>(token);
	case TokenKind::TK_LPAREN:
		{
			auto expr = ParseExpression(it);
			if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
				throw std::runtime_error("Expected ')'");
			}
			return expr;
		}
		break;
	default:
		assert(false);
		throw std::runtime_error("Expected primary expression");
	}
}

std::optional<AST::ExpressionPtr> Parser::ParsePostFixExpression(Iterator& it)
{
	auto lhs = ParsePrimaryExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_LPAREN, TokenKind::TK_LBRACKET, TokenKind::TK_LANGLE_BRACKET, TokenKind::TK_DOT, TokenKind::TK_ARROW);
		if (!token) {
			break;
		}
		switch (token->Kind) {
		case TokenKind::TK_LPAREN:
			{
				std::vector<AST::ExpressionPtr> arguments;
				if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
					do {
						const auto arg = ParseAssignmentExpression(it);
						if (!arg) {
							throw std::runtime_error("Expected function argument");
						}
						arguments.push_back(arg.value());
					} while (it.CheckNext(TokenKind::TK_COMMA) != nullptr);
					if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
						throw std::runtime_error("Expected ')'");
					}
				}
				lhs = std::make_shared<AST::FunctionCall>(token, lhs.value(), arguments);
			}
			break;
		case TokenKind::TK_LBRACKET:
		case TokenKind::TK_LANGLE_BRACKET:
			{
				auto index = ParseExpression(it);
				if (!index) {
					throw std::runtime_error("Expected array index");
				}
				if (it.CheckNext(TokenKind::TK_RBRACKET, TokenKind::TK_RANGLE_BRACKET) == nullptr) {
					throw std::runtime_error("Expected ']'");
				}
				lhs = std::make_shared<AST::ArrayAccess>(lhs.value(), index.value());
			}
			break;
		case TokenKind::TK_DOT:
		case TokenKind::TK_ARROW:
			{
				auto member = it.CheckNext(TokenKind::TK_IDENTIFIER);
				if (!member) {
					throw std::runtime_error("Expected member identifier");
				}
				lhs = std::make_shared<AST::MemberAccess>(lhs.value(), token, AST::Identifier(member));
			}
			break;
		default:
			assert(false);
			throw std::runtime_error("Expected postfix operator");
		}
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseUnaryExpression(Iterator& it)
{
	auto token = it.CheckNext(TokenKind::TK_SIZEOF, TokenKind::TK_AND, TokenKind::TK_ASTERISK, TokenKind::TK_MINUS, TokenKind::TK_PLUS, TokenKind::TK_EXCLAMATION);
	if (!token) {
		return ParsePostFixExpression(it);
	}
	switch (token->Kind) {
	case TokenKind::TK_SIZEOF:
		if (it.PeekNext() == TokenKind::TK_LPAREN && Util::IsTypeName(it.PeekNext(1))) {
			it.CheckNext(TokenKind::TK_LPAREN);
			auto typePtr = ParseTypeName(it);
			if (!typePtr) {
				throw std::runtime_error("Expected type name");
			} else if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
				throw std::runtime_error("Expected ')'");
			}
			return std::make_shared<AST::UnaryOperation>(token, typePtr.value());
		} else {
			auto expr = ParseUnaryExpression(it);
			if (!expr) {
				throw std::runtime_error("Expected expression");
			}
			return std::make_shared<AST::UnaryOperation>(token, expr.value());
		}
		break;
	case TokenKind::TK_AND:
	case TokenKind::TK_ASTERISK:
	case TokenKind::TK_MINUS:
	case TokenKind::TK_PLUS:
	case TokenKind::TK_EXCLAMATION:
		if (auto expr = ParseCastExpression(it)) {
			return std::make_shared<AST::UnaryOperation>(token, expr.value());
		} else {
			throw std::runtime_error("Expected cast expression");
		}
		break;
	default:
		assert(false);
		throw std::runtime_error("Expected unary expression");
	}
}

std::optional<AST::ExpressionPtr> Parser::ParseCastExpression(Iterator& it)
{
	if (it.PeekNext() != TokenKind::TK_LPAREN || !Util::IsTypeName(it.PeekNext(1))) {
		return ParseUnaryExpression(it);
	} else {
		it.CheckNext(TokenKind::TK_LPAREN);
		auto typePtr = ParseTypeName(it);
		if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
			throw std::runtime_error("Expected ')'");
		}
		auto expr = ParseCastExpression(it);
		if (!expr) {
			throw std::runtime_error("Expected expression");
		}
		return std::make_shared<AST::CastExpression>(typePtr.value(), expr.value());
	}
}

std::optional<AST::ExpressionPtr> Parser::ParseMultiplicativeExpression(Iterator& it)
{
	auto lhs = ParseCastExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_ASTERISK, TokenKind::TK_DIV, TokenKind::TK_MOD);
		if (!token)
			break;
		auto rhs = ParseCastExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs cast expression in multiplicative expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseAdditiveExpression(Iterator& it)
{
	auto lhs = ParseMultiplicativeExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_PLUS, TokenKind::TK_MINUS);
		if (!token)
			break;
		auto rhs = ParseMultiplicativeExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs multiplicative expression in additive expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseShiftExpression(Iterator& it)
{
	auto lhs = ParseAdditiveExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_LSHIFT, TokenKind::TK_RSHIFT);
		if (!token)
			break;
		auto rhs = ParseAdditiveExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs additive expression in shift expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseRelationalExpression(Iterator& it)
{
	auto lhs = ParseShiftExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_LT, TokenKind::TK_GT, TokenKind::TK_LE, TokenKind::TK_GE);
		if (!token)
			break;
		auto rhs = ParseShiftExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs shift expression in relational expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseEqualityExpression(Iterator& it)
{
	auto lhs = ParseRelationalExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_EQ, TokenKind::TK_NE);
		if (!token)
			break;
		auto rhs = ParseRelationalExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs relational expression in equality expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseLogicalAndExpression(Iterator& it)
{
	auto lhs = ParseEqualityExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_LOGICAL_AND);
		if (!token)
			break;
		auto rhs = ParseEqualityExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs equality expression in logical and expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseLogicalOrExpression(Iterator& it)
{
	auto lhs = ParseLogicalAndExpression(it);
	if (!lhs)
		return std::nullopt;
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_LOGICAL_OR);
		if (!token)
			break;
		auto rhs = ParseLogicalAndExpression(it);
		if (!rhs)
			throw std::runtime_error("Expected rhs equality expression in logical and expression");
		lhs = std::make_shared<AST::BinaryOperation>(token, lhs.value(), rhs.value());
	}
	return lhs;
}

std::optional<AST::ExpressionPtr> Parser::ParseConditionalExpression(Iterator& it)
{
	auto lhs = ParseLogicalOrExpression(it);
	if (!lhs)
		return std::nullopt;
	const auto token = it.CheckNext(TokenKind::TK_QUESTION);
	if (!token)
		return lhs;
	auto middle = ParseExpression(it);
	if (!middle)
		throw std::runtime_error("Expected middle expression in conditional expression");
	if (it.CheckNext(TokenKind::TK_COLON) == nullptr)
		throw std::runtime_error("Expected ':'");
	auto rhs = ParseConditionalExpression(it);
	if (!rhs)
		throw std::runtime_error("Expected rhs expression in conditional expression");
	return std::make_shared<AST::ConditionalExpression>(lhs.value(), middle.value(), rhs.value());
}

std::optional<AST::ExpressionPtr> Parser::ParseAssignmentExpression(Iterator& it)
{
	auto lhs = ParseConditionalExpression(it);
	if (!lhs)
		return std::nullopt;
	if (static_cast<uint32_t>(lhs.value()->Type()) > static_cast<uint32_t>(AST::NodeType::kUnaryOperation))
		return lhs;
	const auto token = it.CheckNext(TokenKind::TK_ASSIGN);
	if (!token)
		return lhs;
	auto rhs = ParseAssignmentExpression(it);
	if (!rhs)
		throw std::runtime_error("Expected rhs assignment expression");
	return std::make_shared<AST::Assignment>(lhs.value(), token, rhs.value());
}

std::optional<AST::ExpressionPtr> Parser::ParseConstantExpression(Iterator& it)
{
	return ParseConditionalExpression(it);
}

std::optional<AST::NodePtr> Parser::ParseDeclaration(Iterator& it)
{
	auto declSpecifiers = ParseTypeSpecifier(it);
	if (!declSpecifiers) {
		return std::nullopt;
	}
	auto initDeclarator = ParseInitDeclarator(it);
	if (it.CheckNext(TokenKind::TK_SEMICOLON) == nullptr) {
		throw std::runtime_error("Expected ';'");
	}
	return std::make_shared<AST::Declaration>(declSpecifiers.value(), initDeclarator.value_or(nullptr));
}

std::optional<AST::TypeSpecifierPtr> Parser::ParseTypeSpecifier(Iterator& it)
{
	const auto token = it.CheckNext(
			TokenKind::TK_VOID,
			TokenKind::TK_CHAR,
			TokenKind::TK_INT,
			TokenKind::TK_STRUCT);
	if (!token)
		return std::nullopt;
	else if (token->Kind == TokenKind::TK_STRUCT) {
		const auto identifier = it.CheckNext(TokenKind::TK_IDENTIFIER);
		if (it.CheckNext(TokenKind::TK_LBRACE, TokenKind::TK_LBRACE_ALT) != nullptr) {
			auto structBody = ParseStructDeclarationList(it);
			if (!structBody) {
				throw std::runtime_error("Expected struct body specifier");
			}
			if (it.CheckNext(TokenKind::TK_RBRACE, TokenKind::TK_RBRACE_ALT) == nullptr) {
				throw std::runtime_error("Expected '}'");
			}
			return std::make_shared<AST::StructSpecifierImpl>(token, identifier, structBody.value());
		}
		if (!identifier) {
			throw std::runtime_error("Expected struct identifier or body");
		}
		return std::make_shared<AST::StructSpecifier>(token, identifier);
	} else {
		return std::make_shared<AST::TypeSpecifier>(token);
	}
}

std::optional<AST::StructMemberListPtr> Parser::ParseStructDeclarationList(Iterator& it)
{
	std::vector<AST::DeclarationPtr> structDeclarations;
	while (true) {
		auto structDeclaration = ParseStructDeclaration(it);
		if (!structDeclaration) {
			break;
		}
		structDeclarations.push_back(structDeclaration.value());
	}
	return std::make_shared<AST::StructMemberList>(structDeclarations);
}

std::optional<AST::DeclarationPtr> Parser::ParseStructDeclaration(Iterator& it)
{
	auto types = ParseTypeSpecifier(it);
	if (!types) {
		return std::nullopt;
	}
	std::vector<AST::PointerDeclaratorPtr> structDecls{};
	// NOTE: Skipping constant expression decl (declarator : constant-expression)
	// COMEBACK: Expecting only single declarators here ig? Might want to simplify
	auto fst = ParseDeclarator(it, DeclaratorType::kDefault);
	if (!fst) {
		throw std::runtime_error("Expected declarator 1");
	}
	structDecls.push_back(fst.value());
	while (it.CheckNext(TokenKind::TK_COMMA) != nullptr) {
		auto decl = ParseDeclarator(it, DeclaratorType::kDefault);
		if (!decl)
			throw std::runtime_error("Expected declarator N");
		structDecls.push_back(decl.value());
	}
	if (it.CheckNext(TokenKind::TK_SEMICOLON) == nullptr) {
		throw std::runtime_error("Expected ';'");
	}
	// auto declList = std::make_shared<AST::InitDeclaratorList>(structDecls);
	if (structDecls.size() != 1) {
		throw std::runtime_error("Expected single declarator");
	}
	return std::make_shared<AST::Declaration>(types.value(), structDecls.front());
}

std::optional<AST::PointerDeclaratorPtr> Parser::ParseInitDeclarator(Iterator& it)
{
	auto decl = ParseDeclarator(it, DeclaratorType::kDefault);
	if (!decl || it.CheckNext(TokenKind::TK_ASSIGN) == nullptr) {
		return decl;
	}
	auto initializer = ParseAssignmentExpression(it);
	if (!initializer) {
		throw std::runtime_error("Expected initializer");
	}
	return std::make_shared<AST::InitDeclarator>(decl.value(), initializer.value());
}

std::optional<AST::PointerDeclaratorPtr> Parser::ParseDeclarator(Iterator& it, DeclaratorType type) { return ParseDeclaratorRec(it, type); }
std::optional<AST::PointerDeclaratorPtr> Parser::ParseDeclaratorRec(Iterator& it, DeclaratorType& type)
{
	auto fst = it.CheckNext(TokenKind::TK_ASTERISK);
	size_t depth = (fst == nullptr) ? 0 : 1;
	while (it.CheckNext(TokenKind::TK_ASTERISK) != nullptr) {
		depth++;
	}
	std::optional<AST::DirectDeclaratorPtr> decl;
	try {
		decl = ParseDirectDeclarator(it, type);
	} catch (const std::invalid_argument&) {
		decl = std::nullopt;
	}
	if (type == DeclaratorType::kAbstract && !decl && depth == 0) {
		return std::nullopt;
	} else if (type == DeclaratorType::kDefault && !decl) {
		if (depth == 0) {
			return std::nullopt;
		} else {
			throw std::runtime_error("Expected declarator");
		}
	}
	return std::make_shared<AST::PointerDeclarator>(fst, depth, decl.value_or(nullptr));
}

std::optional<AST::DirectDeclaratorPtr> Parser::ParseDirectDeclarator(Iterator& it, DeclaratorType& type)
{
	// Identify Type (if unspecified) and parse lefthand side
	auto lhsN = [&]() -> std::optional<AST::NodePtr> {
		const auto token = it.PeekNext();
		if (token == TokenKind::TK_IDENTIFIER) {
			// If identifier is first token, it must be a default declarator
			if (type == DeclaratorType::kAbstract) {
				throw std::runtime_error("Expected abstract declarator");
			}
			type = DeclaratorType::kDefault;
			return std::make_shared<AST::Identifier>(it.CheckNext(TokenKind::TK_IDENTIFIER));
		} else if (token == TokenKind::TK_LPAREN) {
			// If '(' is first token, then, for abstract, it might be recursive down or a parameter list
			// for default, its always recursive down
			if (type != DeclaratorType::kDefault && Util::IsTypeName(it.PeekNext(1))) {
				// Verify if it is a parameter list
				type = DeclaratorType::kAbstract;
				return std::nullopt;
			}
			// else recursive down. type is set in recursive call
			it.CheckNext(TokenKind::TK_LPAREN);
			auto declarator = ParseDeclaratorRec(it, type);
			if (!declarator) {
				throw std::runtime_error("Expected declarator nested");
			} else if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
				throw std::runtime_error("Expected ')'");
			}
			return declarator;
		}
		throw std::invalid_argument("Expected identifier or '('");
	}();
	if (!lhsN && type == DeclaratorType::kDefault)
		return std::nullopt;
	if (!lhsN && type == DeclaratorType::kEither) {
		type = DeclaratorType::kAbstract;
	}
	auto lhs = std::make_shared<AST::DirectDeclarator>(nullptr, lhsN.value_or(nullptr));
	while (true) {
		const auto token = it.CheckNext(TokenKind::TK_LPAREN, TokenKind::TK_LBRACKET, TokenKind::TK_LANGLE_BRACKET);
		if (!token)
			break;
		switch (token->Kind) {
		case TokenKind::TK_LPAREN:
			lhs = std::make_shared<AST::DirectDeclarator>(lhs, ParseParameterTypeList(token, it));
			if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
				throw std::runtime_error("Expected ')'");
			}
			break;
		case TokenKind::TK_LBRACKET:
		case TokenKind::TK_LANGLE_BRACKET:
			if (it.PeekNext() == TokenKind::TK_ASTERISK && (it.PeekNext(1) == TokenKind::TK_RBRACKET || it.PeekNext(1) == TokenKind::TK_RANGLE_BRACKET)) {
				const auto ptrToken = it.CheckNext(TokenKind::TK_ASTERISK);
				it.CheckNext(TokenKind::TK_RBRACKET, TokenKind::TK_RANGLE_BRACKET);
				auto content = std::make_shared<AST::Node>(ptrToken);
				lhs = std::make_shared<AST::ArrayDeclarator>(lhs, content);
			} else {
				auto expr = ParseAssignmentExpression(it);
				if (it.CheckNext(TokenKind::TK_RBRACKET, TokenKind::TK_RANGLE_BRACKET) == nullptr) {
					throw std::runtime_error("Expected ']'");
				}
				lhs = std::make_shared<AST::ArrayDeclarator>(lhs, expr.value_or(nullptr));
			}
			break;
		default:
			assert(false);
			throw std::runtime_error("Expected '(' or '['");
		}
	}
	if (!lhs)
		return std::nullopt;
	return lhs;
}

std::optional<AST::TypeNamePtr> Parser::ParseTypeName(Iterator& it)
{
	auto declSpecifiers = ParseTypeSpecifier(it);
	if (!declSpecifiers) {
		return std::nullopt;
	}
	auto declarator = ParseDeclarator(it, DeclaratorType::kAbstract).value_or(nullptr);
	return std::make_shared<AST::TypeName>(declSpecifiers.value(), declarator);
}

AST::ParameterListPtr Parser::ParseParameterTypeList(const Token* start, Iterator& it)
{
	std::vector<AST::ParameterDeclarationPtr> parameters;
	do {
		auto declSpecifier = ParseTypeSpecifier(it);
		if (!declSpecifier) {
			if (parameters.empty())
				goto end;
			throw std::runtime_error("Expected declaration specifiers");
		}
		auto declarator = ParseDeclarator(it, DeclaratorType::kEither).value_or(nullptr);
		auto argVal = std::make_shared<AST::ParameterDeclaration>(declSpecifier.value(), declarator);
		parameters.push_back(argVal);
	} while (it.CheckNext(TokenKind::TK_COMMA) != nullptr);
end:
	return std::make_shared<AST::ParameterTypeList>(start, parameters);
}

std::optional<AST::StatementPtr> Parser::ParseStatement(Iterator& it)
{
	auto statement = ParseLabeledStatement(it);
	if (statement) {
		return statement;
	}
	statement = ParseCompoundStatement(it);
	if (statement) {
		return statement;
	}
	statement = ParseSelectionStatement(it);
	if (statement) {
		return statement;
	}
	statement = ParseIterationStatement(it);
	if (statement) {
		return statement;
	}
	statement = ParseJumpStatement(it);
	if (statement) {
		return statement;
	}
	statement = ParseExpressionStatement(it);
	if (statement) {
		return statement;
	}
	return std::nullopt;
}


std::optional<AST::StatementPtr> Parser::ParseLabeledStatement(Iterator& it)
{
	if (it.PeekNext() != TokenKind::TK_IDENTIFIER || it.PeekNext(1) != TokenKind::TK_COLON) {
		return std::nullopt;
	}
	auto label = it.CheckNext(TokenKind::TK_IDENTIFIER);
	assert(label);
	it.CheckNext(TokenKind::TK_COLON);
	auto statement = ParseStatement(it);
	if (!statement) {
		throw std::runtime_error("Expected statement");
	}
	return std::make_shared<AST::LabeledStatement>(label, statement.value());
}

std::optional<AST::StatementPtr> Parser::ParseCompoundStatement(Iterator& it)
{
	std::vector<AST::NodePtr> statements;
	auto token = it.CheckNext(TokenKind::TK_LBRACE, TokenKind::TK_LBRACE_ALT);
	if (token == nullptr) {
		return std::nullopt;
	}
	while (true) {
		if (auto statement = ParseStatement(it)) {
			statements.push_back(statement.value());
			continue;
		}
		if (auto decl = ParseDeclaration(it)) {
			statements.push_back(decl.value());
			continue;
		}
		break;
	}
	if (it.CheckNext(TokenKind::TK_RBRACE, TokenKind::TK_RBRACE_ALT) == nullptr) {
		throw std::runtime_error("Expected '}'");
	}
	return std::make_shared<AST::CompoundStatement>(token, statements);
}

std::optional<AST::StatementPtr> Parser::ParseExpressionStatement(Iterator& it)
{
	auto expr = ParseExpression(it);
	if (it.CheckNext(TokenKind::TK_SEMICOLON) == nullptr) {
		if (expr) {
			throw std::runtime_error("Expected ';'");
		} else {
			return std::nullopt;
		}
	}
	return std::make_shared<AST::ExpressionStatement>(expr.value_or(nullptr));
}

std::optional<AST::StatementPtr> Parser::ParseSelectionStatement(Iterator& it)
{
	auto ifToken = it.CheckNext(TokenKind::TK_IF);
	if (!ifToken) {
		return std::nullopt;
	}
	if (it.CheckNext(TokenKind::TK_LPAREN) == nullptr) {
		throw std::runtime_error("Expected '('");
	}
	auto condition = ParseExpression(it);
	if (!condition) {
		throw std::runtime_error("Expected condition");
	}
	if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
		throw std::runtime_error("Expected ')'");
	}
	auto ifStatement = ParseStatement(it);
	if (!ifStatement) {
		throw std::runtime_error("Expected if statement");
	}
	if (it.CheckNext(TokenKind::TK_ELSE) == nullptr) {
		return std::make_shared<AST::SelectionStatement>(ifToken, condition.value(), ifStatement.value(), nullptr);
	}
	auto elseStatement = ParseStatement(it);
	if (!elseStatement) {
		throw std::runtime_error("Expected else statement");
	}
	return std::make_shared<AST::SelectionStatement>(ifToken, condition.value(), ifStatement.value(), elseStatement.value());
}

std::optional<AST::StatementPtr> Parser::ParseIterationStatement(Iterator& it)
{
	auto whileToken = it.CheckNext(TokenKind::TK_WHILE);
	if (!whileToken) {
		return std::nullopt;
	}
	if (it.CheckNext(TokenKind::TK_LPAREN) == nullptr) {
		throw std::runtime_error("Expected '('");
	}
	auto condition = ParseExpression(it);
	if (!condition) {
		throw std::runtime_error("Expected condition");
	}
	if (it.CheckNext(TokenKind::TK_RPAREN) == nullptr) {
		throw std::runtime_error("Expected ')'");
	}
	auto body = ParseStatement(it);
	if (!body) {
		throw std::runtime_error("Expected body");
	}
	return std::make_shared<AST::IterationStatement>(whileToken, condition.value(), body.value());
}

std::optional<AST::StatementPtr> Parser::ParseJumpStatement(Iterator& it)
{
	auto jumpKind = it.CheckNext(TokenKind::TK_RETURN, TokenKind::TK_BREAK, TokenKind::TK_CONTINUE, TokenKind::TK_GOTO);
	if (!jumpKind) {
		return std::nullopt;
	}
	switch (jumpKind->Kind) {
	case TokenKind::TK_RETURN:
		{
			auto expr = ParseExpression(it).value_or(nullptr);
			if (it.CheckNext(TokenKind::TK_SEMICOLON) == nullptr) {
				throw std::runtime_error("Expected ';'");
			}
			return std::make_shared<AST::JumpStatement>(jumpKind, expr);
		}
		break;
	case TokenKind::TK_BREAK:
	case TokenKind::TK_CONTINUE:
		{
			if (it.CheckNext(TokenKind::TK_SEMICOLON) == nullptr) {
				throw std::runtime_error("Expected ';'");
			}
			return std::make_shared<AST::JumpStatement>(jumpKind, nullptr);
		}
		break;
	case TokenKind::TK_GOTO:
		{
			auto label = it.CheckNext(TokenKind::TK_IDENTIFIER);
			if (!label) {
				throw std::runtime_error("Expected label");
			}
			if (it.CheckNext(TokenKind::TK_SEMICOLON) == nullptr) {
				throw std::runtime_error("Expected ';'");
			}
			auto labelStmt = std::make_shared<AST::Identifier>(label);
			return std::make_shared<AST::JumpStatement>(jumpKind, labelStmt);
		}
		break;
	default:
		assert(false);
		break;
	}
	return std::nullopt;
}
