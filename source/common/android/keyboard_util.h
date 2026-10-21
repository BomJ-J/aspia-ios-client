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

#ifndef COMMON_ANDROID_KEYBOARD_UTIL_H
#define COMMON_ANDROID_KEYBOARD_UTIL_H

#include <QGuiApplication>
#include <QInputMethod>
#include <QWidget>

// The top edge of the on-screen keyboard in global coordinates, in the logical pixels the widget
// geometry uses. The Android plugin passes the raw window insets through, which are in physical
// pixels; the iOS plugin reports points, the same units as the widgets.
inline qreal keyboardTop(const QWidget* widget)
{
    const QRectF keyboard = QGuiApplication::inputMethod()->keyboardRectangle();

    // While the keyboard slides in, Qt for iOS reports the rectangle it starts from, which iOS 26
    // leaves empty. Taken as is, its top of 0 would make the keyboard cover the whole window and the
    // screens would squeeze their content away for the length of the animation. Until the real
    // rectangle arrives the keyboard is taken to sit below the window.
    if (keyboard.isEmpty())
    {
        const QWidget* window = widget->window();
        return window->mapToGlobal(QPoint(0, window->height())).y();
    }

    const qreal top = keyboard.top();
#if defined(Q_OS_ANDROID)
    return top / widget->devicePixelRatioF();
#else
    Q_UNUSED(widget)
    return top;
#endif
}

#endif // COMMON_ANDROID_KEYBOARD_UTIL_H
