/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qplatformprintersupport.h"

#include <QtPrintSupport/qprinterinfo.h>

#include <private/qprinterinfo_p.h>

#ifndef QT_NO_PRINTER

QT_BEGIN_NAMESPACE

/*!
    \class QPlatformPrinterSupport
    \since 5.0
    \internal
    \preliminary
    \ingroup qpa

    \brief The QPlatformPrinterSupport class provides an abstraction for print support.
 */

QPlatformPrinterSupport::QPlatformPrinterSupport()
{
}

QPlatformPrinterSupport::~QPlatformPrinterSupport()
{
}

QPrintEngine *QPlatformPrinterSupport::createNativePrintEngine(QPrinter::PrinterMode)
{
    return 0;
}

QPaintEngine *QPlatformPrinterSupport::createPaintEngine(QPrintEngine *, QPrinter::PrinterMode)
{
    return 0;
}

QList<QPrinter::PaperSize> QPlatformPrinterSupport::supportedPaperSizes(const QPrinterInfo &) const
{
    return QList<QPrinter::PaperSize>();
}

QList<QPrinterInfo> QPlatformPrinterSupport::availablePrinters()
{
    return m_printers;
}

QPrinterInfo QPlatformPrinterSupport::defaultPrinter()
{
    const QList<QPrinterInfo> printers = availablePrinters();
    foreach (const QPrinterInfo &printerInfo, printers) {
        if (printerInfo.isDefault())
            return printerInfo;
    }
    return QPrinterInfo();
}

QPrinterInfo QPlatformPrinterSupport::printerInfo(const QString &printerName)
{
    const QList<QPrinterInfo> printers = availablePrinters();
    foreach (const QPrinterInfo &printerInfo, printers) {
        if (printerInfo.printerName() == printerName)
            return printerInfo;
    }
    return QPrinterInfo();
}

QString QPlatformPrinterSupport::printerOption(const QPrinterInfo &printer, const QString &key) const
{
    Q_UNUSED(printer)
    Q_UNUSED(key)
    return QString();
}

PrinterOptions QPlatformPrinterSupport::printerOptions(const QPrinterInfo &printer) const
{
    Q_UNUSED(printer)
    return PrinterOptions();
}

int QPlatformPrinterSupport::printerIndex(const QPrinterInfo &printer)
{
    return printer.d_func()->index;
}

QPrinterInfo QPlatformPrinterSupport::createPrinterInfo(const QString &name, const QString &description,
                                                        const QString &location, const QString &makeAndModel,
                                                        bool isDefault, int index)
{
    QPrinterInfo printer(name);
    printer.d_func()->description = description;
    printer.d_func()->location = location;
    printer.d_func()->makeAndModel = makeAndModel;
    printer.d_func()->isDefault = isDefault;
    printer.d_func()->index = index;
    return printer;
}

/*
    Converts QSizeF in millimeters to a predefined PaperSize (returns Custom if
    the size isn't a standard size)
*/
extern QPrinter::PaperSize qSizeFTopaperSize(const QSizeF &);
QPrinter::PaperSize QPlatformPrinterSupport::convertQSizeFToPaperSize(const QSizeF &sizef)
{
    return qSizeFTopaperSize(sizef);
}

/*
    Converts a predefined PaperSize to a QSizeF in millimeters (returns
    QSizeF(0.0, 0.0) if PaperSize is Custom)
*/
extern QSizeF qt_paperSizeToQSizeF(QPrinter::PaperSize size);
QSizeF QPlatformPrinterSupport::convertPaperSizeToQSizeF(QPrinter::PaperSize paperSize)
{
    return qt_paperSizeToQSizeF(paperSize);
}

QT_END_NAMESPACE

#endif // QT_NO_PRINTER
