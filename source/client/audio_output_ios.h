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

#ifndef CLIENT_AUDIO_OUTPUT_IOS_H
#define CLIENT_AUDIO_OUTPUT_IOS_H

#include "client/audio_output.h"

#include <QMutex>

#include <AudioToolbox/AudioToolbox.h>

#include <atomic>

// Plays the remote audio through an Audio Queue. The queue runs its callbacks on an internal thread
// of its own and lets the size of every buffer be chosen, so each buffer holds a whole number of the
// 10 ms chunks AudioOutput::onDataRequest() works in.
class AudioOutputIos final : public AudioOutput
{
public:
    explicit AudioOutputIos(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputIos() final;

    // AudioOutput implementation.
    bool start() final;
    void stop() final;

private:
    static void outputCallback(void* user_data, AudioQueueRef queue, AudioQueueBufferRef buffer);
    void fillBuffer(AudioQueueBufferRef buffer);

    static const int kBufferCount = 3;

    // Guards |queue_| between start() and stop().
    QMutex mutex_;
    AudioQueueRef queue_ = nullptr;
    AudioQueueBufferRef buffers_[kBufferCount] = {};
    std::atomic<bool> running_ { false };

    Q_DISABLE_COPY_MOVE(AudioOutputIos)
};

#endif // CLIENT_AUDIO_OUTPUT_IOS_H
