#include "trayicon.h"

#include "src/core/flameshot.h"
#include "src/core/flameshotdaemon.h"
#include "src/utils/globalvalues.h"

#include "src/utils/confighandler.h"
#include <QApplication>
#include <QMenu>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

#if defined(Q_OS_MACOS)
#include <QOperatingSystemVersion>
#endif

TrayIcon::TrayIcon(QObject* parent)
  : QSystemTrayIcon(parent)
{
    initMenu();

    setToolTip(QStringLiteral("Flameshot"));
#if defined(Q_OS_MACOS)
    // Because of the following issues on MacOS "Catalina":
    // https://bugreports.qt.io/browse/QTBUG-86393
    // https://developer.apple.com/forums/thread/126072
    auto currentMacOsVersion = QOperatingSystemVersion::current();
    if (currentMacOsVersion >= currentMacOsVersion.MacOSBigSur) {
        setContextMenu(m_menu);
    }
#else
    setContextMenu(m_menu);
#endif
    QIcon icon =
      QIcon::fromTheme("flameshot-tray", QIcon(GlobalValues::iconPathPNG()));
    setIcon(icon);

#if defined(Q_OS_MACOS)
    if (currentMacOsVersion < currentMacOsVersion.MacOSBigSur) {
        // Because of the following issues on MacOS "Catalina":
        // https://bugreports.qt.io/browse/QTBUG-86393
        // https://developer.apple.com/forums/thread/126072
        auto trayIconActivated = [this](QSystemTrayIcon::ActivationReason r) {
            if (m_menu->isVisible()) {
                m_menu->hide();
            } else {
                m_menu->popup(QCursor::pos());
            }
        };
        connect(this, &QSystemTrayIcon::activated, this, trayIconActivated);
    }
#else
    connect(this, &TrayIcon::activated, this, [this](ActivationReason r) {
        switch (r)
        {
        case QSystemTrayIcon::Trigger:
            startGuiCapture();
            break;
        case QSystemTrayIcon::Context:
            m_unsetMouseTransparentAction->setEnabled(
                FlameshotDaemon::instance()->countMouseTransparent() > 0);
            break;
        case QSystemTrayIcon::DoubleClick:
            break;
        case QSystemTrayIcon::MiddleClick:
            startGuiCapture(ConfigHandler().delayTakeScreenshotTime());
            break;
        default:
            break;
        }
    });
#endif

#ifdef Q_OS_WIN
    // Ensure proper removal of tray icon when program quits on Windows.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &TrayIcon::hide);
#endif

    show(); // TODO needed?

    if (ConfigHandler().showStartupLaunchMessage()) {
        showMessage(
          "Flameshot",
          QObject::tr(
            "Hello, I'm here! Click icon in the tray to take a screenshot or "
            "click with a right button to see more options."),
          icon,
          3000);
    }

    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            [this]() {});
}

TrayIcon::~TrayIcon()
{
    delete m_menu;
}

#if !defined(DISABLE_UPDATE_CHECKER)
QAction* TrayIcon::appUpdates()
{
    return m_appUpdates;
}
#endif

void TrayIcon::initMenu()
{
    m_menu = new QMenu();

    auto* captureAction = new QAction(tr("&Take Screenshot"), this);
    connect(captureAction, &QAction::triggered, this, [this]() {
#if defined(Q_OS_MACOS)
        auto currentMacOsVersion = QOperatingSystemVersion::current();
        if (currentMacOsVersion >= currentMacOsVersion.MacOSBigSur) {
            startGuiCapture();
        } else {
            // It seems it is not relevant for MacOS BigSur (Wait 400 ms to hide
            // the QMenu)
            QTimer::singleShot(400, this, [this]() { startGuiCapture(); });
        }
#else
    // Wait 400 ms to hide the QMenu
    QTimer::singleShot(400, this, [this]() {
        startGuiCapture();
    });
#endif
    });
    auto* delayCaptureAction = new QAction(tr("&Delay Take Screenshot"), this);
    connect(delayCaptureAction, &QAction::triggered, this, [this]() {
            startGuiCapture(ConfigHandler().delayTakeScreenshotTime());
        });
    auto* launcherAction = new QAction(tr("&Open Launcher"), this);
    connect(launcherAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::launcher);
    m_unsetMouseTransparentAction = new QAction(tr("&Unset Mouse Transparent"), this);
    connect(m_unsetMouseTransparentAction,
            &QAction::triggered,
            FlameshotDaemon::instance(),
            &FlameshotDaemon::unsetAllMouseTransparent);
    m_unsetMouseTransparentAction->setEnabled(
        FlameshotDaemon::instance()->countMouseTransparent() > 0);
    auto* configAction = new QAction(tr("&Configuration"), this);
    connect(configAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::config);
    auto* infoAction = new QAction(tr("&About"), this);
    connect(
      infoAction, &QAction::triggered, Flameshot::instance(), &Flameshot::info);

#if !defined(DISABLE_UPDATE_CHECKER)
    m_appUpdates = new QAction(tr("Check for updates"), this);
    connect(m_appUpdates,
            &QAction::triggered,
            FlameshotDaemon::instance(),
            &FlameshotDaemon::checkForUpdates);

    connect(FlameshotDaemon::instance(),
            &FlameshotDaemon::newVersionAvailable,
            this,
            [this](const QVersionNumber& version) {
                QString newVersion =
                  tr("New version %1 is available").arg(version.toString());
                m_appUpdates->setText(newVersion);
            });
#endif

    QAction* quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    // recent screenshots
    QAction* recentAction = new QAction(tr("&Latest Uploads"), this);
    connect(recentAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::history);

    auto* openSavePathAction = new QAction(tr("Open &Save Path"), this);
    connect(openSavePathAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::openSavePath);

    m_menu->addAction(captureAction);
    m_menu->addAction(delayCaptureAction);
    m_menu->addAction(launcherAction);
    m_menu->addAction(m_unsetMouseTransparentAction);
    m_menu->addSeparator();
    m_menu->addAction(recentAction);
    m_menu->addAction(openSavePathAction);
    m_menu->addSeparator();
    m_menu->addAction(configAction);
    m_menu->addSeparator();
#if !defined(DISABLE_UPDATE_CHECKER)
    m_menu->addAction(m_appUpdates);
#endif
    m_menu->addAction(infoAction);
    m_menu->addSeparator();
    m_menu->addAction(quitAction);
}

#if !defined(DISABLE_UPDATE_CHECKER)
void TrayIcon::enableCheckUpdatesAction(bool enable)
{
    if (m_appUpdates != nullptr) {
        m_appUpdates->setVisible(enable);
        m_appUpdates->setEnabled(enable);
    }
    if (enable) {
        FlameshotDaemon::instance()->getLatestAvailableVersion();
    }
}
#endif

void TrayIcon::startGuiCapture(int delay)
{
    if (delay > 0) {
       QTimer::singleShot(delay, [=]() { Flameshot::instance()->gui(); });
       FlameshotDaemon::instance()->showFloatingText(tr("Delay %1 milliseconds Capture screen").arg(delay));
       return;
    }
    auto* widget = Flameshot::instance()->gui();
#if !defined(DISABLE_UPDATE_CHECKER)
    FlameshotDaemon::instance()->showUpdateNotificationIfAvailable(widget);
#endif
}

void TrayIcon::receivedMessage(int instanceId, QByteArray message)
{
    CaptureRequest req(CaptureRequest::GRAPHICAL_MODE);
    if (!req.fromByteArray(message))
        return;

    Flameshot::instance()->requestCapture(req);
}
