/*
 *   File name:	k4dirstat.cpp
 *   Summary:	The K4DirStat Application
 *   License:	GPL - See file COPYING for details.
 *
 *   Author:	Stefan Hundhammer <kdirstat@gmx.de>
 *		Joshua Hodosh <kdirstat@grumpypenguin.org>
 *		Parts auto-generated by KDevelop
 *
 *   Updated:	2010-03-01
 */

#include "k4dirstat.h"
//#include "k4dirstatview.h"
#include "settings.h"

#include <QtGui/QDropEvent>
#include <QtGui/QPainter>

#include <kconfigdialog.h>
#include <QStatusBar>

#include <kactioncollection.h>
#include <kstandardaction.h>
#include <krecentfilesaction.h>

#include <kurlrequesterdialog.h>
#include <QFileDialog>
#include <kmessagebox.h>
#include <krun.h>
#include <KLocalizedString>
#include <QGuiApplication>
#include <ktoolinvocation.h>
#include <ktoggleaction.h>
#include <ktoolbar.h>
#include <kxmlguifactory.h>

#include <QClipboard>
#include <QSplitter>
#include <QMenu>
#include <QList>
#include <QIcon>
#include <KHelpClient>
#include <KIconEngine>
#include <KIconLoader>
#include "kdirtree.h"
#include "ktreemapview.h"
#include "ktreemaptile.h"
#include "kcleanupcollection.h"
#include "kactivitytracker.h"
#include "kdirtreeview.h"
#include "kdirstatsettings.h"
#include "kdirtreecache.h"
#include "kexcluderules.h"

#include "settings.h"

#define	USER_CLEANUPS	10	// Number of user cleanup actions

#define ID_STATUS_MSG	1
#define ID_PACMAN	42
#define PACMAN_WIDTH	350
#define PACMAN_INTERVAL	75	// millisec

using namespace KDirStat;

static QIcon icon(const QString & iconName) {
    return QIcon(new KIconEngine(iconName, KIconLoader::global()));
}

k4dirstat * k4dirstat::instance_ = NULL;

k4dirstat::k4dirstat()
    : KXmlGuiWindow(),
     // m_view(new k4dirstatView(this)),
      m_printer(0)
{
    Q_ASSERT(instance_ == NULL);
    instance_ = this;
    // accept dnd
    setAcceptDrops(true);

    // Simple inits

    _activityTracker	= 0;	// Might or might not be needed


    // Those will be created delayed, only when needed

    _settingsDialog 	= 0;
    _feedbackDialog 	= 0;
    _treemapView	= 0;
    _pacMan		= 0;
    _pacManDelimiter	= 0;
    _treemapViewHeight	= 0;

    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(triggerSaveConfig()));

    // Set up internal (mainWin -> mainWin) connections

    connect( this,	SIGNAL( readConfig       ( void ) ),
             this,	SLOT  ( readMainWinConfig( void ) ) );

    connect( this,	SIGNAL( saveConfig       ( void ) ),
             this,	SLOT  ( saveMainWinConfig( void ) ) );


    // Create main window

    _splitter = new QSplitter( Qt::Vertical, this );
    setCentralWidget( _splitter );

    _treeView = new KDirTreeView( _splitter );

    connect( _treeView, SIGNAL( progressInfo( const QString & ) ),
             this,      SLOT  ( statusMsg   ( const QString & ) ) );

    connect( _treeView, SIGNAL( treeSelectionChanged( KFileInfo * ) ),
             this,      SLOT  ( selectionChanged( KFileInfo * ) ) );

    connect( _treeView, SIGNAL( contextMenu( KDirTreeViewItem *, const QPoint & ) ),
             this,      SLOT  ( contextMenu( KDirTreeViewItem *, const QPoint & ) ) );

    connect( this,	SIGNAL( readConfig() 		), _treeView,	SLOT  ( readConfig() ) );
    connect( this,	SIGNAL( saveConfig() 		), _treeView,	SLOT  ( saveConfig() ) );

    connect( _treeView, SIGNAL( finished()		), this, SLOT( createTreemapViewDelayed() ) );
    connect( _treeView, SIGNAL( aborted()		), this, SLOT( createTreemapViewDelayed() ) );
    connect( _treeView, SIGNAL( startingReading()	), this, SLOT( deleteTreemapView() ) );

    connect( _treeView, SIGNAL( startingReading()	), this, SLOT( updateActions() ) );
    connect( _treeView, SIGNAL( finished()        	), this, SLOT( updateActions() ) );
    connect( _treeView, SIGNAL( aborted()         	), this, SLOT( updateActions() ) );

    // tell the KXmlGuiWindow that this is indeed the main widget
    //setCentralWidget(m_view);

    // set up the elements of the status bar
    initStatusBar();

    // then, setup our actions
    setupActions();

    // setup the cleanup actions
    initCleanups();

    // add a status bar
    statusBar()->show();

    // a call to KXmlGuiWindow::setupGUI() populates the GUI
    // with actions, using KXMLGUI.
    // It also applies the saved mainwindow settings, if any, and ask the
    // mainwindow to automatically save settings if changed: window size,
    // toolbar position, icon size, etc.
    setupGUI();
    \
    _treeViewContextMenu = static_cast<QMenu*> (factory()->container( "treeViewContextMenu", this ));
    _treemapContextMenu	 = static_cast<QMenu*> (factory()->container( "treemapContextMenu",  this ));

    readMainWinConfig();

    // Disable certain actions at startup

    _editCopy->setEnabled( false );
    _reportMailToOwner->setEnabled( false );
    _fileRefreshAll->setEnabled( false );
    _fileRefreshSelected->setEnabled( false );
    updateActions();
}

