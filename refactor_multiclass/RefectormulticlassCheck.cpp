//===--- RefectormulticlassCheck.cpp - clang-tidy -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RefectormulticlassCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace misc {

void RefectormulticlassCheck::traverseAllParentFunction(const CXXMethodDecl* method){
	if(method->getParent()->getName()=="MulticlassMachine"){
		return ;
	}
	  for(auto methods = method->begin_overridden_methods (); 
	  		methods != method->end_overridden_methods (); ++methods){
				  llvm::outs()<<(*methods)-> getParent()->getName()<< "::" 
				  	<<(*methods)->getName()<<"\n";
						traverseAllParentFunction(*methods);
			  }
}
void RefectormulticlassCheck::registerMatchers(MatchFinder *Finder) {
 auto labels = memberExpr(member(hasName("m_labels")), 
 		hasAncestor(cxxRecordDecl(isSameOrDerivedFrom("MulticlassMachine"))));

  auto methods = cxxMethodDecl(hasDescendant(labels), 
              ofClass(isSameOrDerivedFrom("MulticlassMachine")),
			  unless(hasAnyParameter(parmVarDecl(hasType(asString("std::shared_ptr<Labels>")))))
			  );

  auto call_methods = cxxMemberCallExpr(callee(methods));
  Finder->addMatcher(labels.bind("addLabels"), this);
  Finder->addMatcher(methods.bind("addLabels"), this);
  Finder->addMatcher(call_methods.bind("addLabels"), this);
}

void RefectormulticlassCheck::check(const MatchFinder::MatchResult &Result) {
  if(const auto *MatchedDecl = Result.Nodes.getNodeAs<MemberExpr>("addLabels")){
	  auto removeRange = CharSourceRange::getCharRange(
		  MatchedDecl->getEndLoc(), MatchedDecl->getEndLoc().getLocWithOffset(sizeof("m_labels") - 1));
	 diag(MatchedDecl->getBeginLoc(), "memberExpr %0 will be changed")
      << MatchedDecl->getBase();
	  diag(MatchedDecl->getExprLoc(), "rename m_labels", DiagnosticIDs::Note)
	 	 <<FixItHint::CreateReplacement(removeRange, "labs");
  }
	if(const auto *MatchedDecl = Result.Nodes.getNodeAs<CXXMethodDecl>("addLabels")){
	  auto loc = MatchedDecl ->getTypeSourceInfo ()->getTypeLoc ().getAs<FunctionTypeLoc>();
	  llvm::outs()<<MatchedDecl->size_overridden_methods ()<<"\n";
	  for(auto methods = MatchedDecl->begin_overridden_methods (); 
	  	 methods != MatchedDecl->end_overridden_methods (); ++methods){
				  llvm::outs()<<(*methods)-> getParent()->getName()<<"\n";
			traverseAllParentFunction(*methods);
		}
	  diag(MatchedDecl->getBeginLoc(), "function %0 should add argument")
      << MatchedDecl->getName();
	   diag(MatchedDecl->getEndLoc(), "add labs argument", DiagnosticIDs::Note)
	  		<<FixItHint::CreateInsertion(loc.getLocalSourceRange ().getEnd(), 
			  	", const std::shared_ptr<Labels>& labs");
	
  }
  if(const auto *MatchedDecl = Result.Nodes.getNodeAs<CXXMemberCallExpr>("addLabels")){
	  auto parm_size = MatchedDecl->getMethodDecl () ->param_size ();
	  auto paramRange = MatchedDecl->getEndLoc();
	   diag(MatchedDecl->getEndLoc(), "callExpr %0 will be changed")
      << MatchedDecl->getMethodDecl()->getName();
	  StringRef inserted_context;
	  if(parm_size > 0){
		   inserted_context = ", labs" ;
	  }
	  else{
		   inserted_context = "labs" ;
	  }
	   diag(MatchedDecl->getEndLoc(), "add labs argument", DiagnosticIDs::Note)
	  <<FixItHint::CreateInsertion(paramRange, inserted_context);
  }
}

} // namespace misc
} // namespace tidy
} // namespace clang
