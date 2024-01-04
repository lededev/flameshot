// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPinchGesture>

#include "pinwidget.h"
#include "screenshotsaver.h"
#include "src/utils/confighandler.h"
#include "src/utils/globalvalues.h"

#include <QLabel>
#include <QMenu>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <flameshot.h>
#include <flameshotdaemon.h>

namespace {
constexpr int MARGIN = 7;
constexpr int BLUR_RADIUS = 2 * MARGIN;
constexpr qreal SCALING_STEP = 0.025;
constexpr qreal OPACITY_WHEEL_STEP = 0.02;
constexpr qreal OPACITY_STEP = 0.1;
constexpr qreal MIN_SIZE = 100.0;
}

PinWidget::PinWidget(const QPixmap& pixmap,
                     const QRect& geometry,
                     const QByteArray& args)
  : QWidget(nullptr)
  , m_pixmap(pixmap)
  , m_layout(new QVBoxLayout(this))
  , m_label(new QLabel())
  , m_shadowEffect(new QGraphicsDropShadowEffect(this))
{
    setArgs(args);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);
    // set the bottom widget background transparent
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    ConfigHandler conf;
    m_baseColor = conf.uiColor();
    m_hoverColor = conf.contrastUiColor();

    m_layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);

    m_shadowEffect->setColor(m_baseColor);
    m_shadowEffect->setBlurRadius(BLUR_RADIUS);
    m_shadowEffect->setOffset(0, 0);
    setGraphicsEffect(m_shadowEffect);
    setWindowOpacity(m_opacity);

    m_label->setPixmap(m_pixmap);
    m_layout->addWidget(m_label);

    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));

    qreal devicePixelRatio = 1;
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
    if (currentScreen != nullptr) {
        devicePixelRatio = currentScreen->devicePixelRatio();
    }
#endif
    const int margin =
      static_cast<int>(static_cast<double>(MARGIN) * devicePixelRatio);
    QRect adjusted_pos = geometry + QMargins(margin, margin, margin, margin);
    setGeometry(adjusted_pos);
#if defined(Q_OS_LINUX)
    setWindowFlags(Qt::X11BypassWindowManagerHint);
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    if (currentScreen != nullptr) {
        QPoint topLeft = currentScreen->geometry().topLeft();
        adjusted_pos.setX((adjusted_pos.x() - topLeft.x()) / devicePixelRatio +
                          topLeft.x());

        adjusted_pos.setY((adjusted_pos.y() - topLeft.y()) / devicePixelRatio +
                          topLeft.y());
        adjusted_pos.setWidth(adjusted_pos.size().width() / devicePixelRatio);
        adjusted_pos.setHeight(adjusted_pos.size().height() / devicePixelRatio);
        resize(0, 0);
        move(adjusted_pos.x(), adjusted_pos.y());
    }
#endif
    grabGesture(Qt::PinchGesture);

    this->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this,
            &QWidget::customContextMenuRequested,
            this,
            &PinWidget::showContextMenu);
}

void PinWidget::closePin()
{
    update();
    close();
}
bool PinWidget::scrollEvent(QWheelEvent* e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        int scrollDirection = 0;
        if (e->angleDelta().y() >= 60) {
            // mouse scroll (wheel) increment
            scrollDirection = 1;
        } else if (e->angleDelta().y() <= -60) {
            // mouse scroll (wheel) decrement
            scrollDirection = -1;
        } else {
            // touchpad scroll
            qint64 current = QDateTime::currentMSecsSinceEpoch();
            if ((current - m_lastMouseWheel) > 200) {
                if (e->angleDelta().y() > 0) {
                    scrollDirection = 1;
                } else if (e->angleDelta().y() < 0) {
                    scrollDirection = -1;
                }
                m_lastMouseWheel = current;
            } else {
                return true;
            }
        }
        switch (scrollDirection) {
        case 1:
            changeOpacity(OPACITY_WHEEL_STEP);
            break;
        case -1:
            changeOpacity(-OPACITY_WHEEL_STEP);
            break;
        }
        return true;
    }
    const auto phase = e->phase();
    if (phase == Qt::ScrollPhase::ScrollUpdate
#if defined(Q_OS_LINUX) || defined(Q_OS_WINDOWS)
        // Linux is getting only NoScrollPhase events.
        || phase == Qt::ScrollPhase::NoScrollPhase
#endif
    ) {
        const auto angle = e->angleDelta();
        if (angle.y() == 0) {
            return true;
        }
        m_currentStepScaleFactor = angle.y() > 0
            ? m_currentStepScaleFactor + SCALING_STEP
            : m_currentStepScaleFactor - SCALING_STEP;
        m_expanding = m_currentStepScaleFactor >= 1.0;
    }
