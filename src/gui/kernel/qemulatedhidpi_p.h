
#include <QtGui>

// Emulate 2x high-dpi by creating an artificial point coordinate system
// half the size of the real "pixel" coordinate system. The goal is
// to make 2x high-dpi work on platforms that lack explicit support for
// this feature.
//
// High-dpi emulation is enabled by setting the QT_EMULATED_HIGHDPI
// environment variable.
//
// The scaling is applied at the QPA level (QWindow, QScreen, QWindowSystemInterface).
// The QPlatform* classes and subclasses mostly operate in pixels
// and do not know about the scaling. QWidgets and QtQuick items
// are mostly in point space. Exceptions:
//
// QPixmaps, QImges, and OpenGL are in pixels
// The backing store is in pixel space.
//
//
// The functions are pass-through functions if hidpi emulation is
// disabled. (see qemulatedhidpi.cpp).
//
//void qhidpiSetEmulationEnabled(bool enable);
Q_GUI_EXPORT bool qhidpiIsEmulationEnabled();
Q_GUI_EXPORT qreal qhidpiIsEmulationGetScaleFactor();


// ### don't want to export 20 new symbols, but
// the platform plugins need access.
Q_GUI_EXPORT QRect qhidpiPixelToPoint(const QRect &pixelRect);
Q_GUI_EXPORT QRect qhidpiPointToPixel(const QRect &pointRect);
Q_GUI_EXPORT QRectF qhidpiPixelToPoint(const QRectF &pixelRect);
Q_GUI_EXPORT QRectF qhidpiPointToPixel(const QRectF &pointRect);

Q_GUI_EXPORT QSize qhidpiPixelToPoint(const QSize &pixelSize);
Q_GUI_EXPORT QSize qhidpiPointToPixel(const QSize &pointSize);
Q_GUI_EXPORT QSizeF qhidpiPixelToPoint(const QSizeF &pixelSize);
Q_GUI_EXPORT QSizeF qhidpiPointToPixel(const QSizeF &pointSize);

Q_GUI_EXPORT QPoint qhidpiPixelToPoint(const QPoint &pixelPoint);
Q_GUI_EXPORT QPoint qhidpiPointToPixel(const QPoint &pointPoint);
Q_GUI_EXPORT QPointF qhidpiPixelToPoint(const QPointF &pixelPoint);
Q_GUI_EXPORT QPointF qhidpiPointToPixel(const QPointF &pointPoint);

Q_GUI_EXPORT QMargins qhidpiPixelToPoint(const QMargins &pixelMargins);
Q_GUI_EXPORT QMargins qhidpiPointToPixel(const QMargins &pointMargins);

Q_GUI_EXPORT QRegion qhidpiPixelToPoint(const QRegion &pixelRegion);
Q_GUI_EXPORT QRegion qhidpiPointToPixel(const QRegion &pointRegion);

extern const qreal qt_emulated_scale_factor;
extern bool qt_use_emulated_hidpi_mode;

// Any T that has operator/()
template <typename T>
T qhidpiPixelToPoint(const T &pixelValue)
{
    if (!qt_use_emulated_hidpi_mode)
        return pixelValue;

    return pixelValue / qt_emulated_scale_factor;

}

template <typename T>
T qhidpiPointToPixel(const T &pointValue)
{
    if (!qt_use_emulated_hidpi_mode)
        return pointValue;

    return pointValue * qt_emulated_scale_factor;
}

// Any QVector<T> were T has operator/()
template <typename T>
QVector<T> qhidpiPixelToPoint(const QVector<T> &pixelValues)
{
    if (!qt_use_emulated_hidpi_mode)
        return pixelValues;

    QVector<T> pointValues;
    foreach (const T& pixelValue, pixelValues)
        pointValues.append(pixelValue / qt_emulated_scale_factor);
    return pointValues;
}

template <typename T>
QVector<T> qhidpiPointToPixel(const QVector<T> &pointValues)
{
    if (!qt_use_emulated_hidpi_mode)
        return pointValues;

    QVector<T> pixelValues;
    foreach (const T& pointValue, pointValues)
        pixelValues.append(pointValue * qt_emulated_scale_factor);
    return pixelValues;
}

