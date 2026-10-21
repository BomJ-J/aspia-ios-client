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

#ifndef CLIENT_IOS_EDIT_MENU_H
#define CLIENT_IOS_EDIT_MENU_H

#include <QList>
#include <QRect>
#include <QString>

#include <functional>

class QWidget;

struct EditMenuItem
{
    QString title;
    std::function<void()> action;
};

// Shows the edit menu of iOS (the bubble of Paste, Copy, ...) with |items|, pointing at |target|,
// a rectangle in the coordinates of |field|: the cursor or the selection.
// Qt for iOS asks for that menu through UIMenuController, which no longer shows anything on
// current iOS, so the text fields of the touch interface open it themselves.
void showEditMenu(QWidget* field, const QRect& target, const QList<EditMenuItem>& items);

#endif // CLIENT_IOS_EDIT_MENU_H
