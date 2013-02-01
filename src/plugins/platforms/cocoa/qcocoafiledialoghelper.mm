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

#include "qcocoafiledialoghelper.h"

#ifndef QT_NO_FILEDIALOG

/*****************************************************************************
  QFileDialog debug facilities
 *****************************************************************************/
//#define DEBUG_FILEDIALOG_FILTERS

#include <qguiapplication.h>
#include <private/qguiapplication_p.h>
#include "qt_mac_p.h"
#include "qcocoahelpers.h"
#include <qregexp.h>
#include <qbuffer.h>
#include <qdebug.h>
#include <qstringlist.h>
#include <qtextcodec.h>
#include <qvarlengtharray.h>
#include <stdlib.h>
#include <qabstracteventdispatcher.h>
#include "qcocoaautoreleasepool.h"

#include <qpa/qplatformnativeinterface.h>

#import <AppKit/NSSavePanel.h>

QT_FORWARD_DECLARE_CLASS(QString)
QT_FORWARD_DECLARE_CLASS(QStringList)
QT_FORWARD_DECLARE_CLASS(QFileInfo)
QT_FORWARD_DECLARE_CLASS(QWindow)
QT_USE_NAMESPACE

typedef QSharedPointer<QFileDialogOptions> SharedPointerFileDialogOptions;

@class QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate);

@interface QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate)
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
    : NSObject<NSOpenSavePanelDelegate>
#else
    : NSObject
#endif
{
    @public
    NSOpenPanel *mOpenPanel;
    NSSavePanel *mSavePanel;
    NSView *mAccessoryView;
    NSPopUpButton *mPopUpButton;
    NSTextField *mTextField;
    QCocoaFileDialogHelper *mHelper;
    NSString *mCurrentDir;

    int mReturnCode;

    SharedPointerFileDialogOptions mOptions;
    QString *mLastFilterCheckPath;
    QString *mCurrentSelection;
    QStringList *mQDirFilterEntryList;
    QStringList *mNameFilterDropDownList;
    QStringList *mSelectedNameFilter;
}

- (NSString *)strip:(const QString &)label;
- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename;
- (void)filterChanged:(id)sender;
- (void)showModelessPanel;
- (BOOL)runApplicationModalPanel;
- (void)showWindowModalSheet:(QWindow *)docWidget;
- (void)updateProperties;
- (QStringList)acceptableExtensionsForSave;
- (QString)removeExtensions:(const QString &)filter;
- (void)createTextField;
- (void)createPopUpButton:(const QString &)selectedFilter hideDetails:(BOOL)hideDetails;
- (QStringList)findStrippedFilterWithVisualFilterName:(QString)name;
- (void)createAccessory;

@end

@implementation QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate)

