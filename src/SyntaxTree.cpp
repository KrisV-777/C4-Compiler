#include "SyntaxTree.h"

#include <limits>

bool AST::EffectiveType::CanAssign(const EffectiveType& rhs) const
{
	// constexpr auto inBounds = [](const Token* token, int64_t min, int64_t max) {
	// 	try {
	// 		auto val = std::stoi(token->Text);
	// 		return val >= min && val <= max;
	// 	} catch (const std::exception&) {
	// 		return false;
	// 	}
	// };
	if (!rawType || !rhs.rawType || context == TypeContext::kRValue)
		return false;
	if (rhs.rawType->Is(TokenKind::TK_VOID)) {
		return rhs.pointerDepth > 0 && pointerDepth > 0;
	}
	if (pointerDepth > 0 && rhs.IsNull())
		return true;
	if (rhs.returnType != nullptr && pointerDepth == 1) {
		return (pointerDepth - 1) == rhs.returnType->pointerDepth && *rawType == *(rhs.returnType->rawType);
	}
	switch (rawType->Kind) {
	case TokenKind::TK_NUMBER:
	case TokenKind::TK_CHARACTER:
	case TokenKind::TK_STRING:
	case TokenKind::TK_NONE:
		return false;
	case TokenKind::TK_VOID:
		return rhs.pointerDepth > 0 && pointerDepth > 0;
	case TokenKind::TK_INT:
		if (pointerDepth != rhs.pointerDepth)
			return false;
		if (rhs.rawType->IsNot(TokenKind::TK_INT, TokenKind::TK_CHAR, TokenKind::TK_NUMBER, TokenKind::TK_CHARACTER))
			return false;
		return true;
	case TokenKind::TK_CHAR:	// String Literals always have pointer depth 1
		if (pointerDepth != rhs.pointerDepth)
			return false;
		if (rhs.rawType->IsNot(TokenKind::TK_INT, TokenKind::TK_CHAR, TokenKind::TK_NUMBER, TokenKind::TK_CHARACTER, TokenKind::TK_STRING))
			return false;
		return true;
	default:	// Struct Types
		if (pointerDepth == 0 && rhs.pointerDepth == 0) {
			return *rhs.rawType == *rawType;
		}
		return true;
	}
}

AST::Context* AST::Context::GetFunctionLevelContext()
{
	if (scopeContext == ScopeContext::kStruct || scopeContext == ScopeContext::kNone)
		return nullptr;
	auto retVal = this;
	while (retVal->parent) {
		if (retVal->parent->scopeContext == ScopeContext::kNone)
			return retVal;
		retVal = retVal->parent.get();
	}
	return nullptr;
}

std::string AST::FunctionCall::ToString(size_t depth) const
{
	auto ret = Depth(depth) + "(" + function->ToString(0) + "(";
	for (size_t i = 0; i < arguments.size(); ++i) {
		ret += arguments[i]->ToString(0);
		if (i < arguments.size() - 1) {
			ret += ", ";
		}
	}
	return ret + "))";
}

std::string AST::ArrayAccess::ToString(size_t depth) const
{
	return Depth(depth) + "(" + array->ToString(0) + "[" + index->ToString(0) + "])";
}

std::string AST::MemberAccess::ToString(size_t depth) const
{
	return Depth(depth) + "(" + object->ToString(0) + accessKind->Text + member.ToString(0) + ")";
}

std::string AST::UnaryOperation::ToString(size_t depth) const
{
	if (op->Kind == TokenKind::TK_SIZEOF) {
		if (operand->Type() == NodeType::kTypeName) {
			return Depth(depth) + "(sizeof(" + operand->ToString(0) + "))";
		} else {
			auto tmpStr = operand->ToString(0);
			return Depth(depth) + "(sizeof " + tmpStr + ")";
		}
	} else {
		return Depth(depth) + "(" + op->Text + operand->ToString(depth) + ")";
	}
}

std::string AST::CastExpression::ToString(size_t depth) const
{
	return Depth(depth) + "((" + castType->ToString(0) + ")" + castExpression->ToString(0) + ")";
}

std::string AST::BinaryOperation::ToString(size_t depth) const
{
	return Depth(depth) + "(" + lhs->ToString(0) + " " + op->Text + " " + rhs->ToString(0) + ")";
}

std::string AST::ConditionalExpression::ToString(size_t depth) const
{
	return Depth(depth) + "(" + condition->ToString(0) + " ? " + middle->ToString(0) + " : " + rhs->ToString(0) + ")";
}

std::string AST::Assignment::ToString(size_t depth) const
{
	return Depth(depth) + "(" + lhs->ToString(0) + " " + op->Text + " " + rhs->ToString(0) + ")";
}

std::string AST::CommaExpression::ToString(size_t depth) const
{
	std::string ret = Depth(depth);
	for (size_t i = 0; i < expressions.size(); ++i) {
		ret += expressions[i]->ToString(0);
		if (i < expressions.size() - 1) {
			ret += ", ";
		}
	}
	return ret;
}

std::string AST::TypeSpecifier::ToString(size_t depth) const
{
	return Depth(depth) + token->Text;
}

std::string AST::StructSpecifier::ToString(size_t depth) const
{
	return Depth(depth) + "struct " + identifier->Text;
}

std::string AST::StructSpecifierImpl::ToString(size_t depth) const
{
	auto ret = Depth(depth) + "struct " + identifier->Text + "\n" + Depth(depth) + "{\n";
	ret += structDeclarationList->ToString(depth + 1);
	return ret + Depth(depth) + "}";
}

std::string AST::DirectDeclarator::ToString(size_t depth) const
{
	if (previous) {
		return Depth(depth) + "(" + previous->ToString(0) + declarator->ToString(0) + ")";
	} else if (declarator) {
		auto inner = declarator->ToString(0);
		return Depth(depth) + inner;
	} else {	// abstract declarator bottom node
		return "";
	}
}

std::string AST::ArrayDeclarator::ToString(size_t depth) const
{
	if (previous) {
		return Depth(depth) + previous->ToString(0) + "[" + declarator->ToString(0) + "]";
	} else {
		return Depth(depth) + "[" + declarator->ToString(0) + "]";
	}
}

std::string AST::PointerDeclarator::ToString(size_t depth) const
{
	if (pointerDepth == 0)
		return declarator ? declarator->ToString(depth) : "";
	auto retVal = Depth(depth);
	for (size_t i = 0; i < pointerDepth; i++) {
		retVal += "(*";
	}
	return retVal + (declarator ? declarator->ToString(0) : "") + std::string(pointerDepth, ')');
}

std::string AST::InitDeclarator::ToString(size_t depth) const
{
	return Depth(depth) + PointerDeclarator::ToString(0) + " = " + initializer->ToString(0);
}

std::string AST::TypeName::ToString(size_t depth) const
{
	return Depth(depth) + declSpecifiers->ToString(0) + (abstractDeclarator ? " " + abstractDeclarator->ToString(0) : "");
}

std::string AST::Declaration::ToString(size_t depth) const
{
	const auto tmpStr = initDeclarator ? initDeclarator->ToString(0) : "";
	const auto insert = tmpStr.empty() ? "" : " ";
	return declSpecifiers->ToString(depth) + insert + tmpStr + ";\n";
}

std::string AST::StructMemberList::ToString(size_t depth) const
{
	std::string ret;
	for (size_t i = 0; i < declarations.size(); ++i) {
		ret += declarations[i]->ToString(depth);
	}
	return ret;
}

std::string AST::ParameterDeclaration::ToString(size_t depth) const
{
	const auto tmpStr = declarator ? declarator->ToString(0) : "";
	return Depth(depth) + declSpecifiers->ToString(0) + (tmpStr.empty() ? "" : " " + tmpStr);
}

std::string AST::ParameterTypeList::ToString(size_t depth) const
{
	std::string ret = Depth(depth) + "(";
	for (size_t i = 0; i < parameters.size(); ++i) {
		ret += parameters[i]->ToString(0);
		if (i < parameters.size() - 1) {
			ret += ", ";
		}
	}
	return ret + ")";
}

std::string AST::DesginationList::ToString(size_t depth) const
{
	std::string ret = Depth(depth);
	for (size_t i = 0; i < expression.size(); ++i) {
		ret += expression[i]->ToString(0);
		if (i < expression.size() - 1) {
			ret += ", ";
		}
	}
	return ret;
}

std::string AST::LabeledStatement::ToString(size_t depth) const
{
	return token->Text + ":\n" + statement->ToString(depth);
}

std::string AST::CompoundStatement::ToString(size_t depth) const { return ToString(depth, depth); }
std::string AST::CompoundStatement::ToString(size_t depth, size_t innit) const
{
	std::string result = Depth(innit) + "{\n";
	for (const auto& stmt : statements) {
		result += stmt->ToString(depth + 1);
	}
	result += Depth(depth) + "}";
	if (innit == depth) {
		result += "\n";
	}
	return result;
}

std::string AST::ExpressionStatement::ToString(size_t depth) const
{
	return Depth(depth) + (expression ? expression->ToString(0) + ";\n" : ";\n");
}

