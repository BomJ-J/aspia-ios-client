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

#ifndef CLIENT_SESSION_KEEPER_IOS_H
#define CLIENT_SESSION_KEEPER_IOS_H

#include "client/session_keeper.h"

// iOS suspends an application shortly after it leaves the screen. A background task buys the
// sessions the time the system grants (about half a minute) to survive a short switch to another
// application, for example to copy a password. Longer stays in the background end the sessions.
class SessionKeeperIos final : public SessionKeeper
{
public:
    explicit SessionKeeperIos(QObject* parent);
    ~SessionKeeperIos() final;

    // SessionKeeper implementation.
    void acquire() final;
    void release() final;

private:
    void beginTask();
    void endTask();

    bool acquired_ = false;
    unsigned long task_id_;
    void* foreground_observer_ = nullptr;

    Q_DISABLE_COPY_MOVE(SessionKeeperIos)
};

#endif // CLIENT_SESSION_KEEPER_IOS_H
