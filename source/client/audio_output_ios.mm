//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/audio_output_ios.h"

#import <AVFoundation/AVFoundation.h>

#include <cstring>

#include "base/logging.h"

namespace {

// Every buffer holds 20 ms: two of the 10 ms chunks the decoder hands out at a time. Three of them
// are queued, so the delay the queue adds stays at about 60 ms.
const UInt32 kFramesPerBuffer = AudioOutput::kSampleRate * 20 / 1000;
const UInt32 kBytesPerFrame = AudioOutput::kChannels * AudioOutput::kBytesPerSample;
const UInt32 kBytesPerBuffer = kFramesPerBuffer * kBytesPerFrame;

//--------------------------------------------------------------------------------------------------
// The playback category keeps the sound on when the ring/silent switch is set to silent, and
// mixing with others leaves music of other applications playing.
void activateAudioSession()
{
    @autoreleasepool
    {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;

        if (![session setCategory:AVAudioSessionCategoryPlayback
                      withOptions:AVAudioSessionCategoryOptionMixWithOthers
                            error:&error])
        {
            LOG(ERROR) << "Unable to set the audio session category:"
                       << QString::fromNSString([error localizedDescription]);
        }

        if (![session setActive:YES error:&error])
        {
            LOG(ERROR) << "Unable to activate the audio session:"
                       << QString::fromNSString([error localizedDescription]);
        }
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
AudioOutputIos::AudioOutputIos(const NeedMoreDataCB& need_more_data_cb)
    : AudioOutput(need_more_data_cb)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
AudioOutputIos::~AudioOutputIos()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputIos::start()
{
    QMutexLocker locker(&mutex_);

    if (queue_)
        return true;

    activateAudioSession();

    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mSampleRate = kSampleRate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = kBytesPerFrame;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = kBytesPerFrame;
    format.mChannelsPerFrame = kChannels;
    format.mBitsPerChannel = kBitsPerSample;

    // A null run loop makes the queue call back on an internal thread of its own.
    OSStatus status = AudioQueueNewOutput(
        &format, &AudioOutputIos::outputCallback, this, nullptr, nullptr, 0, &queue_);
    if (status != noErr)
    {
        LOG(ERROR) << "AudioQueueNewOutput failed:" << status;
        queue_ = nullptr;
        return false;
    }

    running_.store(true, std::memory_order_release);

    for (int i = 0; i < kBufferCount; ++i)
    {
        status = AudioQueueAllocateBuffer(queue_, kBytesPerBuffer, &buffers_[i]);
        if (status != noErr)
        {
            LOG(ERROR) << "AudioQueueAllocateBuffer failed:" << status;
            locker.unlock();
            stop();
            return false;
        }

        // The first buffers are primed with whatever is ready (silence at the start).
        fillBuffer(buffers_[i]);
    }

    status = AudioQueueStart(queue_, nullptr);
    if (status != noErr)
    {
        LOG(ERROR) << "AudioQueueStart failed:" << status;
        locker.unlock();
        stop();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioOutputIos::stop()
{
    QMutexLocker locker(&mutex_);

    if (!queue_)
        return;

    running_.store(false, std::memory_order_release);

    // Stopping synchronously waits for the callback in flight, then disposing frees the buffers.
    AudioQueueStop(queue_, true);
    AudioQueueDispose(queue_, true);

    queue_ = nullptr;
    for (auto& buffer : buffers_)
        buffer = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
void AudioOutputIos::outputCallback(void* user_data, AudioQueueRef /* queue */,
                                    AudioQueueBufferRef buffer)
{
    AudioOutputIos* self = static_cast<AudioOutputIos*>(user_data);
    if (!self->running_.load(std::memory_order_acquire))
        return;

    self->fillBuffer(buffer);
}

//--------------------------------------------------------------------------------------------------
void AudioOutputIos::fillBuffer(AudioQueueBufferRef buffer)
{
    qint16* samples = static_cast<qint16*>(buffer->mAudioData);
    const size_t samples_count = buffer->mAudioDataBytesCapacity / sizeof(qint16);

    onDataRequest(samples, samples_count);

    buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;

    OSStatus status = AudioQueueEnqueueBuffer(queue_, buffer, 0, nullptr);
    if (status != noErr)
        LOG(ERROR) << "AudioQueueEnqueueBuffer failed:" << status;
}