std::string AST::SelectionStatement::ToString(size_t depth) const { return ToString(depth, depth); }
std::string AST::SelectionStatement::ToString(size_t depth, size_t innit) const
{
	bool compound = ifStatement->Type() == NodeType::kCompoundStatement;
	auto ifStr = Depth(innit) + "if (" + condition->ToString(0) + ")";
	if (compound) {
		ifStr += " " + ifStatement->ToString(depth, 0);
	} else {
		ifStr += "\n" + ifStatement->ToString(depth + 1);
	}
	if (!elseStatement)
		return ifStr;
	ifStr += (compound ? " " : Depth(depth)) + "else";
	if (elseStatement->Type() == NodeType::kCompoundStatement || elseStatement->Type() == NodeType::kSelectionStatement) {
		ifStr += " " + elseStatement->ToString(depth, 0);
		if (elseStatement->Type() == NodeType::kCompoundStatement) {
			ifStr += "\n";
		}
	} else {
		ifStr += "\n" + elseStatement->ToString(depth + 1);
	}
	return ifStr;
}

std::string AST::IterationStatement::ToString(size_t depth) const
{
	auto retVal = Depth(depth) + token->Text + " (" + condition->ToString(0) + ")";
	if (body->Type() == NodeType::kCompoundStatement) {
		retVal += " " + body->ToString(depth, 0) + "\n";
	} else {
		retVal += "\n" + body->ToString(depth + 1);
	}
	return retVal;
}

std::string AST::JumpStatement::ToString(size_t depth) const
{
	const auto expr = expression ? expression->ToString(0) : "";
	const auto insert = expr.empty() ? "" : " ";
	return Depth(depth) + jumpKind->Text + insert + expr + ";\n";
}

std::string AST::TranslationUnit::ToString(size_t depth) const
{
	std::string result;
	for (size_t i = 0; i < externalDeclarations.size(); i++) {
		result += externalDeclarations[i]->ToString(depth);
		if (i < externalDeclarations.size() - 1)
			result += "\n";
	}
	return result;
}

std::string AST::ExternalDeclaration::ToString(size_t depth) const
{
	return Declaration::ToString(depth);
}

std::string AST::FunctionDefinition::ToString(size_t depth) const
{
	return declSpecifiers->ToString(depth) + " " + declarator->ToString(0) + "\n" + body->ToString(depth);
}

// ---------------------------------------------------------------------------
// EffectiveType

AST::EffectiveType AST::Constant::CheckSemantics(std::shared_ptr<Context>) { return { EffectiveType::TypeContext::kRValue, token }; }
AST::EffectiveType AST::StringLiteral::CheckSemantics(std::shared_ptr<Context>) { return { EffectiveType::TypeContext::kNone, token, 1 }; }
AST::EffectiveType AST::Identifier::CheckSemantics(std::shared_ptr<Context> context)
{
	auto ctx = context;
	auto t = ctx->variables.find(token->Text);
	while (t == ctx->variables.end()) {
		ctx = ctx->parent;
		if (!ctx)
			throw AST::SemanticException("Identifier: Undeclared variable " + token->Text, token);
		t = ctx->variables.find(token->Text);
	}
	return *(t->second);
}

AST::EffectiveType AST::FunctionCall::CheckSemantics(std::shared_ptr<Context> context)
{
	auto tFunc = function->CheckSemantics(context);
	if (tFunc.returnType == nullptr || tFunc.pointerDepth > 1) {
		throw SemanticException("Function call on non-function type", token);
	}
	if (arguments.size() != tFunc.parameters.size()) {
		const auto err = "Function call with wrong number of arguments: " + std::to_string(arguments.size()) + "/" + std::to_string(tFunc.parameters.size());
		throw SemanticException(err, token);
	}
	for (size_t i = 0; i < arguments.size(); ++i) {
		auto tArg = arguments[i]->CheckSemantics(context);
		auto& tParam = tFunc.parameters[i];
		if (!tParam.CanAssign(tArg)) {
			throw SemanticException("Function call with wrong argument type on arg " + std::to_string(i + 1), token);
		}
	}
	return *tFunc.returnType;
}

AST::EffectiveType AST::ArrayAccess::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t1 = array->CheckSemantics(context);
	auto t2 = index->CheckSemantics(context);
	if (t1.pointerDepth == 0 && t2.pointerDepth == 0) {
		throw SemanticException("Array access with non-array type", token);
	} else if (t1.pointerDepth > 0 && t2.pointerDepth > 0) {
		throw SemanticException("Array access with pointer type", token);
	} else if (t1.IsArithmetic()) {	 // t2 is array
		t2.pointerDepth--;
		return t2;
	} else if (t2.IsArithmetic()) {	 // t1 is array
		t1.pointerDepth--;
		return t1;
	}
	throw SemanticException("Array access with invalid/missing index", token);
}

AST::EffectiveType AST::MemberAccess::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = object->CheckSemantics(context);
	if (t.structContext == nullptr) {
		throw SemanticException("Member access with non-struct type", token);
	} else if (accessKind->Kind == TokenKind::TK_ARROW && t.pointerDepth != 1) {
		throw SemanticException("Pointer Member access on member with 0 or more than 1 indirection", token);
	} else if (accessKind->Kind == TokenKind::TK_DOT && t.pointerDepth != 0) {
		throw SemanticException("Concrete Member access on pointer type", token);
	}
	return member.CheckSemantics(t.structContext);
}

AST::EffectiveType AST::UnaryOperation::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = operand->CheckSemantics(context);
	switch (op->Kind) {
	case TokenKind::TK_SIZEOF:
		if (t.parameters.size() > 0) {
			throw SemanticException("Sizeof on function type", token);
		}
		return SizeOfResult;
	case TokenKind::TK_ASTERISK:
		if (t.pointerDepth == 0) {
			throw SemanticException("Dereference on non-pointer type", token);
		}
		t.pointerDepth--;
		t.context = EffectiveType::TypeContext::kNone;
		return t;
	case TokenKind::TK_AND:
		if (t.context == EffectiveType::TypeContext::kRValue) {
			throw SemanticException("Address of RValue", token);
		}
		t.pointerDepth++;
		t.context = EffectiveType::TypeContext::kRValue;
		return t;
	case TokenKind::TK_MINUS:
	case TokenKind::TK_PLUS:
		if (!t.IsArithmetic()) {
			throw SemanticException("Unary plus/minus operation on non-integer type", token);
		}
		return t;
	case TokenKind::TK_EXCLAMATION:
		if (!t.IsScalar()) {
			throw SemanticException("Logical not on non-integer type", token);
		}
		return t;
	default:
		throw SemanticException("Unknown unary operation" + token->Text, token);
	}
}

AST::EffectiveType AST::CastExpression::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = castType->CheckSemantics(context);
	auto t2 = castExpression->CheckSemantics(context);
	if (!t.rawType || (t.rawType->Is(TokenKind::TK_VOID) && t.pointerDepth == 0)) {
		throw SemanticException("Invalid cast to void", token);
	} else if (!t2.rawType || (t2.rawType->Is(TokenKind::TK_VOID) && t2.pointerDepth == 0)) {
		throw SemanticException("Invalid cast from void", token);
	}
	// else if (t.pointerDepth == 0 && t2.pointerDepth == 0 && !t.CanAssign(t2)) {
	// 	throw SemanticException("Invalid cast from " + t2.rawType->Text + " to " + t.rawType->Text, token);
	// }
	return t;
}

AST::EffectiveType AST::BinaryOperation::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t1 = lhs->CheckSemantics(context);
	auto t2 = rhs->CheckSemantics(context);
	auto t1_arithmetic = t1.IsArithmetic();
	auto t2_arithmetic = t2.IsArithmetic();
	switch (op->Kind) {
	case TokenKind::TK_MOD:
	case TokenKind::TK_ASTERISK:
	case TokenKind::TK_DIV:
		if (!t1_arithmetic || !t1_arithmetic) {
			throw SemanticException("Multiplicative operation on non-integer type", token);
		}
		break;
	case TokenKind::TK_MINUS:
		if (t1_arithmetic && t2_arithmetic) {
			break;
		} else if (t1.pointerDepth > 0 && t2_arithmetic) {
			return t1;
		} else if (t1 == t2) {
			break;
		}
		throw SemanticException("Subtraction operation on incompatible types", token);
	case TokenKind::TK_PLUS:
		if (t1_arithmetic && t2_arithmetic) {
			break;
		} else if (t1.pointerDepth > 0 && t2_arithmetic) {
			return t1;
		} else if (t2.pointerDepth > 0 && t1_arithmetic) {
			return t2;
		}
		throw SemanticException("Additive operation on incompatible types", token);
	case TokenKind::TK_LSHIFT:
	case TokenKind::TK_RSHIFT:
		if (!t1_arithmetic || !t2_arithmetic) {
			throw SemanticException("Shift operation on non-integer type", token);
		}
		break;
	case TokenKind::TK_LT:
	case TokenKind::TK_GT:
	case TokenKind::TK_LE:
	case TokenKind::TK_GE:
		if ((t1.pointerDepth == t2.pointerDepth && *t1.rawType == *t2.rawType) || (t1_arithmetic && t2_arithmetic)) {
			break;
		}
		throw SemanticException("Comparison operation on incompatible types", token);
	case TokenKind::TK_EQ:
	case TokenKind::TK_NE:
		if ((t1.pointerDepth == t2.pointerDepth && *t1.rawType == *t2.rawType) || (t1_arithmetic && t2_arithmetic)) {
			break;
		} else if ((t1.pointerDepth > 0 && (t2.IsNull() || (t2.pointerDepth == 1 && t2.rawType->Is(TokenKind::TK_VOID)))) ||
							 (t2.pointerDepth > 0 && (t1.IsNull() || (t1.pointerDepth == 1 && t1.rawType->Is(TokenKind::TK_VOID))))) {
			break;
		}
		throw SemanticException("Equality operation on incompatible types", token);
	case TokenKind::TK_LOGICAL_AND:
	case TokenKind::TK_LOGICAL_OR:
		if ((t1_arithmetic || t1.pointerDepth > 0) && (t2_arithmetic || t2.pointerDepth > 0)) {
			break;
		}
		throw SemanticException("Logical operation on non-integer type", token);
	default:
		throw SemanticException("Unknown binary operation" + op->Text, token);
	}
	return { EffectiveType::TypeContext::kRValue, &OpResult };
}

