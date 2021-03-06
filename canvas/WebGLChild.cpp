/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLChild.h"

#include "ClientWebGLContext.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla {
namespace dom {

WebGLChild::WebGLChild(ClientWebGLContext& context) : mContext(&context) {}

WebGLChild::~WebGLChild() { (void)Send__delete__(this); }

// -

static constexpr size_t kDefaultCmdsShmemSize = 100 * 1000;

Maybe<Range<uint8_t>> WebGLChild::AllocPendingCmdBytes(const size_t size) {
  if (!mPendingCmdsShmem) {
    size_t capacity = kDefaultCmdsShmemSize;
    if (capacity < size) {
      capacity = size;
    }

    auto shmem = webgl::RaiiShmem::Alloc(
        this, capacity,
        mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC);
    MOZ_ASSERT(shmem);
    if (!shmem) return {};
    mPendingCmdsShmem = std::move(shmem);
    mPendingCmdsPos = 0;
  }

  const auto range = mPendingCmdsShmem.ByteRange();

  const auto remaining =
      Range<uint8_t>{range.begin() + mPendingCmdsPos, range.end()};
  if (size > remaining.length()) {
    FlushPendingCmds();
    return AllocPendingCmdBytes(size);
  }
  mPendingCmdsPos += size;
  return Some(Range<uint8_t>{remaining.begin(), remaining.begin() + size});
}

void WebGLChild::FlushPendingCmds() {
  if (!mPendingCmdsShmem) return;

  const auto byteSize = mPendingCmdsPos;
  SendDispatchCommands(mPendingCmdsShmem.Extract(), byteSize);

  mFlushedCmdInfo.flushes += 1;
  mFlushedCmdInfo.flushedCmdBytes += byteSize;

  printf_stderr("[WebGLChild] Flushed %zu bytes. (%zu over %zu flushes)\n",
                byteSize, mFlushedCmdInfo.flushedCmdBytes,
                mFlushedCmdInfo.flushes);
}

// -

mozilla::ipc::IPCResult WebGLChild::RecvJsWarning(
    const std::string& text) const {
  if (!mContext) return IPC_OK();
  mContext->JsWarning(text);
  return IPC_OK();
}

mozilla::ipc::IPCResult WebGLChild::RecvOnContextLoss(
    const webgl::ContextLossReason reason) const {
  if (!mContext) return IPC_OK();
  mContext->OnContextLoss(reason);
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
