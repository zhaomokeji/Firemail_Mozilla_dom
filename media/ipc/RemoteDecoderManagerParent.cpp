/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerParent.h"

#if XP_WIN
#  include <objbase.h>
#endif

#include "ImageContainer.h"
#include "RemoteAudioDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"  // for MediaThreadType
#include "mozilla/SyncRunnable.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/VideoBridgeChild.h"

namespace mozilla {

#ifdef XP_WIN
extern const nsCString GetFoundD3D11BlacklistedDLL();
extern const nsCString GetFoundD3D9BlacklistedDLL();
#endif  // XP_WIN

using namespace ipc;
using namespace layers;
using namespace gfx;

StaticRefPtr<TaskQueue> sRemoteDecoderManagerParentThread;

SurfaceDescriptorGPUVideo RemoteDecoderManagerParent::StoreImage(
    Image* aImage, TextureClient* aTexture) {
  SurfaceDescriptorRemoteDecoder ret;
  aTexture->GetSurfaceDescriptorRemoteDecoder(&ret);

  mImageMap[ret.handle()] = aImage;
  mTextureMap[ret.handle()] = aTexture;
  return ret;
}

class RemoteDecoderManagerThreadShutdownObserver : public nsIObserver {
  virtual ~RemoteDecoderManagerThreadShutdownObserver() = default;

 public:
  RemoteDecoderManagerThreadShutdownObserver() = default;

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    RemoteDecoderManagerParent::ShutdownVideoBridge();
    RemoteDecoderManagerParent::ShutdownThreads();
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(RemoteDecoderManagerThreadShutdownObserver, nsIObserver);

bool RemoteDecoderManagerParent::StartupThreads() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sRemoteDecoderManagerParentThread) {
    return true;
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return false;
  }

  sRemoteDecoderManagerParentThread = new TaskQueue(
      GetMediaThreadPool(MediaThreadType::PLAYBACK), "RemVidParent");
  if (XRE_IsGPUProcess()) {
    MOZ_ALWAYS_SUCCEEDS(
        sRemoteDecoderManagerParentThread->Dispatch(NS_NewRunnableFunction(
            "RemoteDecoderManagerParent::StartupThreads",
            []() { layers::VideoBridgeChild::StartupForGPUProcess(); })));
  }

  auto* obs = new RemoteDecoderManagerThreadShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  return true;
}

void RemoteDecoderManagerParent::ShutdownThreads() {
  sRemoteDecoderManagerParentThread->BeginShutdown();
  sRemoteDecoderManagerParentThread->AwaitShutdownAndIdle();
  sRemoteDecoderManagerParentThread = nullptr;
}

/* static */
void RemoteDecoderManagerParent::ShutdownVideoBridge() {
  if (sRemoteDecoderManagerParentThread) {
    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "RemoteDecoderManagerParent::ShutdownVideoBridge",
        []() { VideoBridgeChild::Shutdown(); });
    SyncRunnable::DispatchToThread(sRemoteDecoderManagerParentThread, task);
  }
}

bool RemoteDecoderManagerParent::OnManagerThread() {
  return sRemoteDecoderManagerParentThread->IsOnCurrentThread();
}

bool RemoteDecoderManagerParent::CreateForContent(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_RDD ||
             XRE_GetProcessType() == GeckoProcessType_GPU);
  MOZ_ASSERT(NS_IsMainThread());

  if (!StartupThreads()) {
    return false;
  }

  RefPtr<RemoteDecoderManagerParent> parent =
      new RemoteDecoderManagerParent(sRemoteDecoderManagerParentThread);

  RefPtr<Runnable> task =
      NewRunnableMethod<Endpoint<PRemoteDecoderManagerParent>&&>(
          "dom::RemoteDecoderManagerParent::Open", parent,
          &RemoteDecoderManagerParent::Open, std::move(aEndpoint));
  MOZ_ALWAYS_SUCCEEDS(
      sRemoteDecoderManagerParentThread->Dispatch(task.forget()));
  return true;
}

bool RemoteDecoderManagerParent::CreateVideoBridgeToOtherProcess(
    Endpoint<PVideoBridgeChild>&& aEndpoint) {
  // We never want to decode in the GPU process, but output
  // frames to the parent process.
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_RDD);
  MOZ_ASSERT(NS_IsMainThread());

  if (!StartupThreads()) {
    return false;
  }

  RefPtr<Runnable> task =
      NewRunnableFunction("gfx::VideoBridgeChild::Open",
                          &VideoBridgeChild::Open, std::move(aEndpoint));
  MOZ_ALWAYS_SUCCEEDS(
      sRemoteDecoderManagerParentThread->Dispatch(task.forget()));
  return true;
}