AST::EffectiveType AST::ConditionalExpression::CheckSemantics(std::shared_ptr<Context> context)
{
	auto tCon = condition->CheckSemantics(context);
	if (!tCon.IsArithmetic()) {
		throw SemanticException("Conditional expression with non-integer condition", token);
	}
	auto t1 = middle->CheckSemantics(context);
	auto t2 = rhs->CheckSemantics(context);
	if (t1.IsArithmetic() && t2.IsArithmetic()) {
		return t1;
	} else if (t1.pointerDepth == t2.pointerDepth && *t1.rawType == *t2.rawType) {
		return t1;
	} else if (t1.pointerDepth > 0 && t2.IsNull()) {
		return t1;
	} else if (t2.pointerDepth > 0 && t1.IsNull()) {
		return t2;
	} else if (t1.rawType->Is(TokenKind::TK_VOID) && t2.rawType->Is(TokenKind::TK_VOID)) {
		return t1;
	}
	throw SemanticException("Conditional expression with incompatible types", token);
}

AST::EffectiveType AST::Assignment::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t1 = lhs->CheckSemantics(context);
	auto t2 = rhs->CheckSemantics(context);
	if (t1.CanAssign(t2)) {
		return t1;
	} else {
		throw SemanticException("Assignment with incompatible types", token);
	}
}

AST::EffectiveType AST::CommaExpression::CheckSemantics(std::shared_ptr<Context> context)
{
	EffectiveType lastType;
	for (const auto& expr : expressions) {
		lastType = expr->CheckSemantics(context);
	}
	return lastType;
}

AST::EffectiveType AST::TypeSpecifier::CheckSemantics(std::shared_ptr<Context>)
{
	return EffectiveType{ EffectiveType::TypeContext::kNone, token };
}

AST::EffectiveType AST::StructSpecifier::CheckSemantics(std::shared_ptr<Context> context)
{
	auto retVal = EffectiveType{ EffectiveType::TypeContext::kNone, identifier };
	const auto& structName = identifier->Text;
	auto ctx = context;
	auto t = ctx->structs.find(structName);
	while (t == ctx->structs.end()) {
		ctx = ctx->parent;
		if (!ctx) {
			return retVal;
		}
		t = ctx->structs.find(structName);
	}
	retVal.structContext = t->second->structContext;
	retVal.hasBody = t->second->hasBody;
	return retVal;
}

AST::EffectiveType AST::StructSpecifierImpl::CheckSemantics(std::shared_ptr<Context> context)
{
	if (!identifier) {	// anonymous struct
		auto structBodyType = EffectiveType{ EffectiveType::TypeContext::kNone, identifier };
		auto bodyCtx = structBodyType.structContext = std::make_shared<Context>(context, Context::ScopeContext::kStruct);
		structDeclarationList->CheckSemantics(bodyCtx);
		structBodyType.hasBody = true;
		return structBodyType;
	}
	auto ctx = context;
	if (ctx->structs.count(identifier->Text) > 0)
		throw SemanticException("Struct body already defined", token);
	auto structBodyType = std::make_shared<EffectiveType>(EffectiveType::TypeContext::kNone, identifier);
	do {
		if (ctx->structs.count(identifier->Text) > 0)
			break;
		ctx->structs[identifier->Text] = structBodyType;
	} while ((ctx = ctx->parent));
	auto& bodyCtx = structBodyType->structContext = std::make_shared<Context>(context, Context::ScopeContext::kStruct);
	structDeclarationList->CheckSemantics(bodyCtx);
	structBodyType->hasBody = true;
	return StructSpecifier::CheckSemantics(context);
}

void AST::DirectDeclarator::CheckSemanticsT(std::shared_ptr<Context> context, EffectiveType*& type, const Token* hasEmptyParams)
{
	// NOTE: Directd eclarator ALWAYS terminates with an Identifier or another declarator
	auto it = this;
	do {
		if (!it->declarator)
			return;
		switch (it->declarator->Type()) {
		case NodeType::kIdentifier:
			{
				auto& varName = it->declarator->token->Text;
				if (context->variables.count(varName) > 0) {
					throw SemanticException("Variable already declared: " + varName, it->token);
				} else if (hasEmptyParams) {
					throw SemanticException("Function body with missing parameter name", hasEmptyParams);
				}
				type = (context->variables[varName] = std::make_shared<EffectiveType>(*type)).get();
			}
			return;	 // ---- RETURN ----
		case NodeType::kPointerDeclarator:
			return static_cast<PointerDeclarator*>(it->declarator.get())->CheckSemanticsT(context, type, hasEmptyParams);
		case NodeType::kParameterDeclarationList:
			{
				// Function Declarator, which means the type we gathered thus far is the return type
				// shift previous type info into return type data and gather parameter info
				type->returnType = std::make_shared<EffectiveType>(*type);
				type->pointerDepth = 0;
				type->parameters.clear();
				type->structContext = nullptr;
				hasEmptyParams = nullptr;
				auto parameterScope = std::make_shared<Context>(context, Context::ScopeContext::kFunction);
				for (const auto& param : static_cast<ParameterTypeList*>(declarator.get())->parameters) {
					const auto declCount = parameterScope->variables.size();
					auto paramType = param->CheckSemantics(parameterScope);
					if (paramType.rawType->Is(TokenKind::TK_VOID) && paramType.pointerDepth == 0) {
						if (type->parameters.size() > 0) {
							throw SemanticException("Function with void parameter after non-void parameter", it->token);
						}
						break;
					}
					if (type->hasBody && parameterScope->variables.size() == declCount && paramType.returnType == nullptr)
						hasEmptyParams = it->token;
					type->parameters.push_back(paramType);
				}
				if (type->hasBody && !hasEmptyParams)
					for (auto&& [name, var] : parameterScope->variables) {
						context->variables[PARAMETER_PREFIX + name] = var;
				}
			}
			break;
		default:	// Array
			if (it->Type() != NodeType::kArrayDeclarator)
				throw std::runtime_error("Unknown declarator type: " + it->token->Text);
			type->pointerDepth++;
			if (auto& nested = it->declarator; nested && nested->IsExpression()) {
				if (static_cast<int>(declarator->Type()) > static_cast<int>(NodeType::kAssignment)) {
					if (!token) {
						throw std::runtime_error("Array declarator with invalid type: nullptr");
					}
					throw SemanticException("Array declarator with invalid type", it->token);
				}
				const auto idxType = declarator->CheckSemantics(context);
				if (!idxType.IsArithmetic()) {
					if (!token) {
						throw std::runtime_error("Array declarator with non-integer index: nullptr");
					}
					throw SemanticException("Array declarator with non-integer index", it->token);
				}
			}
			break;
		}
	} while ((it = it->previous.get()));
}

void AST::PointerDeclarator::CheckSemanticsT(std::shared_ptr<Context> context, EffectiveType*& type, const Token* hasEmptyParams)
{
	type->pointerDepth += pointerDepth;
	if (declarator) {
		declarator->CheckSemanticsT(context, type, hasEmptyParams);
	}
}

void AST::InitDeclarator::CheckSemanticsT(std::shared_ptr<Context> context, EffectiveType*& type, const Token* hasEmptyParams)
{
	PointerDeclarator::CheckSemanticsT(context, type, hasEmptyParams);
	auto t2 = initializer->CheckSemantics(context);
	if (type->CanAssign(t2)) {
		return;
	} else if (token) {
		throw SemanticException("InitDeclarator with incompatible types: " + token->Text, token);
	} else {
		throw std::runtime_error("InitDeclarator with incompatible types: nullptr");
	}
}

AST::EffectiveType AST::TypeName::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = declSpecifiers->CheckSemantics(context);
	if (abstractDeclarator) {
		auto typePtr = &t;
		abstractDeclarator->CheckSemanticsT(context, typePtr);
	}
	return t;
}