- (id)initWithAcceptMode:
    (const QString &)selectFile
    options:(SharedPointerFileDialogOptions)options
    helper:(QCocoaFileDialogHelper *)helper
{
    self = [super init];
    mOptions = options;
    if (mOptions->acceptMode() == QT_PREPEND_NAMESPACE(QFileDialogOptions::AcceptOpen)){
        mOpenPanel = [NSOpenPanel openPanel];
        mSavePanel = mOpenPanel;
    } else {
        mSavePanel = [NSSavePanel savePanel];
        [mSavePanel setCanSelectHiddenExtension:YES];
        mOpenPanel = 0;
    }

    if ([mSavePanel respondsToSelector:@selector(setLevel:)])
        [mSavePanel setLevel:NSModalPanelWindowLevel];
    [mSavePanel setDelegate:self];
    mReturnCode = -1;
    mHelper = helper;
    mLastFilterCheckPath = new QString;
    mQDirFilterEntryList = new QStringList;
    mNameFilterDropDownList = new QStringList(mOptions->nameFilters());
    QString selectedVisualNameFilter = mOptions->initiallySelectedNameFilter();
    mSelectedNameFilter = new QStringList([self findStrippedFilterWithVisualFilterName:selectedVisualNameFilter]);

    QFileInfo sel(selectFile);
    if (sel.isDir()){
        mCurrentDir = [QCFString::toNSString(sel.absoluteFilePath()) retain];
        mCurrentSelection = new QString;
    } else {
        mCurrentDir = [QCFString::toNSString(sel.absolutePath()) retain];
        mCurrentSelection = new QString(sel.absoluteFilePath());
    }

    [mSavePanel setTitle:QCFString::toNSString(options->windowTitle())];
    [self createPopUpButton:selectedVisualNameFilter hideDetails:options->testOption(QFileDialogOptions::HideNameFilterDetails)];
    [self createTextField];
    [self createAccessory];
    [mSavePanel setAccessoryView:mNameFilterDropDownList->size() > 1 ? mAccessoryView : nil];


    if (mOptions->isLabelExplicitlySet(QFileDialogOptions::Accept))
        [mSavePanel setPrompt:[self strip:options->labelText(QFileDialogOptions::Accept)]];
    if (mOptions->isLabelExplicitlySet(QFileDialogOptions::FileName))
        [mSavePanel setNameFieldLabel:[self strip:options->labelText(QFileDialogOptions::FileName)]];

    [self updateProperties];
    [mSavePanel retain];
    return self;
}

- (void)dealloc
{
    delete mLastFilterCheckPath;
    delete mQDirFilterEntryList;
    delete mNameFilterDropDownList;
    delete mSelectedNameFilter;
    delete mCurrentSelection;

    if ([mSavePanel respondsToSelector:@selector(orderOut:)])
        [mSavePanel orderOut:mSavePanel];
    [mSavePanel setAccessoryView:nil];
    [mPopUpButton release];
    [mTextField release];
    [mAccessoryView release];
    [mSavePanel setDelegate:nil];
    [mSavePanel release];
    [mCurrentDir release];
    [super dealloc];
}

static QString strippedText(QString s)
{
    s.remove( QString::fromLatin1("...") );
    int i = 0;
    while (i < s.size()) {
        ++i;
        if (s.at(i-1) != QLatin1Char('&'))
            continue;
        if (i < s.size() && s.at(i) == QLatin1Char('&'))
            ++i;
        s.remove(i-1,1);
    }
    return s.trimmed();
}

- (NSString *)strip:(const QString &)label
{
    return QCFString::toNSString(strippedText(label));
}

- (void)closePanel
{
    *mCurrentSelection = QT_PREPEND_NAMESPACE(QCFString::toQString)([[mSavePanel URL] path]);
    if ([mSavePanel respondsToSelector:@selector(close)])
        [mSavePanel close];
    if ([mSavePanel isSheet])
        [NSApp endSheet: mSavePanel];
}

- (void)showModelessPanel
{
    if (mOpenPanel){
        QFileInfo info(!mCurrentSelection->isEmpty() ? *mCurrentSelection : QT_PREPEND_NAMESPACE(QCFString::toQString)(mCurrentDir));
        NSString *filepath = QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.filePath());
        bool selectable = (mOptions->acceptMode() == QFileDialogOptions::AcceptSave)
            || [self panel:nil shouldShowFilename:filepath];

        [self updateProperties];
        [mOpenPanel setAllowedFileTypes:nil];
        [mOpenPanel setDirectoryURL:selectable ? [NSURL fileURLWithPath:QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.filePath())]
                                               : [NSURL fileURLWithPath:QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.path())]];
        [mOpenPanel beginWithCompletionHandler:^(NSInteger result){
            mReturnCode = result;
            if (mHelper)
                mHelper->QNSOpenSavePanelDelegate_panelClosed(result == NSOKButton);
        }];
    }
}

