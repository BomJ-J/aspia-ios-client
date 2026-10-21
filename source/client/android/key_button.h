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

#ifndef CLIENT_ANDROID_KEY_BUTTON_H
#define CLIENT_ANDROID_KEY_BUTTON_H

#include <QPushButton>

// A key of the KeyBar drawn like a key of the system on-screen keyboard: a rounded cap with a thin
// shadow under it. Shows either a text label or a monochrome icon.
class KeyButton final : public QPushButton
{
    Q_OBJECT

public:
    enum class Style
    {
        LETTER,   // Plain key, like a letter key of the keyboard.
        FUNCTION, // Key that acts on the client itself, tinted with the accent.
    };

    KeyButton(const QString& text, Style style, QWidget* parent = nullptr);
    ~KeyButton() final;

    void setIconPath(const QString& icon_file_path);

    // An active key (a held sticky modifier) is filled with the accent.
    void setActive(bool active);
    bool isActive() const { return active_; }

    // Background of the row the keys sit on, matching the keyboard below it.
    static QColor rowColor();

    // QPushButton implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

protected:
    // QPushButton implementation.
    void paintEvent(QPaintEvent* event) final;

private:
    Style style_;
    QString icon_file_path_;
    bool active_ = false;

    Q_DISABLE_COPY_MOVE(KeyButton)
};

#endif // CLIENT_ANDROID_KEY_BUTTON_H