AST::EffectiveType AST::Declaration::CheckSemantics(std::shared_ptr<Context> context)
{
	const auto declCount = context->variables.size();
	auto type = declSpecifiers->CheckSemantics(context);
	auto typePtr = &type;
	if (initDeclarator)
		initDeclarator->CheckSemanticsT(context, typePtr);
	if (type.rawType && type.rawType->IsNot(TokenKind::TK_IDENTIFIER)) {
		if (context->variables.size() == declCount)	// e.g. int;
			throw SemanticException("Declaration with no declarators", token);
	} else {	// e.g. struct S instance;
		if (context->variables.size() != declCount && !type.hasBody && type.pointerDepth == 0) {
			throw SemanticException("Declaration with incomplete struct type", token);
		}
	}
	return type;
}

AST::EffectiveType AST::StructMemberList::CheckSemantics(std::shared_ptr<Context> context)
{
	EffectiveType lastType;
	for (const auto& decl : declarations) {
		lastType = decl->CheckSemantics(context);
	}
	return lastType;
}

AST::EffectiveType AST::ParameterDeclaration::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = declSpecifiers->CheckSemantics(context);
	if (declarator) {
		auto tPtr = &t;
		declarator->CheckSemanticsT(context, tPtr);
	}
	return t;
}

AST::EffectiveType AST::ParameterTypeList::CheckSemantics(std::shared_ptr<Context> context)
{
	EffectiveType lastType;
	for (const auto& param : parameters) {
		lastType = param->CheckSemantics(context);
	}
	return lastType;
}

AST::EffectiveType AST::DesginationList::CheckSemantics(std::shared_ptr<Context> context)
{
	EffectiveType lastType;
	for (const auto& expr : expression) {
		lastType = expr->CheckSemantics(context);
	}
	return lastType;
}

AST::EffectiveType AST::LabeledStatement::CheckSemantics(std::shared_ptr<Context> context)
{
	auto topLvContext = context->GetFunctionLevelContext();
	if (!topLvContext) {
		throw SemanticException("Label outside of function, unable to find function context", token);
	} else if (!topLvContext->returnType) {
		throw SemanticException("Label outside of function, missing Return Type", token);
	} else if (context->variables.count(token->Text) > 0) {
		throw SemanticException("Variable already declared: " + token->Text, token);
	} else if (topLvContext->labels.count(token->Text) > 0) {
		throw SemanticException("Label already declared: " + token->Text, token);
	}
	auto& label = context->variables[token->Text] = std::make_shared<EffectiveType>(EffectiveType::TypeContext::kLabel, token);
	auto hasInit = context->variables.size() == (topLvContext->returnType->parameters.size() + 1);
	topLvContext->labels.emplace(token->Text, Context::Label(label, context, hasInit));
	return statement->CheckSemantics(context);
}

AST::EffectiveType AST::CompoundStatement::CheckSemantics(std::shared_ptr<Context> context)
{
	if (context->scopeContext == Context::ScopeContext::kNone || context->scopeContext == Context::ScopeContext::kStruct) {
		throw SemanticException("Compound statement outside of function", token);
	}
	auto ctx = std::make_shared<Context>(context, context->scopeContext);
	for (const auto& stmt : statements) {
		stmt->CheckSemantics(ctx);
	}
	return statementRetVal;
}

AST::EffectiveType AST::ExpressionStatement::CheckSemantics(std::shared_ptr<Context> context)
{
	if (expression) {
		return expression->CheckSemantics(context);
	}
	return statementRetVal;
}

AST::EffectiveType AST::SelectionStatement::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = condition->CheckSemantics(context);
	if (!t.IsScalar()) {
		throw SemanticException("Selection statement with non-integer condition", token);
	}
	// auto if_ctx = std::make_shared<Context>(context, context->scopeContext);
	ifStatement->CheckSemantics(context);
	if (elseStatement) {
		// auto else_ctx = std::make_shared<Context>(context, context->scopeContext);
		elseStatement->CheckSemantics(context);
	}
	return statementRetVal;
}

AST::EffectiveType AST::IterationStatement::CheckSemantics(std::shared_ptr<Context> context)
{
	auto t = condition->CheckSemantics(context);
	if (!t.IsScalar()) {
		throw SemanticException("Iteration statement with non-integer condition", token);
	}
	auto ctx = std::make_shared<Context>(context, Context::ScopeContext::kLoop);
	body->CheckSemantics(ctx);
	return statementRetVal;
}

AST::EffectiveType AST::JumpStatement::CheckSemantics(std::shared_ptr<Context> context)
{
	switch (jumpKind->Kind) {
	case TokenKind::TK_RETURN:
		if (!context->returnType) {
			throw SemanticException("Return statement with different type", token);
		} else if (expression) {
			auto& retT = context->returnType;
			auto t = expression->CheckSemantics(context);
			if (!retT->CanAssign(t)) {
				const auto err = "Return statement with incompatible type: ";
				throw SemanticException(err, token);
			}
		} else {
			if (context->returnType->rawType->IsNot(TokenKind::TK_VOID)) {
				throw SemanticException("Return statement with missing return value", token);
			}
		}
		break;
	case TokenKind::TK_GOTO:
		{
			auto toplv = context->GetFunctionLevelContext();
			if (!toplv) {
				throw SemanticException("Goto statement outside of function", token);
			} else if (!expression) {
				throw SemanticException("Goto statement without label", token);
			}
			toplv->gotoExpressions.push_back(expression);
		}
		break;
	case TokenKind::TK_BREAK:
	case TokenKind::TK_CONTINUE:
		if (context->scopeContext != Context::ScopeContext::kLoop) {
			throw SemanticException("Break/Continue statement outside of loop", token);
		}
		break;
	default:
		throw SemanticException("Unknown jump statement: " + token->Text, token);
	}
	return statementRetVal;
}

AST::EffectiveType AST::TranslationUnit::CheckSemantics(std::shared_ptr<Context> context)
{
	for (const auto& decl : externalDeclarations)
		decl->CheckSemantics(context);
	return retValType;
}

AST::EffectiveType AST::ExternalDeclaration::CheckSemantics(std::shared_ptr<Context> context)
{
	return Declaration::CheckSemantics(context);
}

AST::EffectiveType AST::FunctionDefinition::CheckSemantics(std::shared_ptr<Context> context)
{
	auto parameterCtx = std::make_shared<Context>(context, Context::ScopeContext::kFunction);
	auto funcType = declSpecifiers->CheckSemantics(parameterCtx);
	funcType.hasBody = true;
	// This pointer will change and no longer reference ^ after semantic check. I.e. pointed to object != funcType
	auto funcPtr = &funcType;
	declarator->CheckSemanticsT(parameterCtx, funcPtr);
	auto varTable = parameterCtx->variables;
	for (auto&& [name, var] : parameterCtx->variables) {
		if (var.get() != funcPtr) {	 // Parameter
			varTable[name.substr(1)] = var;
			varTable.erase(name);
		} else {	// Search, validate func type & copy to parent scope
			auto old = context->variables.find(name);
			if (old != context->variables.end()) {
				if (old->second->returnType == nullptr) {
					throw SemanticException("Variable redlaraction as function type: " + name, token);
				} else if (old->second->hasBody) {
					throw SemanticException("Function already declared: " + name, token);
				} else if (old->second->parameters != funcPtr->parameters) {
					throw SemanticException("Function redeclared with different parameters", token);
				}
				old->second = var;
			} else {
				context->variables[name] = var;
			}
		}
	}
	parameterCtx->variables = varTable;
	parameterCtx->returnType = funcPtr->returnType;
	body->CheckSemantics(parameterCtx);
	for (auto&& gotoExpr : parameterCtx->gotoExpressions) {
		// NOTE: currently not checking if a jump skips initialization of a variable
		if (gotoExpr->Type() != NodeType::kIdentifier) {
			throw SemanticException("Goto statement with non-label", gotoExpr->token);
		}
		auto str = gotoExpr->token->Text;
		auto label = parameterCtx->labels.find(str);
		if (label == parameterCtx->labels.end()) {
			throw SemanticException("Goto statement to undeclared label", gotoExpr->token);
		}
	}
	return funcType;
}

// ---------------------------------------------------------------------------
// LLVM Generation

llvm::Value* CreateTypeCast(llvm::Type* lhsTy, llvm::Value* rhs, AST::Context2& ctx) {
	if (lhsTy == rhs->getType()) {
		return rhs;
	}
	if (lhsTy->isPointerTy() && rhs->getType()->isIntegerTy()) {
		rhs = ctx.builder.CreateIntToPtr(rhs, lhsTy);
	} else if (lhsTy->isIntegerTy() && rhs->getType()->isPointerTy()) {
		rhs = ctx.builder.CreatePtrToInt(rhs, lhsTy);
	} else if (lhsTy->isIntegerTy() && rhs->getType()->isIntegerTy()) {
		bool rhsBool = rhs->getType()->isIntegerTy(1);
		rhs = ctx.builder.CreateIntCast(rhs, lhsTy, !rhsBool);
	} else {
		rhs = ctx.builder.CreateBitCast(rhs, lhsTy);
	}
	return rhs;
}

