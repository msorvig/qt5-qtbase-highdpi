#include "qemulatedhidpi_p.h"
bool qt_use_emulated_hidpi_mode_set = false;
bool qt_use_emulated_hidpi_mode = false;
const qreal qt_emulated_scale_factor = 2.0;

/*void qhidpiSetEmulationEnabled(bool enable)
{
    qt_use_emulated_hidpi_mode = enable;
}
*/
bool qhidpiIsEmulationEnabled()
{
    if (!qt_use_emulated_hidpi_mode_set) {
        qt_use_emulated_hidpi_mode = !qgetenv("QT_EMULATED_HIGHDPI").isEmpty();
        qt_use_emulated_hidpi_mode_set = true;
    }

    return qt_use_emulated_hidpi_mode;
}

qreal qhidpiIsEmulationGetScaleFactor()
{
    if (!qhidpiIsEmulationEnabled())
        return 1.0f;

    return qt_emulated_scale_factor;
}

QRect qhidpiPixelToPoint(const QRect &pixelRect)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelRect;

    return QRect(pixelRect.topLeft() / qt_emulated_scale_factor, pixelRect.size() / qt_emulated_scale_factor);
}

QRect qhidpiPointToPixel(const QRect &pointRect)
{
    if (!qhidpiIsEmulationEnabled())
        return pointRect;

    return QRect(pointRect.topLeft() * qt_emulated_scale_factor, pointRect.size() * qt_emulated_scale_factor);
}

QRectF qhidpiPixelToPoint(const QRectF &pixelRect)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelRect;

    return QRectF(pixelRect.topLeft() / qt_emulated_scale_factor, pixelRect.size() / qt_emulated_scale_factor);
}

QRectF qhidpiPointToPixel(const QRectF &pointRect)
{
    if (!qhidpiIsEmulationEnabled())
        return pointRect;

    return QRectF(pointRect.topLeft() * qt_emulated_scale_factor, pointRect.size() * qt_emulated_scale_factor);
}

QSize qhidpiPixelToPoint(const QSize &pixelSize)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelSize;

    return pixelSize / qt_emulated_scale_factor;
}

QSize qhidpiPointToPixel(const QSize &pointSize)
{
    if (!qhidpiIsEmulationEnabled())
        return pointSize;

    return pointSize * qt_emulated_scale_factor;
}

QSizeF qhidpiPixelToPoint(const QSizeF &pixelSize)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelSize;

    return pixelSize / qt_emulated_scale_factor;
}

QSizeF qhidpiPointToPixel(const QSizeF &pointSize)
{
    if (!qhidpiIsEmulationEnabled())
        return pointSize;

    return pointSize * qt_emulated_scale_factor;
}

QPoint qhidpiPixelToPoint(const QPoint &pixelPoint)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelPoint;

    return pixelPoint / qt_emulated_scale_factor;
}

QPoint qhidpiPointToPixel(const QPoint &pointPoint)
{
    if (!qhidpiIsEmulationEnabled())
        return pointPoint;

    return pointPoint * qt_emulated_scale_factor;
}

QPointF qhidpiPixelToPoint(const QPointF &pixelPoint)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelPoint;

    return pixelPoint / qt_emulated_scale_factor;
}

QPointF qhidpiPointToPixel(const QPointF &pointPoint)
{
    if (!qhidpiIsEmulationEnabled())
        return pointPoint;

    return pointPoint * qt_emulated_scale_factor;
}

QMargins qhidpiPixelToPoint(const QMargins &pixelMargins)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelMargins;

    return QMargins(pixelMargins.left() / qt_emulated_scale_factor, pixelMargins.top() / qt_emulated_scale_factor,
                    pixelMargins.right() / qt_emulated_scale_factor, pixelMargins.bottom() / qt_emulated_scale_factor);
}

QMargins qhidpiPointToPixel(const QMargins &pointMargins)
{
    if (!qhidpiIsEmulationEnabled())
        return pointMargins;

    return QMargins(pointMargins.left() * qt_emulated_scale_factor, pointMargins.top() * qt_emulated_scale_factor,
                    pointMargins.right() * qt_emulated_scale_factor, pointMargins.bottom() * qt_emulated_scale_factor);
}

QRegion qhidpiPixelToPoint(const QRegion &pixelRegion)
{
    if (!qhidpiIsEmulationEnabled())
        return pixelRegion;

    QRegion pointRegion;
    foreach (const QRect &rect, pixelRegion.rects())
        pointRegion += qhidpiPixelToPoint(rect);
    return pixelRegion;
}

QRegion qhidpiPointToPixel(const QRegion &pointRegion)
{
    if (!qhidpiIsEmulationEnabled())
        return pointRegion;

    QRegion pixelRegon;
    foreach (const QRect &rect, pointRegion.rects())
        pixelRegon += qhidpiPointToPixel(rect);
    return pixelRegon; // ### figure it out
}
