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

#include "client/application.h"

#include "version.h"
#include "base/build_config.h"
#include "base/logging.h"
#include "client/database.h"
#include "client/host_url.h"
#include "client/settings.h"
#include "client/workers/router_worker.h"
#include "client/workers/update_worker.h"

#if !defined(ASPIA_MOBILE)
#include "client/mcp/mcp_worker.h"
#endif // !defined(ASPIA_MOBILE)

#include <QIcon>

#if defined(Q_OS_WINDOWS)
#include <QDir>
#include <QSettings>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
#include <QFileOpenEvent>
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_IOS)
#include <QDesktopServices>
#include <QInputMethod>
#include <QInputMethodQueryEvent>
#include <QClipboard>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTextEdit>
#include <QTimer>

#include "client/ios/edit_menu.h"
#include "client/ios/keyboard_gesture.h"
#endif // defined(Q_OS_IOS)

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#endif // defined(Q_OS_ANDROID)

namespace {

const char kActivateWindow[] = "activate_window";
const char kOpenUrl[] = "open_url:";

#if defined(Q_OS_IOS)
//--------------------------------------------------------------------------------------------------
bool isTextWidget(const QObject* object)
{
    return qobject_cast<const QLineEdit*>(object) || qobject_cast<const QTextEdit*>(object) ||
           qobject_cast<const QPlainTextEdit*>(object);
}

//--------------------------------------------------------------------------------------------------
// When the keyboard opens, the iOS plugin of Qt shifts the whole screen up until the text cursor is
// above the keyboard. The touch interface makes room for the keyboard itself (KeyboardInset and the
// like), so both together push the content off the screen. Qt shifts only when it knows where the
// cursor is, so the cursor rectangle is withheld from it.
class KeyboardScrollBlocker final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) final
    {
        if (event->type() != QEvent::InputMethodQuery)
            return false;

        QInputMethodQueryEvent* query = static_cast<QInputMethodQueryEvent*>(event);
        if (!(query->queries() & Qt::ImCursorRectangle))
            return false;

        // The receiver answers first, then the cursor rectangle is replaced with an empty one.
        watched->event(event);
        query->setValue(Qt::ImCursorRectangle, QRectF());
        return true;
    }
};

//--------------------------------------------------------------------------------------------------
// The edit menu of the text fields. Qt for iOS asks a field for a context menu on taps it takes for
// the field, and the text widgets answer with the desktop menu of Qt (Undo, Redo, Cut, ...); its
// own fallback, the edit menu of iOS, no longer shows up on current iOS. Those requests are
// swallowed, and a field opens the edit menu of iOS itself, as the native fields do: on a tap on a
// field that already has the focus, or on a long press.
class TextFieldMenu final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) final
    {
        const bool text_widget = isTextWidget(watched);

        switch (event->type())
        {
            case QEvent::Polish:
            {
                // No selection handles, loupe and edit menu gestures of Qt: they place themselves
                // off the field and take the taps on a selection from it. The hint is set on the
                // field itself, as QLineEdit answers Qt for iOS without a query event.
                if (QLineEdit* edit = qobject_cast<QLineEdit*>(watched))
                    edit->setInputMethodHints(edit->inputMethodHints() | Qt::ImhNoTextHandles);
                return false;
            }

            case QEvent::ContextMenu:
                return text_widget;

            case QEvent::MouseButtonPress:
            {
                QLineEdit* edit = qobject_cast<QLineEdit*>(watched);
                if (!edit)
                    return false;

                pressed_ = edit;
                pressed_focused_ = edit->hasFocus();
                press_pos_ = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
                press_timer_.start();
                return false;
            }

            case QEvent::MouseButtonRelease:
            {
                QLineEdit* edit = qobject_cast<QLineEdit*>(watched);
                if (!edit || edit != pressed_)
                    return false;

                pressed_.clear();

                const QPoint pos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
                const bool moved = (pos - press_pos_).manhattanLength() > kTapSlop;
                const bool long_press = press_timer_.elapsed() >= kLongPressMs;

                if (!moved && (pressed_focused_ || long_press))
                {
                    // After the release, so the field has placed its cursor first.
                    QPointer<QLineEdit> guard(edit);
                    QTimer::singleShot(0, edit, [guard]() { if (guard) showFor(guard); });
                }
                return false;
            }

            default:
                return false;
        }
    }