char GetEscapeSequence(char c)
{
	switch (c) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'r':
		return '\r';
	case '\\':
		return '\\';
	case '\"':
		return '\"';
	case '\'':
		return '\'';
	case '?':
		return '\?';
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'v':
		return '\v';
	case '0':
		return '\0';
	default:
		return c;
	}
}

AST::Context2::ValueData AST::Constant::GenerateCode(llvm::IRBuilder<>& builder, Context2&) const
{
	if (token->Is(TokenKind::TK_NUMBER)) {
		const auto value = std::stoi(token->Text);
		return builder.getInt32(value);
	} else if (token->Is(TokenKind::TK_CHARACTER)) {
		const auto c = token->Text[1];
		if (c == '\\') {
			const auto esc = GetEscapeSequence(token->Text[2]);
			return builder.getInt8(esc);
		}
		return builder.getInt8(c);
	} else {
		throw std::runtime_error("Unknown constant type: " + token->Text);
	}
}

AST::Context2::ValueData AST::StringLiteral::GenerateCode(llvm::IRBuilder<>& builder, Context2&) const
{
	auto str = token->Text.substr(1, token->Text.size() - 2);
	std::string unescapedStr;
	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] == '\\' && i + 1 < str.size()) {
			auto c = GetEscapeSequence(str[i + 1]);
			unescapedStr += c;
			i++;
		} else {
			unescapedStr += str[i];
		}
	}
	str = unescapedStr;
	return builder.CreateGlobalStringPtr(str);
}

AST::Context2::ValueData AST::Identifier::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	const auto identifier = token->Text;
	auto ctxPtr = &ctx;
	auto t = ctxPtr->variables.find(identifier);
	while (t == ctxPtr->variables.end()) {
		ctxPtr = ctxPtr->parent;
		if (!ctxPtr) {
			throw std::runtime_error("Undeclared variable: " + identifier);
		}
		t = ctxPtr->variables.find(identifier);
	}
	return t->second;
}

AST::Context2::ValueData AST::FunctionCall::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto funcData = function->GenerateCode(builder, ctx);
	auto func = funcData.TryCreateLoad(builder);
	auto funcVal = llvm::dyn_cast<llvm::Function>(func);
	auto funcTy = funcVal ? funcVal->getFunctionType() : llvm::dyn_cast<llvm::FunctionType>(funcData.GetUnderlyingType());
	std::vector<llvm::Value*> args;
	for (size_t i = 0; i < arguments.size(); i++) {
		const auto funcArgTy = funcTy->getParamType(i);
		const auto& arg = arguments[i];
		auto valArgData = arg->GenerateCode(builder, ctx);
		auto valArg = valArgData.GetValue();
		if (arg->NeedsExplicitRValueLoad()) {
			valArg = valArgData.TryCreateLoad(builder);
		}
		valArg = CreateTypeCast(funcArgTy, valArg, ctx);
		args.push_back(valArg);
	}
	llvm::Value* funcRet = funcVal ? builder.CreateCall(funcVal, args) : builder.CreateCall(funcTy, func, args);
	// if calling from function pointer, discard pointer-to-function & function type (leaving func return type)
	size_t discardTypes = funcVal ? 0 : 2;
	return { funcRet, funcData, discardTypes };
}

AST::Context2::ValueData AST::ArrayAccess::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto fstData = array->GenerateCode(builder, ctx);
	auto sndData = index->GenerateCode(builder, ctx);
	fstData.TryCreateLoad(builder);
	sndData.TryCreateLoad(builder);
	auto [arrData, idxData] = fstData.GetType()->isIntegerTy() ? std::make_pair(sndData, fstData) : std::make_pair(fstData, sndData);
	auto idx = builder.CreateSExtOrTrunc(idxData.GetValue(), builder.getInt64Ty());
	auto arr = arrData.GetValue();
	if (!arr->getType()->isPointerTy()) {
		llvm::AllocaInst* alloca = ctx.allocaBuilder.CreateAlloca(arr->getType());
		builder.CreateStore(arr, alloca);
		arr = alloca;	 // Now `arr` is a pointer
	}
	std::vector<llvm::Value*> indices{ idx };
	auto arrTy = arrData.GetUnderlyingType();
	auto elemPtr = builder.CreateGEP(arrTy, arr, indices);
	arrData.ReplaceValue(elemPtr);	// Replace ptr to idx 0 with ptr to idx i
	return arrData;
}

AST::Context2::ValueData AST::MemberAccess::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto objData = object->GenerateCode(builder, ctx);
	auto structValue = objData.GetValue();
	if (accessKind->Kind == TokenKind::TK_ARROW) {
		structValue = objData.TryCreateLoad(builder);
	}
	auto structType = structValue->getType()->isStructTy() ? structValue->getType() : objData.GetUnderlyingType();
	if (!structValue->getType()->isPointerTy()) {
		auto& allocaBuilder = ctx.allocaBuilder;
		allocaBuilder.SetInsertPoint(allocaBuilder.GetInsertBlock(), allocaBuilder.GetInsertBlock()->begin());
		auto alloca = allocaBuilder.CreateAlloca(structValue->getType());
		builder.CreateStore(structValue, alloca);
		structValue = alloca;
	}
	auto structName = structType->getStructName().str();
	auto ctxPtr = &ctx;
	auto t = ctxPtr->structs.find(structName);
	while (t == ctxPtr->structs.end()) {
		ctxPtr = ctxPtr->parent;
		if (!ctxPtr) {
			throw std::runtime_error("CodeGen: Undeclared struct: " + structName);
		}
		t = ctxPtr->structs.find(structName);
	}
	const auto& structObj = t->second;
	const auto accessWhere = std::find_if(structObj->memberNames.begin(), structObj->memberNames.end(),
			[&](const auto& it) { return it.first == member.token->Text; });
	const auto accessIdx = std::distance(structObj->memberNames.begin(), accessWhere);
	std::vector<llvm::Value*> elementIdx{
		builder.getInt32(0),
		builder.getInt32(accessIdx)
	};
	llvm::Value* gep = builder.CreateInBoundsGEP(structType, structValue, elementIdx);
	return { gep, accessWhere->second };	// TODO: This here throws assertions
}

AST::Context2::ValueData AST::UnaryOperation::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	if (op->Is(TokenKind::TK_SIZEOF)) {
		if (operand->IsExpression()) {
			auto exprData = static_cast<Expression*>(operand.get())->GenerateCode(builder, ctx);
			auto exprType = operand->NeedsExplicitRValueLoad() ? exprData.GetUnderlyingType() : exprData.GetType();
			auto exprSize = ctx.module.getDataLayout().getTypeAllocSize(exprType);
			return builder.getInt32(exprSize);
		} else {
			auto castType = static_cast<TypeName*>(operand.get())->GenerateType(builder, ctx);
			auto primaryType = castType.back();
			if (primaryType->isPointerTy()) {
				auto pointerSize = builder.getInt32(sizeof(void*));
				return pointerSize;
			} else if (primaryType->isStructTy()) {
				auto allocSize = ctx.module.getDataLayout().getTypeAllocSize(primaryType);
				auto structSize = builder.getInt32(allocSize);
				return structSize;
			} else {
				auto primarySize = primaryType->getPrimitiveSizeInBits() / 8;
				return builder.getInt32(primarySize);
			}
		}
	}
	auto expr = static_cast<Expression*>(operand.get());
	auto valData = expr->GenerateCode(builder, ctx);
	if (op->Is(TokenKind::TK_AND)) {
		return valData;	// Return without loading the underlying data
	}
	auto underlyingVal = operand->NeedsExplicitRValueLoad() ? valData.TryCreateLoad(builder) : valData.GetValue();
	switch (op->Kind) {
	case TokenKind::TK_ASTERISK:
		valData.TryCreateLoad(builder);	// Advance data by 1 after load
		break;
	case TokenKind::TK_PLUS:
		break;	// Do nothing
	case TokenKind::TK_MINUS:
		// Negate the loaded value, then replace it
		underlyingVal = builder.CreateNeg(underlyingVal);
		valData.ReplaceValue(underlyingVal);
		break;
	case TokenKind::TK_EXCLAMATION:
		// Discard active value create comparison
		return builder.CreateICmpEQ(
				underlyingVal,
				llvm::Constant::getNullValue(underlyingVal->getType()));
	default:
		throw std::runtime_error("Unknown unary operation: " + this->op->Text);
	}
	return valData;
}

AST::Context2::ValueData AST::CastExpression::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto valData = castExpression->GenerateCode(builder, ctx);
	auto castTypes = castType->GenerateType(builder, ctx);
	return { CreateTypeCast(castTypes.back(), valData.GetValue(), ctx), castTypes };
}