k4dirstat::~k4dirstat()
{
}

void k4dirstat::setupActions()
{

    _fileAskOpenDir	= KStandardAction::open		( this, SLOT( fileAskOpenDir() ), 		actionCollection() );

    _fileAskOpenUrl	= actionCollection()->addAction("file_open_url",this,SLOT(fileAskOpenUrl()));
    _fileAskOpenUrl->setText( i18n( "Open &URL..." ));
    _fileAskOpenUrl->setIcon(icon("konqueror"));


    _fileOpenRecent	= KStandardAction::openRecent	( this, SLOT( fileOpenRecent( const QUrl& ) ),	actionCollection() );
    _fileCloseDir	= KStandardAction::close		( this, SLOT( fileCloseDir() ), 		actionCollection() );

    _fileRefreshAll = actionCollection()->addAction("file_refresh_all", this, SLOT(refreshAll()));
    _fileRefreshAll->setText(i18n ("Refresh &All"));
    _fileRefreshAll->setIcon(icon("document-revert"));


    _fileRefreshSelected = actionCollection()->addAction("file_refresh_selected", this, SLOT(refreshSelected()));
    _fileRefreshSelected->setText( i18n( "Refresh &Selected" ));

    _fileReadExcludedDir = actionCollection()->addAction("file_read_excluded_dir", this, SLOT(refreshSelected()));
    _fileReadExcludedDir->setText( i18n( "Read &Excluded Directory" ));

    _fileContinueReadingAtMountPoint = actionCollection()->addAction( "file_continue_reading_at_mount_point" ,
                                                                      this, SLOT(refreshSelected()));
    _fileContinueReadingAtMountPoint->setText( i18n( "Continue Reading at &Mount Point" ));
    _fileContinueReadingAtMountPoint->setIcon(icon("drive-harddisk"));

    _fileStopReading = actionCollection()->addAction("file_stop_reading", this, SLOT(stopReading()));
    _fileStopReading->setText(i18n( "Stop Rea&ding" ));
    _fileStopReading->setIcon(icon("process-stop"));

    _fileAskWriteCache = actionCollection()->addAction("file_ask_write_cache",
                                                       this, SLOT(askWriteCache()));
    _fileAskWriteCache->setText(i18n( "&Write to Cache File..." ));
    _fileAskWriteCache->setIcon(icon("document-export"));

    _fileAskReadCache = actionCollection()->addAction("file_ask_read_cache", this, SLOT(askReadCache()));
    _fileAskReadCache->setText(i18n( "&Read Cache File..." ));
    _fileAskReadCache->setIcon(icon("document-import"));

    _fileQuit = KStandardAction::quit(QCoreApplication::instance(), SLOT(quit()), actionCollection());
    _editCopy		= KStandardAction::copy		( this, SLOT( editCopy() 	), actionCollection() );

    _cleanupOpenWith = actionCollection()->addAction( "cleanup_open_with", this, SLOT(cleanupOpenWith()));
    _cleanupOpenWith->setText(i18n( "Open With" ));

    _treemapZoomIn = actionCollection()->addAction("treemap_zoom_in", this, SLOT( treemapZoomIn()));
    _treemapZoomIn->setText( i18n( "Zoom in" ));
    _treemapZoomIn->setIcon(icon("zoom-in"));
    actionCollection()->setDefaultShortcut(_treemapZoomIn, Qt::Key_Plus);

    _treemapZoomOut = actionCollection()->addAction("treemap_zoom_out", this, SLOT(treemapZoomOut()));
    _treemapZoomOut->setText(i18n( "Zoom out" ));
    _treemapZoomOut->setIcon(icon("zoom-out"));
    actionCollection()->setDefaultShortcut(_treemapZoomOut, Qt::Key_Minus);

    _treemapSelectParent = actionCollection()->addAction("treemap_select_parent", this, SLOT(treemapSelectParent()));
    _treemapSelectParent->setText(i18n("Select Parent"));
    _treemapSelectParent->setIcon(icon("go-up"));
    actionCollection()->setDefaultShortcut(_treemapSelectParent, Qt::Key_Asterisk);

    _treemapRebuild = actionCollection()->addAction("treemap_rebuild", this, SLOT(treemapRebuild()));
    _treemapRebuild->setText(i18n("Rebuild Treemap"));

    _showTreemapView = new KToggleAction(i18n("Show Treemap"),actionCollection());
    actionCollection()->addAction("options_show_treemap", _showTreemapView);
    //_showTreemapView->set
    connect(_showTreemapView,SIGNAL(triggered()),this, SLOT(toggleTreemapView()));
    actionCollection()->setDefaultShortcut(_showTreemapView, Qt::Key_F9);

    QAction *newAct = actionCollection()->addAction("treemap_help", this, SLOT(treemapHelp()));
    newAct->setText(i18n( "Help about Treemaps" ));
    newAct->setIcon(icon("help-contents"));

    QAction * pref  = KStandardAction::preferences( this, SLOT( preferences()	), actionCollection() );

    _reportMailToOwner = actionCollection()->addAction("report_mail_to_owner", _treeView, SLOT(sendMailToOwner()));
    _reportMailToOwner->setText(i18n("Send &Mail to Owner"));
    _reportMailToOwner->setIcon(icon("mail-message-new"));

   // _helpSendFeedbackMail = actionCollection()->addAction("help_send_feedback_mail",this, SLOT(sendFeedbackMail()));
   // _helpSendFeedbackMail->setText(i18n("Send &Feedback Mail"));

    _fileAskOpenDir->setStatusTip	( i18n( "Opens a directory"	 		) );
    _fileAskOpenUrl->setStatusTip	( i18n( "Opens a (possibly remote) directory"	) );
    _fileOpenRecent->setStatusTip	( i18n( "Opens a recently used directory"	) );
    _fileCloseDir->setStatusTip	( i18n( "Closes the current directory" 		) );
    _fileRefreshAll->setStatusTip	( i18n( "Re-reads the entire directory tree"	) );
    _fileRefreshSelected->setStatusTip	( i18n( "Re-reads the selected subtree"		) );
    _fileReadExcludedDir->setStatusTip ( i18n( "Scan directory tree that was previously excluded" ) );
    _fileContinueReadingAtMountPoint->setStatusTip( i18n( "Scan mounted file systems"	) );
    _fileStopReading->setStatusTip	( i18n( "Stops directory reading"		) );
    _fileAskWriteCache->setStatusTip	( i18n( "Writes the current directory tree to a cache file that can be loaded much faster" ) );
    _fileAskReadCache->setStatusTip	( i18n( "Reads a directory tree from a cache file" ) );
    _fileQuit->setStatusTip		( i18n( "Quits the application" 		) );
    _editCopy->setStatusTip		( i18n( "Copies the URL of the selected item to the clipboard" ) );
    _cleanupOpenWith->setStatusTip	( i18n( "Open file or directory with arbitrary application" ) );
    _showTreemapView->setStatusTip	( i18n( "Enables/disables the treemap view" 	) );
    _treemapZoomIn->setStatusTip	( i18n( "Zoom treemap in"		 	) );
    _treemapZoomOut->setStatusTip	( i18n( "Zoom treemap out"		 	) );
    _treemapSelectParent->setStatusTip	( i18n( "Select parent"			 	) );
    _treemapRebuild->setStatusTip	( i18n( "Rebuild treemap to fit into available space" ) );
    pref->setStatusTip			( i18n( "Opens the preferences dialog"		) );
    _reportMailToOwner->setStatusTip	( i18n( "Sends a mail to the owner of the selected subtree" ) );

    // custom menu and menu item - the slot is in the class k4dirstatView
    //KAction *custom = new KAction(KIcon("colorize"), i18n("Swi&tch Colors"), this);
    //actionCollection()->addAction( QLatin1String("switch_action"), custom );
    //connect(custom, SIGNAL(triggered(bool)), m_view, SLOT(switchColors()));
}