- (BOOL)runApplicationModalPanel
{
    QFileInfo info(!mCurrentSelection->isEmpty() ? *mCurrentSelection : QT_PREPEND_NAMESPACE(QCFString::toQString)(mCurrentDir));
    NSString *filepath = QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.filePath());
    bool selectable = (mOptions->acceptMode() == QFileDialogOptions::AcceptSave)
        || [self panel:nil shouldShowFilename:filepath];

    [mSavePanel setDirectoryURL:selectable ? [NSURL fileURLWithPath:QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.filePath())]
                                           : [NSURL fileURLWithPath:QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.path())]];

    // Call processEvents in case the event dispatcher has been interrupted, and needs to do
    // cleanup of modal sessions. Do this before showing the native dialog, otherwise it will
    // close down during the cleanup.
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
    mReturnCode = [mSavePanel runModal];

    QAbstractEventDispatcher::instance()->interrupt();
    return (mReturnCode == NSOKButton);
}

- (QT_PREPEND_NAMESPACE(QPlatformDialogHelper::DialogCode))dialogResultCode
{
    return (mReturnCode == NSOKButton) ? QT_PREPEND_NAMESPACE(QPlatformDialogHelper::Accepted) : QT_PREPEND_NAMESPACE(QPlatformDialogHelper::Rejected);
}

- (void)showWindowModalSheet:(QWindow *)parent
{
    QFileInfo info(!mCurrentSelection->isEmpty() ? *mCurrentSelection : QT_PREPEND_NAMESPACE(QCFString::toQString)(mCurrentDir));
    NSString *filepath = QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.filePath());
    bool selectable = (mOptions->acceptMode() == QFileDialogOptions::AcceptSave)
        || [self panel:nil shouldShowFilename:filepath];

    [self updateProperties];
    [mSavePanel setDirectoryURL:selectable ? [NSURL fileURLWithPath:QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.filePath())]
                                           : [NSURL fileURLWithPath:QT_PREPEND_NAMESPACE(QCFString::toNSString)(info.path())]];
    NSWindow *nsparent = static_cast<NSWindow *>(qGuiApp->platformNativeInterface()->nativeResourceForWindow("nswindow", parent));

    [mSavePanel beginSheetModalForWindow:nsparent completionHandler:^(NSInteger result){
        mReturnCode = result;
        if (mHelper)
            mHelper->QNSOpenSavePanelDelegate_panelClosed(result == NSOKButton);
    }];
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename
{
    Q_UNUSED(sender);

    if ([filename length] == 0)
        return NO;

    // Always accept directories regardless of their names (unless it is a bundle):
    BOOL isDir;
    if ([[NSFileManager defaultManager] fileExistsAtPath:filename isDirectory:&isDir] && isDir) {
        if ([mSavePanel treatsFilePackagesAsDirectories] == NO) {
            if ([[NSWorkspace sharedWorkspace] isFilePackageAtPath:filename] == NO)
                return YES;
        }
    }

    QString qtFileName = QT_PREPEND_NAMESPACE(QCFString::toQString)(filename);
    QFileInfo info(qtFileName.normalized(QT_PREPEND_NAMESPACE(QString::NormalizationForm_C)));
    QString path = info.absolutePath();
    if (path != *mLastFilterCheckPath){
        *mLastFilterCheckPath = path;
        *mQDirFilterEntryList = info.dir().entryList(mOptions->filter());
    }
    // Check if the QDir filter accepts the file:
    if (!mQDirFilterEntryList->contains(info.fileName()))
        return NO;

    // No filter means accept everything
    if (mSelectedNameFilter->isEmpty())
        return YES;
    // Check if the current file name filter accepts the file:
    for (int i=0; i<mSelectedNameFilter->size(); ++i) {
        if (QDir::match(mSelectedNameFilter->at(i), qtFileName))
            return YES;
    }
    return NO;
}

- (NSString *)panel:(id)sender userEnteredFilename:(NSString *)filename confirmed:(BOOL)okFlag
{
    Q_UNUSED(sender);
    if (!okFlag)
        return filename;
    if (!mOptions->testOption(QFileDialogOptions::DontConfirmOverwrite))
        return filename;

    // User has clicked save, and no overwrite confirmation should occur.
    // To get the latter, we need to change the name we return (hence the prefix):
    return [@"___qt_very_unlikely_prefix_" stringByAppendingString:filename];
}

