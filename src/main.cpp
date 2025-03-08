#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include "Lexer.h"
#include "Parser.h"
#include "SyntaxTree.h"

enum class Mode
{
	kTokenize,
	kParse,
	kPrintAST,
	kCompile,
	kOptimize,
};

std::string readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		throw std::runtime_error("Unable to open file: " + filename);
	}
	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);
	std::string buffer(fileSize, ' ');
	if (!file.read(buffer.data(), fileSize)) {
		throw std::runtime_error("Error reading file: " + filename);
	}
	return buffer;
}

int main(int argc, char** const argv)
{
	std::vector<std::string_view> args(argv + 1, argv + argc);
	std::string filename;
	Mode modus = Mode::kCompile;
	for (const auto& arg : args) {
		if (arg == "--tokenize") {
			modus = Mode::kTokenize;
		} else if (arg == "--parse") {
			modus = Mode::kParse;
		} else if (arg == "--print-ast") {
			modus = Mode::kPrintAST;
		} else if (arg == "--compile") {
			modus = Mode::kCompile;
		} else {
			filename = std::string(arg);
		}
	}
	// filename = "/home/kris/projects/c4/test.c";
	// modus = Mode::kCompile;
	try {
		const auto file = readFile(filename);
		// Tokenize
		auto tokens = Lexer::Lex(file, filename);
		if (tokens.empty())
			return EXIT_FAILURE;
		if (modus == Mode::kTokenize) {
			auto it = tokens.begin();
			while (it != tokens.end() - 1) {
				std::cout << *it << std::endl;
				it++;
			}
			return EXIT_SUCCESS;
		}
		// Parsing & AST
		auto syntaxTree = Parser::Parse(tokens);
		if (syntaxTree.externalDeclarations.empty())
			return EXIT_FAILURE;
		try {
			auto ctx = AST::Context::EmptyContext();
			syntaxTree.CheckSemantics(ctx);
		} catch (AST::SemanticException& e) {
			const auto& [err, token] = e;
			Locatable loc = *token;
			errorloc(loc, err);
			return EXIT_FAILURE;
		}
		if (modus == Mode::kParse) {
			return EXIT_SUCCESS;
		} else if (modus == Mode::kPrintAST) {
			std::cout << syntaxTree.ToString(0);
			return EXIT_SUCCESS;
		}
		// Compilation
		std::string outputFile = filename.substr(filename.find_last_of("/\\") + 1);
		outputFile = outputFile.substr(0, outputFile.find_last_of('.'));
		llvm::LLVMContext context;
		llvm::Module llvmModule(outputFile, context);
		llvm::IRBuilder<> builder(context), allocaBuilder(context);
		syntaxTree.GenerateCode(context, llvmModule, builder, allocaBuilder);
		llvm::verifyModule(llvmModule);
		if (modus == Mode::kCompile) {
			std::error_code ec;
			llvm::raw_fd_ostream stream(outputFile + ".ll", ec, llvm::sys::fs::OF_Text);
			if (ec) {
				std::cerr << "Error opening file: " << ec.message() << std::endl;
				return EXIT_FAILURE;
			}
			llvmModule.print(stream, nullptr);
			return EXIT_SUCCESS;
		}
		// Optimization ...?
		return EXIT_SUCCESS;
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