private:
    static constexpr int kTapSlop = 12;
    static constexpr qint64 kLongPressMs = 450;

    // The labels of the desktop menu of QLineEdit come with the translations of Qt; the mnemonic
    // markers are dropped.
    static QString label(const char* source)
    {
        return QLineEdit::tr(source).remove('&');
    }

    static void showFor(QLineEdit* edit)
    {
        const bool hidden = edit->echoMode() != QLineEdit::Normal;
        const bool can_copy = edit->hasSelectedText() && !hidden;
        const bool can_edit = !edit->isReadOnly();
        const bool has_text = !edit->text().isEmpty();
        const bool can_paste = can_edit && !QGuiApplication::clipboard()->text().isEmpty();

        QPointer<QLineEdit> guard(edit);

        QList<EditMenuItem> items;
        if (can_copy && can_edit)
            items.append({ label("Cu&t"), [guard]() { if (guard) guard->cut(); } });
        if (can_copy)
            items.append({ label("&Copy"), [guard]() { if (guard) guard->copy(); } });
        if (can_paste)
            items.append({ label("&Paste"), [guard]() { if (guard) guard->paste(); } });
        if (has_text && edit->selectedText() != edit->text())
        {
            // As in the native fields, the menu comes back for the selection.
            items.append({ label("Select All"), [guard]()
            {
                if (!guard)
                    return;
                guard->selectAll();
                QTimer::singleShot(0, guard, [guard]() { if (guard) showFor(guard); });
            }});
        }

        // The menu points at the cursor, or at the whole selection when there is one.
        QRect target = edit->inputMethodQuery(Qt::ImCursorRectangle).toRect();
        if (edit->hasSelectedText())
            target |= edit->inputMethodQuery(Qt::ImAnchorRectangle).toRect();
        if (target.height() <= 0)
            target = edit->rect();
        else if (target.width() <= 0)
            target.setWidth(1); // The cursor is a line; the menu still needs a rectangle.

        showEditMenu(edit, target, items);
    }

    QPointer<QLineEdit> pressed_;
    bool pressed_focused_ = false;
    QPoint press_pos_;
    QElapsedTimer press_timer_;
};
#endif // defined(Q_OS_IOS)

#if defined(Q_OS_ANDROID)
//--------------------------------------------------------------------------------------------------
// Called by ClientActivity.onNewIntent on the Android UI thread when the running application is
// asked to open an aspia:// link.
void onNativeUrlOpened(JNIEnv* /* env */, jclass /* clazz */, jstring url)
{
    QString url_string = QJniObject(url).toString();
    QMetaObject::invokeMethod(Application::instance(), "sig_urlOpened", Qt::QueuedConnection,
                              Q_ARG(QString, url_string));
}
#endif // defined(Q_OS_ANDROID)

} // namespace

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : GuiApplication(argc, argv)
{
    LOG(INFO) << "Ctor";

    setOrganizationName("Aspia");
    setApplicationName("Client");
    setApplicationVersion(ASPIA_VERSION_STRING);
#if !defined(Q_OS_MACOS)
    setWindowIcon(QIcon(":/img/aspia.ico"));
#endif // !defined(Q_OS_MACOS)

    connect(this, &Application::sig_messageReceived, this, [this](const QByteArray& message)
    {
        if (message.startsWith(kActivateWindow))
        {
            emit sig_windowActivated();
        }
        else if (message.startsWith(kOpenUrl))
        {
            emit sig_urlOpened(QString::fromUtf8(message.mid(static_cast<qsizetype>(qstrlen(kOpenUrl)))));
        }
        else
        {
            LOG(ERROR) << "Unhandled message";
        }
    });

    Settings settings;

#if defined(Q_OS_IOS)
    // The language is remembered, but a language picked for Aspia in the iOS Settings (or a new
    // language of the phone) replaces it. iOS reports that choice as the first preferred language.
    const QStringList ui_languages = QLocale::system().uiLanguages();
    const QString system_locale = ui_languages.isEmpty() ? QString() : ui_languages.first();
    if (!system_locale.isEmpty() && system_locale != settings.systemLocale())
    {
        LOG(INFO) << "System language changed to:" << system_locale;
        settings.setSystemLocale(system_locale);
        settings.setLocale(system_locale);
    }
#endif // defined(Q_OS_IOS)

    QString locale = resolveLocale(settings.locale());
    if (locale != settings.locale())
    {
        LOG(INFO) << "Locale changed to:" << locale;
        settings.setLocale(locale);
    }

    setLocale(locale);
    setTheme(settings.theme());

    addWorker(std::make_unique<RouterWorker>());
    addWorker(std::make_unique<UpdateWorker>());

#if !defined(ASPIA_MOBILE)
    if (Database::instance().isMcpEnabled())
        addWorker(std::make_unique<McpWorker>());
#endif // !defined(ASPIA_MOBILE)

#if defined(Q_OS_WINDOWS)
    registerUrlHandler();
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_IOS)
    // iOS hands the links of the schemes listed in Info.plist (CFBundleURLTypes) to the handler,
    // both the one the application is started with and those that come while it runs.
    QDesktopServices::setUrlHandler("aspia", this, "onSystemUrlOpened");

    installEventFilter(new KeyboardScrollBlocker(this));
    installEventFilter(new TextFieldMenu(this));

    // Qt attaches its hide-keyboard gesture to the window that has the focus when the keyboard
    // opens, so the gesture is switched off again each time.
    connect(inputMethod(), &QInputMethod::visibleChanged, this, []()
    {
        QTimer::singleShot(0, []() { disableKeyboardHideGesture(); });
    });
    connect(this, &QGuiApplication::focusWindowChanged, this, []()
    {
        QTimer::singleShot(0, []() { disableKeyboardHideGesture(); });
    });
