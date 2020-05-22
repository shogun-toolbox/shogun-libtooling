#include <memory>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/FixIt.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

auto sgo_like_ptr_matcher() {
  return
    pointsTo(
      cxxRecordDecl(
        isSameOrDerivedFrom("CSGObject")
      )
    );
}
//explicitCastExpr(hasType(pointsTo(cxxRecordDecl(isSameOrDerivedFrom("CSGObject")))))
//cxxDynamicCastExpr(hasType(pointsTo(cxxRecordDecl(isSameOrDerivedFrom("CSGObject")))))


static std::string getType(QualType& qt) {
  auto typePtr = qt.getTypePtr();
  if (typePtr && typePtr->getPointeeCXXRecordDecl()) {
    return typePtr->getPointeeCXXRecordDecl()->getNameAsString();
  } else if (typePtr && typePtr->getAsCXXRecordDecl()) {
    return typePtr->getAsCXXRecordDecl()->getNameAsString();
  }

  return "";
}

class DropRef : public MatchFinder::MatchCallback {
  std::string const bind_name_ = "ref";
public:
  explicit DropRef(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      callExpr(
        anyOf(
          hasDeclaration(functionDecl(hasName("ref"))),
          hasDeclaration(functionDecl(hasName("unref")))
        )
      ).bind("ref");
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    auto sm = result.SourceManager;
    if (const CallExpr* p = result.Nodes.getNodeAs<CallExpr>(bind_name_)) {
      SourceRange sr = p->getSourceRange();
      if (sm->isMacroBodyExpansion(sr.getBegin())){
        auto macroLoc = sm->getImmediateExpansionRange(sr.getBegin());
        SourceRange loc (macroLoc.getBegin(), macroLoc.getEnd().getLocWithOffset(1));
        rewrite_.RemoveText(loc);
      }
    }
  }

private:
  Rewriter& rewrite_;
};