void k4dirstat::initCleanups()
{
    _cleanupCollection = new KCleanupCollection( *actionCollection() );
    Q_CHECK_PTR( _cleanupCollection );
    _cleanupCollection->addStdCleanups();
    _cleanupCollection->addUserCleanups( USER_CLEANUPS );
    _cleanupCollection->readConfig();

    connect( _treeView,          SIGNAL( treeSelectionChanged( KFileInfo * ) ),
             _cleanupCollection, SIGNAL( selectionChanged( KFileInfo * ) ) );

    connect( this,               SIGNAL( readConfig( void ) ),
             _cleanupCollection, SLOT( readConfig( void ) ) );

    connect( this,               SIGNAL( saveConfig( void ) ),
             _cleanupCollection, SLOT( saveConfig( void ) ) );
}

void k4dirstat::revertCleanupsToDefaults()
{
    _cleanupCollection->revertToDefault(USER_CLEANUPS);
}

void k4dirstat::initStatusBar()
{
    statusBar()->showMessage(i18n( "Ready." ));
}

void k4dirstat::openURL( const QUrl& url )
{
    statusMsg( i18n( "Opening directory..." ) );

    _treeView->openURL( url );
    _fileOpenRecent->addUrl( url );
    _fileRefreshAll->setEnabled( true );
    setCaption( url.fileName(), false );

    statusMsg( i18n( "Ready." ) );
}

