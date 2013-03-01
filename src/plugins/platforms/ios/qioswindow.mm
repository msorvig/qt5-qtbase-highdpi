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

#include "qiosglobal.h"
#include "qioswindow.h"
#include "qioscontext.h"
#include "qiosinputcontext.h"
#include "qiosscreen.h"
#include "qiosapplicationdelegate.h"
#include "qiosviewcontroller.h"
#include "qiosintegration.h"
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

#import <QuartzCore/CAEAGLLayer.h>

#include <QtGui/QKeyEvent>
#include <qpa/qwindowsysteminterface.h>

#include <QtDebug>

@interface EAGLView : UIView <UIKeyInput>
{
@public
    UITextAutocapitalizationType autocapitalizationType;
    UITextAutocorrectionType autocorrectionType;
    BOOL enablesReturnKeyAutomatically;
    UIKeyboardAppearance keyboardAppearance;
    UIKeyboardType keyboardType;
    UIReturnKeyType returnKeyType;
    BOOL secureTextEntry;
    QIOSWindow *m_qioswindow;
}

@property(nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property(nonatomic) UITextAutocorrectionType autocorrectionType;
@property(nonatomic) BOOL enablesReturnKeyAutomatically;
@property(nonatomic) UIKeyboardAppearance keyboardAppearance;
@property(nonatomic) UIKeyboardType keyboardType;
@property(nonatomic) UIReturnKeyType returnKeyType;
@property(nonatomic, getter=isSecureTextEntry) BOOL secureTextEntry;

@end

@implementation EAGLView

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

-(id)initWithQIOSWindow:(QIOSWindow *)window
{
    if (self = [self initWithFrame:toCGRect(window->geometry())])
        m_qioswindow = window;

    return self;
}

- (id)initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame])) {
        // Set up EAGL layer
        CAEAGLLayer *eaglLayer = static_cast<CAEAGLLayer *>(self.layer);
        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithBool:YES], kEAGLDrawablePropertyRetainedBacking,
            kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

        // Set up text input
        autocapitalizationType = UITextAutocapitalizationTypeNone;
        autocorrectionType = UITextAutocorrectionTypeNo;
        enablesReturnKeyAutomatically = NO;
        keyboardAppearance = UIKeyboardAppearanceDefault;
        keyboardType = UIKeyboardTypeDefault;
        returnKeyType = UIReturnKeyDone;
        secureTextEntry = NO;

        if (isQtApplication())
            self.hidden = YES;

        self.multipleTouchEnabled = YES;
    }

    return self;
}

- (void)layoutSubviews
{
    // This method is the de facto way to know that view has been resized,
    // or otherwise needs invalidation of its buffers. Note though that we
    // do not get this callback when the view just changes its position, so
    // the position of our QWindow (and platform window) will only get updated
    // when the size is also changed.

    if (!CGAffineTransformIsIdentity(self.transform))
        qWarning() << m_qioswindow->window()
            << "is backed by a UIView that has a transform set. This is not supported.";

    QRect geometry = fromCGRect(self.frame);
    m_qioswindow->QPlatformWindow::setGeometry(geometry);
    QWindowSystemInterface::handleGeometryChange(m_qioswindow->window(), geometry);

    // If we have a new size here we need to resize the FBO's corresponding buffers,
    // but we defer that to when the application calls makeCurrent.

    [super layoutSubviews];
}