class SGOFunctionArg : public MatchFinder::MatchCallback {
  std::string const param_name_ = "param";
public:
  explicit SGOFunctionArg(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      parmVarDecl(
        hasType(sgo_like_ptr_matcher())
      ).bind(param_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *context = result.Context;
    if (const ParmVarDecl* p = result.Nodes.getNodeAs<ParmVarDecl>(param_name_)) {
      SourceRange sr = p->getSourceRange();
      if (sr.isValid()) {
        std::string sharedPtr = "std::shared_ptr<";
        rewrite_.InsertTextBefore(sr.getBegin(), sharedPtr);
        PointerTypeLoc Q = p->getTypeSourceInfo()->getTypeLoc().getAs<PointerTypeLoc>();
        if (!Q.isNull())
          rewrite_.ReplaceText(Q.getStarLoc(), ">");
        //if (p->hasDefaultArg())
      }
    }
  }
private:
  Rewriter& rewrite_;
};

class SGOFunctionReturn : public MatchFinder::MatchCallback {
  std::string const bind_name_ = "return";
public:
  explicit SGOFunctionReturn(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      functionDecl(
        returns(sgo_like_ptr_matcher())
      ).bind(bind_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    if (const FunctionDecl* f = result.Nodes.getNodeAs<FunctionDecl>(bind_name_)) {
      SourceRange sr = f->getReturnTypeSourceRange();
      if (sr.isValid()) {
        rewrite_.InsertTextBefore(sr.getBegin(), "std::shared_ptr<");
        rewrite_.ReplaceText(sr.getEnd(), ">");
      }
    }
  }
private:
  Rewriter& rewrite_;
};


class SGOField : public MatchFinder::MatchCallback {
  std::string const bind_name_ = "field";
public:
  explicit SGOField(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      fieldDecl(
        hasType(
          sgo_like_ptr_matcher()
        )
      ).bind(bind_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    if (const FieldDecl* f = result.Nodes.getNodeAs<FieldDecl>(bind_name_)) {
      SourceRange sr = f->getSourceRange();
      if (sr.isValid()) {
        rewrite_.InsertTextBefore(sr.getBegin(), "std::shared_ptr<");
        PointerTypeLoc Q = f->getTypeSourceInfo()->getTypeLoc().getAs<PointerTypeLoc>();
        if (!Q.isNull())
          rewrite_.ReplaceText(Q.getStarLoc(), ">");
        else
          rewrite_.ReplaceText(f->getEndLoc().getLocWithOffset(-2), ">");
      }
    }
  }
private:
  Rewriter& rewrite_;
};

class SGOCtor : public MatchFinder::MatchCallback {
  std::string const ctor_name_ = "ctor";
  std::string const eq_name_ = "eq";
public:
  explicit SGOCtor(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
//      varDecl(
        //has(
          cxxNewExpr(
            hasType(
              sgo_like_ptr_matcher()
            )
          ).bind(ctor_name_);
        //)
//      ).bind(eq_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    if (const CXXNewExpr* e = result.Nodes.getNodeAs<CXXNewExpr>(ctor_name_)) {
      auto qualType = e->getAllocatedType();
      auto type = getType(qualType);
      if (!type.empty()) {
        auto ctorExpr = e->getConstructExpr();
        SourceRange newExprLocation(e->getBeginLoc(), (ctorExpr ? ctorExpr->getBeginLoc() : e->getEndLoc()));
        rewrite_.ReplaceText(newExprLocation, "");
        auto replacement = "std::make_shared<" + type + ">";
        rewrite_.ReplaceText(newExprLocation, replacement);

        auto parents = Context->getParents(*e);
        for (auto p: parents) {
          if (const VarDecl* v = p.get<VarDecl>()) {
            auto qualType = v->getType();
            auto typePtr = qualType.getTypePtr();
            if (!typePtr->isUndeducedType()) {
              rewrite_.ReplaceText(v->getTypeSourceInfo()->getTypeLoc().getSourceRange (),  "auto");
            }
          }
        }
      }
    }
    /*
    if (const VarDecl* v = result.Nodes.getNodeAs<VarDecl>(eq_name_)) {
      auto qualType = v->getType();
      auto typePtr = qualType.getTypePtr();
      if (!typePtr->isUndeducedType()) {
        rewrite_.ReplaceText(v->getTypeSourceInfo()->getTypeLoc().getSourceRange (),  "auto");
      }
    }*/
  }
private:
  Rewriter& rewrite_;
};

class SGOTemplateCtor : public MatchFinder::MatchCallback {
  std::string const ctor_name_ = "ctor";
  std::string const var_name_ = "var";
public:
  explicit SGOTemplateCtor(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      varDecl(
        hasType(
          pointsTo(
            classTemplateSpecializationDecl(isDerivedFrom("CSGObject"))
          )
        ),
        hasDescendant(
          cxxNewExpr().bind(ctor_name_)
        )
      ).bind(var_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    if (const CXXNewExpr* e = result.Nodes.getNodeAs<CXXNewExpr>(ctor_name_)) {
      auto qualType = e->getAllocatedType();
      auto type = getType(qualType);
      if (!type.empty()) {
        SourceRange newExprLocation(e->getBeginLoc(), e->getConstructExpr()->getBeginLoc());
        rewrite_.ReplaceText(newExprLocation, "");
        auto replacement = "std::make_shared<" + type + ">";
        rewrite_.ReplaceText(newExprLocation, replacement);
      }
    }
    if (const VarDecl* v = result.Nodes.getNodeAs<VarDecl>(var_name_)) {
      auto qualType = v->getType();
      auto typePtr = qualType.getTypePtr();
      if (!typePtr->isUndeducedType()) {
        rewrite_.ReplaceText(v->getTypeSourceInfo()->getTypeLoc().getSourceRange (),  "auto");
      }
    }
  }
private:
  Rewriter& rewrite_;
};

class SGSome : public MatchFinder::MatchCallback {
  std::string const some_name_ = "some";
public:
  explicit SGSome(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      callExpr(
        callee(
          functionDecl(
            hasName("some")
          )
        )
      ).bind(some_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    if (const CallExpr* v = result.Nodes.getNodeAs<CallExpr>(some_name_)) {
      rewrite_.ReplaceText(v->getCallee()->getExprLoc(), "std::make_shared");
    }
  }
private:
  Rewriter& rewrite_;
};

class CommentSGADD : public MatchFinder::MatchCallback {
  std::string const sgadd_name_ = "sg_add";
public:
  explicit CommentSGADD(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return expr(anyOf(memberExpr(member(hasName("m_parameters"))), callExpr(callee(functionDecl(hasName("watch_param")))))).bind(sgadd_name_);
/*      callExpr(
        callee(
          functionDecl(
            hasName("watch_param")
          )
        )
      ).bind(sgadd_name_);
      */
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    auto sm = result.SourceManager;
    if (const CallExpr* p = result.Nodes.getNodeAs<CallExpr>(sgadd_name_)) {
      SourceRange sr = p->getSourceRange();
      if (sm->isMacroBodyExpansion(sr.getBegin())){
        auto macroLoc = sm->getExpansionRange(sr.getBegin());
        rewrite_.InsertTextBefore(macroLoc.getBegin(), "/*");
        rewrite_.InsertTextAfter(macroLoc.getEnd().getLocWithOffset(1), "*/");
      } else {
        rewrite_.InsertTextBefore(sr.getBegin(), "/*");
        rewrite_.InsertTextAfter(sr.getEnd().getLocWithOffset(1), "*/");
      }
    } else if (const MemberExpr* me = result.Nodes.getNodeAs<MemberExpr>(sgadd_name_)) {
        SourceRange sr = me->getSourceRange();
        rewrite_.InsertTextBefore(sr.getBegin(), "/*");
        rewrite_.InsertTextAfter(sr.getEnd().getLocWithOffset(1), "*/");
    }
  }
private:
  Rewriter& rewrite_;
};

class ExplicitCast : public MatchFinder::MatchCallback {
    std::string const cast_name_ = "cast";
public:
  explicit ExplicitCast(Rewriter& r) : rewrite_(r) {}

