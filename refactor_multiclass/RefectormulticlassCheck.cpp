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
#include "clang/Basic/SourceLocation.h"
using namespace clang::ast_matchers;

namespace clang
{
	namespace tidy
	{
		namespace misc
		{

			void RefectormulticlassCheck::changeCorrespondMethod(
				const CXXMethodDecl *method, const SourceManager &SM)
			{

				auto MatchedDecl =
					method->getCorrespondingMethodDeclaredInClass(method->getParent());
				auto loc =
					MatchedDecl->getTypeSourceInfo()->getTypeLoc().getAs<FunctionTypeLoc>();
				// MatchedDecl->getBeginLoc().dump(SM);
				diag(MatchedDecl->getBeginLoc(), "function %0 should add argument")
					<< MatchedDecl->getNameAsString();
				diag(MatchedDecl->getEndLoc(), "add labs argument", DiagnosticIDs::Note)
					<< FixItHint::CreateInsertion(loc.getLocalSourceRange().getEnd(),
												  ", const std::shared_ptr<Labels>& labs");
			}
			void RefectormulticlassCheck::changeAllSharedPtrToConst(
				const CXXMethodDecl *method)
			{
				for (auto parm = method->param_begin(); parm != method->param_end(); ++parm)
				{
					if (StringRef((*parm)->getType().getAsString())
							.startswith("std::shared_ptr<"))
					{
						auto parmEndLoc = (*parm)->getEndLoc();
						if ((*parm)->hasDefaultArg())
						{

							auto range = (*parm)->getDefaultArgRange();
							auto typeRange =
								(*parm)->getTypeSourceInfo()->getTypeLoc().getSourceRange();
							diag(range.getBegin(), "remove %0 default argument")
								<< (*parm)->getNameAsString();
							diag(range.getBegin(), "remove default argument", DiagnosticIDs::Note)
								<< FixItHint::CreateRemoval(SourceRange(
									   typeRange.getEnd().getLocWithOffset(1), range.getEnd()));
							parmEndLoc = typeRange.getEnd().getLocWithOffset(1);
						}
						diag((*parm)->getEndLoc(), "add const reference")
							<< (*parm)->getNameAsString();
						diag((*parm)->getBeginLoc(), "remove labs argument", DiagnosticIDs::Note)
							<< FixItHint::CreateInsertion((*parm)->getBeginLoc(), "const ")
							<< FixItHint::CreateInsertion(parmEndLoc, "&");
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

				auto Initializer = cxxConstructorDecl(
					hasAnyConstructorInitializer(
						cxxCtorInitializer(isBaseInitializer()).bind("init")),
					ofClass(isDerivedFrom("MulticlassMachine")));
				Finder->addMatcher(labels.bind("addLabels"), this);
				Finder->addMatcher(constructor.bind("removeLabels"), this);
				Finder->addMatcher(methods.bind("addLabels"), this);
				Finder->addMatcher(call_methods.bind("addLabels"), this);
				Finder->addMatcher(Initializer, this);
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
					const auto &SM = *(Result.SourceManager);
					auto loc =
						MatchedDecl->getTypeSourceInfo()->getTypeLoc().getAs<FunctionTypeLoc>();
					// llvm::outs()<<MatchedDecl->size_overridden_methods ()<<"\n";
					for (auto methods = MatchedDecl->begin_overridden_methods();
						 methods != MatchedDecl->end_overridden_methods(); ++methods)
					{
						// llvm::outs() << (*methods)->getParent()->getNameAsString() << "\n";
						traverseAllParentFunction(*methods);
					}
					if (auto redecl = MatchedDecl->getPreviousDecl())
					{
						if (auto res = cast_or_null<CXXMethodDecl>(redecl))
						{
							changeCorrespondMethod(res, SM);
							changeAllSharedPtrToConst(res);
						}
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
						for (auto parm : MatchedDecl->parameters())
						{
							if (parm->getType().getAsString() == "std::shared_ptr<Labels>")
							{
								auto loc = SourceRange(parm->getBeginLoc(), parm->getEndLoc());
								const auto &SM = *(Result.SourceManager);
								loc.dump(SM);
								diag(MatchedDecl->getEndLoc(), "should remove argument")
									<< MatchedDecl->getNameAsString();
								diag(MatchedDecl->getEndLoc(), "remove labs argument",
									 DiagnosticIDs::Note)
									<< FixItHint::CreateRemoval(loc);
							}
						}
					}
				}
				if (const auto *MatchedDecl =
						Result.Nodes.getNodeAs<CXXCtorInitializer>("init"))
				{
					if (auto res = dyn_cast<ExprWithCleanups>(MatchedDecl->getInit()))
					{
						for (auto child : res->children())
						{
							if (auto expr = dyn_cast<CXXConstructExpr>(child))
							{
								for (auto e : expr->arguments())
								{
									if (e->getType().getAsString() == "std::shared_ptr<Labels>")
									{
										auto loc = e->getSourceRange();

										diag(loc.getBegin(), "should remove argument");
										diag(loc.getBegin(), "remove labs argument", DiagnosticIDs::Note)
											<< FixItHint::CreateRemoval(loc);
									}
								}
							}
						}
					}
				}

			} // namespace misc

		} // namespace misc
	}	  // namespace tidy
} // namespace clang
