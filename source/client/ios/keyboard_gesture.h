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

#ifndef CLIENT_IOS_KEYBOARD_GESTURE_H
#define CLIENT_IOS_KEYBOARD_GESTURE_H

// Qt for iOS hides the on-screen keyboard when a finger that went down above it slides onto it.
// The key bar sits right on top of the keyboard, so a tap on it that moves a point or two closes
// the keyboard. This turns that gesture off in every window of the application; it has to be
// called again whenever Qt may have moved the gesture to another window (the keyboard is shown).
void disableKeyboardHideGesture();

#endif // CLIENT_IOS_KEYBOARD_GESTURE_H
