// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "notifierbox.h"
#include "src/utils/colorutils.h"
#include "src/utils/confighandler.h"
#include "src/utils/globalvalues.h"
#include "src/core/qguiappcurrentscreen.h"
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>

NotifierBox::NotifierBox(QWidget* parent)
  : QWidget(parent)
{
    m_isTopWnd = (parent == nullptr);
    setAttribute(Qt::WA_DeleteOnClose);
    auto wf = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint;
    if (m_isTopWnd) {
        setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet("background:transparent;");
        wf |= Qt::Tool;
    }
    setWindowFlags(wf);
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(600);
    connect(m_timer, &QTimer::timeout, this, &NotifierBox::hide);
    m_bgColor = ConfigHandler().uiColor();
    m_foregroundColor =
      (ColorUtils::colorIsDark(m_bgColor) ? Qt::white : Qt::black);
    m_bgColor.setAlpha(180);
    if (m_isTopWnd) {
        setFixedHeight(60);
        return;
    }
    const int size =
      (GlobalValues::buttonBaseSize() + GlobalValues::buttonBaseSize() / 2) *
      qApp->devicePixelRatio();
    setFixedSize(QSize(size, size));
}

void NotifierBox::enterEvent(QEvent*)
{
    hide();
}

void NotifierBox::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (m_isTopWnd) {
        findMaxFontSize();
        //painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QBrush(m_bgColor, Qt::SolidPattern));
        painter.setPen(QPen(Qt::transparent));
        painter.drawRoundedRect(rect(), 8.8, 8.8);
        // Draw the text:
        painter.setPen(QPen(m_foregroundColor));
        QFont newFont = painter.font();
        newFont.setPointSize(m_fontSize);
        painter.setFont(newFont);
        painter.drawText(rect(), Qt::AlignCenter, m_message);
        return;
    }
    // draw Ellipse
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QBrush(m_bgColor, Qt::SolidPattern));
    painter.setPen(QPen(Qt::transparent));
    painter.drawEllipse(rect());
    // Draw the text:
    painter.setPen(QPen(m_foregroundColor));
    painter.drawText(rect(), Qt::AlignCenter, m_message);
}

void NotifierBox::showMessage(const QString& msg, QPoint pos)
{
    m_message = msg;
    if (m_isTopWnd) {
        findMaxFontSize();
        setGeometryByMessage(msg, pos);
    } else if (!pos.isNull()){
        move(pos);
    }
    repaint();
    show();
    m_timer->start();
}

void NotifierBox::showColor(const QColor& color)
{
    Q_UNUSED(color)
    m_message = QLatin1String("");
}

void NotifierBox::hideEvent(QHideEvent* event)
{
    emit hidden();
}

void NotifierBox::findMaxFontSize()
{
    if (m_fontSize != 0)
        return;
    QPainter painter(this);
    const QString text = "WM";
    QFontMetrics fm(painter.font());
    int fontSize = fm.height();
    const auto width = geometry().width();
    const auto height = geometry().height();
    while (fm.width(text) < width && fm.height() < height) {
        fontSize++;
        QFont newFont = painter.font();
        newFont.setPointSize(fontSize);
        fm = QFontMetrics(newFont);
    }
    m_fontSize = fontSize > 1? fontSize - 1: fontSize;
}

void NotifierBox::setGeometryByMessage(const QString& msg, const QPoint& pos)
{
    const auto currentScreen = QGuiAppCurrentScreen().currentScreen();
    const auto screenRect = currentScreen->geometry();
    const qreal devicePixelRatio = currentScreen->devicePixelRatio();
    QPainter painter(this);
    QFont newFont = painter.font();
    newFont.setPointSize(m_fontSize);
    QFontMetrics fm(newFont);
    auto fmWidth = fm.horizontalAdvance(msg);
    auto newSize = m_fontSize;
    const auto maxWidth = std::min(screenRect.width(), 5120);
    while (fmWidth > maxWidth) {
        newSize--;
        QFont newFont = painter.font();
        newFont.setPointSize(newSize);
        QFontMetrics fm(newFont);
        fmWidth = fm.horizontalAdvance(msg);
    }
    if (newSize != m_fontSize) {
        m_fontSize = newSize > 1 ? newSize - 1 : newSize;
    }
    if (pos.isNull())
    {
        int x = screenRect.left() + (screenRect.width() - fmWidth) / 2;
        int y = screenRect.top() + geometry().height();
        setGeometry(x, y, fmWidth, geometry().height());
    }
    else
        setGeometry(pos.x(), pos.y(), fmWidth, geometry().height());
}
