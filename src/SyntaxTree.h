#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/Constant.h>		 /* Constant::getNullValue */
#include <llvm/IR/Function.h>		 /* Function */
#include <llvm/IR/GlobalValue.h> /* GlobaleVariable, LinkageTypes */
#include <llvm/IR/IRBuilder.h>	 /* IRBuilder */
#include <llvm/IR/LLVMContext.h> /* LLVMContext */
#include <llvm/IR/Module.h>			 /* Module */
#include <llvm/IR/Verifier.h>		 /* verifyFunction, verifyModule */
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h> /* Nice stacktrace output */
#include <llvm/Support/SystemUtils.h>

#include "Token.h"
namespace AST
{
	struct Node;
	struct Expression;
	struct Context;
	struct TypeName;
	struct DirectDeclarator;
	struct StructMemberList;
	using NodePtr = std::shared_ptr<Node>;
	using ExpressionPtr = std::shared_ptr<Expression>;
	using TypeNamePtr = std::shared_ptr<TypeName>;
	using DirectDeclaratorPtr = std::shared_ptr<DirectDeclarator>;
	using StructMemberListPtr = std::shared_ptr<StructMemberList>;

	using SemanticException = std::pair<std::string, const Token*>;

	inline constexpr char PARAMETER_PREFIX = '%';

	enum class NodeType
	{
		kNone,
		// Primary
		kConstant,
		kStringLiteral,
		kIdentifier,
		// Postfix
		kFunctionCall,
		kArrayAccess,
		kMemberAccess,
		// Unary, Cast, ...
		kUnaryOperation,
		kCastExpression,
		kBinaryOperation,
		kConditionalExpression,
		kAssignment,
		kExpression,

		// Declaration
		kDeclaration,
		kParameterDeclaration,
		kStructMemberList,
		kParameterDeclarationList,
		kIdentifierList,
		kTypeSpecifier,
		kStructSpecifier,			 // TypeSpecifier with identifier
		kStructSpecifierImpl,	 // StructSpecifier with body
		kSpecifierList,
		kPointerDeclarator,
		kInitDeclarator,
		kDirectDeclarator,
		kPointer,
		kTypeName,
		kDesgination,
		kDesginationList,
		kArrayDeclarator,
		kInitializerList,

		// Statements
		kLabeledStatement,
		kCompoundStatement,
		kExpressionStatement,
		kSelectionStatement,
		kIterationStatement,
		kJumpStatement,

		// External Declarations
		kTranslationUnit,
		kExternalDeclaration,
		kFunctionDefinition,
	};

	struct EffectiveType
	{
		enum class TypeContext
		{
			kNone,
			kFunction,
			kStruct,
			kLabel,
			kRValue,
		};

		EffectiveType() = default;
		EffectiveType(TypeContext typeContext, const Token* rawType, size_t depth = 0) :
			context(typeContext), rawType(rawType), pointerDepth(depth) {}

		TypeContext context{ TypeContext::kNone };
		const Token* rawType{ nullptr };
		size_t pointerDepth{ 0 };
		// Function Types
		bool hasBody{ false };
		std::shared_ptr<EffectiveType> returnType{};
		std::vector<EffectiveType> parameters{};
		// Structs
		std::shared_ptr<Context> structContext{ nullptr };

		bool CanAssign(const EffectiveType& rhs) const;
		bool IsArithmetic() const { return rawType && rawType->Is(TokenKind::TK_INT, TokenKind::TK_CHAR, TokenKind::TK_NUMBER, TokenKind::TK_CHARACTER) && pointerDepth == 0; }
		bool IsScalar() const { return rawType->Is(TokenKind::TK_INT, TokenKind::TK_CHAR, TokenKind::TK_NUMBER, TokenKind::TK_CHARACTER) || pointerDepth > 0; }
		bool IsNull() const { return rawType->Is(TokenKind::TK_NUMBER) && rawType->Text == "0"; }
		bool operator==(const EffectiveType& rhs) const
		{
			return *rawType == *rhs.rawType && pointerDepth == rhs.pointerDepth && structContext == rhs.structContext;
			// NOTE: Could prbly make this more strict here: 
			// && parameters == rhs.parameters
			// && ((!returnType && !rhs.returnType) || (returnType && rhs.returnType && *returnType == *rhs.returnType));
		}
	};

