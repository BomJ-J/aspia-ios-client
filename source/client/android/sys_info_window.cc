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

#include "client/android/sys_info_window.h"

#include <QCoreApplication>
#include <QHeaderView>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/threading/worker.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "client/session_keeper.h"
#include "client/session_state.h"
#include "client/workers/network_worker.h"
#include "common/android/app_bar.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/touch_scroller.h"
#include "common/android/tree_widget.h"
#include "common/sys_info/sys_info_widget_applications.h"
#include "common/sys_info/sys_info_widget_connections.h"
#include "common/sys_info/sys_info_widget_cpu.h"
#include "common/sys_info/sys_info_widget_devices.h"
#include "common/sys_info/sys_info_widget_dmi.h"
#include "common/sys_info/sys_info_widget_drivers.h"
#include "common/sys_info/sys_info_widget_drives.h"
#include "common/sys_info/sys_info_widget_env_vars.h"
#include "common/sys_info/sys_info_widget_event_logs.h"
#include "common/sys_info/sys_info_widget_licenses.h"
#include "common/sys_info/sys_info_widget_local_user_groups.h"
#include "client/android/sys_info_widget_active_users.h"
#include "common/sys_info/sys_info_widget_local_users.h"
#include "common/sys_info/sys_info_widget_monitors.h"
#include "common/sys_info/sys_info_widget_net_adapters.h"
#include "common/sys_info/sys_info_widget_net_shares.h"
#include "common/sys_info/sys_info_widget_open_files.h"
#include "common/sys_info/sys_info_widget_power_options.h"
#include "common/sys_info/sys_info_widget_printers.h"
#include "common/sys_info/sys_info_widget_processes.h"
#include "common/sys_info/sys_info_widget_routes.h"
#include "common/sys_info/sys_info_widget_services.h"
#include "common/sys_info/sys_info_widget_smart.h"
#include "common/sys_info/sys_info_widget_summary.h"
#include "common/sys_info/sys_info_widget_video_adapters.h"
#include "proto/peer.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/system_info.h"

