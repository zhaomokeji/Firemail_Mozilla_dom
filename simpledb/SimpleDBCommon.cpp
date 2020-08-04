/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleDBCommon.h"

namespace mozilla {
namespace dom {

const char* kPrefSimpleDBEnabled = "dom.simpleDB.enabled";

namespace simpledb {

void HandleError(const nsLiteralCString& aExpr,
                 const nsLiteralCString& aSourceFile, int32_t aSourceLine) {
#ifdef DEBUG
  NS_DebugBreak(NS_DEBUG_WARNING, "Error", aExpr.get(), aSourceFile.get(),
                aSourceLine);
#endif

  // TODO: Report to browser console
}

}  // namespace simpledb

}  // namespace dom
}  // namespace mozilla