	struct Context
	{
		enum class ScopeContext
		{
			kNone,
			kFunction,
			kStruct,
			kLoop,
		};

		struct Label
		{
			Label(std::shared_ptr<EffectiveType> labelType, std::shared_ptr<Context> context, bool hasPreDeclarations) :
				labelType(labelType), context(context), hasPreDeclarations(hasPreDeclarations) {}

			std::shared_ptr<EffectiveType> labelType;
			std::shared_ptr<Context> context;
			bool hasPreDeclarations;	// If there are declarations before the label within the owning scope
		};

		Context(std::shared_ptr<Context> parent, ScopeContext scopeContext) :
			parent(parent), scopeContext(scopeContext), returnType(parent ? parent->returnType : nullptr) {}

		static std::shared_ptr<Context> EmptyContext() { return std::make_shared<Context>(nullptr, ScopeContext::kNone); }
		Context* GetFunctionLevelContext();

		std::shared_ptr<Context> parent;
		ScopeContext scopeContext;
		std::map<std::string, Label> labels{};
		std::vector<NodePtr> gotoExpressions{};
		std::shared_ptr<EffectiveType> returnType;
		std::map<std::string, std::shared_ptr<EffectiveType>> variables{};
		std::map<std::string, std::shared_ptr<EffectiveType>> structs{};
	};

	struct Context2
	{
		struct StructObject
		{
			std::shared_ptr<Context2> context;
			llvm::StructType* type;
			std::vector<std::pair<llvm::StringRef, std::vector<llvm::Type*>>> memberNames;	// Name, [Underlying Types]
		};

		class ValueData
		{
			llvm::Value* value{ nullptr };
			std::vector<llvm::Type*> typeIndirections{};

		public:
			ValueData() = default;
			ValueData(llvm::Value* value) :
				value(value), typeIndirections(std::vector<llvm::Type*>{ value->getType() }) {}
			ValueData(llvm::Value* value, const ValueData& predecessorData, size_t copyOffset) :
				value(value), typeIndirections(predecessorData.typeIndirections.begin(), predecessorData.typeIndirections.end() - copyOffset) {}
			ValueData(llvm::Value* value, std::vector<llvm::Type*> types) :
				value(value), typeIndirections(types) { assert(value->getType() == types.back() && "Custom type data should match provided value"); }

			llvm::Value* GetValue() const { return value; }
			llvm::Type* GetType() const { return typeIndirections.back(); }
			std::vector<llvm::Type*> GetTypeIndirections() const { return typeIndirections; }
			llvm::Type* GetUnderlyingType() const { return (typeIndirections.size() >= 2) ? *(typeIndirections.end() - 2) : nullptr; }
			llvm::Value* TryCreateLoad(llvm::IRBuilder<>& builder)
			{
				if (llvm::isa<llvm::Function>(value) || typeIndirections.size() < 2 || GetUnderlyingType()->isFunctionTy())
					return value;	 // Functions arent loaded or no underlying type to load from
				typeIndirections.pop_back();
				return value = builder.CreateLoad(typeIndirections.back(), value);
			}
			void ReplaceValue(llvm::Value* replace) {
				// assert(replace->getType() == value->getType() && "Replacement value type mismatch");
				value = replace;
			}
			void AddType(llvm::Type* type) { typeIndirections.push_back(type); }

		private:
			bool IsSkipLoad(llvm::Type* ty) { return ty->isArrayTy() || ty->isFunctionTy() || ty->isStructTy(); }
		};

		Context2(llvm::Module& module, llvm::LLVMContext& llvmContext, llvm::IRBuilder<>& builder, llvm::IRBuilder<>& allocaBuilder) :
			parent(nullptr), module(module), llvmContext(llvmContext), builder(builder), allocaBuilder(allocaBuilder),
			function(nullptr), labels(nullptr) {}
		Context2(Context2* parent) :
			parent(parent), module(parent->module), llvmContext(parent->llvmContext), builder(parent->builder),
			allocaBuilder(parent->allocaBuilder), function(parent->function), labels(parent->labels) {}

		Context2* parent;
		llvm::Module& module;
		llvm::LLVMContext& llvmContext;
		llvm::IRBuilder<>&builder, &allocaBuilder;
		llvm::Function* function;
		std::map<std::string, llvm::BasicBlock*>* labels;

		std::map<std::string, ValueData> variables{};
		std::map<std::string, std::shared_ptr<StructObject>> structs{};
		std::pair<llvm::BasicBlock*, llvm::BasicBlock*> loopBlocks{};
	};