- (void)setNameFilters:(const QStringList &)filters hideDetails:(BOOL)hideDetails
{
    [mPopUpButton removeAllItems];
    *mNameFilterDropDownList = filters;
    if (filters.size() > 0){
        for (int i=0; i<filters.size(); ++i) {
            QString filter = hideDetails ? [self removeExtensions:filters.at(i)] : filters.at(i);
            [mPopUpButton addItemWithTitle:QT_PREPEND_NAMESPACE(QCFString::toNSString)(filter)];
        }
        [mPopUpButton selectItemAtIndex:0];
        [mSavePanel setAccessoryView:mAccessoryView];
    } else
        [mSavePanel setAccessoryView:nil];

    [self filterChanged:self];
}

- (void)filterChanged:(id)sender
{
    // This mDelegate function is called when the _name_ filter changes.
    Q_UNUSED(sender);
    QString selection = mNameFilterDropDownList->value([mPopUpButton indexOfSelectedItem]);
    *mSelectedNameFilter = [self findStrippedFilterWithVisualFilterName:selection];
    if ([mSavePanel respondsToSelector:@selector(validateVisibleColumns:)])
        [mSavePanel validateVisibleColumns];
    [self updateProperties];
    if (mHelper)
        mHelper->QNSOpenSavePanelDelegate_filterSelected([mPopUpButton indexOfSelectedItem]);
}

- (QString)currentNameFilter
{
    return mNameFilterDropDownList->value([mPopUpButton indexOfSelectedItem]);
}

- (QStringList)selectedFiles
{
    if (mOpenPanel) {
        QStringList result;
        NSArray* array = [mOpenPanel URLs];
        for (NSUInteger i=0; i<[array count]; ++i)
            result << QCFString::toQString([[array objectAtIndex:i] path]);
        return result;
    } else {
        QStringList result;
        QString filename = QT_PREPEND_NAMESPACE(QCFString::toQString)([[mSavePanel URL] path]);
        result << filename.remove(QLatin1String("___qt_very_unlikely_prefix_"));
        return result;
    }
}

- (void)updateProperties
{
    // Call this functions if mFileMode, mFileOptions,
    // mNameFilterDropDownList or mQDirFilter changes.
    // The savepanel does not contain the neccessary functions for this.
    const QT_PREPEND_NAMESPACE(QFileDialogOptions::FileMode) fileMode = mOptions->fileMode();
    bool chooseFilesOnly = fileMode == QT_PREPEND_NAMESPACE(QFileDialogOptions::ExistingFile)
        || fileMode == QT_PREPEND_NAMESPACE(QFileDialogOptions::ExistingFiles);
    bool chooseDirsOnly = fileMode == QT_PREPEND_NAMESPACE(QFileDialogOptions::Directory)
        || fileMode == QT_PREPEND_NAMESPACE(QFileDialogOptions::DirectoryOnly)
        || mOptions->testOption(QT_PREPEND_NAMESPACE(QFileDialogOptions::ShowDirsOnly));

    [mOpenPanel setCanChooseFiles:!chooseDirsOnly];
    [mOpenPanel setCanChooseDirectories:!chooseFilesOnly];
    [mSavePanel setCanCreateDirectories:!(mOptions->testOption(QT_PREPEND_NAMESPACE(QFileDialogOptions::ReadOnly)))];
    [mOpenPanel setAllowsMultipleSelection:(fileMode == QT_PREPEND_NAMESPACE(QFileDialogOptions::ExistingFiles))];
    [mOpenPanel setResolvesAliases:!(mOptions->testOption(QT_PREPEND_NAMESPACE(QFileDialogOptions::DontResolveSymlinks)))];
    [mOpenPanel setTitle:QCFString::toNSString(mOptions->windowTitle())];
    [mSavePanel setTitle:QCFString::toNSString(mOptions->windowTitle())];
    [mPopUpButton setHidden:chooseDirsOnly];    // TODO hide the whole sunken pane instead?

    QStringList ext = [self acceptableExtensionsForSave];
    const QString defaultSuffix = mOptions->defaultSuffix();
    if (!ext.isEmpty() && !defaultSuffix.isEmpty())
        ext.prepend(defaultSuffix);
    [mSavePanel setAllowedFileTypes:ext.isEmpty() ? nil : QT_PREPEND_NAMESPACE(qt_mac_QStringListToNSMutableArray(ext))];

    if ([mSavePanel respondsToSelector:@selector(isVisible)] && [mSavePanel isVisible]) {
        if ([mSavePanel respondsToSelector:@selector(validateVisibleColumns)])
            [mSavePanel validateVisibleColumns];
    }
}

