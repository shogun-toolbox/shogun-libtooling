#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "clang/AST/Stmt.h"
#include "llvm/Support/CommandLine.h"
#include <set>
using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.

auto LabelMatcher = cxxRecordDecl(isDerivedFrom(hasName("MulticlassMachine"))).bind("multiclass");

std::set<std::string> func_names;
class labels_matcher : public MatchFinder::MatchCallback
{
public:
	void run(const MatchFinder::MatchResult &Result)
	{
		ASTContext *Context = Result.Context;
		const CXXRecordDecl *FS = Result.Nodes.getNodeAs<CXXRecordDecl>("multiclass");
		const auto &SM = Result.SourceManager;
		llvm::outs() << FS->getName () ;		
	}
};

int main(int argc, const char **argv)
{
	CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
	ClangTool Tool(OptionsParser.getCompilations(),
				   OptionsParser.getSourcePathList());

	labels_matcher Printer;
	MatchFinder Finder;
	Finder.addMatcher(LabelMatcher, &Printer);

	return Tool.run(newFrontendActionFactory(&Finder).get());
}