	struct Node
	{
		virtual NodeType Type() const { return NodeType::kNone; }

		Node(const Token* loc) :
			token(loc) {}
		virtual ~Node() = default;

		const Token* token;

	protected:
		static std::string Depth(size_t depth) { return std::string(depth, '\t'); }

	public:
		virtual bool IsExpression() const { return false; }
		virtual bool IsStatement() const { return false; }
		virtual bool NeedsExplicitRValueLoad() const { return false; }
		virtual std::string ToString(size_t depth) const { return Depth(depth) + token->Text; }
		virtual std::string ToString(size_t depth, size_t) const { return ToString(depth); }
		virtual EffectiveType CheckSemantics(std::shared_ptr<Context>) { throw SemanticException("Not implemented: " + static_cast<int>(Type()), token); }
	};

	// ---------------------------------------------------------------------------
	// Expressions

	struct Expression : public Node
	{
		Expression(const Token* loc) :
			Node(loc) {}

		bool IsExpression() const override { return true; }
		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>&, Context2&) const { return nullptr; }
	};

	struct Constant final : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kConstant; }

		Constant(const Token* token) :
			Expression(token) {}

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
	};

	struct StringLiteral final : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kStringLiteral; }

		StringLiteral(const Token* token) :
			Expression(token) {}

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
	};

	struct Identifier final : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kIdentifier; }

		Identifier(const Token* token) :
			Expression(token) {}

		virtual bool NeedsExplicitRValueLoad() const override { return true; }
		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
	};

	struct FunctionCall : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kFunctionCall; }

		FunctionCall(const Token* lParen, const ExpressionPtr& function, const std::vector<ExpressionPtr>& arguments) :
			Expression(lParen), function(function), arguments(arguments) {}

		ExpressionPtr function;
		std::vector<ExpressionPtr> arguments;

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct ArrayAccess : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kArrayAccess; }

		ArrayAccess(const ExpressionPtr& array, const ExpressionPtr& index) :
			Expression(array->token), array(array), index(index) {}

		ExpressionPtr array;
		ExpressionPtr index;

		virtual bool NeedsExplicitRValueLoad() const override { return true; }
		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct MemberAccess : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kMemberAccess; }

		MemberAccess(const ExpressionPtr& object, const Token* accessKind, Identifier member) :
			Expression(accessKind), object(object), accessKind(accessKind), member(member) {}

		ExpressionPtr object;
		const Token* accessKind;
		Identifier member;

		virtual bool NeedsExplicitRValueLoad() const override { return true; }
		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct UnaryOperation : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kUnaryOperation; }

		UnaryOperation(const Token* op, const NodePtr& operand) :
			Expression(op), op(op), operand(operand) {}

		const Token* op;
		NodePtr operand;	// expression or type name

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;

	private:
		// Op Result here should be a non-null RValue
		Token SizeOfToken{ (Locatable)(*op), TokenKind::TK_NUMBER, "42" };
		EffectiveType SizeOfResult{ EffectiveType::TypeContext::kNone, &SizeOfToken };
	};

	struct CastExpression : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kCastExpression; }

		CastExpression(const TypeNamePtr& castType, const ExpressionPtr& castExpression) :
			Expression(castExpression->token), castType(castType), castExpression(castExpression) {}

		TypeNamePtr castType;
		ExpressionPtr castExpression;

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct BinaryOperation : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kBinaryOperation; }

		BinaryOperation(const Token* op, ExpressionPtr& lhs, ExpressionPtr& rhs) :
			Expression(op), op(op), lhs(lhs), rhs(rhs) {}

		const Token* op;
		ExpressionPtr lhs;
		ExpressionPtr rhs;

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;

	private:
		// Op Result here should be a non-null RValue
		Token OpResult{ (Locatable)(*op), TokenKind::TK_NUMBER, "42" };
	};

	struct ConditionalExpression : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kConditionalExpression; }

		ConditionalExpression(ExpressionPtr& condition, ExpressionPtr& middle, ExpressionPtr& rhs) :
			Expression(condition->token), condition(condition), middle(middle), rhs(rhs) {}

		ExpressionPtr condition;
		ExpressionPtr middle;
		ExpressionPtr rhs;

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct Assignment : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kAssignment; }

		Assignment(ExpressionPtr& lhs, const Token* op, ExpressionPtr& rhs) :
			Expression(op), lhs(lhs), op(op), rhs(rhs) {}

		ExpressionPtr lhs;
		const Token* op;
		ExpressionPtr rhs;

		virtual bool NeedsExplicitRValueLoad() const override { return true; }
		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct CommaExpression : public Expression
	{
		virtual NodeType Type() const override { return NodeType::kExpression; }

		CommaExpression(std::vector<ExpressionPtr> expressions) :
			Expression(expressions.front()->token), expressions(expressions) {}

		std::vector<ExpressionPtr> expressions;

		virtual Context2::ValueData GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	// ---------------------------------------------------------------------------
	// Declarations

	struct TypeSpecifier : public Node
	{
		virtual NodeType Type() const override { return NodeType::kTypeSpecifier; }

		TypeSpecifier(const Token* token) :
			Node(token) {}

		virtual llvm::Type* GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};
	using TypeSpecifierPtr = std::shared_ptr<TypeSpecifier>;

	struct StructSpecifier : public TypeSpecifier
	{
		virtual NodeType Type() const override { return NodeType::kStructSpecifier; }

		StructSpecifier(const Token* structToken, const Token* identifier) :
			TypeSpecifier(structToken), identifier(identifier) {}

		const Token* identifier;

		virtual llvm::Type* GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct StructSpecifierImpl final : public StructSpecifier
	{
		virtual NodeType Type() const override { return NodeType::kStructSpecifierImpl; }

		StructSpecifierImpl(const Token* structToken, const Token* identifier, const StructMemberListPtr& structDeclarationList) :
			StructSpecifier(structToken, identifier), structDeclarationList(structDeclarationList) {}

		StructMemberListPtr structDeclarationList;

		virtual llvm::Type* GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct DirectDeclarator : public Node	 // ()
	{
		virtual NodeType Type() const override { return NodeType::kDirectDeclarator; }

		DirectDeclarator(const DirectDeclaratorPtr& previous, const NodePtr& declarator) :
			Node(declarator ? declarator->token :
					 previous		? previous->token :
												nullptr),
			previous(previous), declarator(declarator) {}

		DirectDeclaratorPtr previous;	 // maybe nullptr
		NodePtr declarator;

		void GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type) const
		{
			std::vector<std::string> names{};
			GenerateDeclarator(builder, ctx, type, names, nullptr);
		}
		void GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type,
				std::vector<std::string>& paramNames, std::pair<std::string, std::vector<llvm::Type*>>* alt_out) const;
		void CheckSemanticsT(std::shared_ptr<Context> context, EffectiveType*& type, const Token* hasEmptyParams = nullptr);
		std::string ToString(size_t) const override;
	};

	struct ArrayDeclarator : public DirectDeclarator	// []
	{
		virtual NodeType Type() const override { return NodeType::kArrayDeclarator; }

		ArrayDeclarator(const DirectDeclaratorPtr& previous, const NodePtr& declarator) :
			DirectDeclarator(previous, declarator) {}

		std::string ToString(size_t) const override;
	};

	struct PointerDeclarator : public Node
	{
		virtual NodeType Type() const override { return NodeType::kPointerDeclarator; }

		PointerDeclarator(const Token* start, size_t ptrDepth, const DirectDeclaratorPtr& declarator) :
			Node(declarator ? declarator->token : start), pointerDepth(ptrDepth), declarator(declarator) {}

		size_t pointerDepth;
		DirectDeclaratorPtr declarator;

		virtual void GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type) const
		{
			std::vector<std::string> names{};
			GenerateDeclarator(builder, ctx, type, names, nullptr);
		}
		virtual void GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type,
			std::vector<std::string>& paramNames, std::pair<std::string, std::vector<llvm::Type*>>* alt_out) const;
		virtual void CheckSemanticsT(std::shared_ptr<Context> context, EffectiveType*& type, const Token* hasEmptyParams = nullptr);
		virtual std::string ToString(size_t) const override;
	};
	using PointerDeclaratorPtr = std::shared_ptr<PointerDeclarator>;

	struct InitDeclarator : public PointerDeclarator
	{
		virtual NodeType Type() const override { return NodeType::kInitDeclarator; }

		InitDeclarator(const PointerDeclaratorPtr& declarator, const ExpressionPtr& initializer) :
			PointerDeclarator(*declarator), initializer(initializer) {}

		ExpressionPtr initializer;

		virtual void GenerateDeclarator(llvm::IRBuilder<>& builder, Context2& ctx, Context2::ValueData& type,
				std::vector<std::string>& paramNames, std::pair<std::string, std::vector<llvm::Type*>>* alt_out) const override;
		void CheckSemanticsT(std::shared_ptr<Context> context, EffectiveType*& type, const Token* hasEmptyParams = nullptr) override;
		std::string ToString(size_t) const override;
	};

	struct TypeName : public Node
	{
		virtual NodeType Type() const override { return NodeType::kTypeName; }

		TypeName(const TypeSpecifierPtr& declSpecifiers, const PointerDeclaratorPtr& abstractDeclarator) :
			Node(declSpecifiers->token), declSpecifiers(declSpecifiers), abstractDeclarator(abstractDeclarator) {}

		TypeSpecifierPtr declSpecifiers;
		PointerDeclaratorPtr abstractDeclarator;	// maybe nullptr

		virtual std::vector<llvm::Type*> GenerateType(llvm::IRBuilder<>& builder, Context2& ctx) const;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct Declaration : public Node
	{
		virtual NodeType Type() const override { return NodeType::kDeclaration; }

		Declaration(const TypeSpecifierPtr& declSpecifiers, const PointerDeclaratorPtr& initDeclarator) :
			Node(declSpecifiers->token), declSpecifiers(declSpecifiers), initDeclarator(initDeclarator) {}

		TypeSpecifierPtr declSpecifiers;			// int, long, unsigned, etc.
		PointerDeclaratorPtr initDeclarator;	//	*ptr, **ptr, var, var = 3, etc.

		virtual Context2::ValueData GenerateDeclaration(llvm::IRBuilder<>& builder, Context2& ctx) const;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};
	using DeclarationPtr = std::shared_ptr<Declaration>;

	struct StructMemberList : public Node
	{
		virtual NodeType Type() const override { return NodeType::kStructMemberList; }

		StructMemberList(const std::vector<DeclarationPtr>& declarations) :
			Node(declarations.empty() ? nullptr : declarations.front()->token), declarations(declarations) {}

		std::vector<DeclarationPtr> declarations;

		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct ParameterDeclaration : public Node
	{
		virtual NodeType Type() const override { return NodeType::kParameterDeclaration; }

		ParameterDeclaration(const TypeSpecifierPtr& declSpecifiers, const PointerDeclaratorPtr& declarator) :
			Node(declSpecifiers->token), declSpecifiers(declSpecifiers), declarator(declarator) {}

		TypeSpecifierPtr declSpecifiers;
		PointerDeclaratorPtr declarator;	// maybe nullptr

		virtual std::pair<std::string, std::vector<llvm::Type*>> GenerateDeclaration(llvm::IRBuilder<>& builder, Context2& ctx) const;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};
	using ParameterDeclarationPtr = std::shared_ptr<ParameterDeclaration>;

	struct ParameterTypeList : public Node
	{
		virtual NodeType Type() const override { return NodeType::kParameterDeclarationList; }

		ParameterTypeList(const Token* lBrack, const std::vector<ParameterDeclarationPtr>& parameters) :
			Node(lBrack), parameters(parameters) {}

		std::vector<ParameterDeclarationPtr> parameters;

		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};
	using ParameterListPtr = std::shared_ptr<ParameterTypeList>;

	struct DesginationList : public Node
	{
		virtual NodeType Type() const override { return NodeType::kDesginationList; }

		DesginationList(const std::vector<NodePtr>& expression) :
			Node(expression.empty() ? nullptr : expression.front()->token), expression(expression) {}

		std::vector<NodePtr> expression;

		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	// ---------------------------------------------------------------------------
	// Statements

	struct Statement : public Node
	{
		virtual NodeType Type() const override { return NodeType::kNone; }

		Statement(const Token* loc) :
			Node(loc) {}

		bool IsStatement() const override { return true; }
		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>&, Context2&, bool = true) const = 0;

	protected:
		EffectiveType statementRetVal{ EffectiveType::TypeContext::kNone, token };
	};
	using StatementPtr = std::shared_ptr<Statement>;

	struct LabeledStatement : public Statement
	{
		virtual NodeType Type() const override { return NodeType::kLabeledStatement; }

		LabeledStatement(const Token* label, const StatementPtr& statement) :
			Statement(label), statement(statement) {}

		StatementPtr statement;

		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct CompoundStatement : public Statement
	{
		virtual NodeType Type() const override { return NodeType::kCompoundStatement; }

		CompoundStatement(const Token* lBrace, const std::vector<NodePtr>& statements) :
			Statement(lBrace), statements(statements) {}

		std::vector<NodePtr> statements;

		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
		std::string ToString(size_t, size_t) const override;
	};

	struct ExpressionStatement : public Statement
	{
		virtual NodeType Type() const override { return NodeType::kExpressionStatement; }

		ExpressionStatement(const ExpressionPtr& expression) :
			Statement(expression ? expression->token : nullptr), expression(expression) {}

		ExpressionPtr expression;	 // maybe nullptr

		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct SelectionStatement : public Statement
	{
		virtual NodeType Type() const override { return NodeType::kSelectionStatement; }

		SelectionStatement(const Token* ifToken, const ExpressionPtr& condition, const StatementPtr& ifStatement, const StatementPtr& elseStatement) :
			Statement(ifToken), condition(condition), ifStatement(ifStatement), elseStatement(elseStatement) {}

		ExpressionPtr condition;
		StatementPtr ifStatement;
		StatementPtr elseStatement;	 // maybe nullptr

		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
		virtual std::string ToString(size_t, size_t) const override;
	};

	struct IterationStatement : public Statement
	{
		virtual NodeType Type() const override { return NodeType::kIterationStatement; }

		IterationStatement(const Token* token, const ExpressionPtr& condition, const StatementPtr& body) :
			Statement(token), token(token), condition(condition), body(body) {}

		const Token* token;
		ExpressionPtr condition;
		StatementPtr body;

		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct JumpStatement : public Statement
	{
		virtual NodeType Type() const override { return NodeType::kJumpStatement; }

		JumpStatement(const Token* jumpKind, const NodePtr& expression) :
			Statement(jumpKind), jumpKind(jumpKind), expression(expression) {}

		const Token* jumpKind;
		NodePtr expression;	 // expr (return) or identifer (label), maybe nullptr

		virtual llvm::BasicBlock* GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx, bool openBlock) const override;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	// ---------------------------------------------------------------------------
	// External Declarations

	struct Extern : public Node
	{
		virtual NodeType Type() const override { return NodeType::kNone; }

		Extern(const Token* loc) :
			Node(loc) {}
	};
	using ExternPtr = std::shared_ptr<Extern>;

	struct TranslationUnit final : public Extern
	{
		virtual NodeType Type() const override { return NodeType::kTranslationUnit; }

		TranslationUnit(const std::vector<ExternPtr>& externalDeclarations) :
			Extern(externalDeclarations.empty() ? nullptr : externalDeclarations.front()->token), externalDeclarations(externalDeclarations) {}

		std::vector<ExternPtr> externalDeclarations;

		void GenerateCode(llvm::LLVMContext& ctx, llvm::Module& m, llvm::IRBuilder<>& builder, llvm::IRBuilder<>& allocaBuilder) const;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;

	private:
		EffectiveType retValType{ EffectiveType::TypeContext::kNone, token };
	};

	struct ExternalDeclaration :
		public Extern,
		public Declaration
	{
		virtual NodeType Type() const override { return NodeType::kExternalDeclaration; }

		ExternalDeclaration(const TypeSpecifierPtr& declSpecifiers, const PointerDeclaratorPtr& initDeclarator) :
			Extern(declSpecifiers->token), Declaration(declSpecifiers, initDeclarator) {}

		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

	struct FunctionDefinition :
		public Extern
	{
		virtual NodeType Type() const override { return NodeType::kFunctionDefinition; }

		FunctionDefinition(const TypeSpecifierPtr& declSpecifiers, const PointerDeclaratorPtr& declarator, const StatementPtr& body) :
			Extern(declSpecifiers->token), declSpecifiers(declSpecifiers), declarator(declarator), body(body) {}

		TypeSpecifierPtr declSpecifiers;
		PointerDeclaratorPtr declarator;
		StatementPtr body;	// CompoundStatement

		void GenerateCode(llvm::IRBuilder<>& builder, Context2& ctx) const;
		EffectiveType CheckSemantics(std::shared_ptr<Context> context) override;
		std::string ToString(size_t) const override;
	};

}	 // namespace AST