#if defined(Q_OS_MACOS)
    // ScrollEnd is currently supported only on Mac OSX
    if (phase == Qt::ScrollPhase::ScrollEnd) {
#else
    else {
#endif
        m_scaleFactor *= m_currentStepScaleFactor;
        m_currentStepScaleFactor = 1.0;
        m_expanding = false;
    }

    m_sizeChanged = true;
    update();
    FlameshotDaemon::instance()->showFloatingText(
        tr("Zoom %1%").arg(m_currentStepScaleFactor * 100, 0, 'f', 1));
    return true;
}

void PinWidget::enterEvent(QEvent*)
{
    if(m_shadowEffect) m_shadowEffect->setColor(m_hoverColor);
}

void PinWidget::leaveEvent(QEvent*)
{
    if(m_shadowEffect) m_shadowEffect->setColor(m_baseColor);
}

void PinWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    closePin();
}

void PinWidget::mousePressEvent(QMouseEvent* e)
{
    m_dragStart = e->globalPos();
    m_offsetX = e->localPos().x() / width();
    m_offsetY = e->localPos().y() / height();
}

void PinWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint delta = e->globalPos() - m_dragStart;
    const int offsetW = width() * m_offsetX;
    const int offsetH = height() * m_offsetY;
    move(m_dragStart.x() + delta.x() - offsetW,
         m_dragStart.y() + delta.y() - offsetH);
}

void PinWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_0) {
        m_opacity = 1.0;
    } else if (event->key() == Qt::Key_9) {
        m_opacity = 0.9;
    } else if (event->key() == Qt::Key_8) {
        m_opacity = 0.8;
    } else if (event->key() == Qt::Key_7) {
        m_opacity = 0.7;
    } else if (event->key() == Qt::Key_6) {
        m_opacity = 0.6;
    } else if (event->key() == Qt::Key_5) {
        m_opacity = 0.5;
    } else if (event->key() == Qt::Key_4) {
        m_opacity = 0.4;
    } else if (event->key() == Qt::Key_3) {
        m_opacity = 0.3;
    } else if (event->key() == Qt::Key_2) {
        m_opacity = 0.2;
    } else if (event->key() == Qt::Key_1) {
        m_opacity = 0.1;
    }

    setWindowOpacity(m_opacity);
}
bool PinWidget::gestureEvent(QGestureEvent* event)
{
    if (QGesture* pinch = event->gesture(Qt::PinchGesture)) {
        pinchTriggered(static_cast<QPinchGesture*>(pinch));
    }
    return true;
}

void PinWidget::rotateLeft()
{
    m_sizeChanged = true;

    auto rotateTransform = QTransform().rotate(270);
    m_pixmap = m_pixmap.transformed(rotateTransform);
}

void PinWidget::rotateRight()
{
    m_sizeChanged = true;

    auto rotateTransform = QTransform().rotate(90);
    m_pixmap = m_pixmap.transformed(rotateTransform);
}

void PinWidget::changeOpacity(qreal step)
{
    m_opacity += step;
    if (m_opacity < 0.0) {
        m_opacity = 0.0;
    } else if (m_opacity > 1.0) {
        m_opacity = 1.0;
    }

    setWindowOpacity(m_opacity);
    FlameshotDaemon::instance()->showFloatingText(
        QString("%1").arg(static_cast<long>(m_opacity * 100)));
}

void PinWidget::setArgs(const QByteArray& args)
{
    if (!args.size())
        return;
    QDataStream qs(args);
    qs >> m_expanding
        >> m_scaleFactor
        >> m_opacity
        >> m_currentStepScaleFactor;
    if (m_currentStepScaleFactor < 0.9999 || m_currentStepScaleFactor > 1.0001)
        m_sizeChanged = true;
}

QByteArray PinWidget::packArgs()
{
    QByteArray args;
    QDataStream qs(&args, QIODevice::WriteOnly);
    qs << m_expanding
        << m_scaleFactor
        << m_opacity
        << m_currentStepScaleFactor;
    return args;
}

bool PinWidget::event(QEvent* event)
{
    if (event->type() == QEvent::Gesture) {
        return gestureEvent(static_cast<QGestureEvent*>(event));
    } else if (event->type() == QEvent::Wheel) {
        return scrollEvent(static_cast<QWheelEvent*>(event));
    }
    return QWidget::event(event);
}

void PinWidget::paintEvent(QPaintEvent* event)
{
    if (m_sizeChanged) {
        const auto aspectRatio =
          m_expanding ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
        const auto transformType = ConfigHandler().antialiasingPinZoom()
                                     ? Qt::SmoothTransformation
                                     : Qt::FastTransformation;
        const qreal iw = m_pixmap.width();
        const qreal ih = m_pixmap.height();
        const qreal nw = qBound(MIN_SIZE,
                                iw * m_currentStepScaleFactor * m_scaleFactor,
                                static_cast<qreal>(maximumWidth()));
        const qreal nh = qBound(MIN_SIZE,
                                ih * m_currentStepScaleFactor * m_scaleFactor,
                                static_cast<qreal>(maximumHeight()));

        const QPixmap pix = m_pixmap.scaled(nw, nh, aspectRatio, transformType);

        m_label->setPixmap(pix);
        adjustSize();
        m_sizeChanged = false;
    }
}

