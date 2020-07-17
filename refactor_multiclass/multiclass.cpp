#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "clang/AST/Stmt.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"


#include <set>
using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");


static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);


auto constructorParameter = cxxConstructorDecl(
		hasAnyParameter(hasType(asString("std::shared_ptr<Labels>"))),
		ofClass(isDerivedFrom("MulticlassMachine"))
		).bind("constructor");

auto method_mathcher = memberExpr(member(hasName("m_labels")),
	hasAncestor(cxxMethodDecl(ofClass(isDerivedFrom("MulticlassMachine"))).bind("method"))).bind("labels");

std::set<std::string> func_names;
class labels_matcher : public MatchFinder::MatchCallback
{
public:
	labels_matcher(Rewriter& rewriter) : m_rewriter(rewriter) {}
	void run(const MatchFinder::MatchResult &Result) override
	{
		ASTContext *Context = Result.Context;
		const CXXMethodDecl *FS = Result.Nodes.getNodeAs<CXXMethodDecl>("method");
		const MemberExpr* expr = Result.Nodes.getNodeAs<MemberExpr>("labels");
		auto SM = Result.SourceManager;
		std::string func_name = FS->getNameAsString();
		std::string record_name = FS->getParent()->getNameAsString();
		bool has_labels = false;
		for(auto para = FS->param_begin (); para != FS->param_end(); ++ para){
			llvm::outs()<<FS->getName ()<<"\n";
			if((*para)->getTypeSourceInfo ()->getType ().getAsString()== "std::shared_ptr<Labels>"){
				has_labels = true;
			}
		}
		m_rewriter.ReplaceText (expr->getMemberLoc(), 8, "labs");
		if(!has_labels && func_names.find(func_name) == func_names.end()){
			func_names.insert(func_name);
			auto loc = FS ->getTypeSourceInfo ()->getTypeLoc ().getAs<FunctionTypeLoc>();
			StringRef to_be_inserted;
			if(FS->param_size () ==0){
				to_be_inserted = "const std::shared_ptr<Labels>& labs";
			}
			else 
			{
				to_be_inserted = ", const std::shared_ptr<Labels>& labs";
			}
			m_rewriter.InsertTextAfter(loc.getLocalSourceRange ().getEnd(), to_be_inserted);
			//llvm::outs()<<loc.getLocalSourceRange ().printToString(*SM);
		}
		
	}
private:
	Rewriter& m_rewriter;
};


class RemoveConstructorParameter : public MatchFinder::MatchCallback{
public:
	RemoveConstructorParameter(Rewriter& rewriter) : m_rewriter(rewriter) {}
	void run(const MatchFinder::MatchResult &Result) override{
		ASTContext *Context = Result.Context;
		const CXXConstructorDecl *FS = Result.Nodes.getNodeAs<CXXConstructorDecl>("constructor");
		
		const auto &SM = Result.SourceManager;
		
			
		if(FS->param_size() == 1 && FS->getParamDecl(0)->getType()
							.getAsString()== "std::shared_ptr<Labels>")
		{
			m_rewriter.RemoveText(FS->getSourceRange());
		}
		else
		{
			for(auto parm = FS->param_begin(); parm != FS->param_end(); ++parm)
			{
				if((*parm)->getType().getAsString() == "std::shared_ptr<Labels>")
				{
					 SourceRange loc ((*parm)->getSourceRange());	
       				 m_rewriter.RemoveText(loc.getBegin(), m_rewriter.getRangeSize(loc) + 1);
				}
			}
		}
		
	}

private:
	Rewriter& m_rewriter;
};
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : HandlerForConstructor(R), HandlerForlabels(R)  {
    Matcher.addMatcher(method_mathcher, &HandlerForlabels);
	Matcher.addMatcher(constructorParameter, &HandlerForConstructor);

  }
    void HandleTranslationUnit(ASTContext &Context) override {
    // Run the matchers when we have the whole TU parsed.
    Matcher.matchAST(Context);
  }

private:
  labels_matcher HandlerForlabels;
  RemoveConstructorParameter HandlerForConstructor;
  MatchFinder Matcher;
};


class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
    std::error_code error_code;
    auto fEntry = TheRewriter.getSourceMgr().getFileEntryForID(TheRewriter.getSourceMgr()
				.getMainFileID());
    llvm::raw_fd_ostream outFile(fEntry->getName(), error_code, llvm::sys::fs::F_None);
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID())
        .write(outFile);
    outFile.close();
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }

private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv)
{
	CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
	ClangTool Tool(OptionsParser.getCompilations(),
				   OptionsParser.getSourcePathList());

	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
