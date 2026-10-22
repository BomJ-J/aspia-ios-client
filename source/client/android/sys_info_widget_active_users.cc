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

#include "client/android/sys_info_widget_active_users.h"

#include <QCoreApplication>
#include <QHeaderView>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"

namespace {

// The labels shared with the Users page come with its translations.
QString usersText(const char* source)
{
    return QCoreApplication::translate("SysInfoWidgetLocalUsers", source);
}

// The name of the account without the domain or computer name in front of it.
QString accountName(const QString& name)
{
    const qsizetype separator = name.lastIndexOf('\\');
    return (separator >= 0 ? name.mid(separator + 1) : name).toLower();
}

// The accounts Windows runs the desktop windows manager and the font driver under in every
// session; they are not people.
bool isSystemAccount(const QString& account)
{
    return account.isEmpty() || account.startsWith("dwm-") || account.startsWith("umfd-") ||
           account == "system" || account == "local service" || account == "network service";
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetActiveUsers::SysInfoWidgetActiveUsers(QWidget* parent)
    : SysInfoWidget(parent),
      tree_(new QTreeWidget(this))
{
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({ tr("User"), usersText("Last Logon") });
    tree_->setRootIsDecorated(false);
    tree_->header()->setStretchLastSection(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_);
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetActiveUsers::~SysInfoWidgetActiveUsers() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetActiveUsers::category() const
{
    return kSystemInfo_LocalUsers;
}

//--------------------------------------------------------------------------------------------------
std::vector<proto::system_info::SystemInfoRequest> SysInfoWidgetActiveUsers::requests() const
{
    std::vector<proto::system_info::SystemInfoRequest> result;
    result.emplace_back().set_category(kSystemInfo_LocalUsers);
    result.emplace_back().set_category(kSystemInfo_Processes);
    return result;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetActiveUsers::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    const std::string& category = system_info.header().category();

    if (category == kSystemInfo_LocalUsers)
    {
        users_ = system_info.local_users();
    }
    else if (category == kSystemInfo_Processes)
    {
        // The owners of the processes outside the session of the services are signed in.
        QHash<QString, QString> signed_in;
        for (const auto& process : system_info.processes().process())
        {
            const QString user = QString::fromStdString(process.user());
            const QString account = accountName(user);
            if (process.sid() != 0 && !isSystemAccount(account))
                signed_in.insert(account, user.mid(user.lastIndexOf('\\') + 1));
        }
        signed_in_ = std::move(signed_in);
    }
    else
    {
        return;
    }

    rebuild();
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetActiveUsers::treeWidget()
{
    return tree_;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetActiveUsers::rebuild()
{
    tree_->clear();

    // Shown once both reports are in, so the marks of who is signed in are not missing at first.
    if (!users_.has_value() || !signed_in_.has_value())
        return;

    std::vector<const proto::system_info::LocalUsers::LocalUser*> users;
    for (const auto& user : users_->local_user())
        users.push_back(&user);

    std::stable_sort(users.begin(), users.end(), [](const auto* a, const auto* b)
    {
        return a->last_logon_time() > b->last_logon_time();
    });

    const QIcon icon(kUserIcon);
    const QString now = tr("signed in now");

    // The latest sign-in on top, on a row of its own.
    if (!users.empty() && users.front()->last_logon_time() != 0)
    {
        const auto* latest = users.front();
        QTreeWidgetItem* item = new QTreeWidgetItem(tree_);
        item->setText(0, tr("Last Signed In"));
        item->setText(1, QString::fromStdString(latest->name()) + " - " +
            Formatter::timeToString(static_cast<time_t>(latest->last_logon_time())));
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
        item->setFont(1, font);
    }

    QSet<QString> listed;
    for (const auto* user : users)
    {
        const QString name = QString::fromStdString(user->name());
        const QString account = accountName(name);
        listed.insert(account);

        QString last_logon = user->last_logon_time() == 0 ?
            usersText("Never") :
            Formatter::timeToString(static_cast<time_t>(user->last_logon_time()));
        if (signed_in_->contains(account))
            last_logon += " (" + now + ")";

        QTreeWidgetItem* item = new QTreeWidgetItem(tree_);
        item->setIcon(0, icon);
        item->setText(0, user->full_name().empty() ?
            name : name + " (" + QString::fromStdString(user->full_name()) + ")");
        item->setText(1, last_logon);
        item->setDisabled(user->disabled());
    }

    // Domain and other accounts are not among the local users, yet they may be signed in.
    for (auto it = signed_in_->cbegin(); it != signed_in_->cend(); ++it)
    {
        if (listed.contains(it.key()))
            continue;

        QTreeWidgetItem* item = new QTreeWidgetItem(tree_);
        item->setIcon(0, icon);
        item->setText(0, it.value());
        item->setText(1, now);
    }

    fitColumns();
}