- (void)panelSelectionDidChange:(id)sender
{
    Q_UNUSED(sender);
    if (mHelper) {
        QString selection = QT_PREPEND_NAMESPACE(QCFString::toQString([[mSavePanel URL] path]));
        if (selection != mCurrentSelection) {
            *mCurrentSelection = selection;
            mHelper->QNSOpenSavePanelDelegate_selectionChanged(selection);
        }
    }
}

- (void)panel:(id)sender directoryDidChange:(NSString *)path
{
    Q_UNUSED(sender);
    if (!mHelper)
        return;
    if ([path isEqualToString:mCurrentDir])
        return;

    [mCurrentDir release];
    mCurrentDir = [path retain];
    mHelper->QNSOpenSavePanelDelegate_directoryEntered(QT_PREPEND_NAMESPACE(QCFString::toQString(mCurrentDir)));
}

/*
    Returns a list of extensions (e.g. "png", "jpg", "gif")
    for the current name filter. If a filter do not conform
    to the format *.xyz or * or *.*, an empty list
    is returned meaning accept everything.
*/
- (QStringList)acceptableExtensionsForSave
{
    QStringList result;
    for (int i=0; i<mSelectedNameFilter->count(); ++i) {
        const QString &filter = mSelectedNameFilter->at(i);
        if (filter.startsWith(QLatin1String("*."))
                && !filter.contains(QLatin1Char('?'))
                && filter.count(QLatin1Char('*')) == 1) {
            result += filter.mid(2);
        } else {
            return QStringList(); // Accept everything
        }
    }
    return result;
}

- (QString)removeExtensions:(const QString &)filter
{
    QRegExp regExp(QT_PREPEND_NAMESPACE(QString::fromLatin1)(QT_PREPEND_NAMESPACE(QPlatformFileDialogHelper::filterRegExp)));
    if (regExp.indexIn(filter) != -1)
        return regExp.cap(1).trimmed();
    return filter;
}

- (void)createTextField
{
    NSRect textRect = { { 0.0, 3.0 }, { 100.0, 25.0 } };
    mTextField = [[NSTextField alloc] initWithFrame:textRect];
    [[mTextField cell] setFont:[NSFont systemFontOfSize:
            [NSFont systemFontSizeForControlSize:NSRegularControlSize]]];
    [mTextField setAlignment:NSRightTextAlignment];
    [mTextField setEditable:false];
    [mTextField setSelectable:false];
    [mTextField setBordered:false];
    [mTextField setDrawsBackground:false];
    if (mOptions->isLabelExplicitlySet(QFileDialogOptions::FileType))
        [mTextField setStringValue:[self strip:mOptions->labelText(QFileDialogOptions::FileType)]];
}

