/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "window.h"

#include <QtGui>

#include <private/qguiapplication_p.h>

static int colorIndexId = 0;

QColor colorTable[] =
{
    QColor("#f09f8f"),
    QColor("#a2bff2"),
    QColor("#c0ef8f")
};

Window::Window(QScreen *screen)
    : QWindow(screen)
    , m_backgroundColorIndex(colorIndexId++)
{
    initialize();
}

Window::Window(QWindow *parent)
    : QWindow(parent)
    , m_backgroundColorIndex(colorIndexId++)
{
    initialize();
}

void Window::initialize()
{
    if (parent())
        setGeometry(QRect(160, 120, 320, 240));
    else {
        setGeometry(QRect(10, 10, 320, 200));

        setSizeIncrement(QSize(10, 10));
//        setBaseSize(QSize(640, 480));
        setMinimumSize(QSize(240, 160));
        setMaximumSize(QSize(800, 600));
    }

    create();
    m_backingStore = new QBackingStore(this);

    qreal scaleFactor = this->devicePixelRatio();
    m_image = QImage(geometry().size() * scaleFactor, QImage::Format_RGB32);
    m_image.fill(colorTable[m_backgroundColorIndex % (sizeof(colorTable) / sizeof(colorTable[0]))].rgba());

    m_lastPos = QPoint(-1, -1);
    m_renderTimer = 0;
}

void Window::mousePressEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
}

void Window::mouseMoveEvent(QMouseEvent *event)
{
    if (m_lastPos != QPoint(-1, -1)) {
        QPainter p(&m_image);
        p.setRenderHint(QPainter::Antialiasing);
        p.drawLine(m_lastPos, event->pos());
        m_lastPos = event->pos();
    }
       scheduleRender();
}

void Window::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_lastPos != QPoint(-1, -1)) {
        QPainter p(&m_image);
        p.setRenderHint(QPainter::Antialiasing);
        p.drawLine(m_lastPos, event->pos());
        m_lastPos = QPoint(-1, -1);
    }

    scheduleRender();
}

void Window::exposeEvent(QExposeEvent *)
{
    scheduleRender();
}

void Window::resizeEvent(QResizeEvent *)
{
    QImage old = m_image;

  //  qDebug() << "Window::resizeEvent" << geometry().size();
    qreal scaleFactor = this->devicePixelRatio();
    QSize backingSize = geometry().size() * scaleFactor;

//    qDebug() << "backingSize" << backingSize;

    int width = qMax(backingSize.width(), old.width());
    int height = qMax(backingSize.height(), old.height());

    if (width > old.width() || height > old.height()) {
        m_image = QImage(width, height, QImage::Format_RGB32);
        m_image.fill(colorTable[(m_backgroundColorIndex) % (sizeof(colorTable) / sizeof(colorTable[0]))].rgba());

        QPainter p(&m_image);
        p.drawImage(0, 0, old);
    }

    render();
}

void Window::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Backspace:
        m_text.chop(1);
        break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        m_text.append('\n');
        break;
    default:
        m_text.append(event->text());
        break;
    }
    scheduleRender();
}

void Window::scheduleRender()
{
    if (!m_renderTimer)
        m_renderTimer = startTimer(1);
}

void Window::timerEvent(QTimerEvent *)
{
    render();
    killTimer(m_renderTimer);
    m_renderTimer = 0;
}

void Window::render()
{
    qreal scaleFactor = this->devicePixelRatio();

    QRect windowRect(QPoint(), geometry().size());
    QRect slightltSmallerWindowRect(QPoint(), geometry().size() * 0.9);
    QRect backingRect(QPoint(), geometry().size() * scaleFactor);

    qDebug() << "\nrender window size" << windowRect.size() << "backing size" << backingRect.size() << "image size" << m_image.size();

    m_backingStore->resize(windowRect.size());
    m_backingStore->beginPaint(windowRect);
    QPaintDevice *device = m_backingStore->paintDevice();
    qDebug() << "qpaintdevice size" << device->width() << device->height();


    QPainter p(device);
    p.fillRect(backingRect, Qt::red); // fill with red
    p.fillRect(windowRect, Qt::blue); // fill with blue - red should not be visible
    p.fillRect(slightltSmallerWindowRect, Qt::gray);

  //  p.drawImage(QRect(QPoint(0,0), backingRect.size()), // ### drawImage coordinate confusion
  //              m_image,
  //              QRect(QPoint(0,0), backingRect.size()));

//    p.drawImage(QRect(QPoint(0,0), windowRect.size()), // ### drawImage coordinate confusion
//                m_image,
//                QRect(QPoint(0,0), backingRect.size()));


    QFont font;
    // font.setPixelSize(32); // ### pixels should really mean pixels
    font.setPointSize(32);

    p.setFont(font);
    p.drawText(windowRect, 0, m_text);

    p.drawEllipse(10, 10, 150, 150);

    m_backingStore->endPaint();
    m_backingStore->flush(windowRect);
}