void k4dirstat::readMainWinConfig()
{
    KConfigGroup config = KSharedConfig::openConfig()->group("General Options");

    // Status settings of the various bars and views

    _showTreemapView->setChecked( config.readEntry( "Show Treemap", true ) );
    toggleTreemapView();


    // Position settings of the various bars

   // KToolBar::BarPosition toolBarPos;
    //toolBarPos = ( KToolBar::BarPosition ) config->readEntry( "ToolBarPos", KToolBar::Top );
    //toolBar( "mainToolBar" )->setBarPos( toolBarPos );

    _treemapViewHeight = config.readEntry( "TreemapViewHeight", 250 );

    // initialize the recent file list
    _fileOpenRecent->loadEntries( config);

    QSize size = config.readEntry( "Geometry", QSize() );

    if( ! size.isEmpty() )
        resize( size );

    config = KSharedConfig::openConfig()->group("Animation");
    _treeView->enablePacManAnimation( config.readEntry( "DirTreePacMan", false ) );

    config = KSharedConfig::openConfig()->group("Exclude");
    QStringList excludeRules = config.readEntry ( "ExcludeRules", QStringList() );
    KExcludeRules::excludeRules()->clear();

    for ( QStringList::Iterator it = excludeRules.begin(); it != excludeRules.end(); ++it )
    {
        QString ruleText = *it;
        KExcludeRules::excludeRules()->add( new KExcludeRule( QRegExp( ruleText ) ) );
        qDebug() << "Adding exclude rule: " << ruleText << endl;
    }

    if ( excludeRules.size() == 0 )
        qDebug() << "No exclude rules defined" << endl;
}


