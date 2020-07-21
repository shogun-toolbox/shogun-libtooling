//===--- RefectormulticlassCheck.h - clang-tidy -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_REFECTORMULTICLASSCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_REFECTORMULTICLASSCHECK_H

#include "../ClangTidyCheck.h"
#include <string>
#include <unordered_set>
namespace clang
{
  namespace tidy
  {
    namespace misc
    {

      /// FIXME: Write a short description.
      ///
      /// For the user-facing documentation see:
      /// http://clang.llvm.org/extra/clang-tidy/checks/misc-RefectorMulticlass.html
      class RefectormulticlassCheck : public ClangTidyCheck
      {
      public:
        RefectormulticlassCheck(StringRef Name, ClangTidyContext *Context)
            : ClangTidyCheck(Name, Context) {}
        void registerMatchers(ast_matchers::MatchFinder *Finder) override;
        void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

      private:
        void traverseAllParentFunction(const CXXMethodDecl *method);

        std::string getFunctionName(const CXXMethodDecl *method);

        void changeAllSharedPtrToConst(const CXXMethodDecl *method);
        std::unordered_set<std::string> funcRefactored;
      };

    } // namespace misc
  }   // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_REFECTORMULTICLASSCHECK_H