RemoteDecoderManagerParent::RemoteDecoderManagerParent(
    nsISerialEventTarget* aThread)
    : mThread(aThread) {
  MOZ_COUNT_CTOR(RemoteDecoderManagerParent);
}

RemoteDecoderManagerParent::~RemoteDecoderManagerParent() {
  MOZ_COUNT_DTOR(RemoteDecoderManagerParent);
}

void RemoteDecoderManagerParent::ActorDestroy(
    mozilla::ipc::IProtocol::ActorDestroyReason) {
  mThread = nullptr;
}

PRemoteDecoderParent* RemoteDecoderManagerParent::AllocPRemoteDecoderParent(
    const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier, bool* aSuccess,
    nsCString* aErrorDescription) {
  RefPtr<TaskQueue> decodeTaskQueue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                    "RemoteVideoDecoderParent::mDecodeTaskQueue");

  if (aRemoteDecoderInfo.type() ==
      RemoteDecoderInfoIPDL::TVideoDecoderInfoIPDL) {
    const VideoDecoderInfoIPDL& decoderInfo =
        aRemoteDecoderInfo.get_VideoDecoderInfoIPDL();
    return new RemoteVideoDecoderParent(
        this, decoderInfo.videoInfo(), decoderInfo.framerate(), aOptions,
        aIdentifier, sRemoteDecoderManagerParentThread, decodeTaskQueue,
        aSuccess, aErrorDescription);
  } else if (aRemoteDecoderInfo.type() == RemoteDecoderInfoIPDL::TAudioInfo) {
    return new RemoteAudioDecoderParent(
        this, aRemoteDecoderInfo.get_AudioInfo(), aOptions,
        sRemoteDecoderManagerParentThread, decodeTaskQueue, aSuccess,
        aErrorDescription);
  }

  MOZ_CRASH("unrecognized type of RemoteDecoderInfoIPDL union");
  return nullptr;
}

bool RemoteDecoderManagerParent::DeallocPRemoteDecoderParent(
    PRemoteDecoderParent* actor) {
  RemoteDecoderParent* parent = static_cast<RemoteDecoderParent*>(actor);
  parent->Destroy();
  return true;
}

void RemoteDecoderManagerParent::Open(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind RemoteDecoderManagerParent to endpoint");
  }
  AddRef();
}

void RemoteDecoderManagerParent::ActorDealloc() { Release(); }

mozilla::ipc::IPCResult RemoteDecoderManagerParent::RecvReadback(
    const SurfaceDescriptorGPUVideo& aSD, SurfaceDescriptor* aResult) {
  const SurfaceDescriptorRemoteDecoder& sd = aSD;
  RefPtr<Image> image = mImageMap[sd.handle()];
  if (!image) {
    *aResult = null_t();
    return IPC_OK();
  }

  RefPtr<SourceSurface> source = image->GetAsSourceSurface();
  if (!source) {
    *aResult = null_t();
    return IPC_OK();
  }

  SurfaceFormat format = source->GetFormat();
  IntSize size = source->GetSize();
  size_t length = ImageDataSerializer::ComputeRGBBufferSize(size, format);

  Shmem buffer;
  if (!length ||
      !AllocShmem(length, Shmem::SharedMemory::TYPE_BASIC, &buffer)) {
    *aResult = null_t();
    return IPC_OK();
  }

  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
      gfx::BackendType::CAIRO, buffer.get<uint8_t>(), size,
      ImageDataSerializer::ComputeRGBStride(format, size.width), format);
  if (!dt) {
    DeallocShmem(buffer);
    *aResult = null_t();
    return IPC_OK();
  }

  dt->CopySurface(source, IntRect(0, 0, size.width, size.height), IntPoint());
  dt->Flush();

  *aResult = SurfaceDescriptorBuffer(RGBDescriptor(size, format, true),
                                     MemoryOrShmem(std::move(buffer)));
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemoteDecoderManagerParent::RecvDeallocateSurfaceDescriptorGPUVideo(
    const SurfaceDescriptorGPUVideo& aSD) {
  const SurfaceDescriptorRemoteDecoder& sd = aSD;
  mImageMap.erase(sd.handle());
  mTextureMap.erase(sd.handle());
  return IPC_OK();
}

}  // namespace mozilla