void k4dirstat::saveMainWinConfig()
{
    KConfigGroup config = KSharedConfig::openConfig()->group("General Options");


    config.writeEntry( "Geometry", 		size() );
    config.writeEntry( "Show Treemap",		_showTreemapView->isChecked() );
    // Config entries for 2.0? seriously?
    //config.writeEntry( "ToolBarPos", 		(int) toolBar( "mainToolBar" )->pos() );

    if ( _treemapView )
        config.writeEntry( "TreemapViewHeight", _treemapView->height() );

    _fileOpenRecent->saveEntries( config );
}


//============================================================================
//				     Slots
//============================================================================


void k4dirstat::fileAskOpenDir()
{
    statusMsg( i18n( "Opening directory..." ) );
    QUrl url = QFileDialog::getExistingDirectoryUrl(this, i18n( "Open Directory..." ));

    if( ! url.isEmpty() )
        openURL(url);

    statusMsg( i18n( "Ready." ) );
}

void k4dirstat::fileAskOpenUrl()
{
    statusMsg( i18n( "Opening URL..." ) );

    QUrl url = KUrlRequesterDialog::getUrl( QUrl(),	// startDir
                                         this, i18n( "Open URL..." ) );

    if( ! url.isEmpty() )
        openURL( fixedUrl( url.url() ) );

    statusMsg( i18n( "Ready." ) );
}

void k4dirstat::fileOpenRecent( const QUrl& url )
{
    statusMsg( i18n( "Opening directory..." ) );

    if( ! url.isEmpty() )
        openURL( fixedUrl( url.url() ) );

    statusMsg( i18n( "Ready." ) );
}

void k4dirstat::fileCloseDir()
{
    statusMsg( i18n( "Closing directory..." ) );

    _treeView->clear();
    _fileRefreshAll->setEnabled( false );
    close();

    statusMsg( i18n( "Ready." ) );
}


void k4dirstat::refreshAll()
{
    statusMsg( i18n( "Refreshing directory tree..." ) );
    _treeView->refreshAll();
    statusMsg( i18n( "Ready." ) );
}


void k4dirstat::refreshSelected()
{
    if ( ! _treeView->selection() )
        return;

    statusMsg( i18n( "Refreshing selected subtree..." ) );
    _treeView->refreshSelected();
    statusMsg( i18n( "Ready." ) );
}

void k4dirstat::stopReading()
{
    _treeView->abortReading();
}


void k4dirstat::askWriteCache()
{
    QString file_name;

    do
    {
        file_name =
            QFileDialog::getSaveFileName(this, i18n("Write to Cache File"), DEFAULT_CACHE_NAME);

        if ( file_name.isEmpty() )		// user hit "cancel"
            return;

        if ( access( file_name.toLatin1(), F_OK ) == 0 )	// file exists
        {
            int button =
                KMessageBox::questionYesNoCancel( this,
                                                  i18n( "File %1 exists. Overwrite?" ).arg( file_name ),
                                                  i18n( "Overwrite?" ) );	// caption

            if ( button == KMessageBox::Cancel 	)	return;
            if ( button == KMessageBox::No	)	file_name = "";
        }
    } while ( file_name.isEmpty() );

    statusMsg( i18n( "Writing cache file..." ) );

    if ( ! _treeView || ! _treeView->writeCache( file_name ) )
    {
        QString errMsg = i18n( "Error writing cache file %1" ).arg( file_name );
        statusMsg( errMsg );
        KMessageBox::sorry( this, errMsg,
                            i18n( "Write Error" ) );			// caption
    }

    statusMsg( i18n( "Wrote cache file %1" ).arg( file_name ) );
}


