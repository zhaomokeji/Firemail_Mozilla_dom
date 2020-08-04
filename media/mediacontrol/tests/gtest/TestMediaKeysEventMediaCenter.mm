/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#import "MediaPlayerWrapper.h"

#include "MediaHardwareKeysEventSourceMacMediaCenter.h"
#include "MediaKeyListenerTest.h"
#include "nsCocoaFeatures.h"

extern _Nullable Class mpNowPlayingInfoCenterClass;

extern _Nullable Class mpRemoteCommandCenterClass;

extern _Nullable Class mpRemoteCommandClass;

using namespace mozilla::dom;

NS_ASSUME_NONNULL_BEGIN

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPlayPauseEvent)
{
  if (nsCocoaFeatures::IsAtLeastVersion(10, 12, 2)) {
    ASSERT_TRUE(MediaPlayerWrapperInit());
    RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
        new MediaHardwareKeysEventSourceMacMediaCenter();

    ASSERT_TRUE(source->GetListenersNum() == 0);

    RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

    MPNowPlayingInfoCenter* center =
        (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];

    source->AddListener(listener.get());

    ASSERT_TRUE(source->Open());

    ASSERT_TRUE(source->GetListenersNum() == 1);
    ASSERT_TRUE(!listener->IsReceivedResult());
    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

    MediaCenterEventHandler playPauseHandler = source->CreatePlayPauseHandler();
    playPauseHandler(nil);

    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePaused);
    ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Playpause));

    listener->Clear();  // Reset stored media key

    playPauseHandler(nil);

    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);
    ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Playpause));
    MediaPlayerWrapperClose();
  }
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPlayEvent)
{
  if (nsCocoaFeatures::IsAtLeastVersion(10, 12, 2)) {
    ASSERT_TRUE(MediaPlayerWrapperInit());
    RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
        new MediaHardwareKeysEventSourceMacMediaCenter();

    ASSERT_TRUE(source->GetListenersNum() == 0);

    RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

    MPNowPlayingInfoCenter* center =
        (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];

    source->AddListener(listener.get());

    ASSERT_TRUE(source->Open());

    ASSERT_TRUE(source->GetListenersNum() == 1);
    ASSERT_TRUE(!listener->IsReceivedResult());
    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

    MediaCenterEventHandler playHandler = source->CreatePlayHandler();

    center.playbackState = MPNowPlayingPlaybackStatePaused;

    playHandler(nil);

    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);
    ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Play));
    MediaPlayerWrapperClose();
  }
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPauseEvent)
{
  if (nsCocoaFeatures::IsAtLeastVersion(10, 12, 2)) {
    ASSERT_TRUE(MediaPlayerWrapperInit());
    RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
        new MediaHardwareKeysEventSourceMacMediaCenter();

    ASSERT_TRUE(source->GetListenersNum() == 0);

    RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

    MPNowPlayingInfoCenter* center =
        (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];

    source->AddListener(listener.get());

    ASSERT_TRUE(source->Open());

    ASSERT_TRUE(source->GetListenersNum() == 1);
    ASSERT_TRUE(!listener->IsReceivedResult());
    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

    MediaCenterEventHandler pauseHandler = source->CreatePauseHandler();

    pauseHandler(nil);

    ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePaused);
    ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Pause));
    MediaPlayerWrapperClose();
  }
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPrevNextEvent)
{
  if (nsCocoaFeatures::IsAtLeastVersion(10, 12, 2)) {
    ASSERT_TRUE(MediaPlayerWrapperInit());
    RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
        new MediaHardwareKeysEventSourceMacMediaCenter();

    ASSERT_TRUE(source->GetListenersNum() == 0);

    RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

    source->AddListener(listener.get());

    ASSERT_TRUE(source->Open());

    MediaCenterEventHandler nextHandler = source->CreateNextTrackHandler();

    nextHandler(nil);

    ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Nexttrack));

    MediaCenterEventHandler previousHandler = source->CreatePreviousTrackHandler();

    previousHandler(nil);

    ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Previoustrack));
    MediaPlayerWrapperClose();
  }
}

NS_ASSUME_NONNULL_END
