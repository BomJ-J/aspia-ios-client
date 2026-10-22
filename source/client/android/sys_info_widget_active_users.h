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

#ifndef CLIENT_ANDROID_SYS_INFO_WIDGET_ACTIVE_USERS_H
#define CLIENT_ANDROID_SYS_INFO_WIDGET_ACTIVE_USERS_H

#include <QHash>
#include <QString>

#include <optional>

#include "common/sys_info/sys_info_widget.h"
#include "proto/system_info.h"

class QTreeWidget;

// The users of the host by their last sign-in, the latest first, with the ones signed in right now
// marked. Built from two reports: the local users (their last logon) and the processes (whose
// owners in the user sessions are the ones signed in).
class SysInfoWidgetActiveUsers final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetActiveUsers(QWidget* parent = nullptr);
    ~SysInfoWidgetActiveUsers() final;

    // SysInfoWidget implementation.
    std::string category() const final;
    std::vector<proto::system_info::SystemInfoRequest> requests() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

private:
    void rebuild();

    QTreeWidget* tree_;
    std::optional<proto::system_info::LocalUsers> users_;
    // The accounts signed in: the name in lower case to the name as the host shows it.
    std::optional<QHash<QString, QString>> signed_in_;

    Q_DISABLE_COPY_MOVE(SysInfoWidgetActiveUsers)
};

#endif // CLIENT_ANDROID_SYS_INFO_WIDGET_ACTIVE_USERS_H
