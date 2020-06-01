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
static cl::extrahelp MoreHelp("\nMore help text...\n");

auto LabelMatcher = memberExpr(member(hasName("m_labels"))).bind("func");

std::set<std::string> func_names;

class labels_matcher : public MatchFinder::MatchCallback
{
public:
  void run(const MatchFinder::MatchResult &Result)
  {

    ASTContext *Context = Result.Context;
    const MemberExpr *FS = Result.Nodes.getNodeAs<MemberExpr>("func");
    // We do not want to convert header files!
    if (!FS || !Context->getSourceManager().isWrittenInMainFile(FS->getLocStart()))
      return;

    const auto &SM = Result.SourceManager;
    const auto &Loc = FS->getLocStart();
    auto ast_list = Context->getParents(*FS);
    std::stack<clang::ast_type_traits::DynTypedNode> nodes;
    for (auto &&ast : ast_list)
    {
      nodes.push(ast);
    }
    while (!nodes.empty())
    {
      auto &&node = nodes.top();
      nodes.pop();
      if (auto method = node.get<CXXMethodDecl>())
      {
        std::string func_name = method->getNameAsString();
        auto node_parents = Context->getParents(node);
        std::string record_name = method->getParent()->getNameAsString();

        func_name = record_name + "::" + func_name;
        if (func_names.find(func_name) == func_names.end())
        {
          func_names.insert(func_name);
          llvm::outs() << func_name << "\n";
          break;
        }
        else
        {
          auto node_parents = Context->getParents(node);
          for (const auto &sub_node : node_parents)
          {
            nodes.push(sub_node);
          }
        }
      }
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
