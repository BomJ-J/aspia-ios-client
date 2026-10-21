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

#include "client/android/key_bar.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include <utility>

#include "client/android/key_button.h"
#include "common/android/scroll_area.h"
#include "common/android/touch_scroller.h"

namespace {

// Height of the bar keys and the gaps around them, close to the keys of the keyboard below so the
// bar reads as a part of it.
constexpr int kKeyHeight = 36;
constexpr int kKeySpacing = 6;
constexpr int kRowMargin = 5;

// The bar is a rounded panel floating above the keyboard: the radius of its corners, the inset from
// the sides of the window and the gap left between it and the keyboard.
constexpr double kPanelRadius = 12.0;
constexpr int kPanelSideInset = 4;
constexpr int kPanelBottomGap = 2;

// Width of the key that hides the keyboard and of the divider before it.
constexpr int kHideKeyWidth = 52;
constexpr int kDividerWidth = 1;

// USB HID usage codes (page 0x07) for the keys offered by the bar.
const quint32 kUsbLeftCtrl  = 0x0700e0;
const quint32 kUsbLeftShift = 0x0700e1;
const quint32 kUsbLeftAlt   = 0x0700e2;
const quint32 kUsbLeftMeta  = 0x0700e3;
const quint32 kUsbEnter     = 0x070028;
const quint32 kUsbEscape    = 0x070029;
const quint32 kUsbTab       = 0x07002b;
const quint32 kUsbDelete    = 0x07004c;
const quint32 kUsbHome      = 0x07004a;
const quint32 kUsbEnd       = 0x07004d;
const quint32 kUsbPageUp    = 0x07004b;
const quint32 kUsbPageDown  = 0x07004e;
const quint32 kUsbRight     = 0x07004f;
const quint32 kUsbLeft      = 0x070050;
const quint32 kUsbDown      = 0x070051;
const quint32 kUsbUp        = 0x070052;

} // namespace

//--------------------------------------------------------------------------------------------------
KeyBar::KeyBar(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);

    // The panel is drawn in paintEvent(); everything outside its rounded shape stays transparent and
    // shows the desktop beneath.
    setAutoFillBackground(false);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, KeyButton::rowColor());
    setPalette(pal);

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);

    // The keys do not fit the width of a phone, so the row is dragged sideways.
    if (TouchScroller* scroller = scroll->findChild<TouchScroller*>())
        scroller->setOrientation(Qt::Horizontal);

    QWidget* content = new QWidget(scroll);
    content->setFocusPolicy(Qt::NoFocus);

    QHBoxLayout* row = new QHBoxLayout(content);
    row->setContentsMargins(kRowMargin, kRowMargin, kRowMargin, kRowMargin);
    row->setSpacing(kKeySpacing);
    row->setSizeConstraint(QLayout::SetMinimumSize);

    addSpecialKey(row, "Esc", kUsbEscape);
    addSpecialKey(row, "Tab", kUsbTab);
    addSpecialKey(row, "Enter", kUsbEnter);
    addSpecialKey(row, "Del", kUsbDelete);
    addModifier(row, "Ctrl", kUsbLeftCtrl);
    addModifier(row, "Alt", kUsbLeftAlt);
    addModifier(row, "Shift", kUsbLeftShift);
    addModifier(row, "Win", kUsbLeftMeta);
    addSpecialKey(row, "Left", kUsbLeft);
    addSpecialKey(row, "Up", kUsbUp);
    addSpecialKey(row, "Down", kUsbDown);
    addSpecialKey(row, "Right", kUsbRight);
    addSpecialKey(row, "Home", kUsbHome);
    addSpecialKey(row, "End", kUsbEnd);
    addSpecialKey(row, "PgUp", kUsbPageUp);
    addSpecialKey(row, "PgDn", kUsbPageDown);

    scroll->setWidget(content);
    // setWidget() turns the background filling on, which would square off the rounded panel.
    content->setAutoFillBackground(false);

    // Stays outside the scrolled row so it can be reached without dragging. The keyboard's own
    // return key cannot be used to close it: it is sent to the remote computer as Enter.
    // A divider and the accent tint set it apart from the keys that go to the remote computer.
    QWidget* divider = new QWidget(this);
    divider->setFixedSize(kDividerWidth, kKeyHeight);
    divider->setAutoFillBackground(true);
    QPalette divider_palette = divider->palette();
    divider_palette.setColor(QPalette::Window, palette().color(QPalette::Mid));
    divider->setPalette(divider_palette);

    KeyButton* hide_keyboard = new KeyButton(QString(), KeyButton::Style::FUNCTION, this);
    hide_keyboard->setIconPath(":/img/material/keyboard_hide.svg");
    hide_keyboard->setFixedSize(kHideKeyWidth, kKeyHeight);
    connect(hide_keyboard, &KeyButton::clicked, this, &KeyBar::sig_hideKeyboard);

    QHBoxLayout* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(
        kPanelSideInset, 0, kPanelSideInset + kRowMargin, kPanelBottomGap);
    main_layout->setSpacing(kRowMargin);
    main_layout->addWidget(scroll);
    main_layout->addWidget(divider);
    main_layout->addWidget(hide_keyboard);

    setFixedHeight(content->sizeHint().height() + kPanelBottomGap);
}

//--------------------------------------------------------------------------------------------------
KeyBar::~KeyBar() = default;

//--------------------------------------------------------------------------------------------------
void KeyBar::clearModifiers()
{
    for (KeyButton* button : std::as_const(modifier_buttons_))
        button->setActive(false);
}

//--------------------------------------------------------------------------------------------------
void KeyBar::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF panel =
        QRectF(rect()).adjusted(kPanelSideInset, 0, -kPanelSideInset, -kPanelBottomGap);

    QPainterPath path;
    path.addRoundedRect(panel, kPanelRadius, kPanelRadius);
    painter.fillPath(path, KeyButton::rowColor());
}

//--------------------------------------------------------------------------------------------------
void KeyBar::addModifier(QHBoxLayout* layout, const QString& text, quint32 usb_keycode)
{
    KeyButton* button = new KeyButton(text, KeyButton::Style::LETTER, layout->parentWidget());
    button->setFixedHeight(kKeyHeight);

    connect(button, &QPushButton::clicked, this, [this, button, usb_keycode]()
    {
        // The filled state marks a sticky modifier; tapping again cancels it.
        const bool active = !button->isActive();
        button->setActive(active);
        emit sig_modifierToggled(usb_keycode, active);
    });

    layout->addWidget(button);
    modifier_buttons_.append(button);
}

//--------------------------------------------------------------------------------------------------
void KeyBar::addSpecialKey(QHBoxLayout* layout, const QString& text, quint32 usb_keycode)
{
    KeyButton* button = new KeyButton(text, KeyButton::Style::LETTER, layout->parentWidget());
    button->setFixedHeight(kKeyHeight);

    connect(button, &QPushButton::clicked, this, [this, usb_keycode]()
    {
        emit sig_specialKey(usb_keycode);
    });

    layout->addWidget(button);
}