- (void)createPopUpButton:(const QString &)selectedFilter hideDetails:(BOOL)hideDetails
{
    NSRect popUpRect = { { 100.0, 5.0 }, { 250.0, 25.0 } };
    mPopUpButton = [[NSPopUpButton alloc] initWithFrame:popUpRect pullsDown:NO];
    [mPopUpButton setTarget:self];
    [mPopUpButton setAction:@selector(filterChanged:)];

    if (mNameFilterDropDownList->size() > 0) {
        int filterToUse = -1;
        for (int i=0; i<mNameFilterDropDownList->size(); ++i) {
            QString currentFilter = mNameFilterDropDownList->at(i);
            if (selectedFilter == currentFilter ||
                (filterToUse == -1 && currentFilter.startsWith(selectedFilter)))
                filterToUse = i;
            QString filter = hideDetails ? [self removeExtensions:currentFilter] : currentFilter;
            [mPopUpButton addItemWithTitle:QT_PREPEND_NAMESPACE(QCFString::toNSString)(filter)];
        }
        if (filterToUse != -1)
            [mPopUpButton selectItemAtIndex:filterToUse];
    }
}

- (QStringList) findStrippedFilterWithVisualFilterName:(QString)name
{
    for (int i=0; i<mNameFilterDropDownList->size(); ++i) {
        if (mNameFilterDropDownList->at(i).startsWith(name))
            return QPlatformFileDialogHelper::cleanFilterList(mNameFilterDropDownList->at(i));
    }
    return QStringList();
}

- (void)createAccessory
{
    NSRect accessoryRect = { { 0.0, 0.0 }, { 450.0, 33.0 } };
    mAccessoryView = [[NSView alloc] initWithFrame:accessoryRect];
    [mAccessoryView addSubview:mTextField];
    [mAccessoryView addSubview:mPopUpButton];
}

@end

QT_BEGIN_NAMESPACE

QCocoaFileDialogHelper::QCocoaFileDialogHelper()
    :mDelegate(0)
{
}

