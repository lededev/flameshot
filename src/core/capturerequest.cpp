// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "capturerequest.h"
#include "confighandler.h"
#include "src/config/cacheutils.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <stdexcept>
#include <utility>

CaptureRequest::CaptureRequest(CaptureRequest::CaptureMode mode,
                               const uint delay,
                               QVariant data,
                               CaptureRequest::ExportTask tasks)
  : m_mode(mode)
  , m_delay(delay)
  , m_tasks(tasks)
  , m_data(std::move(data))
{

    ConfigHandler config;
    if (m_mode == CaptureRequest::CaptureMode::GRAPHICAL_MODE &&
        config.saveLastRegion()) {
        setInitialSelection(getLastRegion());
    }
}

CaptureRequest::CaptureMode CaptureRequest::captureMode() const
{
    return m_mode;
}

uint CaptureRequest::delay() const
{
    return m_delay;
}

QString CaptureRequest::path() const
{
    return m_path;
}

QVariant CaptureRequest::data() const
{
    return m_data;
}

CaptureRequest::ExportTask CaptureRequest::tasks() const
{
    return m_tasks;
}

QRect CaptureRequest::initialSelection() const
{
    return m_initialSelection;
}

QRect CaptureRequest::pinWindowGeometry() const
{
    return m_pinWindowGeometry;
}

void CaptureRequest::addTask(CaptureRequest::ExportTask task)
{
    if (task == SAVE) {
        throw std::logic_error("SAVE task must be added using addSaveTask");
    }
    m_tasks |= task;
}

void CaptureRequest::removeTask(ExportTask task)
{
    ((int&)m_tasks) &= ~task;
}

void CaptureRequest::addSaveTask(const QString& path)
{
    m_tasks |= SAVE;
    m_path = path;
}

void CaptureRequest::addPinTask(const QRect& pinWindowGeometry)
{
    m_tasks |= PIN;
    m_pinWindowGeometry = pinWindowGeometry;
}

void CaptureRequest::setInitialSelection(const QRect& selection)
{
    m_initialSelection = selection;
}

void CaptureRequest::toByteArray(QByteArray& arr) const
{
    QDataStream qd(&arr, QIODevice::WriteOnly);
    QString vF0("F0"), vF9("F9");
    qd << vF0
        << m_mode << m_delay << m_path << m_tasks
        << m_pinWindowGeometry << m_initialSelection << vF9;
}

bool CaptureRequest::fromByteArray(QByteArray& arr)
{
    QDataStream qd(&arr, QIODevice::ReadOnly);
    QString vF0, vF9;

    CaptureMode mode;
    uint delay;
    QString path;
    ExportTask tasks;
    QRect pinWindowGeometry, initialSelection;

    qd >> vF0
        >> mode >> delay >> path >> tasks
        >> pinWindowGeometry >> initialSelection >> vF9;
    if (vF0 != "F0" || vF9 != "F9")
        return false;
    m_mode = mode;
    m_delay = delay;
    m_path = path;
    m_tasks = tasks;
    m_pinWindowGeometry = pinWindowGeometry;
    m_initialSelection = initialSelection;
    return true;
}
