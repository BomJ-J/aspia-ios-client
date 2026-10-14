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

#include "client/session_keeper_ios.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
SessionKeeperIos::SessionKeeperIos(QObject* parent)
    : SessionKeeper(parent),
      task_id_(UIBackgroundTaskInvalid)
{
    // Every return to the screen starts a fresh background task, so the next switch to another
    // application gets the full time again.
    id observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:UIApplicationWillEnterForegroundNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification* /* notification */)
    {
        if (!acquired_)
            return;

        endTask();
        beginTask();
    }];

    // The project builds Objective-C without ARC, so the observer token is retained by hand.
    foreground_observer_ = static_cast<void*>([observer retain]);
}

//--------------------------------------------------------------------------------------------------
SessionKeeperIos::~SessionKeeperIos()
{
    if (foreground_observer_)
    {
        id observer = static_cast<id>(foreground_observer_);
        [[NSNotificationCenter defaultCenter] removeObserver:observer];
        [observer release];
        foreground_observer_ = nullptr;
    }

    acquired_ = false;
    endTask();
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperIos::acquire()
{
    if (acquired_)
        return;

    acquired_ = true;
    beginTask();
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperIos::release()
{
    if (!acquired_)
        return;

    acquired_ = false;
    endTask();
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperIos::beginTask()
{
    if (task_id_ != UIBackgroundTaskInvalid)
        return;

    UIApplication* application = [UIApplication sharedApplication];

    task_id_ = [application beginBackgroundTaskWithName:@"Aspia session"
                                      expirationHandler:^
    {
        // The time is up. The task has to be ended here, or the system kills the application.
        LOG(INFO) << "Background time expired";
        endTask();
    }];

    if (task_id_ == UIBackgroundTaskInvalid)
        LOG(WARNING) << "Unable to begin a background task";
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperIos::endTask()
{
    if (task_id_ == UIBackgroundTaskInvalid)
        return;

    [[UIApplication sharedApplication] endBackgroundTask:task_id_];
    task_id_ = UIBackgroundTaskInvalid;
}