AST::Context2::ValueData AST::BinaryOperation::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto lhsValData = lhs->GenerateCode(builder, ctx);
	auto rhsValData = rhs->GenerateCode(builder, ctx);
	auto lhsVal = lhs->NeedsExplicitRValueLoad() ? lhsValData.TryCreateLoad(builder) : lhsValData.GetValue();
	auto rhsVal = rhs->NeedsExplicitRValueLoad() ? rhsValData.TryCreateLoad(builder) : rhsValData.GetValue();
	const auto validateTypes = [&]() {
		if (lhsVal->getType()->isPointerTy() && rhsVal->getType()->isIntegerTy() && llvm::isa<llvm::ConstantInt>(rhsVal)) {
			rhsVal = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(lhsVal->getType()));
		} else if (rhsVal->getType()->isPointerTy() && lhsVal->getType()->isIntegerTy() && llvm::isa<llvm::ConstantInt>(lhsVal)) {
			lhsVal = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(rhsVal->getType()));
		} else if (lhsVal->getType() != rhsVal->getType()) {
			auto int32 = builder.getInt32Ty();
			lhsVal = CreateTypeCast(int32, lhsVal, ctx);
			rhsVal = CreateTypeCast(int32, rhsVal, ctx);
		}
	};
	switch (op->Kind) {
	case TokenKind::TK_MOD:
		validateTypes();
		return builder.CreateSRem(lhsVal, rhsVal);
	case TokenKind::TK_ASTERISK:
		validateTypes();
		return builder.CreateMul(lhsVal, rhsVal);
	case TokenKind::TK_DIV:
		validateTypes();
		return builder.CreateSDiv(lhsVal, rhsVal);
	case TokenKind::TK_MINUS:
		if (lhsVal->getType()->isPointerTy()) {
			auto elementType = lhsValData.GetUnderlyingType();
			if (rhsVal->getType()->isPointerTy()) {	 // Subtract pointers
				return builder.CreatePtrDiff(elementType, lhsVal, rhsVal);
			} else if (rhsVal->getType()->isIntegerTy()) {	// Pointer - integer
				auto repl = builder.CreateInBoundsGEP(elementType, lhsVal, rhsVal);
				lhsValData.ReplaceValue(repl);
				return lhsValData;
			} else {
				throw std::runtime_error("Pointer subtraction with invalid type");
			}
		} else {
			validateTypes();
			return builder.CreateSub(lhsVal, rhsVal);
		}
	case TokenKind::TK_PLUS:
		if (lhsVal->getType()->isPointerTy() || rhsVal->getType()->isPointerTy()) {
			auto [elementData, elementType] = lhsVal->getType()->isPointerTy() ? 
				std::make_pair(lhsValData, lhsValData.GetUnderlyingType()) :
				std::make_pair(rhsValData, rhsValData.GetUnderlyingType());
			auto repl = builder.CreateInBoundsGEP(elementType, lhsVal, rhsVal);
			elementData.ReplaceValue(repl);
			return elementData;
		} else {
			validateTypes();
			return builder.CreateAdd(lhsVal, rhsVal);
		}
	case TokenKind::TK_LSHIFT:
		return builder.CreateShl(lhsVal, rhsVal);
	case TokenKind::TK_RSHIFT:
		return builder.CreateAShr(lhsVal, rhsVal);
	case TokenKind::TK_LT:
		validateTypes();
		return builder.CreateICmpSLT(lhsVal, rhsVal);
	case TokenKind::TK_GT:
		validateTypes();
		return builder.CreateICmpSGT(lhsVal, rhsVal);
	case TokenKind::TK_LE:
		validateTypes();
		return builder.CreateICmpSLE(lhsVal, rhsVal);
	case TokenKind::TK_GE:
		validateTypes();
		return builder.CreateICmpSGE(lhsVal, rhsVal);
	case TokenKind::TK_EQ:
		validateTypes();
		return builder.CreateICmpEQ(lhsVal, rhsVal);
	case TokenKind::TK_NE:
		validateTypes();
		return builder.CreateICmpNE(lhsVal, rhsVal);
	case TokenKind::TK_LOGICAL_AND:
		{
			auto lhsTy = lhsVal->getType(), rhsTy = rhsVal->getType();
			auto lhsBool = lhsTy->isIntegerTy(1) ? lhsVal : builder.CreateICmpNE(lhsVal, llvm::Constant::getNullValue(lhsTy));
			auto rhsBool = rhsTy->isIntegerTy(1) ? rhsVal : builder.CreateICmpNE(rhsVal, llvm::Constant::getNullValue(rhsTy));
			return builder.CreateAnd(lhsBool, rhsBool);
		}
	case TokenKind::TK_LOGICAL_OR:
		{
			auto lhsTy = lhsVal->getType(), rhsTy = rhsVal->getType();
			auto lhsBool = lhsTy->isIntegerTy(1) ? lhsVal : builder.CreateICmpNE(lhsVal, llvm::Constant::getNullValue(lhsTy));
			auto rhsBool = rhsTy->isIntegerTy(1) ? rhsVal : builder.CreateICmpNE(rhsVal, llvm::Constant::getNullValue(rhsTy));
			return builder.CreateOr(lhsBool, rhsBool);
		}
	default:
		throw std::runtime_error("Unknown binary operation: " + op->Text);
	}
}

AST::Context2::ValueData AST::ConditionalExpression::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto condValData = condition->GenerateCode(builder, ctx);
	auto condVal = condition->NeedsExplicitRValueLoad() ? condValData.TryCreateLoad(builder) : condValData.GetValue();
	if (!condVal->getType()->isIntegerTy(1)) {
		condVal = builder.CreateICmpNE(condVal, llvm::Constant::getNullValue(condVal->getType()));
	}
	auto thenValData = middle->GenerateCode(builder, ctx);
	auto elseValData = rhs->GenerateCode(builder, ctx);
	auto thenVal = middle->NeedsExplicitRValueLoad() ? thenValData.TryCreateLoad(builder) : thenValData.GetValue();
	auto elseVal = rhs->NeedsExplicitRValueLoad() ? elseValData.TryCreateLoad(builder) : elseValData.GetValue();
	auto retVal = builder.CreateSelect(condVal, thenVal, elseVal);
	return { retVal, thenValData, 0 };	// typedata for then & else are the same
}

AST::Context2::ValueData AST::Assignment::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto lhsValData = lhs->GenerateCode(builder, ctx);
	auto rhsValData = rhs->GenerateCode(builder, ctx);
	auto rhsVal = rhs->NeedsExplicitRValueLoad() ? rhsValData.TryCreateLoad(builder) : rhsValData.GetValue();
	auto lhsVal = lhsValData.GetValue();
	auto lhsTy = lhsValData.GetUnderlyingType();
	rhsVal = CreateTypeCast(lhsTy, rhsVal, ctx);
	builder.CreateStore(rhsVal, lhsVal);
	return lhsValData;
}

AST::Context2::ValueData AST::CommaExpression::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	for (size_t i = 0; i < expressions.size() - 1; ++i) {
		expressions[i]->GenerateCode(builder, ctx);
	}
	return expressions.back()->GenerateCode(builder, ctx);
}

llvm::Type* AST::TypeSpecifier::GenerateType(llvm::IRBuilder<>& builder, Context2&) const
{
	switch (token->Kind) {
	case TokenKind::TK_INT:
		return builder.getInt32Ty();
	case TokenKind::TK_CHAR:
		return builder.getInt8Ty();
	case TokenKind::TK_VOID:
		return builder.getVoidTy();
	default:
		throw std::runtime_error("Unknown type specifier: " + token->Text);
	}
}

llvm::Type* AST::StructSpecifier::GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	const auto& structName = identifier->Text;
	auto ctxPtr = &ctx;
	auto t = ctxPtr->structs.find(structName);
	while (t == ctxPtr->structs.end()) {
		ctxPtr = ctxPtr->parent;
		if (!ctxPtr) {
			throw std::runtime_error("CodeGen: Undeclared struct: " + structName);
		}
		t = ctxPtr->structs.find(structName);
	}
	return t->second->type;
}

llvm::Type* AST::StructSpecifierImpl::GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	const auto& structName = identifier->Text;
	auto structTypePtr = std::make_shared<Context2::StructObject>();
	auto& structCtx = structTypePtr->context = std::make_shared<Context2>(&ctx);
	auto ctxPtr = structCtx.get();
	do {
		if (ctxPtr->structs.count(structName) > 0)
			break;
		ctxPtr->structs.emplace(structName, structTypePtr);
	} while ((ctxPtr = ctxPtr->parent));
	auto& structBodyType = structTypePtr->type = llvm::StructType::create(builder.getContext(), structName);
	std::vector<llvm::Type*> StructMemberTypes{};
	for (auto&& it : structDeclarationList->declarations) {
		auto objData = it->GenerateDeclaration(builder, *structCtx.get());
		auto structTypes = objData.GetTypeIndirections();
		auto entryName = objData.GetValue()->getName();
		auto pos = entryName.find('.');
		if (pos != std::string::npos)
			entryName = entryName.substr(0, pos);
		auto entry = std::make_pair(entryName, structTypes);
		structTypePtr->memberNames.push_back(entry);
		StructMemberTypes.push_back(objData.GetUnderlyingType());
	}
	structBodyType->setBody(StructMemberTypes);
	return structTypePtr->type;
}