namespace {

// The page a category row opens. Group rows carry none.
constexpr int kPageRole = Qt::UserRole;

// The strings below are those of the desktop client and of the other session screens, so the
// existing translations apply.
QString sysInfoText(const char* text)
{
    return QCoreApplication::translate("SysInfoView", text);
}

QString sessionText(const char* text)
{
    return QCoreApplication::translate("FileTransferWindow", text);
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWindow::SysInfoWindow(const HostConfig& host, bool credentials_saved, QWidget* parent)
    : QWidget(parent),
      host_(host),
      credentials_saved_(credentials_saved),
      app_bar_(new AppBar(this)),
      button_refresh_(new IconButton(":/img/material/refresh.svg", this)),
      label_status_(new Label(QString(), Label::Role::CAPTION, this)),
      stack_(new QStackedWidget(this)),
      categories_(new TreeWidget(this)),
      page_container_(new QWidget(this))
{
    LOG(INFO) << "Ctor";

    app_bar_->setTitle(QCoreApplication::translate("SystemInfoWindow", "System Information"));
    app_bar_->setBackVisible(true);
    app_bar_->setActions({ button_refresh_ });
    connect(app_bar_, &AppBar::sig_backClicked, this, &SysInfoWindow::onBackClicked);
    connect(button_refresh_, &IconButton::clicked, this, &SysInfoWindow::onRefreshClicked);

    label_status_->setAlignment(Qt::AlignCenter);
    label_status_->setWordWrap(true);

    categories_->setHeaderHidden(true);
    categories_->setColumnCount(1);
    connect(categories_, &QTreeWidget::itemClicked, this, &SysInfoWindow::onCategoryClicked);

    page_layout_ = new QVBoxLayout(page_container_);
    page_layout_->setContentsMargins(0, 0, 0, 0);
    page_layout_->setSpacing(0);

    stack_->addWidget(categories_);
    stack_->addWidget(page_container_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(label_status_);
    layout->addWidget(stack_, 1);

    buildCategories();
    start();
}

//--------------------------------------------------------------------------------------------------
SysInfoWindow::~SysInfoWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool SysInfoWindow::goBack()
{
    if (stack_->currentWidget() != page_container_)
        return false;

    showList();
    return true;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    switch (status)
    {
        case NetworkWorker::Status::HOST_CONNECTING:
            setStatusText(sessionText("Connecting to host %1...").arg(session_state_->hostAddress()));
            break;

        case NetworkWorker::Status::HOST_CONNECTED:
        {
            connected_ = true;
            label_status_->setVisible(false);

            if (session_keeper_)
                session_keeper_->acquire();

            // Now the session can receive incoming messages.
            emit sig_sessionReady();

            SysInfoWidgetSummary* summary = static_cast<SysInfoWidgetSummary*>(summary_);
            summary->setClientVersion(kCurrentVersion);
            summary->setHostVersion(session_state_->hostVersion());
            summary->setRouterVersion(session_state_->routerVersion());

            // The summary is what the window opens with on the desktop as well.
            requestPage(summary_, true);
            if (current_page_)
                requestPage(current_page_, true);
        }
        break;

        case NetworkWorker::Status::HOST_DISCONNECTED:
        {
            connected_ = false;
            pending_.clear();

            if (session_keeper_)
                session_keeper_->release();

            const TcpChannel::ErrorCode error_code = data.value<TcpChannel::ErrorCode>();
            if (error_code == TcpChannel::ErrorCode::ACCESS_DENIED)
            {
                forgetHostCredentials();
                setStatusText(TcpChannel::errorToString(error_code));
            }
            else
            {
                setStatusText(sessionText("The connection to the host has been lost."));
            }
        }
        break;

        case NetworkWorker::Status::RELAY_ERROR:
            setStatusText(data.toString());
            break;

        case NetworkWorker::Status::VERSION_MISMATCH:
            setStatusText(sessionText(
                "The host version is newer than the client. Please update the application."));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::onChannelMessage(const QByteArray& buffer)
{
    proto::system_info::SystemInfo system_info;
    if (!parse(buffer, &system_info))
    {
        LOG(ERROR) << "Unable to parse system info";
        return;
    }

    const std::string& category = system_info.header().category();
    pending_.erase(category);

    // As on the desktop, the summary is built from whichever reports arrive, and the other pages
    // from the report of their own category.
    for (SysInfoWidget* page : std::as_const(pages_))
    {
        if (page == summary_ || page->category() == category || asksFor(page, category))
            page->setSystemInfo(system_info);
    }
}

//--------------------------------------------------------------------------------------------------
// static
bool SysInfoWindow::asksFor(const SysInfoWidget* page, const std::string& category)
{
    // A page may be built from the reports of several categories (Active Users).
    const std::vector<proto::system_info::SystemInfoRequest> requests = page->requests();
    return std::any_of(requests.cbegin(), requests.cend(),
                       [&category](const auto& request) { return request.category() == category; });
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::onCategoryClicked(QTreeWidgetItem* item, int /* column */)
{
    if (!item)
        return;

    SysInfoWidget* page = item->data(0, kPageRole).value<SysInfoWidget*>();
    if (!page)
    {
        // A group row only folds and unfolds its categories.
        item->setExpanded(!item->isExpanded());
        return;
    }

    showPage(page, item->text(0));
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::onBackClicked()
{
    if (!goBack())
        emit sig_closed();
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::onRefreshClicked()
{
    requestPage(current_page_ ? current_page_ : summary_, true);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    if (!connected_)
        return;

    pending_.insert(request.category());
    emit sig_sendMessage(proto::peer::CHANNEL_ID_0, serialize(request));
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::buildCategories()
{
    summary_ = new SysInfoWidgetSummary(page_container_);

    struct Category
    {
        SysInfoWidget* page;
        const char* icon;
        const char* name;
    };

    auto add_group = [this](const char* name, const QList<Category>& categories)
    {
        QTreeWidgetItem* group = nullptr;
        if (name)
        {
            group = new QTreeWidgetItem(categories_, { sysInfoText(name) });
            group->setIcon(0, QIcon(SysInfoWidget::kFolderIcon));
            group->setExpanded(true);
        }

        for (const Category& category : categories)
        {
            QTreeWidgetItem* item = group ? new QTreeWidgetItem(group, { sysInfoText(category.name) })
                                          : new QTreeWidgetItem(categories_, { sysInfoText(category.name) });
            item->setIcon(0, QIcon(category.icon));
            item->setData(0, kPageRole, QVariant::fromValue(category.page));

            category.page->hide();
            pages_.append(category.page);

            connect(category.page, &SysInfoWidget::sig_systemInfoRequest,
                    this, &SysInfoWindow::onSystemInfoRequest);

            // The pages are the desktop ones: a finger drags their lists (some pages have two)
            // instead of selecting rows, sideways as well, since their columns are wider than a phone.
            const QList<QTreeWidget*> trees = category.page->findChildren<QTreeWidget*>();
            for (QTreeWidget* tree : trees)
            {
                tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
                tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

                TouchScroller* scroller = new TouchScroller(tree);
                scroller->setOrientations(Qt::Horizontal | Qt::Vertical);
            }
        }
    };

    QWidget* p = page_container_;

    add_group(nullptr, {
        { summary_, SysInfoWidget::kComputerIcon, QT_TRANSLATE_NOOP("SysInfoView", "Summary") } });

    add_group(QT_TRANSLATE_NOOP("SysInfoView", "Hardware"), {
        { new SysInfoWidgetDmi(p), SysInfoWidget::kMotherboardIcon, QT_TRANSLATE_NOOP("SysInfoView", "DMI") },
        { new SysInfoWidgetCpu(p), SysInfoWidget::kMicrochipIcon, QT_TRANSLATE_NOOP("SysInfoView", "Processor") },
        { new SysInfoWidgetDevices(p), SysInfoWidget::kNetworkCardIcon, QT_TRANSLATE_NOOP("SysInfoView", "Devices") },
        { new SysInfoWidgetDrives(p), SysInfoWidget::kHddIcon, QT_TRANSLATE_NOOP("SysInfoView", "Drives") },
        { new SysInfoWidgetSmart(p), SysInfoWidget::kHddIcon, QT_TRANSLATE_NOOP("SysInfoView", "S.M.A.R.T.") },
        { new SysInfoWidgetVideoAdapters(p), SysInfoWidget::kVideoCardIcon, QT_TRANSLATE_NOOP("SysInfoView", "Video Adapters") },
        { new SysInfoWidgetMonitors(p), SysInfoWidget::kImacIcon, QT_TRANSLATE_NOOP("SysInfoView", "Monitors") },
        { new SysInfoWidgetPrinters(p), SysInfoWidget::kPrinterIcon, QT_TRANSLATE_NOOP("SysInfoView", "Printers") },
        { new SysInfoWidgetPowerOptions(p), SysInfoWidget::kElectricalIcon, QT_TRANSLATE_NOOP("SysInfoView", "Power Options") } });

    add_group(QT_TRANSLATE_NOOP("SysInfoView", "Software"), {
        { new SysInfoWidgetApplications(p), SysInfoWidget::kSoftwareIcon, QT_TRANSLATE_NOOP("SysInfoView", "Applications") },
        { new SysInfoWidgetDrivers(p), SysInfoWidget::kNetworkCardIcon, QT_TRANSLATE_NOOP("SysInfoView", "Drivers") },
        { new SysInfoWidgetServices(p), SysInfoWidget::kGearIcon, QT_TRANSLATE_NOOP("SysInfoView", "Services") },
        { new SysInfoWidgetProcesses(p), SysInfoWidget::kHeartMonitorIcon, QT_TRANSLATE_NOOP("SysInfoView", "Processes") },
        { new SysInfoWidgetLicenses(p), SysInfoWidget::kCertificateIcon, QT_TRANSLATE_NOOP("SysInfoView", "Licenses") } });

    add_group(QT_TRANSLATE_NOOP("SysInfoView", "Network"), {
        { new SysInfoWidgetNetAdapters(p), SysInfoWidget::kNetworkCardIcon, QT_TRANSLATE_NOOP("SysInfoView", "Network Adapters") },
        { new SysInfoWidgetRoutes(p), SysInfoWidget::kFlowChartIcon, QT_TRANSLATE_NOOP("SysInfoView", "Routes") },
        { new SysInfoWidgetConnections(p), SysInfoWidget::kConnectedIcon, QT_TRANSLATE_NOOP("SysInfoView", "Connections") },
        { new SysInfoWidgetNetShares(p), SysInfoWidget::kNasIcon, QT_TRANSLATE_NOOP("SysInfoView", "Network Shares") },
        { new SysInfoWidgetOpenFiles(p), SysInfoWidget::kNasIcon, QT_TRANSLATE_NOOP("SysInfoView", "Open Files") } });

    add_group(QT_TRANSLATE_NOOP("SysInfoView", "Operating System"), {
        { new SysInfoWidgetEnvVars(p), SysInfoWidget::kDayViewIcon, QT_TRANSLATE_NOOP("SysInfoView", "Environment Variables") },
        { new SysInfoWidgetEventLogs(p), SysInfoWidget::kLogIcon, QT_TRANSLATE_NOOP("SysInfoView", "Event Logs") },
        { new SysInfoWidgetLocalUsers(p), SysInfoWidget::kUserIcon, QT_TRANSLATE_NOOP("SysInfoView", "Users") },
        { new SysInfoWidgetActiveUsers(p), SysInfoWidget::kUserIcon, QT_TRANSLATE_NOOP("SysInfoView", "Active Users") },
        { new SysInfoWidgetLocalUserGroups(p), SysInfoWidget::kUserAccountIcon, QT_TRANSLATE_NOOP("SysInfoView", "User Groups") } });
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::showPage(SysInfoWidget* page, const QString& title)
{
    if (current_page_)
    {
        page_layout_->removeWidget(current_page_);
        current_page_->hide();
    }

    current_page_ = page;
    page_layout_->addWidget(page);
    page->show();

    app_bar_->setTitle(title);
    stack_->setCurrentWidget(page_container_);

    requestPage(page, false);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::showList()
{
    if (current_page_)
    {
        page_layout_->removeWidget(current_page_);
        current_page_->hide();
        current_page_ = nullptr;
    }

    app_bar_->setTitle(QCoreApplication::translate("SystemInfoWindow", "System Information"));
    stack_->setCurrentWidget(categories_);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::requestPage(SysInfoWidget* page, bool force)
{
    if (!page || !connected_)
        return;

    // A page that already shows its report is left alone until the user asks to refresh it.
    if (!force && hasData(page))
        return;

    const std::vector<proto::system_info::SystemInfoRequest> requests = page->requests();
    for (const proto::system_info::SystemInfoRequest& request : requests)
    {
        if (!force && pending_.contains(request.category()))
            continue;

        onSystemInfoRequest(request);
    }
}

//--------------------------------------------------------------------------------------------------
bool SysInfoWindow::hasData(SysInfoWidget* page) const
{
    const QTreeWidget* tree = page->treeWidget();
    return tree && tree->topLevelItemCount() > 0;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::forgetHostCredentials()
{
    if (!credentials_saved_)
        return;

    credentials_saved_ = false;

    LOG(INFO) << "Access denied. Removing the credentials saved for this host";

    if (host_.entryId() > 0)
    {
        LocalHostConfig local_host;
        const Database::FindResult found = Database::instance().findLocalHost(host_.entryId(), &local_host);
        if (found != Database::FindResult::FOUND)
        {
            LOG(ERROR) << "Unable to read local host" << host_.entryId() << ":" << found;
            return;
        }

        local_host.setCredentialId(0);
        local_host.setUsername(QString());
        local_host.setPassword(SecureString());

        if (!Database::instance().modifyLocalHost(local_host))
            LOG(ERROR) << "Unable to remove credentials of local host" << host_.entryId();
        return;
    }

    const HostId host_id = stringToHostId(host_.address());

    if (!Database::instance().removeRouterHost(host_.routerId(), host_id))
        LOG(ERROR) << "Unable to remove credentials of host" << host_id;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::start()
{
    // When connecting with a one-time password the authorization screen leaves the user name
    // empty, and the host registers that password under the special user "#<host_id>".
    if (host_.username().isEmpty())
        host_.setUsername(u"#" + host_.address());

    session_state_ = std::make_shared<SessionState>(
        host_, proto::peer::SESSION_TYPE_SYSTEM_INFO, Database::instance().displayName());

    setStatusText(sessionText("Connecting..."));

    if (session_state_->isConnectionByHostId())
        fetchConnectionOffer();
    else
        startNewSession();
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::fetchConnectionOffer()
{
    // The working session exists only while the record is online.
    if (RouterSession* session = RouterController::session(session_state_->routerId()))
    {
        requestConnectionOffer(session);
        return;
    }

    RouterController& controller = RouterController::instance();
    const RouterStatus router_status = RouterController::status(session_state_->routerId());
    if (router_status == RouterStatus::OFFLINE || router_status == RouterStatus::UNREADABLE)
    {
        setStatusText(router_status == RouterStatus::UNREADABLE ?
            sessionText("The data of the router is damaged. Edit the router and enter it again.") :
            sessionText("The specified router is unavailable."));
        return;
    }

    // The record is logging in again; the offer is requested once it is let in.
    setStatusText(sessionText("Connecting to router..."));

    disconnect(&controller, nullptr, this, nullptr);
    connect(&controller, &RouterController::sig_statusChanged, this,
        [this](qint64 router_id, RouterStatus status)
    {
        if (router_id != session_state_->routerId() || status != RouterStatus::ONLINE)
            return;

        disconnect(&RouterController::instance(), nullptr, this, nullptr);

        if (RouterSession* session = RouterController::session(session_state_->routerId()))
            requestConnectionOffer(session);
    });
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::requestConnectionOffer(RouterSession* session)
{
    session_state_->setRouterVersion(session->version());
    setStatusText(sessionText("Requesting connection to the host..."));

    session->requestConnection(session_state_->hostId(), { this,
        [this](const proto::router::ConnectionOffer& offer)
    {
        if (offer.error_code() == proto::router::kErrorOk)
        {
            session_state_->setConnectionOffer(offer);
            startNewSession();
            return;
        }

        setStatusText(sessionText("Error requesting connection via router."));
    } });
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::startNewSession()
{
    if (!session_keeper_)
        session_keeper_ = SessionKeeper::create(this);

    worker_manager_.reset();
    worker_manager_ = std::make_unique<WorkerManager>();

    std::unique_ptr<NetworkWorker> network_worker = std::make_unique<NetworkWorker>();
    network_worker_ = network_worker.get();
    worker_manager_->add(std::move(network_worker));

    connect(this, &SysInfoWindow::sig_startConnection, network_worker_, &NetworkWorker::onStartConnection,
            Qt::QueuedConnection);
    connect(this, &SysInfoWindow::sig_sessionReady, network_worker_, &NetworkWorker::onSessionReady,
            Qt::QueuedConnection);
    connect(this, &SysInfoWindow::sig_sendMessage, network_worker_, &NetworkWorker::onSendMessage,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_statusChanged, this, &SysInfoWindow::onNetworkStatusChanged,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_channel_0, this, &SysInfoWindow::onChannelMessage,
            Qt::QueuedConnection);

    worker_manager_->start();

    emit sig_startConnection(session_state_);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWindow::setStatusText(const QString& text)
{
    label_status_->setText(text);
    label_status_->setVisible(true);
}