  auto matcher() {
    return
      explicitCastExpr(
        hasType(
          sgo_like_ptr_matcher()
        )
      ).bind(cast_name_);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    clang::ASTContext *Context = result.Context;
    auto sm = result.SourceManager;
    if (const ExplicitCastExpr* cast = result.Nodes.getNodeAs<ExplicitCastExpr>(cast_name_)) {
      auto qt = cast->getType();
      auto type = getType(qt);
      if(const CStyleCastExpr* cCast = dyn_cast<CStyleCastExpr>(cast)) {
        if (!type.empty()) {
          std::string replacement = "std::static_pointer_cast<" + type + ">(";
          rewrite_.ReplaceText(SourceRange( cCast->getBeginLoc(), cCast->getRParenLoc()), replacement);
          rewrite_.InsertTextAfter(cast->getEndLoc(), ")");
        }
      }
      else if(const CXXNamedCastExpr* namedCast = dyn_cast<CXXNamedCastExpr>(cast)) {
        if (!type.empty()) {
          rewrite_.RemoveText(CharSourceRange::getCharRange(namedCast->getBeginLoc(),
          result.SourceManager->getExpansionLoc(namedCast->getSubExpr()->getBeginLoc())));
          std::string replacement = "std::dynamic_pointer_cast<" + type + ">(";
          rewrite_.InsertTextBefore(namedCast->getBeginLoc(),replacement);
        }
      }

      auto parents = Context->getParents(*cast);
      for (auto p: parents) {
        if (const VarDecl* v = p.get<VarDecl>()) {
          auto qualType = v->getType();
          auto typePtr = qualType.getTypePtr();
          if (!typePtr->isUndeducedType()) {
            rewrite_.ReplaceText(v->getTypeSourceInfo()->getTypeLoc().getSourceRange (),  "auto");
          }
        }
      }
    }
  }
private:
  Rewriter& rewrite_;
};


class SmrtPtrizeASTConsumer : public ASTConsumer {
public:
  SmrtPtrizeASTConsumer(Rewriter &R) :
    funcArg_(R), dropRef_(R), funcReturn_(R),
    field_(R), ctor_(R), templateCtor_(R), some_(R),
    paramComment_(R), explicitCast_(R)
  {
    matcher_.addMatcher(funcArg_.matcher(), &funcArg_);
    matcher_.addMatcher(funcReturn_.matcher(), &funcReturn_);
    matcher_.addMatcher(field_.matcher(), &field_);
    matcher_.addMatcher(ctor_.matcher(), &ctor_);
    matcher_.addMatcher(dropRef_.matcher(), &dropRef_);
    //matcher_.addMatcher(templateCtor_.matcher(), &templateCtor_);
    matcher_.addMatcher(some_.matcher(), &some_);
    matcher_.addMatcher(paramComment_.matcher(), &paramComment_);
    matcher_.addMatcher(explicitCast_.matcher(), &explicitCast_);

  }

  void HandleTranslationUnit(ASTContext &Context) override {
    // Run the matchers when we have the whole TU parsed.
    matcher_.matchAST(Context);
  }

private:
  MatchFinder matcher_;
  SGOFunctionArg funcArg_;
  DropRef dropRef_;
  SGOFunctionReturn funcReturn_;
  SGOField field_;
  SGOCtor ctor_;
  SGOTemplateCtor templateCtor_;
  SGSome some_;
  CommentSGADD paramComment_;
  ExplicitCast explicitCast_;
};


class SmrtPtrizeFrontendAction : public ASTFrontendAction {
public:
  SmrtPtrizeFrontendAction() {}
  void EndSourceFileAction() override {
#if 0
    rewriter_.getEditBuffer(rewriter_.getSourceMgr().getMainFileID())
        .write(llvm::outs());
#else
    std::error_code error_code;
    auto fEntry = rewriter_.getSourceMgr().getFileEntryForID(rewriter_.getSourceMgr().getMainFileID());
    llvm::raw_fd_ostream outFile(fEntry->getName(), error_code, llvm::sys::fs::F_None);
    rewriter_.getEditBuffer(rewriter_.getSourceMgr().getMainFileID())
        .write(outFile);
    outFile.close();
#endif
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<SmrtPtrizeASTConsumer>(rewriter_);
  }

private:
  Rewriter rewriter_;
};


static llvm::cl::OptionCategory SmartPtrizeCategory("smart-ptrize options");

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, SmartPtrizeCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<SmrtPtrizeFrontendAction>().get());
}