void AST::DirectDeclarator::GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type, std::vector<std::string>& paramNames,
	std::pair<std::string, std::vector<llvm::Type*>>* alt_out) const
{
	// NOTE: Directd eclarator ALWAYS terminates with an Identifier or another declarator
	auto it = this;
	do {
		if (!it->declarator)
			return;
		switch (it->declarator->Type()) {
		case NodeType::kIdentifier:
			if (alt_out) {
				alt_out->first = it->declarator->token->Text;
				alt_out->second = type.GetTypeIndirections();
			} else if (auto funcType = llvm::dyn_cast<llvm::FunctionType>(type.GetType())) {
				auto& name = it->declarator->token->Text;
				auto func = ctx.module.getFunction(name);
				if (!func)
					func = llvm::Function::Create(
							funcType /* FunctionType *Ty */,
							llvm::GlobalValue::ExternalLinkage /* LinkageType */,
							name /* const Twine &N="" */,
							&ctx.module /* Module *M=0 */);
				if (paramNames.size() == func->arg_size()) {
					llvm::Function::arg_iterator FuncFacArgIt = func->arg_begin();
					for (auto& val : paramNames) {
						FuncFacArgIt->setName(val);
						FuncFacArgIt++;
					}
				}
				type.ReplaceValue(func);
				// type.AddType(func->getType());	// TODO: <- Adding this doesnt change anything other than printing new error values
				ctx.variables[name] = type;
			} else if (ctx.function != nullptr) {
				auto& name = it->declarator->token->Text;
				auto& alloca = ctx.allocaBuilder;
				alloca.SetInsertPoint(alloca.GetInsertBlock(), alloca.GetInsertBlock()->begin());
				auto varType = type.GetType();
				llvm::Value* var = alloca.CreateAlloca(varType,
						varType->isStructTy() ? builder.getInt32(1) : nullptr,
						name);
				type.ReplaceValue(var);
				type.AddType(var->getType());
				ctx.variables[name] = type;
			} else {	// Global Scope
				auto& name = it->declarator->token->Text;
				auto globalVal = new llvm::GlobalVariable(
						ctx.module /* Module & */,
						type.GetType() /* Type * */,
						false /* bool isConstant */,
						llvm::GlobalValue::CommonLinkage /* LinkageType */,
						llvm::Constant::getNullValue(type.GetType()) /* Constant * Initializer */,
						name /* const Twine &Name = "" */);
				type.ReplaceValue(globalVal);
				type.AddType(globalVal->getType());
				ctx.variables[name] = type;
			}
			return;	 // ---- RETURN ----
		case NodeType::kPointerDeclarator:
			return static_cast<PointerDeclarator*>(it->declarator.get())->GenerateDeclarator(builder, ctx, type, paramNames, alt_out);
		case NodeType::kParameterDeclarationList:
			{
				paramNames.clear();
				auto paramList = static_cast<ParameterTypeList*>(it->declarator.get());
				std::vector<llvm::Type*> paramTypes{};
				for (const auto& param : paramList->parameters) {
					const auto& [paramName, paramType] = param->GenerateDeclaration(builder, ctx);
					if (paramType.back()->isVoidTy())
						break;	// Singly void param: f(void)
					ctx.variables[paramName] = Context2::ValueData{ llvm::UndefValue::get(paramType.back()), paramType };
					paramNames.push_back(paramName);
					paramTypes.push_back(paramType.back());
				}
				auto funcType = llvm::FunctionType::get(type.GetType(), paramTypes, false);
				type.AddType(funcType);
			}
			break;
		default:
			if (it->Type() != NodeType::kArrayDeclarator)
				throw std::runtime_error("Unknown declarator type: " + it->token->Text);
			if (auto& nested = it->declarator) {
				if (nested && nested->IsExpression()) {
					auto idxValData = static_cast<Expression*>(declarator.get())->GenerateCode(builder, ctx);
					auto idxVal = idxValData.TryCreateLoad(builder);
					auto arraySize = llvm::dyn_cast<llvm::ConstantInt>(idxVal);
					if (idxVal->getType()->isIntegerTy() && arraySize) {
						auto arrType = llvm::ArrayType::get(type.GetType(), arraySize->getZExtValue());
						type.AddType(arrType);
						break;
					}
				}
			}
			type.AddType(llvm::PointerType::get(ctx.llvmContext, 0));
			break;
		}
	} while ((it = it->previous.get()));
}

void AST::PointerDeclarator::GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type,
		std::vector<std::string>& paramNames, std::pair<std::string, std::vector<llvm::Type*>>* alt_out) const
{
	for (size_t i = 0; i < pointerDepth; ++i) {
		type.AddType(llvm::PointerType::get(ctx.llvmContext, 0));
	}
	if (declarator) {
		declarator->GenerateDeclarator(builder, ctx, type, paramNames, alt_out);
	} else {
		if (alt_out) {
			alt_out->first = "";
			alt_out->second = type.GetTypeIndirections();
		}
	}
}

void AST::InitDeclarator::GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type,
		std::vector<std::string>& paramNames, std::pair<std::string, std::vector<llvm::Type*>>* alt_out) const
{
	PointerDeclarator::GenerateDeclarator(builder, ctx, type, paramNames, alt_out);
	if (initializer) {	// NOTE: This here would need extra boilerplate similar to Assignment Expression, but meh
		auto initValData = initializer->GenerateCode(builder, ctx);
		auto initVal = initializer->NeedsExplicitRValueLoad() ? initValData.TryCreateLoad(builder) : initValData.GetValue();
		builder.CreateStore(initVal, type.GetValue());
	}
}

std::vector<llvm::Type*> AST::TypeName::GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	auto baseType = declSpecifiers->GenerateType(builder, ctx);
	std::vector<llvm::Type*> retVal{ baseType };
	if (abstractDeclarator) {
		Context2::ValueData tmp;
		tmp.AddType(baseType);
		std::pair<std::string, std::vector<llvm::Type*>> out{};
		std::vector<std::string> tmp2;
		abstractDeclarator->GenerateDeclarator(builder, ctx, tmp, tmp2, &out);
		return out.second;
	}
	return retVal;
}

AST::Context2::ValueData AST::Declaration::GenerateDeclaration(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	Context2::ValueData retVal{};
	retVal.AddType(declSpecifiers->GenerateType(builder, ctx));
	if (initDeclarator) {
		initDeclarator->GenerateDeclarator(builder, ctx, retVal);
	} else {
		retVal.ReplaceValue(llvm::UndefValue::get(retVal.GetType()));
	}
	return retVal;
}

std::pair<std::string, std::vector<llvm::Type*>> AST::ParameterDeclaration::GenerateDeclaration(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	Context2::ValueData tmp{};
	tmp.AddType(declSpecifiers->GenerateType(builder, ctx));
	if (declarator) {
		std::pair<std::string, std::vector<llvm::Type*>> out;
		std::vector<std::string> names{};
		declarator->GenerateDeclarator(builder, ctx, tmp, names, &out);
		return out;
	} else {
		return { "", tmp.GetTypeIndirections() };
	}
}

llvm::BasicBlock* AST::LabeledStatement::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool) const
{
	auto& label = token->Text;
	auto where = ctx.labels->find(label);
	llvm::BasicBlock* labelBlock;
	if (where != ctx.labels->end()) {
		labelBlock = where->second;
	} else {
		labelBlock = llvm::BasicBlock::Create(builder.getContext(), label, ctx.function);
		ctx.labels->insert_or_assign(label, labelBlock);
	}
	builder.CreateBr(labelBlock);
	builder.SetInsertPoint(labelBlock);
	ctx.allocaBuilder.SetInsertPoint(labelBlock);
	statement->GenerateCode(builder, ctx);
	return labelBlock;
}

llvm::BasicBlock* AST::CompoundStatement::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const
{
	auto block = builder.GetInsertBlock();
	if (openBlock) {
		block = llvm::BasicBlock::Create(builder.getContext(), "entry", ctx.function);
		builder.SetInsertPoint(block);
		ctx.allocaBuilder.SetInsertPoint(block);
	}
	Context2 newCtx = Context2{ ctx };
	for (const auto& stmt : statements) {
		if (stmt->IsStatement()) {
			auto s = static_cast<Statement*>(stmt.get());
			s->GenerateCode(builder, newCtx);
		} else {
			auto d = static_cast<Declaration*>(stmt.get());
			d->GenerateDeclaration(builder, newCtx);
		}
	}
	return block;
}

llvm::BasicBlock* AST::ExpressionStatement::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool) const
{
	if (expression) {
		expression->GenerateCode(builder, ctx);
	}
	return nullptr;
}