/*
    Touch handling:

    UIKit generates [Began -> Moved -> Ended] event sequences for
    each touch point. The iOS plugin tracks each individual
    touch and assigns it an id for use by Qt. The id counter is
    incremented on each began and decrement as follows:
    1) by one when the most recent touch ends.
    2) to zero when all touches ends.

    The TouchPoint list is reused between events.
*/
- (void)updateTouchList:(NSSet *)touches withState:(Qt::TouchPointState)state
{
    QList<QWindowSystemInterface::TouchPoint> &touchPoints = m_qioswindow->touchPoints();
    QHash<UITouch *, int> &activeTouches = m_qioswindow->activeTouches();

    // Mark all touch points as stationary
    for (QList<QWindowSystemInterface::TouchPoint>::iterator it = touchPoints.begin(); it != touchPoints.end(); ++it)
        it->state = Qt::TouchPointStationary;

     // Update changed touch points with the new state
    for (UITouch *touch in touches) {
        const int touchId = activeTouches.value(touch);
        QWindowSystemInterface::TouchPoint &touchPoint = touchPoints[touchId];
        touchPoint.state = state;
        if (state == Qt::TouchPointPressed)
            touchPoint.pressure = 1.0;
        else if (state == Qt::TouchPointReleased)
            touchPoint.pressure = 0.0;

        // Set position
        QRect viewGeometry = fromCGRect(self.frame);
        QPoint touchViewLocation = fromCGPoint([touch locationInView:self]);
        QPoint touchScreenLocation = touchViewLocation + viewGeometry.topLeft();
        touchPoint.area = QRectF(touchScreenLocation , QSize(0, 0));

        CGSize fullscreenSize = self.window.rootViewController.view.bounds.size;
        touchPoint.normalPosition = QPointF(touchScreenLocation.x() / fullscreenSize.width, touchScreenLocation.y() / fullscreenSize.height);
    }
}

- (void) sendTouchEventWithTimestamp:(ulong)timeStamp
{
    // Send touch event synchronously
    QIOSIntegration *iosIntegration = static_cast<QIOSIntegration *>(QGuiApplicationPrivate::platformIntegration());
    QWindowSystemInterface::handleTouchEvent(m_qioswindow->window(), timeStamp,
                                             iosIntegration->touchDevice(), m_qioswindow->touchPoints());
    QWindowSystemInterface::flushWindowSystemEvents();
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    QWindow *window = m_qioswindow->window();

    // Transfer focus to the touched window:
    if (window != QGuiApplication::focusWindow())
        m_qioswindow->requestActivateWindow();

    // Track Cocoa touch id to Qt touch id. The UITouch pointer is constant
    // for the touch duration.
    QHash<UITouch *, int> &activeTouches = m_qioswindow->activeTouches();
    QList<QWindowSystemInterface::TouchPoint> &touchPoints = m_qioswindow->touchPoints();
    for (UITouch *touch in touches)
        activeTouches.insert(touch, m_qioswindow->touchId()++);

    // Create new touch points if needed.
    int newTouchPointsNeeded = m_qioswindow->touchId() - touchPoints.count();
    for (int i =  0; i < newTouchPointsNeeded; ++i) {
        QWindowSystemInterface::TouchPoint touchPoint;
        touchPoint.id = touchPoints.count(); // id is the index in the touchPoints list.
        touchPoints.append(touchPoint);
    }

    [self updateTouchList:touches withState:Qt::TouchPointPressed];
    [self sendTouchEventWithTimestamp:ulong(event.timestamp * 1000)];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self updateTouchList:touches withState:Qt::TouchPointMoved];
    [self sendTouchEventWithTimestamp:ulong(event.timestamp * 1000)];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self updateTouchList:touches withState:Qt::TouchPointReleased];
    [self sendTouchEventWithTimestamp:ulong(event.timestamp * 1000)];

    // Remove ended touch points from the active set (event processing has completed at this point)
    QHash<UITouch *, int> &activeTouches = m_qioswindow->activeTouches();
    for (UITouch *touch in touches) {
        int id = activeTouches.take(touch);

        // If this touch is the most recent touch we can reuse its id
        if (id == m_qioswindow->touchId() - 1)
            --m_qioswindow->touchId();
    }

    // Reset the touch id when there are no more active touches
    if (activeTouches.isEmpty())
        m_qioswindow->touchId() = 0;
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    Q_UNUSED(touches) // ### can a subset of the active touches be cancelled?

    // Clear current touch points
    m_qioswindow->activeTouches().clear();
    m_qioswindow->touchId() = 0;

    // Send cancel touch event synchronously
    QIOSIntegration *iosIntegration = static_cast<QIOSIntegration *>(QGuiApplicationPrivate::platformIntegration());
    QWindowSystemInterface::handleTouchCancelEvent(m_qioswindow->window(), ulong(event.timestamp * 1000), iosIntegration->touchDevice());
    QWindowSystemInterface::flushWindowSystemEvents();
}