void k4dirstat::askReadCache()
{
    QString file_name =
        QFileDialog::getOpenFileName(this, i18n("Read Cache File"), DEFAULT_CACHE_NAME);

    statusMsg( i18n( "Reading cache file..." ) );

    if ( _treeView )
    {
        _fileRefreshAll->setEnabled( true );
        _treeView->readCache( file_name );
    }
}



void k4dirstat::editCopy()
{
    if(_treeView->selection()) {
        QGuiApplication * app = dynamic_cast<QGuiApplication*>(QCoreApplication::instance());
        if(app)
            app->clipboard()->setText(_treeView->selection()->orig()->url());
    }

#if 0
#warning debug
    if ( _activityTracker )
        _activityTracker->trackActivity( 800 );
#endif
}

void k4dirstat::cleanupOpenWith()
{
    if ( ! _treeView->selection() )
        return;

    KFileInfo * sel = _treeView->selection()->orig();

    if ( sel->isDotEntry() )
        return;

    QList<QUrl> urlList;
    urlList.append(QUrl::fromUserInput(sel->url(), QDir::currentPath(),
        QUrl::AssumeLocalFile));
    KRun::displayOpenWithDialog( urlList, this, false );
}

void k4dirstat::selectionChanged( KFileInfo *selection )
{
    if ( selection )
    {
        _editCopy->setEnabled( true );
        _reportMailToOwner->setEnabled( true );
        _fileRefreshSelected->setEnabled( ! selection->isDotEntry() );
        _cleanupOpenWith->setEnabled( ! selection->isDotEntry() );
        _fileReadExcludedDir->setEnabled( selection->isExcluded() );

        if ( selection->isMountPoint() &&
             selection->readState() == KDirOnRequestOnly )
        {
            _fileContinueReadingAtMountPoint->setEnabled( true );
        }
        else
            _fileContinueReadingAtMountPoint->setEnabled( false );

        statusMsg( QString::fromLocal8Bit(selection->url().toAscii()) );
    }
    else
    {
        _editCopy->setEnabled( false );
        _reportMailToOwner->setEnabled( false );
        _fileRefreshSelected->setEnabled( false );
        _fileContinueReadingAtMountPoint->setEnabled( false );
        _cleanupOpenWith->setEnabled( false );
        statusMsg( "" );
    }

    updateActions();
}


void k4dirstat::updateActions()
{
    _treemapZoomIn->setEnabled ( _treemapView && _treemapView->canZoomIn() );
    _treemapZoomOut->setEnabled( _treemapView && _treemapView->canZoomOut() );
    _treemapRebuild->setEnabled( _treemapView && _treemapView->rootTile() );
    _treemapSelectParent->setEnabled( _treemapView && _treemapView->canSelectParent() );

    if ( _treeView->tree() && _treeView->tree()->isBusy() )
        _fileStopReading->setEnabled( true );
    else
        _fileStopReading->setEnabled( false );
}


void k4dirstat::treemapZoomIn()
{
    if ( _treemapView )
    {
        _treemapView->zoomIn();
        updateActions();
    }
}


void k4dirstat::treemapZoomOut()
{
    if ( _treemapView )
    {
        _treemapView->zoomOut();
        updateActions();
    }
}

void k4dirstat::treemapSelectParent()
{
    if ( _treemapView )
    {
        _treemapView->selectParent();
        updateActions();
    }
}


void k4dirstat::treemapRebuild()
{
    if ( _treemapView )
    {
        _treemapView->rebuildTreemap();
        updateActions();
    }
}