llvm::BasicBlock* AST::SelectionStatement::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const
{
	/* Add a basic block for the header of the IfStmt */
	auto ifHeaderBlock = llvm::BasicBlock::Create(ctx.llvmContext, "if-header", ctx.function, 0);
	builder.CreateBr(ifHeaderBlock);
	builder.SetInsertPoint(ifHeaderBlock);
	/* Create code for the condition */
	auto valCondData = condition->GenerateCode(builder, ctx);
	auto valCond = condition->NeedsExplicitRValueLoad() ? valCondData.TryCreateLoad(builder) : valCondData.GetValue();
	auto ifCondition = valCond->getType()->isIntegerTy(1) ?
												 valCond :
												 builder.CreateICmpNE(valCond, llvm::Constant::getNullValue(valCond->getType()));
	ifCondition->setName("if-condition");
	/* Add a basic block for the consequence of the IfStmt */
	auto ifConsequenceBlock = llvm::BasicBlock::Create(ctx.llvmContext, "if-consequence", ctx.function, 0);
	llvm::BasicBlock* ifAlternativeBlock = nullptr;
	auto ifEndBlock = llvm::BasicBlock::Create(ctx.llvmContext, "if-end", ctx.function, 0);
	if (elseStatement) {
		ifAlternativeBlock = llvm::BasicBlock::Create(ctx.llvmContext, "if-alternative", ctx.function, 0);
		builder.CreateCondBr(ifCondition, ifConsequenceBlock, ifAlternativeBlock);
	} else {
		builder.CreateCondBr(ifCondition, ifConsequenceBlock, ifEndBlock);
	}
	/* Generate the 'then' block */
	builder.SetInsertPoint(ifConsequenceBlock);
	ifStatement->GenerateCode(builder, ctx, false);
	builder.CreateBr(ifEndBlock);
	/* Generate the 'else' block */
	if (elseStatement) {
		builder.SetInsertPoint(ifAlternativeBlock);
		elseStatement->GenerateCode(builder, ctx, false);
		builder.CreateBr(ifEndBlock);
	}
	/* Continue in the if end block */
	builder.SetInsertPoint(ifEndBlock);
	return nullptr;
}

llvm::BasicBlock* AST::IterationStatement::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const
{
	/* Add a basic block for the header of the IfStmt */
	auto whileHeaderBlock = llvm::BasicBlock::Create(ctx.llvmContext, "while-header", ctx.function, 0);
	builder.CreateBr(whileHeaderBlock);
	builder.SetInsertPoint(whileHeaderBlock);
	/* Create the condition, consider (n) <==> (n != 0) */
	auto valCondData = condition->GenerateCode(builder, ctx);
	auto valCond = condition->NeedsExplicitRValueLoad() ? valCondData.TryCreateLoad(builder) : valCondData.GetValue();
	auto whileCondition = valCond->getType()->isIntegerTy(1) ?
														valCond :
														builder.CreateICmpNE(valCond, llvm::Constant::getNullValue(valCond->getType()));
	whileCondition->setName("while-condition");
	auto whileBodyBlock = llvm::BasicBlock::Create(ctx.llvmContext, "while-body", ctx.function, 0);
	auto whileEndBlock = llvm::BasicBlock::Create(ctx.llvmContext, "while-end", ctx.function, 0);
	ctx.loopBlocks = { whileHeaderBlock, whileEndBlock };
	/* Create the conditional branch */
	builder.CreateCondBr(whileCondition, whileBodyBlock, whileEndBlock);
	/* Start inserting in the while body block */
	builder.SetInsertPoint(whileBodyBlock);
	body->GenerateCode(builder, ctx, false);
	builder.CreateBr(whileHeaderBlock);
	/* The while was created, adjust the inserting point to the while end block */
	builder.SetInsertPoint(whileEndBlock);
	return nullptr;
}

llvm::BasicBlock* AST::JumpStatement::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool) const
{
	switch (jumpKind->Kind) {
	case TokenKind::TK_RETURN:
		{
			if (expression) {
				auto expr = static_cast<Expression*>(expression.get());
				auto retExprData = expr->GenerateCode(builder, ctx);
				auto retExpr = expr->NeedsExplicitRValueLoad() ? retExprData.TryCreateLoad(builder) : retExprData.GetValue();
				auto retTy = ctx.function->getReturnType();
				if (retExpr->getType() != retTy) {
					auto castedRetExpr = CreateTypeCast(retTy, retExpr, ctx);
					builder.CreateRet(castedRetExpr);
				} else {
					builder.CreateRet(retExpr);
				}
			} else {
				builder.CreateRetVoid();
			}
			/* Always create a new block after a return statement
			 *
			 *  This will prevent you from inserting code after a block terminator (here
			 *  the return instruction), but it will create a dead basic block instead.
			 */
			auto returnDeadBlock = llvm::BasicBlock::Create(ctx.llvmContext, "DEAD_BLOCK", ctx.function);
			builder.SetInsertPoint(returnDeadBlock);
		}
		break;
	case TokenKind::TK_GOTO:
		{
			auto label = expression->token->Text;
			auto labelBlock = ctx.labels->find(label);
			if (labelBlock == ctx.labels->end()) {
				auto newLabelBlock = llvm::BasicBlock::Create(builder.getContext(), label, ctx.function);
				ctx.labels->insert_or_assign(label, newLabelBlock);
				builder.CreateBr(newLabelBlock);
			} else {
				builder.CreateBr(labelBlock->second);
			}
		}
		break;
	case TokenKind::TK_BREAK:
	case TokenKind::TK_CONTINUE:
		{
			auto ctxPtr = &ctx;
			do {
				if (ctxPtr->loopBlocks.first) {
					const auto& [start, end] = ctxPtr->loopBlocks;
					builder.CreateBr(jumpKind->Is(TokenKind::TK_BREAK) ? end : start);
					break;
				}
				ctxPtr = ctxPtr->parent;
			} while (true);
		}
		break;
	default:
		throw std::runtime_error("Unknown jump statement: " + jumpKind->Text);
	}
	return nullptr;
}

void AST::TranslationUnit::GenerateCode(llvm::LLVMContext& lctx, llvm::Module& m, llvm::IRBuilder<>& b, llvm::IRBuilder<>& ab) const
{
	Context2 ctx{ m, lctx, b, ab };
	for (const auto& decl : externalDeclarations) {
		if (decl->Type() == NodeType::kFunctionDefinition) {
			auto f = static_cast<FunctionDefinition*>(decl.get());
			f->GenerateCode(ctx.builder, ctx);
		} else {
			auto d = static_cast<ExternalDeclaration*>(decl.get());
			d->GenerateDeclaration(ctx.builder, ctx);
		}
	}
}

void AST::FunctionDefinition::GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const
{
	Context2 funcCtx{ ctx };
	auto funcReturnType = declSpecifiers->GenerateType(builder, funcCtx);
	Context2::ValueData funcData{};
	funcData.AddType(funcReturnType);
	declarator->GenerateDeclarator(builder, funcCtx, funcData);
	auto funcVal = llvm::dyn_cast<llvm::Function>(funcData.GetValue());

	auto funcName = funcVal->getName().str();
	ctx.variables[funcName] = funcData;

	std::map<std::string, llvm::BasicBlock*> labels;
	funcCtx.labels = &labels;
	funcCtx.function = funcVal;

	llvm::BasicBlock* funcBB = llvm::BasicBlock::Create(
			ctx.llvmContext /* LLVMContext &Context */,
			"entry" /* const Twine &Name="" */,
			funcVal /* Function *Parent=0 */,
			0 /* BasicBlock *InsertBefore=0 */);

	/* Set a new insert point for the LLVM-IR builder */
	builder.SetInsertPoint(funcBB);

	/* The alloca builder always inserts instructions into the entry block.
   * This allows an easier implementation of the 'memory to register' pass and
   * removes the problems caused by an alloca inside a loop. However, this
   * might not be an optimal solution...
   */
	auto& allocaBuilder = ctx.allocaBuilder;
	allocaBuilder.SetInsertPoint(funcBB);

	/* Reset the alloca builder each time before using it
   *
   *   It should not insert any instruction behind the terminator of the entry
   *   block, the easiest way to ensure this is to set it to the begining of
   *   the entry block each time we insert an alloca. */
	allocaBuilder.SetInsertPoint(allocaBuilder.GetInsertBlock(),
			allocaBuilder.GetInsertBlock()->begin());

	for (auto& ArgN : funcVal->args()) {
		/* Store each argument on the stack to allow easy (phi-less) modifications.
  	 *   1. Allocate a stack slot */
		auto argType = ArgN.getType();
		llvm::Value* argVar = allocaBuilder.CreateAlloca(argType);
		/*   2. Store the argument value */
		builder.CreateStore(&ArgN, argVar);
		/*   3. Add the variable to the symbol table */
		std::string argName = ArgN.getName().str();
		auto& varData = funcCtx.variables.at(argName);
		varData.ReplaceValue(argVar);
		varData.AddType(argVar->getType());
	}

	body->GenerateCode(builder, funcCtx, false);

	/* All code was emitted,.. but the last block might be empty.
	 * If the last block does not end with a terminator statement the simple
	 * rules created either dead code or the function is a void function without
	 * a return on each path. Either way we need to add a terminator instruction
	 * to the last block. The idea is to look at the return type of the current
	 * function and emit either a void return or a return with the 'NULL' value
	 * for this type */
	if (builder.GetInsertBlock()->getTerminator() == nullptr) {
		auto curFuncReturnType = builder.getCurrentFunctionReturnType();
		if (curFuncReturnType->isVoidTy()) {
			builder.CreateRetVoid();
		} else {
			builder.CreateRet(llvm::Constant::getNullValue(curFuncReturnType));
		}
	}
	llvm::verifyFunction(*funcVal);
}