@synthesize autocapitalizationType;
@synthesize autocorrectionType;
@synthesize enablesReturnKeyAutomatically;
@synthesize keyboardAppearance;
@synthesize keyboardType;
@synthesize returnKeyType;
@synthesize secureTextEntry;

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (BOOL)hasText
{
    return YES;
}

- (void)insertText:(NSString *)text
{
    QString string = QString::fromUtf8([text UTF8String]);
    int key = 0;
    if ([text isEqualToString:@"\n"])
        key = (int)Qt::Key_Return;

    // Send key event to window system interface
    QWindowSystemInterface::handleKeyEvent(
        0, QEvent::KeyPress, key, Qt::NoModifier, string, false, int(string.length()));
    QWindowSystemInterface::handleKeyEvent(
        0, QEvent::KeyRelease, key, Qt::NoModifier, string, false, int(string.length()));
}

- (void)deleteBackward
{
    // Send key event to window system interface
    QWindowSystemInterface::handleKeyEvent(
        0, QEvent::KeyPress, (int)Qt::Key_Backspace, Qt::NoModifier);
    QWindowSystemInterface::handleKeyEvent(
        0, QEvent::KeyRelease, (int)Qt::Key_Backspace, Qt::NoModifier);
}

@end

@implementation UIView (QIOS)

- (QWindow *)qwindow
{
    if ([self isKindOfClass:[EAGLView class]])
        return static_cast<EAGLView *>(self)->m_qioswindow->window();
    return nil;
}

@end

QT_BEGIN_NAMESPACE

QIOSWindow::QIOSWindow(QWindow *window)
    : QPlatformWindow(window)
    , m_view([[EAGLView alloc] initWithQIOSWindow:this])
    , m_touchId(0)
    , m_requestedGeometry(QPlatformWindow::geometry())
    , m_windowLevel(0)
    , m_devicePixelRatio(1.0)
{
    setParent(parent());
    setWindowState(window->windowState());

    // Retina support: get screen scale factor and set it in the content view.
    // This will make framebufferObject() create a 2x frame buffer on retina
    // displays. Also set m_devicePixelRatio which is used for scaling the
    // paint device.
    if ([[UIScreen mainScreen] respondsToSelector:@selector(scale)] == YES) {
        m_devicePixelRatio = [[UIScreen mainScreen] scale];
        [m_view setContentScaleFactor: m_devicePixelRatio];
    }
}

QIOSWindow::~QIOSWindow()
{
    [m_view removeFromSuperview];
    [m_view release];
}

bool QIOSWindow::blockedByModal()
{
    QWindow *modalWindow = QGuiApplication::modalWindow();
    return modalWindow && modalWindow != window();
}

void QIOSWindow::setVisible(bool visible)
{
    QPlatformWindow::setVisible(visible);
    m_view.hidden = !visible;

    if (!isQtApplication())
        return;

    // Since iOS doesn't do window management the way a Qt application
    // expects, we need to raise and activate windows ourselves:
    if (visible)
        updateWindowLevel();

    if (blockedByModal()) {
        if (visible)
            raise();
        return;
    }

    if (visible) {
        requestActivateWindow();
    } else {
        // Activate top-most visible QWindow:
        NSArray *subviews = rootViewController().view.subviews;
        for (int i = int(subviews.count) - 1; i >= 0; --i) {
            UIView *view = [subviews objectAtIndex:i];
            if (!view.hidden) {
                if (QWindow *window = view.qwindow) {
                    static_cast<QIOSWindow *>(window->handle())->requestActivateWindow();
                    break;
                }
            }
        }
    }
}

void QIOSWindow::setGeometry(const QRect &rect)
{
    // If the window is in fullscreen, just bookkeep the requested
    // geometry in case the window goes into Qt::WindowNoState later:
    m_requestedGeometry = rect;
    if (window()->windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen))
        return;

    // Since we don't support transformations on the UIView, we can set the frame
    // directly and let UIKit deal with translating that into bounds and center.
    // Changing the size of the view will end up in a call to -[EAGLView layoutSubviews]
    // which will update QWindowSystemInterface with the new size.
    m_view.frame = toCGRect(rect);
}

