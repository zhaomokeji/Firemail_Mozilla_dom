/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheCommon_h
#define mozilla_dom_cache_CacheCommon_h

#include "mozilla/dom/quota/QuotaCommon.h"

// Cache equivalent of QM_TRY.
#define CACHE_TRY(...) QM_TRY_META(mozilla::dom::cache, ##__VA_ARGS__)

// Cache equivalent of QM_TRY_VAR.
#define CACHE_TRY_VAR(...) QM_TRY_VAR_META(mozilla::dom::cache, ##__VA_ARGS__)

namespace mozilla::dom::cache {

void HandleError(const nsLiteralCString& aExpr,
                 const nsLiteralCString& aSourceFile, int32_t aSourceLine);

}  // namespace mozilla::dom::cache

#endif  // mozilla_dom_cache_CacheCommon_h
