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

namespace clang
{
	namespace tidy
	{
		namespace misc
		{

			void RefectormulticlassCheck::changeAllSharedPtrToConst(
				const CXXMethodDecl *method)
			{
				for (auto parm = method->param_begin(); parm != method->param_end(); ++parm)
				{
					if (StringRef((*parm)->getType().getAsString())
							.startswith("std::shared_ptr<"))
					{
						diag((*parm)->getEndLoc(), "add const reference")
							<< (*parm)->getNameAsString();
						diag((*parm)->getBeginLoc(), "remove labs argument", DiagnosticIDs::Note)
							<< FixItHint::CreateInsertion((*parm)->getBeginLoc(), "const ")
							<< FixItHint::CreateInsertion((*parm)->getEndLoc(), "&");
					}
				}
			}
			std::string
			RefectormulticlassCheck::getFunctionName(const CXXMethodDecl *method)
			{
				std::string func_name = method->getNameAsString();
				std::string record_name = method->getParent()->getNameAsString();
				return record_name + "::" + func_name;
			}
			void RefectormulticlassCheck::traverseAllParentFunction(
				const CXXMethodDecl *method)
			{
				if (method->getParent()->getNameAsString() == "Machine")
				{
					return;
				}
				changeAllSharedPtrToConst(method);
				std::string funcName = getFunctionName(method);
				if (funcRefactored.find(funcName) == funcRefactored.end())
				{
					funcRefactored.insert(funcName);
					auto loc =
						method->getTypeSourceInfo()->getTypeLoc().getAs<FunctionTypeLoc>();
					diag(method->getBeginLoc(), "function %0 should add argument")
						<< method->getNameAsString();
					diag(method->getEndLoc(), "add labs argument", DiagnosticIDs::Note)
						<< FixItHint::CreateInsertion(loc.getLocalSourceRange().getEnd(),
													  ", const std::shared_ptr<Labels>& labs");
				}
				for (auto methods = method->begin_overridden_methods();
					 methods != method->end_overridden_methods(); ++methods)
				{
					// llvm::outs() << (*methods)->getParent()->getNameAsString()
					//              << "::" << (*methods)->getNameAsString() << "\n";
					traverseAllParentFunction(*methods);
				}
			}
			void RefectormulticlassCheck::registerMatchers(MatchFinder *Finder)
			{
				auto labels = memberExpr(member(hasName("m_labels")),
										 hasAncestor(cxxMethodDecl(ofClass(
											 isSameOrDerivedFrom("MulticlassMachine")))));
				// cxxConstructorDecl(hasAnyParameter(parmVarDecl(hasType(asString("std::shared_ptr<Labels>")))))
				auto methods = cxxMethodDecl(
					hasDescendant(labels), ofClass(isSameOrDerivedFrom("MulticlassMachine")),
					unless(hasAnyParameter(
						parmVarDecl(hasType(asString("std::shared_ptr<Labels>"))))));
				auto constructor =
					cxxConstructorDecl(hasAnyParameter(parmVarDecl(
										   hasType(asString("std::shared_ptr<Labels>")))),
									   ofClass(isSameOrDerivedFrom("MulticlassMachine")));

				auto call_methods = cxxMemberCallExpr(callee(methods));
				Finder->addMatcher(labels.bind("addLabels"), this);
				Finder->addMatcher(constructor.bind("removeLabels"), this);
				Finder->addMatcher(methods.bind("addLabels"), this);
				Finder->addMatcher(call_methods.bind("addLabels"), this);
			}

			void RefectormulticlassCheck::check(const MatchFinder::MatchResult &Result)
			{
				if (const auto *MatchedDecl =
						Result.Nodes.getNodeAs<MemberExpr>("addLabels"))
				{
					auto removeRange = CharSourceRange::getCharRange(
						MatchedDecl->getEndLoc(),
						MatchedDecl->getEndLoc().getLocWithOffset(sizeof("m_labels") - 1));
					diag(MatchedDecl->getBeginLoc(), "memberExpr %0 will be changed")
						<< MatchedDecl->getBase();
					diag(MatchedDecl->getExprLoc(), "rename m_labels", DiagnosticIDs::Note)
						<< FixItHint::CreateReplacement(removeRange, "labs");
				}
				if (const auto *MatchedDecl =
						Result.Nodes.getNodeAs<CXXMethodDecl>("addLabels"))
				{
					changeAllSharedPtrToConst(MatchedDecl);
					auto loc =
						MatchedDecl->getTypeSourceInfo()->getTypeLoc().getAs<FunctionTypeLoc>();
					// llvm::outs()<<MatchedDecl->size_overridden_methods ()<<"\n";
					for (auto methods = MatchedDecl->begin_overridden_methods();
						 methods != MatchedDecl->end_overridden_methods(); ++methods)
					{
						// llvm::outs() << (*methods)->getParent()->getNameAsString() << "\n";
						traverseAllParentFunction(*methods);
					}
					std::string funcName = getFunctionName(MatchedDecl);
					if (funcRefactored.find(funcName) == funcRefactored.end())
					{
						funcRefactored.insert(funcName);
						diag(MatchedDecl->getBeginLoc(), "function %0 should add argument")
							<< MatchedDecl->getNameAsString();
						diag(MatchedDecl->getEndLoc(), "add labs argument", DiagnosticIDs::Note)
							<< FixItHint::CreateInsertion(
								   loc.getLocalSourceRange().getEnd(),
								   ", const std::shared_ptr<Labels>& labs");
					}
				}
				if (const auto *MatchedDecl =
						Result.Nodes.getNodeAs<CXXMemberCallExpr>("addLabels"))
				{
					auto parm_size = MatchedDecl->getMethodDecl()->param_size();
					auto paramRange = MatchedDecl->getEndLoc();

					diag(MatchedDecl->getEndLoc(), "callExpr %0 will be changed")
						<< MatchedDecl->getMethodDecl()->getNameAsString();
					std::string inserted_context;
					if (parm_size > 0)
					{
						inserted_context = ", labs";
					}
					else
					{
						inserted_context = "labs";
					}
					diag(MatchedDecl->getEndLoc(), "add labs argument", DiagnosticIDs::Note)
						<< FixItHint::CreateInsertion(paramRange, inserted_context);
				}

				if (const auto *MatchedDecl =
						Result.Nodes.getNodeAs<CXXConstructorDecl>("removeLabels"))
				{

					if (MatchedDecl->param_size() == 1 &&
						MatchedDecl->getParamDecl(0)->getType().getAsString() ==
							"std::shared_ptr<Labels>")
					{
						diag(MatchedDecl->getEndLoc(), "should remove argument")
							<< MatchedDecl->getNameAsString();
						diag(MatchedDecl->getEndLoc(), "should remove argument",
							 DiagnosticIDs::Note)
							<< FixItHint::CreateRemoval(MatchedDecl->getSourceRange());
					}
					else
					{
						for (auto parm = MatchedDecl->param_begin();
							 parm != MatchedDecl->param_end(); ++parm)
						{
							if ((*parm)->getType().getAsString() == "std::shared_ptr<Labels>")
							{
								SourceRange loc((*parm)->getSourceRange());
								diag(MatchedDecl->getEndLoc(), "should remove argument")
									<< MatchedDecl->getNameAsString();
								diag(MatchedDecl->getEndLoc(), "remove labs argument",
									 DiagnosticIDs::Note)
									<< FixItHint::CreateRemoval(loc);
							}
						}
					}
				}
			}

		} // namespace misc
	}	  // namespace tidy
} // namespace clang
