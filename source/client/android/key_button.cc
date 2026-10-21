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

#include "client/android/key_button.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "common/android/controls.h"

namespace {

constexpr int kHorizontalPadding = 12;
constexpr int kMinWidth = 44;
constexpr int kIconSize = 22;
constexpr double kCornerRadius = 7.0;
constexpr double kShadowOffset = 1.0;
constexpr double kFontScale = 1.1;
constexpr double kFunctionTint = 0.3;
constexpr double kPressedShade = 0.25;

// Colors of the system keyboard: the row behind the keys, the key caps and the shadow under them.
const QColor kLightRow(0xD1, 0xD4, 0xDA);
const QColor kLightKey(0xFF, 0xFF, 0xFF);
const QColor kLightShadow(0x89, 0x8A, 0x8D);
const QColor kLightText(0x00, 0x00, 0x00);
const QColor kDarkRow(0x2B, 0x2B, 0x2E);
const QColor kDarkKey(0x6B, 0x6B, 0x6E);
const QColor kDarkShadow(0x10, 0x10, 0x10);
const QColor kDarkText(0xFF, 0xFF, 0xFF);

//--------------------------------------------------------------------------------------------------
bool isDarkTheme()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

} // namespace

//--------------------------------------------------------------------------------------------------
KeyButton::KeyButton(const QString& text, Style style, QWidget* parent)
    : QPushButton(text, parent),
      style_(style)
{
    setFocusPolicy(Qt::NoFocus);
    setFont(Controls::scaledFont(font(), kFontScale));
}

//--------------------------------------------------------------------------------------------------
KeyButton::~KeyButton() = default;

//--------------------------------------------------------------------------------------------------
void KeyButton::setIconPath(const QString& icon_file_path)
{
    icon_file_path_ = icon_file_path;
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------------------------------
void KeyButton::setActive(bool active)
{
    if (active_ == active)
        return;

    active_ = active;
    update();
}

//--------------------------------------------------------------------------------------------------
// static
QColor KeyButton::rowColor()
{
    return isDarkTheme() ? kDarkRow : kLightRow;
}

//--------------------------------------------------------------------------------------------------
QSize KeyButton::sizeHint() const
{
    const int content = icon_file_path_.isEmpty() ?
        fontMetrics().horizontalAdvance(text()) : kIconSize;
    return QSize(qMax(kMinWidth, content + 2 * kHorizontalPadding), fontMetrics().height() + 12);
}

//--------------------------------------------------------------------------------------------------
QSize KeyButton::minimumSizeHint() const
{
    return sizeHint();
}

//--------------------------------------------------------------------------------------------------
void KeyButton::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool dark = isDarkTheme();
    const QColor accent = Controls::accentColor();

    QColor cap = dark ? kDarkKey : kLightKey;
    QColor label = dark ? kDarkText : kLightText;

    if (active_)
    {
        cap = accent;
        label = Controls::contrastColor(accent);
    }
    else if (style_ == Style::FUNCTION)
    {
        cap = Controls::blend(cap, accent, kFunctionTint);
        label = accent;
    }

    // Pressed keys darken toward the row, as the keyboard keys do.
    if (isDown())
        cap = Controls::blend(cap, rowColor().darker(130), kPressedShade);

    const QRectF cap_rect = QRectF(rect()).adjusted(0, 0, 0, -kShadowOffset);

    QPainterPath shadow;
    shadow.addRoundedRect(cap_rect.translated(0, kShadowOffset), kCornerRadius, kCornerRadius);
    painter.fillPath(shadow, dark ? kDarkShadow : kLightShadow);

    QPainterPath cap_path;
    cap_path.addRoundedRect(cap_rect, kCornerRadius, kCornerRadius);
    painter.fillPath(cap_path, cap);

    if (!icon_file_path_.isEmpty())
    {
        const QPixmap icon =
            Controls::tintedPixmap(icon_file_path_, QSize(kIconSize, kIconSize), label);
        painter.drawPixmap(QPointF(cap_rect.center().x() - kIconSize / 2.0,
                                   cap_rect.center().y() - kIconSize / 2.0), icon);
        return;
    }

    painter.setPen(label);
    painter.setFont(font());
    painter.drawText(cap_rect, Qt::AlignCenter, text());
}
