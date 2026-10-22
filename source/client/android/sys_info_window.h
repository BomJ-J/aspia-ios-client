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

#ifndef CLIENT_ANDROID_SYS_INFO_WINDOW_H
#define CLIENT_ANDROID_SYS_INFO_WINDOW_H

#include <QList>
#include <QPointer>
#include <QWidget>

#include <memory>
#include <set>
#include <string>

#include "client/config.h"
#include "client/workers/network_worker.h"

namespace proto::system_info {
class SystemInfoRequest;
} // namespace proto::system_info

class AppBar;
class IconButton;
class Label;
class QStackedWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class RouterSession;
class SessionKeeper;
class SessionState;
class SysInfoWidget;
class TreeWidget;
class WorkerManager;

// The system information session of the touch interface. The first screen lists the categories the
// way the desktop client does; a tap opens the page of a category, and the report behind it is
// asked for only then, so a phone does not pull the processes or the event logs it never looks at.
// The pages themselves are the ones of the desktop client (common/sys_info).
class SysInfoWindow final : public QWidget
{
    Q_OBJECT

public:
    SysInfoWindow(const HostConfig& host, bool credentials_saved, QWidget* parent = nullptr);
    ~SysInfoWindow() final;

    // Goes from a page back to the list of categories. Returns false on the list itself, where the
    // back action closes the window.
    bool goBack();

signals:
    void sig_closed();
    void sig_startConnection(std::shared_ptr<SessionState> session_state);
    void sig_sessionReady();
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);

private slots:
    void onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data);
    void onChannelMessage(const QByteArray& buffer);
    void onCategoryClicked(QTreeWidgetItem* item, int column);
    void onBackClicked();
    void onRefreshClicked();
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);

private:
    // Whether |page| asks for the reports of |category| besides its own.
    static bool asksFor(const SysInfoWidget* page, const std::string& category);

    void buildCategories();
    void showPage(SysInfoWidget* page, const QString& title);
    void showList();
    void requestPage(SysInfoWidget* page, bool force);
    bool hasData(SysInfoWidget* page) const;
    void forgetHostCredentials();

    void start();
    void fetchConnectionOffer();
    void requestConnectionOffer(RouterSession* session);
    void startNewSession();
    void setStatusText(const QString& text);

    HostConfig host_;
    bool credentials_saved_ = false;
    bool connected_ = false;
    std::shared_ptr<SessionState> session_state_;

    std::unique_ptr<WorkerManager> worker_manager_;
    QPointer<NetworkWorker> network_worker_;
    SessionKeeper* session_keeper_ = nullptr;

    AppBar* app_bar_ = nullptr;
    IconButton* button_refresh_ = nullptr;
    Label* label_status_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    TreeWidget* categories_ = nullptr;
    QWidget* page_container_ = nullptr;
    QVBoxLayout* page_layout_ = nullptr;

    QList<SysInfoWidget*> pages_;
    SysInfoWidget* summary_ = nullptr;
    SysInfoWidget* current_page_ = nullptr;

    // Reports asked for and not received yet, so a page is not asked for twice.
    std::set<std::string> pending_;

    Q_DISABLE_COPY_MOVE(SysInfoWindow)
};

#endif // CLIENT_ANDROID_SYS_INFO_WINDOW_H
