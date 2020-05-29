#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

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
static cl::extrahelp MoreHelp("\nMore help text...\n");

//match t = m_labels;
auto LabelMatcher = cxxRecordDecl(
		isDerivedFrom(hasName("Machine")),
			hasMethod(cxxMethodDecl(
				hasBody(compoundStmt(hasAnySubstatement(
				declStmt(
					hasSingleDecl(varDecl(hasInitializer(
							cxxConstructExpr(
						hasAnyArgument(ignoringParenImpCasts(memberExpr(member(hasName("m_labels"))))))
					)))))))).bind("func"))).bind("labels");

//match m_labels = t;
auto LabelMatcher2 = 
cxxRecordDecl(
	isDerivedFrom(hasName("Machine")),
	hasMethod(cxxMethodDecl(
		hasBody(compoundStmt(
			hasAnySubstatement(cxxOperatorCallExpr(
				hasOverloadedOperatorName("="),
				hasAnyArgument(ignoringParenImpCasts(
					memberExpr(member(hasName("m_labels")))))))))).bind("func"))).bind("labels");


class labels_matcher : public MatchFinder::MatchCallback {
public :
 void run(const MatchFinder::MatchResult &Result) {
	   

  ASTContext *Context = Result.Context;
  const CXXMethodDecl *FS = Result.Nodes.getNodeAs<CXXMethodDecl>("func");
  // We do not want to convert header files!
  if (!FS || !Context->getSourceManager().isWrittenInMainFile(FS->getLocStart()))
    return;
 	const auto& SM = *Result.SourceManager;
    const auto& Loc = FS->getLocStart();
    llvm::outs() << SM.getFilename(Loc) << ":"
                   << SM.getSpellingLineNumber(Loc) << ":"
                   << SM.getSpellingColumnNumber(Loc) << "\n";}
};


int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  labels_matcher Printer;
  MatchFinder Finder;
  Finder.addMatcher(LabelMatcher, &Printer);
  Finder.addMatcher(LabelMatcher2, &Printer);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}