void k4dirstat::treemapHelp()
{
    KHelpClient::invokeHelp("treemap_intro");
}


void k4dirstat::toggleTreemapView()
{
    if   ( _showTreemapView->isChecked() )
    {
        if ( ! _treemapView )
            createTreemapView();
    }
    else
    {
        if ( _treemapView )
            deleteTreemapView();
    }
}

void k4dirstat::preferences()
{
    if(_settingsDialog && _settingsDialog->isVisible())
        return;
    // TODO no longer store the instance as it's always recreated.
    // need to detect closing
    _settingsDialog = new KDirStat::KSettingsDialog( this );
    _settingsDialog->show();
}



void k4dirstat::statusMsg( const QString &text )
{
    // Change status message permanently
    statusBar()->showMessage(text);
}

void k4dirstat::contextMenu( KDirTreeViewItem * item, const QPoint &pos )
{
    NOT_USED( item );

    if ( _treeViewContextMenu )
        _treeViewContextMenu->popup( pos );
}


void k4dirstat::contextMenu( KTreemapTile * tile, const QPoint &pos )
{
    NOT_USED( tile );

    if ( _treemapContextMenu )
        _treemapContextMenu->popup( pos );
}


void k4dirstat::createTreemapViewDelayed()
{
    QTimer::singleShot( 0, this, SLOT( createTreemapView() ) );
}


void k4dirstat::createTreemapView()
{
    if ( ! _showTreemapView->isChecked() || ! _treeView->tree() )
        return;

    if ( _treemapView )
        delete _treemapView;

    // qDebug() << "Creating KTreemapView" << endl;
    _treemapView = new KTreemapView( _treeView->tree(), _splitter,
                                     QSize( _splitter->width(), _treemapViewHeight ) );
    Q_CHECK_PTR( _treemapView );

    connect( _treemapView,	SIGNAL( contextMenu( KTreemapTile *, const QPoint & ) ),
             this,		SLOT  ( contextMenu( KTreemapTile *, const QPoint & ) ) );

    connect( _treemapView,	SIGNAL( treemapChanged()	),
             this,		SLOT  ( updateActions()	) 	);

    connect( _treemapView,	SIGNAL( selectionChanged( KFileInfo * ) ),
             this,      	SLOT  ( selectionChanged( KFileInfo * ) ) );

    if ( _activityTracker )
    {
        connect( _treemapView,		SIGNAL( userActivity ( int )	),
                 _activityTracker,	SLOT  ( trackActivity( int ) 	) );
    }

    _treemapView->show(); // QSplitter needs explicit show() for new children
    updateActions();
}


void k4dirstat::deleteTreemapView()
{
    if ( _treemapView )
    {
        // qDebug() << "Deleting KTreemapView" << endl;
        _treemapViewHeight = _treemapView->height();

        delete _treemapView;
        _treemapView = 0;
    }

    updateActions();
}

void k4dirstat::triggerSaveConfig() {
    emit saveConfig();
}

/*void k4dirstat::fileNew()
{
    // this slot is called whenever the File->New menu is selected,
    // the New shortcut is pressed (usually CTRL+N) or the New toolbar
    // button is clicked

    // create a new window
    (new k4dirstat)->show();
}

void k4dirstat::optionsPreferences()
{
    // The preference dialog is derived from prefs_base.ui
    //
    // compare the names of the widgets in the .ui file
    // to the names of the variables in the .kcfg file
    //avoid to have 2 dialogs shown
    if ( KConfigDialog::showDialog( "settings" ) )  {
        return;
    }
    KConfigDialog *dialog = new KConfigDialog(this, "settings", Settings::self());
    QWidget *generalSettingsDlg = new QWidget;
    ui_prefs_base.setupUi(generalSettingsDlg);
    dialog->addPage(generalSettingsDlg, i18n("General"), "package_setting");
    connect(dialog, SIGNAL(settingsChanged(QString)), m_view, SLOT(settingsChanged()));
    dialog->setAttribute( Qt::WA_DeleteOnClose );
    dialog->show();
}*/