void PinWidget::pinchTriggered(QPinchGesture* gesture)
{
    const QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        m_currentStepScaleFactor = gesture->totalScaleFactor();
        m_expanding = m_currentStepScaleFactor > gesture->lastScaleFactor();
    }
    if (gesture->state() == Qt::GestureFinished) {
        m_scaleFactor *= m_currentStepScaleFactor;
        m_currentStepScaleFactor = 1;
        m_expanding = false;
    }
    m_sizeChanged = true;
    update();
}

void PinWidget::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu(tr("Context menu"), this);

    QAction copyToClipboardAction(tr("&Copy to clipboard"), this);
    connect(&copyToClipboardAction,
            &QAction::triggered,
            this,
            [=](){ saveToClipboard(m_pixmap); });
    contextMenu.addAction(&copyToClipboardAction);

    QAction saveToFileAction(tr("&Save to file"), this);
    connect(
      &saveToFileAction, &QAction::triggered, this, &PinWidget::saveToFile);
    contextMenu.addAction(&saveToFileAction);

    QAction cloneAction(tr("Clo&ne"), this);
    connect(&cloneAction,
        &QAction::triggered,
        this,
        [=]() {
            FlameshotDaemon::instance()->attachPin(
                m_pixmap,
                geometry() - layout()->contentsMargins(),
                packArgs());
        });
    contextMenu.addAction(&cloneAction);

    QAction editAction(tr("&Edit"), this);
    connect(
      &editAction, &QAction::triggered, this,
        [=]() {
            CaptureRequest req(CaptureRequest::GRAPHICAL_MODE);
            req.addTask(CaptureRequest::PIN);
            req.setInitialSelection(geometry() - layout()->contentsMargins());
            Flameshot::instance()->requestCapture(req);
            connect(Flameshot::instance(), &Flameshot::captureTaken, this,
                [=]() { close(); });
        });
    contextMenu.addAction(&editAction);

    contextMenu.addSeparator();

    QAction rotateRightAction(tr("Rotate &Right"), this);
    connect(
      &rotateRightAction, &QAction::triggered, this, &PinWidget::rotateRight);
    contextMenu.addAction(&rotateRightAction);

    QAction rotateLeftAction(tr("Rotate &Left"), this);
    connect(
      &rotateLeftAction, &QAction::triggered, this, &PinWidget::rotateLeft);
    contextMenu.addAction(&rotateLeftAction);

    QAction increaseOpacityAction(tr("&Increase Opacity"), this);
    connect(&increaseOpacityAction,
            &QAction::triggered,
            this,
            [=]() { changeOpacity(OPACITY_STEP); });
    contextMenu.addAction(&increaseOpacityAction);

    QAction decreaseOpacityAction(tr("&Decrease Opacity"), this);
    connect(&decreaseOpacityAction,
            &QAction::triggered,
            this,
            [=]() { changeOpacity(-OPACITY_STEP); });
    contextMenu.addAction(&decreaseOpacityAction);

    QAction hideShadowAction(tr("&Hide Shadow"), this);
    if (m_shadowEffect && m_shadowEffect->isEnabled()) {
        connect(&hideShadowAction,
            &QAction::triggered,
            this,
            [=]() {
                if (m_shadowEffect == nullptr)
                    return;
                m_shadowEffect->setEnabled(false);
                m_shadowEffect = nullptr;
            });
        contextMenu.addAction(&hideShadowAction);
    }

    QAction mouseTransparentAction(tr("&Mouse transparent"), this);
    connect(&mouseTransparentAction,
        &QAction::triggered,
        this,
        [=]() { setMouseTransparent(true); });
        contextMenu.addAction(&mouseTransparentAction);

    QAction closePinAction(tr("Cl&ose"), this);
    connect(&closePinAction, &QAction::triggered, this, &PinWidget::closePin);
    contextMenu.addSeparator();
    contextMenu.addAction(&closePinAction);

    contextMenu.exec(mapToGlobal(pos));
}

void PinWidget::saveToFile()
{
    hide();
    saveToFilesystemGUI(m_pixmap);
    show();
}

void PinWidget::setMouseTransparent(bool on)
{
    if (on) {
        hide();
        if (m_opacity >= 0.99999) {
            setWindowOpacity(0.5);
        }
        setWindowFlags(windowFlags() | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        show();
        m_mouseTrans = true;
    }
    else {
        if (!m_mouseTrans)
            return;
        m_mouseTrans = false;
        FlameshotDaemon::instance()->
            attachPin(m_pixmap,
                geometry() - layout()->contentsMargins(),
                packArgs());
        close();
    }
}

bool PinWidget::isMouseTransparent() const
{
    return m_mouseTrans;
}