void QIOSWindow::setWindowState(Qt::WindowState state)
{
    // FIXME: Figure out where or how we should disable/enable the statusbar.
    // Perhaps setting QWindow to maximized should also mean that we'll show
    // the statusbar, and vice versa for fullscreen?

    switch (state) {
    case Qt::WindowMaximized:
    case Qt::WindowFullScreen: {
        // Since UIScreen does not take orientation into account when
        // reporting geometry, we need to look at the top view instead:
        CGSize fullscreenSize = m_view.window.rootViewController.view.bounds.size;
        m_view.frame = CGRectMake(0, 0, fullscreenSize.width, fullscreenSize.height);
        m_view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        break; }
    default:
        m_view.frame = toCGRect(m_requestedGeometry);
        m_view.autoresizingMask = UIViewAutoresizingNone;
        break;
    }
}

void QIOSWindow::setParent(const QPlatformWindow *parentWindow)
{
    if (parentWindow) {
        UIView *parentView = reinterpret_cast<UIView *>(parentWindow->winId());
        [parentView addSubview:m_view];
    } else if (isQtApplication()) {
        [rootViewController().view addSubview:m_view];
    }
}

void QIOSWindow::requestActivateWindow()
{
    // Note that several windows can be active at the same time if they exist in the same
    // hierarchy (transient children). But only one window can be QGuiApplication::focusWindow().
    // Dispite the name, 'requestActivateWindow' means raise and transfer focus to the window:
    if (blockedByModal())
        return;

    raise();
    QPlatformInputContext *context = QGuiApplicationPrivate::platformIntegration()->inputContext();
    static_cast<QIOSInputContext *>(context)->focusViewChanged(m_view);
    QPlatformWindow::requestActivateWindow();
}

void QIOSWindow::raiseOrLower(bool raise)
{
    // Re-insert m_view at the correct index among its sibling views
    // (QWindows) according to their current m_windowLevel:
    if (!isQtApplication())
        return;

    NSArray *subviews = m_view.superview.subviews;
    if (subviews.count == 1)
        return;

    for (int i = int(subviews.count) - 1; i >= 0; --i) {
        UIView *view = static_cast<UIView *>([subviews objectAtIndex:i]);
        if (view.hidden || view == m_view)
            continue;
        int level = static_cast<QIOSWindow *>(view.qwindow->handle())->m_windowLevel;
        if (m_windowLevel > level || (raise && m_windowLevel == level)) {
            [m_view.superview insertSubview:m_view aboveSubview:view];
            return;
        }
    }
    [m_view.superview insertSubview:m_view atIndex:0];
}

void QIOSWindow::updateWindowLevel()
{
    Qt::WindowType type = static_cast<Qt::WindowType>(int(window()->flags() & Qt::WindowType_Mask));

    if (type == Qt::ToolTip)
        m_windowLevel = 120;
    else if (window()->flags() & Qt::WindowStaysOnTopHint)
        m_windowLevel = 100;
    else if (window()->isModal())
        m_windowLevel = 30;
    else if (type & Qt::Popup & ~Qt::Window)
        m_windowLevel = 20;
    else if (type == Qt::Tool)
        m_windowLevel = 10;
    else
        m_windowLevel = 0;

    // A window should be in at least the same m_windowLevel as its parent:
    QWindow *transientParent = window()->transientParent();
    QIOSWindow *transientParentWindow = transientParent ? static_cast<QIOSWindow *>(transientParent->handle()) : 0;
    if (transientParentWindow)
        m_windowLevel = qMax(transientParentWindow->m_windowLevel, m_windowLevel);
}

void QIOSWindow::handleContentOrientationChange(Qt::ScreenOrientation orientation)
{
    // Keep the status bar in sync with content orientation. This will ensure
    // that the task bar (and associated gestures) are aligned correctly:
    UIDeviceOrientation uiOrientation = fromQtScreenOrientation(orientation);
    [[UIApplication sharedApplication] setStatusBarOrientation:uiOrientation animated:NO];
}

qreal QIOSWindow::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

int QIOSWindow::effectiveWidth() const
{
    return geometry().width() * m_devicePixelRatio;
}

int QIOSWindow::effectiveHeight() const
{
    return geometry().height() * m_devicePixelRatio;
}

QT_END_NAMESPACE