QCocoaFileDialogHelper::~QCocoaFileDialogHelper()
{
    if (!mDelegate)
        return;
    QCocoaAutoReleasePool pool;
    [reinterpret_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate) release];
    mDelegate = 0;
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_selectionChanged(const QString &newPath)
{
    emit currentChanged(newPath);
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_panelClosed(bool accepted)
{
    if (accepted) {
        emit accept();
    } else {
        emit reject();
    }
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_directoryEntered(const QString &newDir)
{
    // ### fixme: priv->setLastVisitedDirectory(newDir);
    emit directoryEntered(newDir);
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_filterSelected(int menuIndex)
{
    const QStringList filters = options()->nameFilters();
    emit filterSelected(menuIndex >= 0 && menuIndex < filters.size() ? filters.at(menuIndex) : QString());
}

extern OSErr qt_mac_create_fsref(const QString &, FSRef *); // qglobal.cpp
extern void qt_mac_to_pascal_string(QString s, Str255 str, TextEncoding encoding=0, int len=-1); // qglobal.cpp

void QCocoaFileDialogHelper::setDirectory(const QString &directory)
{
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if (delegate)
        [delegate->mSavePanel setDirectoryURL:[NSURL fileURLWithPath:QCFString::toNSString(directory)]];
}

QString QCocoaFileDialogHelper::directory() const
{
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if (delegate)
        return QCFString::toQString([delegate->mSavePanel directory]);
    return QString();
}

void QCocoaFileDialogHelper::selectFile(const QString &filename)
{
    QString filePath = filename;
    if (QDir::isRelativePath(filePath))
        filePath = QFileInfo(directory(), filePath).filePath();

    // There seems to no way to select a file once the dialog is running.
    // So do the next best thing, set the file's directory:
    setDirectory(QFileInfo(filePath).absolutePath());
}

QStringList QCocoaFileDialogHelper::selectedFiles() const
{
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if (delegate)
        return [delegate selectedFiles];
    return QStringList();
}

void QCocoaFileDialogHelper::setFilter()
{
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if (!delegate)
        return;
    const SharedPointerFileDialogOptions &opts = options();
    [delegate->mSavePanel setTitle:QCFString::toNSString(opts->windowTitle())];
    if (opts->isLabelExplicitlySet(QFileDialogOptions::Accept))
        [delegate->mSavePanel setPrompt:[delegate strip:opts->labelText(QFileDialogOptions::Accept)]];
    if (opts->isLabelExplicitlySet(QFileDialogOptions::FileName))
        [delegate->mSavePanel setNameFieldLabel:[delegate strip:opts->labelText(QFileDialogOptions::FileName)]];

    [delegate updateProperties];
}

void QCocoaFileDialogHelper::selectNameFilter(const QString &filter)
{
    if (!options())
        return;
    const int index = options()->nameFilters().indexOf(filter);
    if (index != -1) {
        QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
        if (!delegate)
            return;
        [delegate->mPopUpButton selectItemAtIndex:index];
        [delegate filterChanged:nil];
    }
}

QString QCocoaFileDialogHelper::selectedNameFilter() const
{
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if (!delegate)
        return QString();
    int index = [delegate->mPopUpButton indexOfSelectedItem];
    if (index >= options()->nameFilters().count())
        return QString();
    return index != -1 ? options()->nameFilters().at(index) : QString();
}

void QCocoaFileDialogHelper::hide()
{
    hideCocoaFilePanel();
}

bool QCocoaFileDialogHelper::show(Qt::WindowFlags windowFlags, Qt::WindowModality windowModality, QWindow *parent)
{
//    Q_Q(QFileDialog);
    if (windowFlags & Qt::WindowStaysOnTopHint) {
        // The native file dialog tries all it can to stay
        // on the NSModalPanel level. And it might also show
        // its own "create directory" dialog that we cannot control.
        // So we need to use the non-native version in this case...
        return false;
    }

    return showCocoaFilePanel(windowModality, parent);
}

void QCocoaFileDialogHelper::createNSOpenSavePanelDelegate()
{
    if (mDelegate)
        return;
    QCocoaAutoReleasePool pool;
    const SharedPointerFileDialogOptions &opts = options();
    const QStringList selectedFiles = opts->initiallySelectedFiles();
    const QString directory = opts->initialDirectory();
    const bool selectDir = selectedFiles.isEmpty();
    QString selection(selectDir ? directory : selectedFiles.front());
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = [[QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) alloc]
        initWithAcceptMode:
            selection
            options:opts
            helper:this];

    mDelegate = delegate;
}

bool QCocoaFileDialogHelper::showCocoaFilePanel(Qt::WindowModality windowModality, QWindow *parent)
{
    createNSOpenSavePanelDelegate();
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if (!delegate)
        return false;
    if (windowModality == Qt::NonModal)
        [delegate showModelessPanel];
    else if (windowModality == Qt::WindowModal && parent)
        [delegate showWindowModalSheet:parent];
    // no need to show a Qt::ApplicationModal dialog here, since it will be done in _q_platformRunNativeAppModalPanel()
    return true;
}

bool QCocoaFileDialogHelper::hideCocoaFilePanel()
{
    if (!mDelegate){
        // Nothing to do. We return false to leave the question
        // open regarding whether or not to go native:
        return false;
    } else {
        QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
        [delegate closePanel];
        // Even when we hide it, we are still using a
        // native dialog, so return true:
        return true;
    }
}

void QCocoaFileDialogHelper::exec()
{
    // Note: If NSApp is not running (which is the case if e.g a top-most
    // QEventLoop has been interrupted, and the second-most event loop has not
    // yet been reactivated (regardless if [NSApp run] is still on the stack)),
    // showing a native modal dialog will fail.
    QCocoaAutoReleasePool pool;
    QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *delegate = static_cast<QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate) *>(mDelegate);
    if ([delegate runApplicationModalPanel])
        emit accept();
    else
        emit reject();

}

bool QCocoaFileDialogHelper::defaultNameFilterDisables() const
{
    return true;
}

QT_END_NAMESPACE

#endif // QT_NO_FILEDIALOG
