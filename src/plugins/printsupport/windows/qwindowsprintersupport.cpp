/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwindowsprintersupport.h"

#include <QtCore/QList>
#include <QtPrintSupport/QPrinterInfo>
#include <qprintengine_win_p.h>
#include <private/qpaintengine_alpha_p.h>
#include <private/qprinterinfo_p.h>

QT_BEGIN_NAMESPACE

QWindowsPrinterSupport::QWindowsPrinterSupport()
    : QPlatformPrinterSupport()
{
    DWORD needed = 0;
    DWORD returned = 0;
    if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, 0, 0, &needed, &returned)) {
        LPBYTE buffer = new BYTE[needed];
        if (EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, buffer, needed, &needed, &returned)) {
            PPRINTER_INFO_4 infoList = reinterpret_cast<PPRINTER_INFO_4>(buffer);
            QString defaultPrinterName;
            QString program;
            QString port;
            QWin32PrintEngine::queryDefaultPrinter(defaultPrinterName, program, port);
            for (uint i = 0; i < returned; ++i) {
                QString printerName(QString::fromWCharArray(infoList[i].pPrinterName));
                bool isDefault = (printerName == defaultPrinterName);
                QPrinterInfo printerInfo = createPrinterInfo(printerName, QString(), QString(), QString(), isDefault, i);
                m_printers.append(printerInfo);
            }
        }
        delete [] buffer;
    }
}

QWindowsPrinterSupport::~QWindowsPrinterSupport()
{
}

QPrintEngine *QWindowsPrinterSupport::createNativePrintEngine(QPrinter::PrinterMode printerMode)
{
        return new QWin32PrintEngine(printerMode);
}

QPaintEngine *QWindowsPrinterSupport::createPaintEngine(QPrintEngine *engine, QPrinter::PrinterMode printerMode)
{
    Q_UNUSED(printerMode)
    return static_cast<QWin32PrintEngine *>(engine);
}

QList<QPrinter::PaperSize> QWindowsPrinterSupport::supportedPaperSizes(const QPrinterInfo &printerInfo) const
{
    return QWin32PrintEngine::supportedPaperSizes(printerInfo);
}

QT_END_NAMESPACE
