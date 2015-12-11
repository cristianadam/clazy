/*
   This file is part of the clang-lazy static checker.

  Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Sérgio Martins <sergio.martins@kdab.com>

  Copyright (C) 2015 Sergio Martins <smartins@kde.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

  As a special exception, permission is given to link this program
  with any edition of Qt, and distribute the resulting executable,
  without including the source code for Qt in the source distribution.
*/

#include "qdatetimeutc.h"
#include "Utils.h"
#include "checkmanager.h"
#include "StringUtils.h"
#include "FixItUtils.h"

#include <clang/AST/AST.h>
#include <clang/Lex/Lexer.h>

using namespace clang;
using namespace std;

enum Fixit {
    FixitNone = 0,
    FixitAll = 0x1 // More granularity isn't needed I guess
};

QDateTimeUtc::QDateTimeUtc(const std::string &name)
    : CheckBase(name)
{
}

void QDateTimeUtc::VisitStmt(clang::Stmt *stmt)
{
    CXXMemberCallExpr *secondCall = dyn_cast<CXXMemberCallExpr>(stmt);
    if (!secondCall || !secondCall->getMethodDecl())
        return;
    CXXMethodDecl *secondMethod = secondCall->getMethodDecl();
    const string secondMethodName = secondMethod->getQualifiedNameAsString();
    const bool isTimeT = secondMethodName == "QDateTime::toTime_t";
    if (!isTimeT && secondMethodName != "QDateTime::toUTC")
        return;

    vector<CallExpr*> chainedCalls = Utils::callListForChain(secondCall);
    if (chainedCalls.size() < 2)
        return;

    CallExpr *firstCall = chainedCalls[chainedCalls.size() - 1];
    FunctionDecl *firstFunc = firstCall->getDirectCallee();
    if (!firstFunc)
        return;

    CXXMethodDecl *firstMethod = dyn_cast<CXXMethodDecl>(firstFunc);
    if (!firstMethod || firstMethod->getQualifiedNameAsString() != "QDateTime::currentDateTime")
        return;

    std::vector<FixItHint> fixits;
    Expr *implicitArgument = secondCall->getImplicitObjectArgument();
    if (isFixitEnabled(FixitAll) && implicitArgument) {
        SourceLocation start = implicitArgument->getLocStart();
        start = FixItUtils::locForEndOfToken(start, 0);
        const SourceLocation end = secondCall->getLocEnd();
        if (start.isValid() && end.isValid()) {
            std::string replacement = "::currentDateTimeUtc()";
            if (isTimeT) {
                replacement += ".toTime_t()";
            }
            fixits.push_back(FixItUtils::createReplacement({ start, end }, replacement));
        } else {
            // This shouldn't happen
            queueManualFixitWarning(secondCall->getLocStart(), FixitAll);
        }
    }

    if (isTimeT)
        emitWarning(stmt->getLocStart(), "Use QDateTime::currentDateTimeUtc().toTime_t() instead", fixits);
    else
        emitWarning(stmt->getLocStart(), "Use QDateTime::currentDateTimeUtc() instead", fixits);
}

const char *const s_checkName = "qdatetime-utc";
REGISTER_CHECK_WITH_FLAGS(s_checkName, QDateTimeUtc, CheckLevel0)
REGISTER_FIXIT(FixitAll, "fix-qdatetime-utc", s_checkName)