#endif // defined(Q_OS_IOS)

#if defined(Q_OS_ANDROID)
    QJniEnvironment jni;
    const JNINativeMethod methods[] = {
        { "nativeUrlOpened", "(Ljava/lang/String;)V", reinterpret_cast<void*>(onNativeUrlOpened) }};
    if (!jni.registerNativeMethods("org/aspia/client/ClientActivity", methods, 1))
        LOG(ERROR) << "Unable to register ClientActivity native methods";
#endif // defined(Q_OS_ANDROID)
}

//--------------------------------------------------------------------------------------------------
Application::~Application()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Application::onSystemUrlOpened(const QUrl& url)
{
    LOG(INFO) << "URL opened by the system";
    emit sig_urlOpened(url.toString());
}

//--------------------------------------------------------------------------------------------------
// static
Application* Application::instance()
{
    return static_cast<Application*>(QApplication::instance());
}

//--------------------------------------------------------------------------------------------------
void Application::activateWindow()
{
    sendMessage(kActivateWindow);
}

//--------------------------------------------------------------------------------------------------
void Application::openUrl(const QString& url)
{
    sendMessage(kOpenUrl + url.toUtf8());
}

#if defined(Q_OS_MACOS)
//--------------------------------------------------------------------------------------------------
bool Application::event(QEvent* event)
{
    // macOS delivers clicked aspia:// links to the running instance as FileOpen events
    // (CFBundleURLTypes in Info.plist), not through the command line.
    if (event->type() == QEvent::FileOpen)
    {
        QString url = static_cast<QFileOpenEvent*>(event)->url().toString();
        if (HostUrl::isHostUrl(url))
        {
            LOG(INFO) << "URL open event:" << url;
            emit sig_urlOpened(url);
            return true;
        }
    }

    return GuiApplication::event(event);
}
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
// static
void Application::registerUrlHandler()
{
    // Register the aspia:// scheme for the current user so links work even without the
    // installer (the MSI additionally registers the scheme machine-wide).
    QString executable = QDir::toNativeSeparators(applicationFilePath());
    QString command = QString("\"%1\" \"%2\"").arg(executable, "%1");

    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes\\aspia", QSettings::NativeFormat);
    if (settings.value("shell/open/command/Default").toString() == command)
        return;

    settings.setValue("Default", "URL:Aspia Protocol");
    settings.setValue("URL Protocol", "");
    settings.setValue("DefaultIcon/Default", QString("\"%1\",0").arg(executable));
    settings.setValue("shell/open/command/Default", command);
}
#endif // defined(Q_OS_WINDOWS)
