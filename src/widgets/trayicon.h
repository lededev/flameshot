#include <QSystemTrayIcon>

#pragma once

class QAction;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
public:
    TrayIcon(QObject* parent = nullptr);
    virtual ~TrayIcon();

#if !defined(DISABLE_UPDATE_CHECKER)
    QAction* appUpdates();
#endif

public slots:
    void receivedMessage(int instanceId, QByteArray message);

private:
    void initTrayIcon();
    void initMenu();
#if !defined(DISABLE_UPDATE_CHECKER)
    void enableCheckUpdatesAction(bool enable);
#endif

    void startGuiCapture(int delay = 0);

    QMenu* m_menu;
    QAction* m_unsetMouseTransparentAction;
#if !defined(DISABLE_UPDATE_CHECKER)
    QAction* m_appUpdates;
#endif
};
