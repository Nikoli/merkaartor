#include "MainWindow.h"

#include "LayerDock.h"
#include "MapView.h"
#include "PropertiesDock.h"
#include "InfoDock.h"
#include "DirtyDock.h"
#include "Command/Command.h"
#include "Command/DocumentCommands.h"
#include "Command/FeatureCommands.h"
#include "ImportExport/ImportExportOsmBin.h"
#include "ImportExport/ExportGPX.h"
#include "ImportExport/ImportExportKML.h"
#include "Interaction/CreateAreaInteraction.h"
#include "Interaction/CreateDoubleWayInteraction.h"
#include "Interaction/CreateNodeInteraction.h"
#include "Interaction/CreateRoundaboutInteraction.h"
#include "Interaction/CreateSingleWayInteraction.h"
#include "Interaction/EditInteraction.h"
#include "Interaction/MoveTrackPointInteraction.h"
#include "Interaction/ZoomInteraction.h"
#include "Map/Coord.h"
#include "Map/DownloadOSM.h"
#include "Map/ImportGPX.h"
#include "Map/ImportNGT.h"
#include "Map/ImportOSM.h"
#include "Map/MapDocument.h"
#include "Map/MapLayer.h"
#include "Map/MapFeature.h"
#include "Map/Relation.h"
#include "Map/Road.h"
#include "Map/FeatureManipulations.h"
#include "Map/TrackPoint.h"
#include "PaintStyle/EditPaintStyle.h"
#include "PaintStyle/PaintStyleEditor.h"
#include "Sync/SyncOSM.h"
#include <ui_AboutDialog.h>
#include <ui_UploadMapDialog.h>
#include <ui_SelectionDialog.h>
#include <ui_ExportDialog.h>
#include "Preferences/PreferencesDialog.h"
#include "Preferences/MerkaartorPreferences.h"
#include "Utils/SelectionDialog.h"
#include "QMapControl/imagemanager.h"
#include "QMapControl/browserimagemanager.h"
#include "QMapControl/mapadapter.h"
#include "QMapControl/wmsmapadapter.h"
#include "Tools/WorldOsbManager.h"
#include "Tools/ActionsDialog.h"
#include "GotoDialog.h"

#ifdef GEOIMAGE
#include "GeoImageDock.h"
#endif

#include "Render/NativeRenderDialog.h"
#ifdef OSMARENDER
	#include "Render/OsmaRenderDialog.h"
#endif

#include "qgps.h"
#include "qgpsdevice.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QtGlobal>
#include <QtCore/QTimer>
#include <QtGui/QDialog>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QMouseEvent>
#include <QInputDialog>
#include <QClipboard>
#include <QProgressDialog>
#include <QMenuBar>
#include <QTranslator>
#include <QLocale>
#include <QMessageBox>

#include "QtStyles/skulpture/skulpture.h"

SlippyMapCache* SlippyMapWidget::theSlippyCache = 0;

class MainWindowPrivate
{
	public:
		MainWindowPrivate()
			: lastPrefTabIndex(0)
		{
		}
		int lastPrefTabIndex;
		QString defStyle;
};

MainWindow::MainWindow(void)
		: fileName(""), theDocument(0),
		gpsRecLayer(0),curGpsTrackSegment(0),
		qtTranslator(0), merkaartorTranslator(0)
{
	p = new MainWindowPrivate;

	p->defStyle = QApplication::style()->objectName();
	if (M_PREFS->getMerkaartorStyle())
		QApplication::setStyle(new SkulptureStyle);

	setupUi(this);
	M_STYLE->loadPainters(MerkaartorPreferences::instance()->getDefaultStyle());

	blockSignals(true);

    updateLanguage();

	SlippyMapWidget::theSlippyCache = new SlippyMapCache;

	ViewportStatusLabel = new QLabel(this);
	ViewportStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	pbImages = new QProgressBar(this);
	pbImages->setMaximumWidth(200);
	PaintTimeLabel = new QLabel(this);
	pbImages->setFormat(tr("tile %v / %m"));
	//PaintTimeLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	PaintTimeLabel->setMinimumWidth(23);
	statusBar()->addPermanentWidget(ViewportStatusLabel);
	statusBar()->addPermanentWidget(pbImages);
	statusBar()->addPermanentWidget(PaintTimeLabel);
#ifdef _MOBILE
	ViewportStatusLabel->setVisible(false);
	pbImages->setVisible(false);
#endif

	theView = new MapView(this);
	setCentralWidget(theView);

	theLayers = new LayerDock(this);

	theDocument = new MapDocument(theLayers);
	theView->setDocument(theDocument);
	theDocument->history().setActions(editUndoAction, editRedoAction, fileUploadAction);


	theProperties = new PropertiesDock(this);
	on_editPropertiesAction_triggered();

	theInfo = new InfoDock(this);
	theDirty = new DirtyDock(this);
	theGPS = new QGPS(this);

#ifdef GEOIMAGE
	theGeoImage = new GeoImageDock(this);
	theGeoImage->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, theGeoImage);
#endif

	connect (theDocument, SIGNAL(historyChanged()), theDirty, SLOT(updateList()));
	connect (theLayers, SIGNAL(layersChanged(bool)), this, SLOT(adjustLayers(bool)));

	connect (MerkaartorPreferences::instance(), SIGNAL(bookmarkChanged()), this, SLOT(updateBookmarksMenu()));
	updateBookmarksMenu();
	connect (menuBookmarks, SIGNAL(triggered(QAction *)), this, SLOT(bookmarkTriggered(QAction *)));

	updateRecentOpenMenu();
	connect (menuRecentOpen, SIGNAL(triggered(QAction *)), this, SLOT(recentOpenTriggered(QAction *)));

	updateRecentImportMenu();
	connect (menuRecentImport, SIGNAL(triggered(QAction *)), this, SLOT(recentImportTriggered(QAction *)));

	updateProjectionMenu();

	viewDownloadedAction->setChecked(MerkaartorPreferences::instance()->getDownloadedVisible());
	viewScaleAction->setChecked(M_PREFS->getScaleVisible());
	viewStyleBackgroundAction->setChecked(M_PREFS->getStyleBackgroundVisible());
	viewStyleForegroundAction->setChecked(M_PREFS->getStyleForegroundVisible());
	viewStyleTouchupAction->setChecked(M_PREFS->getStyleTouchupVisible());
	viewNamesAction->setChecked(M_PREFS->getNamesVisible());
	viewTrackPointsAction->setChecked(M_PREFS->getTrackPointsVisible());
	viewTrackSegmentsAction->setChecked(M_PREFS->getTrackSegmentsVisible());
	viewRelationsAction->setChecked(M_PREFS->getRelationsVisible());

	updateMenu();

	QActionGroup* actgrpArrows = new QActionGroup(this);
	actgrpArrows->addAction(viewArrowsNeverAction);
	actgrpArrows->addAction(viewArrowsOnewayAction);
	actgrpArrows->addAction(viewArrowsAlwaysAction);
	switch (M_PREFS->getDirectionalArrowsVisible()) {
		case DirectionalArrows_Never:
			viewArrowsNeverAction->setChecked(true);
			break;
		case DirectionalArrows_Oneway:
			viewArrowsOnewayAction->setChecked(true);
			break;
		case DirectionalArrows_Always:
			viewArrowsAlwaysAction->setChecked(true);
			break;
	}

	gpsCenterAction->setChecked(M_PREFS->getGpsMapCenter());

	connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardChanged()));

	setWindowTitle(QString("Merkaartor - untitled"));

#ifndef _MOBILE
	theLayers->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::LeftDockWidgetArea, theLayers);

	theProperties->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::LeftDockWidgetArea, theProperties);

	theInfo->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, theInfo);

	theDirty->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, theDirty);

	theGPS->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, theGPS);

	MerkaartorPreferences::instance()->restoreMainWindowState( this );
#else
	theProperties->setVisible(false);
	theInfo->setVisible(false);
	theLayers->setVisible(false);
	theDirty->setVisible(false);
	toolBar->setVisible(false);
	theGPS->setVisible(false);

#ifdef _WINCE
	windowPropertiesAction->setText(tr("Properties..."));
	menuBar()->setDefaultAction(windowPropertiesAction);
#endif // _WINCE
#endif // _MOBILE

#ifndef OSMARENDER
	renderSVGAction->setVisible(false);
#endif

#ifndef GEOIMAGE
	windowGeoimageAction->setVisible(false);
#endif

//#ifdef NDEBUG
	viewStyleBackgroundAction->setVisible(false);
	viewStyleForegroundAction->setVisible(false);
	viewStyleTouchupAction->setVisible(false);
//#endif
	
	MerkaartorPreferences::instance()->initialPosition(theView);

	QList<QAction*> actions = findChildren<QAction*>();
	for (int i=0; i<actions.size(); i++) {
		shortcutsDefault[actions[i]->objectName()] = actions[i]->shortcut().toString();
	}
	QStringList shortcuts = M_PREFS->getShortcuts();
	for (int i=0; i<shortcuts.size(); i+=2) {
		QAction* act = findChild<QAction*>(shortcuts[i]);
		act->setShortcut(QKeySequence(shortcuts[i+1]));
	}

#define NUMOP 3
	static const char *opStr[NUMOP] = {
		QT_TR_NOOP("Low"), QT_TR_NOOP("High"), QT_TR_NOOP("Opaque")};

	int o = M_PREFS->getAreaOpacity();
	QActionGroup* actgrp = new QActionGroup(this);
	for (int i=0; i<NUMOP; i++) {
		QAction* act = new QAction(tr(opStr[i]), mnuAreaOpacity);
		actgrp->addAction(act);
		qreal a = M_PREFS->getAlpha(opStr[i]);
		act->setData(a);
		act->setCheckable(true);
		mnuAreaOpacity->addAction(act);
		if (o == int(a*100))
			act->setChecked(true);
	}
	connect(mnuAreaOpacity, SIGNAL(triggered(QAction*)), this, SLOT(setAreaOpacity(QAction*)));

	blockSignals(false);
}

MainWindow::~MainWindow(void)
{
    theProperties->setSelection(NULL);

	if (EditPaintStyle::instance())
		delete EditPaintStyle::instance();
	MerkaartorPreferences::instance()->setWorkingDir(QDir::currentPath());
	delete MerkaartorPreferences::instance();
	delete theDocument;
	delete theView;
	delete theProperties;

    delete qtTranslator;
    delete merkaartorTranslator;

	delete SlippyMapWidget::theSlippyCache;

	delete p;
}

void MainWindow::setAreaOpacity(QAction *act)
{
	qreal a = act->data().toDouble();
	M_PREFS->setAreaOpacity(int(a*100));

	theView->invalidate(true, false);
}

void MainWindow::adjustLayers(bool adjustViewport)
{
	if (adjustViewport) {
		if (MerkaartorPreferences::instance()->getProjectionType() == Proj_Background)
			theView->projection().setViewport(theView->projection().viewport(), theView->rect());
		else
			theView->projection().zoom(1, QPoint(theView->width() / 2, theView->height() / 2), theView->rect());
	}
	invalidateView(true);
}

void MainWindow::invalidateView(bool UpdateDock)
{
	theView->invalidate(true, true);
	//theLayers->updateContent();
	if (UpdateDock)
		theProperties->resetValues();
}

PropertiesDock* MainWindow::properties()
{
	return theProperties;
}

InfoDock* MainWindow::info()
{
	return theInfo;
}

#ifdef GEOIMAGE
GeoImageDock* MainWindow::geoImage()
{
	return theGeoImage;
}
#endif

MapDocument* MainWindow::document()
{
	return theDocument;
}

MapDocument* MainWindow::getDocumentFromClipboard()
{
	QClipboard *clipboard = QApplication::clipboard();
	QDomDocument* theXmlDoc = new QDomDocument();
	if (!theXmlDoc->setContent(clipboard->text())) {
		QMessageBox::critical(this, tr("Clipboard invalid"), tr("Clipboard is not valid XML."));
		return NULL;
	}

	QDomElement c = theXmlDoc->documentElement();

	if (c.tagName() != "osm") {
		QMessageBox::critical(this, tr("Clipboard invalid"), tr("Clipboard do not contain valid OSM."));
		return NULL;
	}

	MapDocument* NewDoc = new MapDocument(NULL);
	MapLayer* l = NewDoc->getDirtyLayer();

	c = c.firstChildElement();
	while(!c.isNull()) {
		if (c.tagName() == "bound") {
		} else
		if (c.tagName() == "way") {
			Road::fromXML(NewDoc, l, c);
		} else
		if (c.tagName() == "relation") {
			Relation::fromXML(NewDoc, l, c);
		} else
		if (c.tagName() == "node") {
			TrackPoint::fromXML(NewDoc, l, c);
		}

		c = c.nextSiblingElement();
	}

	return NewDoc;
}

void MainWindow::on_editCopyAction_triggered()
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(theDocument->exportOSM(theProperties->selection()));
	invalidateView();
}

void MainWindow::on_editPasteOverwriteAction_triggered()
{
	QVector<MapFeature*> sel = properties()->selection();
	if (!sel.size())
		return;

	MapDocument* doc;
	if (!(doc = getDocumentFromClipboard()))
		return;

	CommandList* theList = new CommandList();
	theList->setDescription("Paste tags (overwrite)");

	for(int i=0; i < sel.size(); ++i) {
		theList->add(new ClearTagsCommand(sel[i], theDocument->getDirtyOrOriginLayer(sel[i]->layer())));
		for (FeatureIterator k(doc); !k.isEnd(); ++k) {
			MapFeature::mergeTags(theDocument, theList, sel[i], k.get());
		}
	}

	if (theList->size())
		document()->addHistory(theList);
	else
		delete theList;

	delete doc;
	invalidateView();
}

void MainWindow::on_editPasteMergeAction_triggered()
{
	QVector<MapFeature*> sel = properties()->selection();
	if (!sel.size())
		return;

	MapDocument* doc;
	if (!(doc = getDocumentFromClipboard()))
		return;

	CommandList* theList = new CommandList();
	theList->setDescription("Paste tags (merge)");

	for(int i=0; i < sel.size(); ++i) {
		for (FeatureIterator k(doc); !k.isEnd(); ++k) {
			MapFeature::mergeTags(theDocument, theList, sel[i], k.get());
		}
	}

	if (theList->size())
		document()->addHistory(theList);
	else
		delete theList;

	delete doc;
	invalidateView();
}

void MainWindow::on_editPasteFeaturesAction_triggered()
{
	invalidateView();
}

void MainWindow::clipboardChanged()
{
	editPasteFeaturesAction->setEnabled(false);
	editPasteMergeAction->setEnabled(false);
	editPasteOverwriteAction->setEnabled(false);

	QClipboard *clipboard = QApplication::clipboard();
	QDomDocument* theXmlDoc = new QDomDocument();
	if (!theXmlDoc->setContent(clipboard->text())) {
		delete theXmlDoc;
		return;
	}

	QDomElement c = theXmlDoc->documentElement();

	if (c.tagName() != "osm") {
		return;
	}

	editPasteFeaturesAction->setEnabled(true);
	editPasteMergeAction->setEnabled(true);
	editPasteOverwriteAction->setEnabled(true);

	delete theXmlDoc;
}

void MainWindow::on_editRedoAction_triggered()
{
	theDocument->redoHistory();
	invalidateView();
}

void MainWindow::on_editUndoAction_triggered()
{
	theDocument->undoHistory();
	invalidateView();
}

void MainWindow::on_editPropertiesAction_triggered()
{
	if (theView->interaction() && dynamic_cast<EditInteraction*>(theView->interaction()))
		theProperties->setSelection(0);
	theView->unlockSelection();
	invalidateView();
	theView->launch(new EditInteraction(theView));
}

void MainWindow::on_editRemoveAction_triggered()
{
	emit remove_triggered();
}

void MainWindow::on_editMoveAction_triggered()
{
	if (M_PREFS->getSeparateMoveMode())
		view()->launch(new MoveTrackPointInteraction(view()));
}


void MainWindow::on_editReverseAction_triggered()
{
	emit reverse_triggered();
}

static void changeCurrentDirToFile(const QString& s)
{
	QFileInfo info(s);
	QDir::setCurrent(info.absolutePath());
	MerkaartorPreferences::instance()->setWorkingDir(QDir::currentPath());
}

#ifndef GEOIMAGE
#define FILTER_OPEN_SUPPORTED \
	tr("Supported formats")+" (*.mdc *.gpx *.osm *.osb *.ngt *.nmea *.nma *.kml)\n" \
	+tr("Merkaartor document (*.mdc)\n") \
	+tr("GPS Exchange format (*.gpx)\n") \
	+tr("OpenStreetMap format (*.osm)\n") \
	+tr("OpenStreetMap binary format (*.osb)\n") \
	+tr("Noni GPSPlot format (*.ngt)\n") \
	+tr("NMEA GPS log format (*.nmea *.nma)\n") \
	+tr("KML file (*.kml)\n") \
	+tr("All Files (*)")
#else
#define FILTER_OPEN_SUPPORTED \
	tr("Supported formats")+" (*.mdc *.gpx *.osm *.osb *.ngt *.nmea *.nma *.kml *.jpg)\n" \
	+tr("Merkaartor document (*.mdc)\n") \
	+tr("GPS Exchange format (*.gpx)\n") \
	+tr("OpenStreetMap format (*.osm)\n") \
	+tr("OpenStreetMap binary format (*.osb)\n") \
	+tr("Noni GPSPlot format (*.ngt)\n") \
	+tr("NMEA GPS log format (*.nmea *.nma)\n") \
	+tr("KML file (*.kml)\n") \
	+tr("Geotagged images (*.jpg)\n") \
	+tr("All Files (*)")
#endif
#define FILTER_IMPORT_SUPPORTED \
	tr("Supported formats")+" (*.gpx *.osm *.osb *.ngt *.nmea *.nma *.kml)\n" \
	+tr("GPS Exchange format (*.gpx)\n") \
	+tr("OpenStreetMap format (*.osm)\n") \
	+tr("OpenStreetMap binary format (*.osb)\n") \
	+tr("Noni GPSPlot format (*.ngt)\n") \
	+tr("NMEA GPS log format (*.nmea *.nma)\n") \
	+tr("KML file (*.kml)\n") \
	+tr("All Files (*)")

void MainWindow::on_fileImportAction_triggered()
{
	QStringList fileNames = QFileDialog::getOpenFileNames(
					this,
					tr("Open track file"),
					"", FILTER_IMPORT_SUPPORTED);

	if (fileNames.isEmpty())
		return;

	view()->setUpdatesEnabled(false);
	theLayers->setUpdatesEnabled(false);

	QStringList importedFiles;
	importFiles(theDocument, fileNames, &importedFiles);

	foreach (QString currentFileName, importedFiles)
		M_PREFS->addRecentImport(currentFileName);

	updateRecentImportMenu();

	view()->setUpdatesEnabled(true);
	theLayers->setUpdatesEnabled(true);

	on_editPropertiesAction_triggered();
	theDocument->history().setActions(editUndoAction, editRedoAction, fileUploadAction);
}

static bool mayDiscardUnsavedChanges(QWidget* aWidget)
{
	return QMessageBox::question(aWidget, MainWindow::tr("Unsaved changes"),
								 MainWindow::tr("The current map contains unsaved changes that will be lost when starting a new one.\n"
												"Do you want to cancel starting a new map or continue and discard the old changes?"),
								 QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard;
}

bool MainWindow::importFiles(MapDocument * mapDocument, const QStringList & fileNames, QStringList * importedFileNames )
{
	QApplication::setOverrideCursor(Qt::BusyCursor);

	bool foundImport = false;

	QStringListIterator it(fileNames);
	while (it.hasNext())
	{
		const QString & fn = it.next();
		changeCurrentDirToFile(fn);

		QString baseFileName = fn.section('/', - 1);
		MapLayer* newLayer = NULL;

		bool importOK = false;
		bool importAborted = false;

		if (fn.endsWith(".gpx")) {
			QVector<TrackMapLayer*> theTracklayers;
			TrackMapLayer* newLayer = new TrackMapLayer( baseFileName + " - " + tr("Waypoints"), baseFileName);
			mapDocument->add(newLayer);
			theTracklayers.append(newLayer);
			importOK = importGPX(this, baseFileName, mapDocument, theTracklayers);
			if (!importOK) {
				for (int i=0; i<theTracklayers.size(); i++) {
					mapDocument->remove(theTracklayers[i]);
					delete theTracklayers[i];
				}
			} else {
				if (!newLayer->size()) {
					mapDocument->remove(newLayer);
					delete newLayer;
				}
				for (int i=1; i<theTracklayers.size(); i++) {
					if (theTracklayers[i]->name().isEmpty())
						theTracklayers[i]->setName(QString(baseFileName + " - " + tr("Track %1").arg(i)));
					if (importOK && MerkaartorPreferences::instance()->getAutoExtractTracks()) {
						theTracklayers[i]->extractLayer();
					}
				}
			}
		}
		else if (fn.endsWith(".osm")) {
			newLayer = new DrawingMapLayer( baseFileName );
			mapDocument->add(newLayer);
			importOK = importOSM(this, baseFileName, mapDocument, newLayer);
		}
		else if (fn.endsWith(".osb")) {
			newLayer = new OsbMapLayer( baseFileName, fn );
			mapDocument->add(newLayer);
			importOK = mapDocument->importOSB(fn, (DrawingMapLayer *)newLayer);
		}
		else if (fn.endsWith(".ngt")) {
			newLayer = new TrackMapLayer( baseFileName );
			mapDocument->add(newLayer);
			importOK = importNGT(this, baseFileName, mapDocument, newLayer);
			if (importOK && MerkaartorPreferences::instance()->getAutoExtractTracks()) {
				((TrackMapLayer *)newLayer)->extractLayer();
			}
		}
		else if (fn.endsWith(".nmea") || (fn.endsWith(".nma"))) {
			newLayer = new TrackMapLayer( baseFileName );
			mapDocument->add(newLayer);
			importOK = mapDocument->importNMEA(baseFileName, (TrackMapLayer *)newLayer);
			if (importOK && MerkaartorPreferences::instance()->getAutoExtractTracks()) {
				((TrackMapLayer *)newLayer)->extractLayer();
			}
		}
		else if (fn.endsWith(".kml")) {
			if (QMessageBox::warning(this, MainWindow::tr("Big Fat Copyright Warning"),
					 MainWindow::tr(
					 "You are trying to import a KML file. Please be aware that:\n"
					 "\n"
					 " - You cannot import to OSM a KML file created from Google Earth. While you might\n"
					 "   think that nodes you created from GE are yours, they are not!\n"
					 "   They are still a derivative work from GE, and, as such, cannot be used in OSM.\n"
					 "\n"
					 " - If you downloaded it from the Internet, chances are that there is a copyright on it.\n"
					 "   Please be absolutely sure that using those data in OSM is permitted by the author, or\n"
					 "   that the data is public domain.\n"
					 "\n"
					 "If unsure, please seek advice on the \"legal\" or \"talk\" openstreetmap mailing lists.\n"
					 "\n"
					 "Are you absolutely sure this KML can legally be imported in OSM?"),
					 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
			{
				newLayer = new TrackMapLayer( baseFileName );
				mapDocument->add(newLayer);
				importOK = mapDocument->importKML(baseFileName, (TrackMapLayer *)newLayer);
			} else
				importAborted = true;
		}

		if (!importOK && newLayer)
			mapDocument->remove(newLayer);

		if (importOK)
		{
			foundImport = true;

			if (importedFileNames)
				importedFileNames->append(fn);
		}
		else
		if (!importAborted)
		{
 			delete newLayer;
			QMessageBox::warning(this, tr("No valid file"), tr("%1 could not be opened.").arg(fn));
		}
	}
	QApplication::restoreOverrideCursor();

	return foundImport;
}

void MainWindow::loadFiles(const QStringList & fileList)
{

	QStringList fileNames(fileList);

#ifdef GEOIMAGE
	QStringList images = fileList.filter(".jpg", Qt::CaseInsensitive);
	if (!images.isEmpty()) {
		theGeoImage->loadImages(images, theDocument, theView);
		QString cur;
		foreach (cur, images)
			fileNames.removeAll(cur);
	}
#endif
	
	if (fileNames.isEmpty())
		return;

	QApplication::setOverrideCursor(Qt::BusyCursor);
	theLayers->setUpdatesEnabled(false);
	view()->setUpdatesEnabled(false);

        // Load only the first merkaartor document
	bool foundDocument = false;
	QMutableStringListIterator it(fileNames);
	while (it.hasNext())
	{
		const QString & fn = it.next();

		if (fn.endsWith(".mdc") == false)
			continue;

		if (foundDocument == false)
		{
			changeCurrentDirToFile(fn);
			loadDocument(fn);
			foundDocument = true;
		}

		it.remove();
	}

	MapDocument* newDoc = theDocument;
	if (foundDocument == false)
		newDoc = new MapDocument(theLayers);

	QStringList openedFiles;
	bool foundImport = importFiles(newDoc, fileNames, &openedFiles);

	foreach (QString currentFileName, openedFiles)
		M_PREFS->addRecentOpen(currentFileName);

	updateRecentOpenMenu();

	theProperties->setSelection(0);

	if (foundDocument == false)
	{
 		if (foundImport)
		{
			// only imported some tracks
			delete theDocument;
			theDocument = newDoc;
			connect (theDocument, SIGNAL(historyChanged()), theDirty, SLOT(updateList()));
			theDirty->updateList();
			theView->setDocument(theDocument);
			on_viewZoomAllAction_triggered();
		}
		else
		{
			// we didn't really open anything successfully
			delete newDoc;
		}
	} else {
		if (foundImport) {
			on_viewZoomAllAction_triggered();
		}
	}

	on_editPropertiesAction_triggered();
	theDocument->history().setActions(editUndoAction, editRedoAction, fileUploadAction);

	theLayers->setUpdatesEnabled(true);
	view()->setUpdatesEnabled(true);
	QApplication::restoreOverrideCursor();
}

void MainWindow::on_fileOpenAction_triggered()
{
	if (hasUnsavedChanges(*theDocument) && !mayDiscardUnsavedChanges(this))
		return;

	QStringList fileNames = QFileDialog::getOpenFileNames(
					this,
					tr("Open track files"),
					"", FILTER_OPEN_SUPPORTED);

	loadFiles(fileNames);
}

void MainWindow::on_fileUploadAction_triggered()
{

	if (QString(qVersion()) < "4.3.3")
	{
		if (QMessageBox::question(this,
			tr("Old Qt version detected"),
			tr("Your setup uses Qt %1, which contains various known errors in uploading "
			"data to OpenStreetMap leading to 401 server response codes. Are you sure you want to continue (which is not "
			"recommended).\n"
			"For more information see http://wiki.openstreetmap.org/index.php/Problem_uploading_with_Merkaartor").arg(qVersion()),QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes)
			return;
	}

	MerkaartorPreferences* p = MerkaartorPreferences::instance();
	while (p->getOsmUser().isEmpty()) {
		int ret = QMessageBox::warning(this, tr("Upload OSM"), tr("You don't seem to have specified your\n"
			"OpenStreetMap username and password.\nDo you want to do this now?"), QMessageBox::Yes | QMessageBox::No);
		if (ret == QMessageBox::Yes) {
			toolsPreferencesAction_triggered(true);
		} else
			return;
	}
	on_editPropertiesAction_triggered();
	syncOSM(this, p->getOsmWebsite(), p->getOsmUser(), p->getOsmPassword(), p->getProxyUse(),
		p->getProxyHost(), p->getProxyPort());

	theDocument->history().updateActions();
	theDirty->updateList();
}

void MainWindow::on_fileDownloadAction_triggered()
{
	if (downloadOSM(this, theView->projection().viewport(), theDocument)) {
		on_editPropertiesAction_triggered();
	} else
		QMessageBox::warning(this, tr("Error downloading"), tr("The map could not be downloaded"));

	updateBookmarksMenu();
}

void MainWindow::on_fileDownloadMoreAction_triggered()
{
	if (!downloadMoreOSM(this, theView->projection().viewport(), theDocument)) {
		QMessageBox::warning(this, tr("Error downloading"), tr("The map could not be downloaded"));
	}
}

void MainWindow::on_fileWorkOfflineAction_triggered()
{
	M_PREFS->setOfflineMode(!M_PREFS->getOfflineMode());
	updateMenu();
}

void MainWindow::on_helpAboutAction_triggered()
{
	QDialog dlg(this);
	Ui::AboutDialog About;
	About.setupUi(&dlg);
	About.Version->setText(About.Version->text().arg(VERSION).arg(REVISION));
	About.QTVersion->setText(About.QTVersion->text().arg(qVersion()).arg(QT_VERSION_STR));
	QFile ct(":/Utils/CHANGELOG");
	ct.open(QIODevice::ReadOnly);
	QTextStream cl(&ct);
	About.txtChangelog->setPlainText(cl.readAll());
	QPixmap px(":/Utils/Merkaartor_About.png");
	About.pxIcon->setPixmap(px);
	About.lblUrl->setOpenExternalLinks(true);
	dlg.exec();
}

void MainWindow::on_viewZoomAllAction_triggered()
{
	std::pair<bool, CoordBox> BBox(boundingBox(theDocument));
	if (BBox.first) {
		BBox.second.resize(1.01);
		theView->projection().setViewport(BBox.second, theView->rect());
		invalidateView();
	}
}

void MainWindow::on_viewZoomInAction_triggered()
{
	theView->projection().zoom(1.33333, theView->rect().center(), theView->rect());
	invalidateView();
}

void MainWindow::on_viewZoomOutAction_triggered()
{
	theView->projection().zoom(0.75, theView->rect().center(), theView->rect());
	invalidateView();
}

void MainWindow::on_viewZoomWindowAction_triggered()
{
	theView->launch(new ZoomInteraction(theView));
}

void MainWindow::on_viewDownloadedAction_triggered()
{
	M_PREFS->setDownloadedVisible(!M_PREFS->getDownloadedVisible());
	viewDownloadedAction->setChecked(M_PREFS->getDownloadedVisible());
	invalidateView();
}

void MainWindow::on_viewScaleAction_triggered()
{
	M_PREFS->setScaleVisible(!M_PREFS->getScaleVisible());
	viewScaleAction->setChecked(M_PREFS->getScaleVisible());
	invalidateView();
}

void MainWindow::on_viewStyleBackgroundAction_triggered()
{
	M_PREFS->setStyleBackgroundVisible(!M_PREFS->getStyleBackgroundVisible());
	viewStyleBackgroundAction->setChecked(M_PREFS->getStyleBackgroundVisible());
	invalidateView();
}

void MainWindow::on_viewStyleForegroundAction_triggered()
{
	M_PREFS->setStyleForegroundVisible(!M_PREFS->getStyleForegroundVisible());
	viewStyleForegroundAction->setChecked(M_PREFS->getStyleForegroundVisible());
	invalidateView();
}

void MainWindow::on_viewStyleTouchupAction_triggered()
{
	M_PREFS->setStyleTouchupVisible(!M_PREFS->getStyleTouchupVisible());
	viewStyleTouchupAction->setChecked(M_PREFS->getStyleTouchupVisible());
	invalidateView();
}

void MainWindow::on_viewNamesAction_triggered()
{
	M_PREFS->setNamesVisible(!M_PREFS->getNamesVisible());
	viewNamesAction->setChecked(M_PREFS->getNamesVisible());
	invalidateView();
}

void MainWindow::on_viewTrackPointsAction_triggered()
{
	M_PREFS->setTrackPointsVisible(!M_PREFS->getTrackPointsVisible());
	viewTrackPointsAction->setChecked(M_PREFS->getTrackPointsVisible());
	invalidateView();
}

void MainWindow::on_viewTrackSegmentsAction_triggered()
{
	M_PREFS->setTrackSegmentsVisible(!M_PREFS->getTrackSegmentsVisible());
	viewTrackSegmentsAction->setChecked(M_PREFS->getTrackSegmentsVisible());
	invalidateView();
}

void MainWindow::on_viewRelationsAction_triggered()
{
	M_PREFS->setRelationsVisible(!M_PREFS->getRelationsVisible());
	viewRelationsAction->setChecked(M_PREFS->getRelationsVisible());
	invalidateView();
}

void MainWindow::on_viewGotoAction_triggered()
{
	GotoDialog* Dlg = new GotoDialog(theView->projection(), this);
	if (Dlg->exec() == QDialog::Accepted) {
		if (!Dlg->newViewport().isNull()) {
			theView->projection().setViewport(Dlg->newViewport(), theView->rect());
			invalidateView();
		}
	}
	delete Dlg;
}

void MainWindow::on_viewArrowsNeverAction_triggered(bool checked)
{
	if (checked) {
		M_PREFS->setDirectionalArrowsVisible(DirectionalArrows_Never);
		invalidateView();
	}
}

void MainWindow::on_viewArrowsOnewayAction_triggered(bool checked)
{
	if (checked) {
		M_PREFS->setDirectionalArrowsVisible(DirectionalArrows_Oneway);
		invalidateView();
	}
}

void MainWindow::on_viewArrowsAlwaysAction_triggered(bool checked)
{
	if (checked) {
		M_PREFS->setDirectionalArrowsVisible(DirectionalArrows_Always);
		invalidateView();
	}
}

void MainWindow::on_fileNewAction_triggered()
{
	theView->launch(0);
	theProperties->setSelection(0);
	if (!hasUnsavedChanges(*theDocument) || mayDiscardUnsavedChanges(this)) {
		delete theDocument;
		theDocument = new MapDocument(theLayers);
		theView->setDocument(theDocument);
		theDocument->history().setActions(editUndoAction, editRedoAction, fileUploadAction);
		connect (theDocument, SIGNAL(historyChanged()), theDirty, SLOT(updateList()));
		theDirty->updateList();

		fileName = "";
		setWindowTitle(QString("Merkaartor - untitled"));

		on_editPropertiesAction_triggered();
		invalidateView();
	}
}

void MainWindow::on_createDoubleWayAction_triggered()
{
	theView->launch(new CreateDoubleWayInteraction(this, theView));
}

void MainWindow::on_createRoundaboutAction_triggered()
{
	theView->launch(new CreateRoundaboutInteraction(this, theView));
}

void MainWindow::on_createRoadAction_triggered()
{
	TrackPoint * firstPoint = NULL;

	if (theProperties->size() == 1)
	{
		MapFeature * feature = theProperties->selection(0);
		firstPoint = dynamic_cast<TrackPoint*>(feature);
	}

	theView->launch(new CreateSingleWayInteraction(this, theView, firstPoint, false));
}

void MainWindow::on_createCurvedRoadAction_triggered()
{
	theView->launch(new CreateSingleWayInteraction(this, theView, NULL, true));
}

void MainWindow::on_createAreaAction_triggered()
{
	theView->launch(new CreateAreaInteraction(this, theView));
}

void MainWindow::on_createNodeAction_triggered()
{
	theView->launch(new CreateNodeInteraction(theView));
}

void MainWindow::on_roadJoinAction_triggered()
{
	CommandList* theList = new CommandList(MainWindow::tr("Join Roads"), NULL);
	joinRoads(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
	{
		theDocument->addHistory(theList);
		invalidateView();
	}
}

void MainWindow::on_roadSplitAction_triggered()
{
	CommandList* theList = new CommandList(MainWindow::tr("Split Roads"), NULL);
	splitRoads(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
		theDocument->addHistory(theList);
}

void MainWindow::on_roadBreakAction_triggered()
{
	CommandList* theList = new CommandList(MainWindow::tr("Break Roads"), NULL);
	breakRoads(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
		theDocument->addHistory(theList);
}

void MainWindow::on_featureCommitAction_triggered()
{
	CommandList* theList = new CommandList(MainWindow::tr("Commit Roads"), NULL);
	commitFeatures(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
		theDocument->addHistory(theList);
}

void MainWindow::on_nodeAlignAction_triggered()
{
	//MapFeature* F = theView->properties()->selection(0);
	CommandList* theList = new CommandList(MainWindow::tr("Align Nodes"), NULL);
	alignNodes(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
	{
		theDocument->addHistory(theList);
	//	theView->properties()->setSelection(F);
		invalidateView();
	}
}

void MainWindow::on_nodeMergeAction_triggered()
{
	MapFeature* F = theProperties->selection(0);
	CommandList* theList = new CommandList(MainWindow::tr("Merge Nodes into %1").arg(F->id()), F);
	mergeNodes(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
	{
		theDocument->addHistory(theList);
		theProperties->setSelection(F);
		invalidateView();
	}
}

void MainWindow::on_nodeDetachAction_triggered()
{
	MapFeature* F = theProperties->selection(0);
	CommandList* theList = new CommandList(MainWindow::tr("Detach Node %1").arg(F->id()), F);
	detachNode(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
	{
		theDocument->addHistory(theList);
		theProperties->setSelection(F);
		invalidateView();
	}
}

void MainWindow::on_relationAddMemberAction_triggered()
{
	CommandList* theList = new CommandList(MainWindow::tr("Add member to relation"), NULL);
	addRelationMember(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
		theDocument->addHistory(theList);
}

void MainWindow::on_relationRemoveMemberAction_triggered()
{
	CommandList* theList = new CommandList(MainWindow::tr("Remove member from relation"), NULL);
	removeRelationMember(theDocument, theList, theProperties);
	if (theList->empty())
		delete theList;
	else
		theDocument->addHistory(theList);
}

void MainWindow::on_createRelationAction_triggered()
{
	Relation* R = new Relation;
	for (unsigned int i = 0; i < theProperties->size(); ++i)
		R->add("", theProperties->selection(i));
	CommandList* theList = new CommandList(MainWindow::tr("Create Relation %1").arg(R->description()), R);
	theList->add(
		new AddFeatureCommand(document()->getDirtyLayer(), R, true));
	theDocument->addHistory(theList);
	theProperties->setSelection(R);
}

void MainWindow::on_editMapStyleAction_triggered()
{
	PaintStyleEditor* dlg = new PaintStyleEditor(this, M_STYLE->getGlobalPainter(), M_STYLE->getPainters());
	connect(dlg, SIGNAL(stylesApplied(QVector<FeaturePainter>* )), this, SLOT(applyStyles(QVector<FeaturePainter>* )));
	GlobalPainter saveGlobalPainter = M_STYLE->getGlobalPainter();
	QVector<FeaturePainter> savePainters = M_STYLE->getPainters();
	if (dlg->exec() == QDialog::Accepted) {
		M_STYLE->setGlobalPainter(dlg->theGlobalPainter);
		M_STYLE->setPainters(dlg->thePainters);
	} else {
		M_STYLE->setGlobalPainter(saveGlobalPainter);
		M_STYLE->setPainters(savePainters);
	}

	for (VisibleFeatureIterator i(theDocument); !i.isEnd(); ++i)
		i.get()->invalidatePainter();
	invalidateView();
	delete dlg;
}

void MainWindow::applyStyles(QVector<FeaturePainter>* thePainters)
{
	M_STYLE->setPainters(*thePainters);
	for (VisibleFeatureIterator i(theDocument); !i.isEnd(); ++i)
		i.get()->invalidatePainter();
	invalidateView();
}

//MapLayer* MainWindow::activeLayer()
//{
////	return theLayers->activeLayer();
//	//The "active" layer is always the dirty layer
//	return theDocument->getDirtyLayer();
//}

MapView* MainWindow::view()
{
	return theView;
}

void MainWindow::on_mapStyleSaveAction_triggered()
{
	QString f = QFileDialog::getSaveFileName(this, tr("Save map style"), QString(), tr("Merkaartor map style (*.mas)"));
	if (!f.isNull()) {
		if (!f.endsWith(".mas"))
			f.append(".mas");
		M_STYLE->savePainters(f);
	}
}

void MainWindow::on_mapStyleLoadAction_triggered()
{
	QString f = QFileDialog::getOpenFileName(this, tr("Load map style"), QString(), tr("Merkaartor map style (*.mas)"));
	if (!f.isNull()) {
		M_STYLE->loadPainters(f);
		for (VisibleFeatureIterator i(theDocument); !i.isEnd(); ++i)
			i.get()->invalidatePainter();
		invalidateView();
	}
}

void MainWindow::on_toolsWorldOsbAction_triggered()
{
	WorldOsbManager osbMgr(this);
	osbMgr.exec();
}

void MainWindow::on_toolsShortcutsAction_triggered()
{
	QList<QAction*> theActions;
	theActions.append(fileNewAction);
	theActions.append(fileOpenAction);
	theActions.append(fileImportAction);
	theActions.append(fileSaveAction);
	theActions.append(fileSaveAsAction);
	theActions.append(fileDownloadAction);
	theActions.append(fileDownloadMoreAction);
	theActions.append(fileUploadAction);
	theActions.append(fileQuitAction);
	theActions.append(editUndoAction);
	theActions.append(editRedoAction);
	theActions.append(editCopyAction);
	theActions.append(editPasteMergeAction);
	theActions.append(editPasteOverwriteAction);
	theActions.append(editRemoveAction);
	theActions.append(editPropertiesAction);
	theActions.append(editSelectAction);
	theActions.append(viewZoomAllAction);
	theActions.append(viewZoomWindowAction);
	theActions.append(viewZoomOutAction);
	theActions.append(viewZoomInAction);
	theActions.append(viewDownloadedAction);
	theActions.append(viewRelationsAction);
	theActions.append(viewScaleAction);
	theActions.append(viewNamesAction);
	theActions.append(viewTrackPointsAction);
	theActions.append(viewTrackSegmentsAction);
	theActions.append(viewGotoAction);
	theActions.append(gpsConnectAction);
	theActions.append(gpsReplayAction);
	theActions.append(gpsRecordAction);
	theActions.append(gpsPauseAction);
	theActions.append(gpsDisconnectAction);
	theActions.append(gpsCenterAction);
	theActions.append(createNodeAction);
	theActions.append(createRoadAction);
	theActions.append(createDoubleWayAction);
	theActions.append(createRoundaboutAction);
	theActions.append(createAreaAction);
	theActions.append(createRelationAction);
	theActions.append(featureCommitAction);
	theActions.append(roadSplitAction);
	theActions.append(roadBreakAction);
	theActions.append(roadJoinAction);
	theActions.append(editReverseAction);
	theActions.append(nodeMergeAction);
	theActions.append(nodeAlignAction);
	theActions.append(toolsPreferencesAction);
	theActions.append(toolsShortcutsAction);
	theActions.append(toolsWorldOsbAction);
	theActions.append(windowPropertiesAction);
	theActions.append(windowLayersAction);
	theActions.append(windowInfoAction);
	theActions.append(windowDirtyAction);
	theActions.append(windowGPSAction);
	theActions.append(windowGeoimageAction);
	theActions.append(helpAboutAction);

	ActionsDialog dlg(theActions, this);
	dlg.exec();
}

void MainWindow::toolsPreferencesAction_triggered(bool focusData)
{
	PreferencesDialog* Pref = new PreferencesDialog(this);
	if (focusData)
		Pref->tabPref->setCurrentWidget(Pref->tabData);
	else
		Pref->tabPref->setCurrentIndex(p->lastPrefTabIndex);
	connect (Pref, SIGNAL(preferencesChanged()), this, SLOT(preferencesChanged()));
	Pref->exec();
	p->lastPrefTabIndex = Pref->tabPref->currentIndex();
}

void MainWindow::preferencesChanged(void)
{
	if (M_PREFS->getMerkaartorStyle())
		QApplication::setStyle(new SkulptureStyle);
	else
		QApplication::setStyle(p->defStyle);

	theDocument->getImageLayer()->setMapAdapter(M_PREFS->getBgType());
	theDocument->getImageLayer()->updateWidget();
	adjustLayers(true);

	ImageManager::instance()->setCacheDir(M_PREFS->getCacheDir());
	ImageManager::instance()->setCacheMaxSize(M_PREFS->getCacheSize());
#ifdef YAHOO
	BrowserImageManager::instance()->setCacheDir(M_PREFS->getCacheDir());
	BrowserImageManager::instance()->setCacheMaxSize(M_PREFS->getCacheSize());
#endif
	if (M_PREFS->getProxyUse()) {
		ImageManager::instance()->setProxy(M_PREFS->getProxyHost(),
			M_PREFS->getProxyPort());
	} else {
		ImageManager::instance()->setProxy("",0);
	}
	theView->projection().setProjectionType(M_PREFS->getProjectionType());
	
	updateMenu();
}

void MainWindow::on_fileSaveAsAction_triggered()
{
	fileName = QFileDialog::getSaveFileName(this,
		tr("Save Merkaartor document"), QString("%1/%2.mdc").arg(MerkaartorPreferences::instance()->getWorkingDir()).arg(tr("untitled")), tr("Merkaartor documents Files (*.mdc)"));

	if (fileName != "") {
		saveDocument();
	}

	M_PREFS->addRecentOpen(fileName);
	updateRecentOpenMenu();
}

void MainWindow::on_fileSaveAction_triggered()
{
	if (fileName != "") {
		saveDocument();
	} else {
		on_fileSaveAsAction_triggered();
	}
}

void MainWindow::saveDocument()
{
	QApplication::setOverrideCursor(Qt::BusyCursor);

	QDomElement root;
	QDomDocument* theXmlDoc;

	theXmlDoc = new QDomDocument();
	theXmlDoc->appendChild(theXmlDoc->createProcessingInstruction("xml", "version=\"1.0\""));
	root = theXmlDoc->createElement("MerkaartorDocument");
	root.setAttribute("version", "1.1");
	root.setAttribute("creator", QString("Merkaartor %1").arg(VERSION));

	theXmlDoc->appendChild(root);

	QProgressDialog progress("Saving document...", "Cancel", 0, 0);
	progress.setWindowModality(Qt::WindowModal);

	theDocument->toXML(root, progress);
	theView->toXML(root);

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::critical(this, tr("Unable to open save file"), tr("%1 could not be opened for writing.").arg(fileName));
		return;
	}
	file.write(theXmlDoc->toString().toUtf8());
	file.close();
	delete theXmlDoc;

	progress.setValue(progress.maximum());

	setWindowTitle(QString("Merkaartor - %1").arg(fileName));

	QApplication::restoreOverrideCursor();

}

void MainWindow::loadDocument(QString fn)
{
	QFile file(fn);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(this, tr("Invalid file"), tr("%1 could not be opened.").arg(fn));
		return;
	}

	QDomDocument* theXmlDoc = new QDomDocument();
	if (!theXmlDoc->setContent(&file)) {
		QMessageBox::critical(this, tr("Invalid file"), tr("%1 is not a valid XML file.").arg(fn));
		file.close();
		delete theXmlDoc;
		theXmlDoc = NULL;
		return;
	}
	file.close();

	QDomElement docElem = theXmlDoc->documentElement();
	if (docElem.tagName() != "MerkaartorDocument") {
		QMessageBox::critical(this, tr("Invalid file"), tr("%1 is not a valid Merkaartor document.").arg(fn));
		return;
	}
	double version = docElem.attribute("version").toDouble();

	QProgressDialog progress("Loading document...", "Cancel", 0, 0);
	progress.setWindowModality(Qt::WindowModal);

	progress.setMaximum(progress.maximum() + theXmlDoc->elementsByTagName("relation").count());
	progress.setMaximum(progress.maximum() + theXmlDoc->elementsByTagName("way").count());
	progress.setMaximum(progress.maximum() + theXmlDoc->elementsByTagName("node").count());
	progress.setMaximum(progress.maximum() + theXmlDoc->elementsByTagName("trkpt").count());
	progress.setMaximum(progress.maximum() + theXmlDoc->elementsByTagName("wpt").count());

	QDomElement e = docElem.firstChildElement();
	while(!e.isNull()) {
		if (e.tagName() == "MapDocument") {
			MapDocument* newDoc = MapDocument::fromXML(e, version, theLayers, progress);

			if (progress.wasCanceled())
				break;

			if (newDoc) {
				theProperties->setSelection(0);
				delete theDocument;
				theDocument = newDoc;
				theView->setDocument(theDocument);
				on_editPropertiesAction_triggered();
				theDocument->history().setActions(editUndoAction, editRedoAction, fileUploadAction);
				connect (theDocument, SIGNAL(historyChanged()), theDirty, SLOT(updateList()));
				theDirty->updateList();
				fileName = fn;
				setWindowTitle(QString("Merkaartor - %1").arg(fileName));
			}
		} else
		if (e.tagName() == "MapView") {
			view()->fromXML(e);
		}

		if (progress.wasCanceled())
			break;

		e = e.nextSiblingElement();
	}
	progress.reset();
	delete theXmlDoc;

	M_PREFS->addRecentOpen(fn);
	updateRecentOpenMenu();
}

void MainWindow::on_exportOSMAction_triggered()
{
	QVector<MapFeature*> theFeatures;
	if (!selectExportedFeatures(theFeatures))
		return;

	QString fileName = QFileDialog::getSaveFileName(this,
		tr("Export OSM"), MerkaartorPreferences::instance()->getWorkingDir() + "/untitled.osm", tr("OSM Files (*.osm)"));

	if (fileName != "") {
		QFile file(fileName);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;

		file.write(theDocument->exportOSM(theFeatures).toUtf8());
		file.close();
	}
}

void MainWindow::on_exportOSMBinAction_triggered()
{
	QVector<MapFeature*> theFeatures;
	if (!selectExportedFeatures(theFeatures))
		return;

	QString fileName = QFileDialog::getSaveFileName(this,
		tr("Export Binary OSM"), MerkaartorPreferences::instance()->getWorkingDir() + "/untitled.osb", tr("OSM Binary Files (*.osb)"));

	if (fileName != "") {
		QApplication::setOverrideCursor(Qt::BusyCursor);

		ImportExportOsmBin osb(document());
		if (osb.saveFile(fileName)) {
			osb.export_(theFeatures);
		}

		QApplication::restoreOverrideCursor();
	}
}

void MainWindow::on_exportGPXAction_triggered()
{
	QVector<MapFeature*> theFeatures;

	QString fileName = QFileDialog::getSaveFileName(this,
		tr("Export GPX"), MerkaartorPreferences::instance()->getWorkingDir() + "/untitled.gpx", tr("GPX Files (*.gpx)"));

	if (fileName != "") {
		QApplication::setOverrideCursor(Qt::BusyCursor);

		for (VisibleFeatureIterator i(document()); !i.isEnd(); ++i) {
			if (dynamic_cast<TrackMapLayer*>(i.get()->layer())) {
				theFeatures.push_back(i.get());
			}
		}

		ExportGPX gpx(document());
		if (gpx.saveFile(fileName)) {
			gpx.export_(theFeatures);
		}

		QApplication::restoreOverrideCursor();
	}
}

void MainWindow::on_exportKMLAction_triggered()
{
	QVector<MapFeature*> theFeatures;
	if (!selectExportedFeatures(theFeatures))
		return;

	QString fileName = QFileDialog::getSaveFileName(this,
		tr("Export KML"), MerkaartorPreferences::instance()->getWorkingDir() + "/untitled.kml", tr("KML Files (*.kml)"));

	if (fileName != "") {
		QApplication::setOverrideCursor(Qt::BusyCursor);

		ImportExportKML kml(document());
		if (kml.saveFile(fileName)) {
			kml.export_(theFeatures);
		}

		QApplication::restoreOverrideCursor();
	}
}

bool MainWindow::selectExportedFeatures(QVector<MapFeature*>& theFeatures)
{
	QDialog dlg(this);
	Ui::ExportDialog dlgExport;
	dlgExport.setupUi(&dlg);
	switch(MerkaartorPreferences::instance()->getExportType()) {
		case Export_All:
			dlgExport.rbAll->setChecked(true);
			break;
		case Export_Viewport:
			dlgExport.rbViewport->setChecked(true);
			break;
		case Export_Selected:
			dlgExport.rbSelected->setChecked(true);
			break;
		default:
			return false;

	}
	if (dlg.exec()) {
		if (dlgExport.rbAll->isChecked()) {
			theFeatures = document()->getFeatures();
			MerkaartorPreferences::instance()->setExportType(Export_All);
			return true;
		}
		if (dlgExport.rbViewport->isChecked()) {
			CoordBox aCoordBox = view()->projection().viewport();

			theFeatures.clear();
			for (VisibleFeatureIterator i(document()); !i.isEnd(); ++i) {
				if (TrackPoint* P = dynamic_cast<TrackPoint*>(i.get())) {
					if (aCoordBox.contains(P->position())) {
						theFeatures.append(P);
					}
				} else
					if (Road* G = dynamic_cast<Road*>(i.get())) {
						if (aCoordBox.intersects(G->boundingBox())) {
							for (unsigned int j=0; j < G->size(); j++) {
								if (TrackPoint* P = dynamic_cast<TrackPoint*>(G->get(j)))
									if (!aCoordBox.contains(P->position()))
										theFeatures.append(P);
							}
							theFeatures.append(G);
						}
					} else
						//FIXME Not working for relation (not made of point?)
						if (Relation* G = dynamic_cast<Relation*>(i.get())) {
							if (aCoordBox.intersects(G->boundingBox())) {
								for (unsigned int j=0; j < G->size(); j++) {
									if (Road* R = dynamic_cast<Road*>(G->get(j))) {
										if (!aCoordBox.contains(R->boundingBox())) {
											for (unsigned int k=0; k < R->size(); k++) {
												if (TrackPoint* P = dynamic_cast<TrackPoint*>(R->get(k)))
													if (!aCoordBox.contains(P->position()))
														theFeatures.append(P);
											}
											theFeatures.append(R);
										}
									}
								}
								theFeatures.append(G);
							}
						}
			}
			MerkaartorPreferences::instance()->setExportType(Export_Viewport);
			return true;
		}
		if (dlgExport.rbSelected->isChecked()) {
			theFeatures = theProperties->selection();
			MerkaartorPreferences::instance()->setExportType(Export_Selected);
			return true;
		}
	}
	return false;
}

void MainWindow::on_editSelectAction_triggered()
{
	SelectionDialog* Sel = new SelectionDialog(this);

	if (Sel->exec() == QDialog::Accepted) {
		MerkaartorPreferences::instance()->setLastSearchName(Sel->edName->text());
		MerkaartorPreferences::instance()->setLastSearchKey(Sel->cbKey->currentText());
		MerkaartorPreferences::instance()->setLastSearchValue(Sel->cbValue->currentText());
		MerkaartorPreferences::instance()->setLastMaxSearchResults(Sel->sbMaxResult->value());

		QRegExp selName(Sel->edName->text(), Qt::CaseInsensitive, QRegExp::RegExp);
		QRegExp selKey(Sel->cbKey->currentText(), Qt::CaseInsensitive, QRegExp::RegExp);
		QRegExp selValue(Sel->cbValue->currentText(), Qt::CaseInsensitive, QRegExp::RegExp);
		int selMaxResult = Sel->sbMaxResult->value();

		std::vector <MapFeature *> selection;
		int added = 0;
		for (VisibleFeatureIterator i(theDocument); !i.isEnd() && added < selMaxResult; ++i) {
			MapFeature* F = i.get();
			if (selName.indexIn(F->description()) == -1) {
				continue;
			}
			int ok = false;
			for (unsigned int j=0; j < F->tagSize(); j++) {
				if ((selKey.indexIn(F->tagKey(j)) > -1) && (selValue.indexIn(F->tagValue(j)) > -1)) {
					ok = true;
					break;
				}
			}
			if (ok) {
				selection.push_back(F);
				++added;
			}
		}
		theProperties->setMultiSelection(selection);
		view()->properties()->checkMenuStatus();
	}
}

void MainWindow::closeEvent(QCloseEvent * event)
{
	if (hasUnsavedChanges(*theDocument) && !mayDiscardUnsavedChanges(this)) {
		event->ignore();
		return;
	}

	MerkaartorPreferences::instance()->saveMainWindowState( this );
	MerkaartorPreferences::instance()->setInitialPosition(theView);
//4.3237,50.8753,4.3378,50.8838
	QMainWindow::closeEvent(event);
}

void MainWindow::on_renderNativeAction_triggered()
{
	NativeRenderDialog osmR(theDocument, theView->projection().viewport(), this);
	osmR.exec();
}

void MainWindow::on_renderSVGAction_triggered()
{
#ifdef OSMARENDER
	OsmaRenderDialog osmR(theDocument, theView->projection().viewport(), this);
	osmR.exec();
#endif
}

void MainWindow::updateBookmarksMenu()
{
	for(int i=menuBookmarks->actions().count()-1; i > 2 ; i--) {
		menuBookmarks->removeAction(menuBookmarks->actions()[3]);
	}

	QMapIterator<QString, CoordBox> i(MerkaartorPreferences::instance()->getBookmarks());
	while (i.hasNext()) {
		i.next();
		QAction* a = new QAction(i.key(), menuBookmarks);
		menuBookmarks->addAction(a);
	}
}

void MainWindow::updateRecentOpenMenu()
{
	for(int i=menuRecentOpen->actions().count()-1; i >= 0; i--) {
		menuRecentOpen->removeAction(menuRecentOpen->actions()[0]);
	}

	if (!M_PREFS->getRecentOpen().size()) {
		menuRecentOpen->setEnabled(false);
		return;
	}

	menuRecentOpen->setEnabled(true);
	QStringList RecentOpen = M_PREFS->getRecentOpen();
	for (int i=0; i<RecentOpen.size(); i++) {
		QAction* a = new QAction(RecentOpen[i], menuRecentOpen);
		menuRecentOpen->addAction(a);
	}
}

void MainWindow::updateRecentImportMenu()
{
	for(int i=menuRecentImport->actions().count()-1; i >= 0; i--) {
		menuRecentImport->removeAction(menuRecentImport->actions()[0]);
	}

	if (!M_PREFS->getRecentImport().size()) {
		menuRecentImport->setEnabled(false);
		return;
	}

	menuRecentImport->setEnabled(true);
	QStringList RecentImport = M_PREFS->getRecentImport();
	for (int i=0; i<RecentImport.size(); i++) {
		QAction* a = new QAction(RecentImport[i], menuRecentImport);
		menuRecentImport->addAction(a);
	}
}

void MainWindow::updateProjectionMenu()
{
	QStringList Projections = MerkaartorPreferences::instance()->getProjectionTypes();
	QActionGroup* actgrp = new QActionGroup(this);
	for (int i=0; i<Projections.size(); i++) {
		QAction* a = new QAction(Projections[i], mnuProjections);
		actgrp->addAction(a);
		a->setCheckable (true);
		if (i == (int)MerkaartorPreferences::instance()->getProjectionType())
			a->setChecked(true);
		mnuProjections->addAction(a);
	}

	connect (mnuProjections, SIGNAL(triggered(QAction *)), this, SLOT(projectionTriggered(QAction *)));
}

void MainWindow::on_bookmarkAddAction_triggered()
{
	bool ok = true;
	QString text;

	QMap<QString, CoordBox>& Bookmarks = MerkaartorPreferences::instance()->getBookmarks();
	QStringList bkName;
	QMapIterator<QString, CoordBox> i(Bookmarks);
	while (i.hasNext()) {
		i.next();
		bkName.append(i.key());
	}
	while (ok) {
		text = QInputDialog::getItem(this, MainWindow::tr("Add Bookmark"),
						MainWindow::tr("Specify the name of the bookmark."), bkName, 0, true, &ok);
		if (ok) {
			if (text.isEmpty()) {
				QMessageBox::critical(this, tr("Invalid bookmark name"),
					tr("Bookmark cannot be blank."), QMessageBox::Ok);
				continue;
			}
			if (Bookmarks.contains(text)) {
				QString newBk = QInputDialog::getText(this, MainWindow::tr("Warning: Bookmark name already exists"),
						MainWindow::tr("Enter a new one, keep the same to overwrite or cancel."), QLineEdit::Normal,
									   text, &ok);
				if (ok && Bookmarks.contains(newBk)) {
					Bookmarks.remove(newBk);

					for(int i=2; i < menuBookmarks->actions().count(); i++) {
						if (menuBookmarks->actions()[i]->text() == newBk) {
							menuBookmarks->removeAction(menuBookmarks->actions()[i]);
							break;
						}
					}
				}
				text = newBk;
			}
			break;
		}
	}
	if (ok) {
		CoordBox Clip = view()->projection().viewport();
		Bookmarks[text] = Clip;
		M_PREFS->setBookmarks();

		QAction* a = new QAction(text, menuBookmarks);
		menuBookmarks->addAction(a);
	}
}

void MainWindow::on_bookmarkRemoveAction_triggered()
{
	bool ok;

	QMap<QString, CoordBox>& Bookmarks = MerkaartorPreferences::instance()->getBookmarks();
	QStringList bkName;
	QMapIterator<QString, CoordBox> i(Bookmarks);
	while (i.hasNext()) {
		i.next();
		bkName.append(i.key());
	}
	QString item = QInputDialog::getItem(this, MainWindow::tr("Remove Bookmark"),
						MainWindow::tr("Select the bookmark to remove."), bkName, 0, false, &ok);
	if (ok) {
		Bookmarks.remove(item);
		M_PREFS->setBookmarks();

		for(int i=2; i < menuBookmarks->actions().count(); i++) {
			if (menuBookmarks->actions()[i]->text() == item) {
				menuBookmarks->removeAction(menuBookmarks->actions()[i]);
				break;
			}
		}
	}
}

void MainWindow::bookmarkTriggered(QAction* anAction)
{
	if (anAction == bookmarkAddAction || anAction == bookmarkRemoveAction)
		return;
	QMap<QString, CoordBox> Bookmarks = MerkaartorPreferences::instance()->getBookmarks();
	theView->projection().setViewport(Bookmarks[anAction->text()], theView->rect());

	invalidateView();
}

void MainWindow::recentOpenTriggered(QAction* anAction)
{
	if (hasUnsavedChanges(*theDocument) && !mayDiscardUnsavedChanges(this))
		return;

	QStringList fileNames(anAction->text());
	loadFiles(fileNames);
}

void MainWindow::recentImportTriggered(QAction* anAction)
{
	view()->setUpdatesEnabled(false);
	theLayers->setUpdatesEnabled(false);

	QStringList fileNames(anAction->text());
	QStringList importedFiles;
	importFiles(theDocument, fileNames, &importedFiles);

	foreach (QString currentFileName, importedFiles)
		M_PREFS->addRecentImport(currentFileName);

	updateRecentImportMenu();

	view()->setUpdatesEnabled(true);
	theLayers->setUpdatesEnabled(true);

	on_editPropertiesAction_triggered();
	theDocument->history().setActions(editUndoAction, editRedoAction, fileUploadAction);
}

void MainWindow::projectionTriggered(QAction* anAction)
{
	QStringList Projections = MerkaartorPreferences::instance()->getProjectionTypes();
	int idx = Projections.indexOf(anAction->text());
	MerkaartorPreferences::instance()->setProjectionType((ProjectionType)idx);
	theView->projection().setProjectionType((ProjectionType)idx);
	theView->projection().setViewport(theView->projection().viewport(), theView->rect());
	invalidateView();
}

void MainWindow::on_windowPropertiesAction_triggered()
{
	theProperties->setVisible(!theProperties->isVisible());
}

void MainWindow::on_windowLayersAction_triggered()
{
	theLayers->setVisible(!theLayers->isVisible());
}

void MainWindow::on_windowInfoAction_triggered()
{
	theInfo->setVisible(!theInfo->isVisible());
}

void MainWindow::on_windowDirtyAction_triggered()
{
	theDirty->setVisible(!theDirty->isVisible());
}

void MainWindow::on_windowToolbarAction_triggered()
{
	toolBar->setVisible(!toolBar->isVisible());
}

void MainWindow::on_windowGPSAction_triggered()
{
	theGPS->setVisible(!theGPS->isVisible());
}

#ifdef GEOIMAGE
void MainWindow::on_windowGeoimageAction_triggered()
{
	theGeoImage->setVisible(!theGeoImage->isVisible());
}
#endif

void MainWindow::on_windowHideAllAction_triggered()
{
	fullscreenState = saveState(1);

	windowHideAllAction->setEnabled(false);
	windowHideAllAction->setVisible(false);
	windowShowAllAction->setEnabled(true);
	windowShowAllAction->setVisible(true);

//	toolBar->setVisible(false);
	theInfo->setVisible(false);
	theDirty->setVisible(false);
	theLayers->setVisible(false);
	theProperties->setVisible(false);
	theGPS->setVisible(false);
#ifdef GEOIMAGE
	theGeoImage->setVisible(false);
#endif
}

void MainWindow::on_windowShowAllAction_triggered()
{
	restoreState(fullscreenState, 1);

	windowHideAllAction->setEnabled(true);
	windowHideAllAction->setVisible(true);
	windowShowAllAction->setEnabled(false);
	windowShowAllAction->setVisible(false);
}

void MainWindow::on_layersAddImageAction_triggered()
{
	ImageMapLayer* il = new ImageMapLayer(tr("Background imagery"), theView->layermanager);
	theDocument->add(il);
}

void MainWindow::on_gpsConnectAction_triggered()
{
#ifdef USEGPSD
	QGPSDDevice* aGps = new QGPSDDevice("gpsd");
#else
	QGPSComDevice* aGps = new QGPSComDevice(M_PREFS->getGpsPort());
#endif
	if (aGps->openDevice()) {
		connect(aGps, SIGNAL(updatePosition(float, float, QDateTime, float, float, float)), 
			this, SLOT(updateGpsPosition(float, float, QDateTime, float, float, float)));

		gpsConnectAction->setEnabled(false);
		gpsReplayAction->setEnabled(false);
		gpsDisconnectAction->setEnabled(true);
		gpsRecordAction->setEnabled(true);
		gpsPauseAction->setEnabled(true);
		theGPS->setGpsDevice(aGps);
		theGPS->resetGpsStatus();
		theGPS->startGps();
	} else {
		QMessageBox::critical(this, tr("GPS error"),
			tr("Unable to open GPS port."), QMessageBox::Ok);
		delete aGps;
	}
}

void MainWindow::on_gpsReplayAction_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(
					this,
					tr("Open NMEA log file"),
					"", "NMEA GPS log format (*.nmea *.nma)" );

	if (fileName.isEmpty())
		return;

	QGPSFileDevice* aGps = new QGPSFileDevice(fileName);
	if (aGps->openDevice()) {
		connect(aGps, SIGNAL(updatePosition(float, float, QDateTime, float, float, float)), 
			this, SLOT(updateGpsPosition(float, float, QDateTime, float, float, float)));

		gpsConnectAction->setEnabled(false);
		gpsReplayAction->setEnabled(false);
		gpsDisconnectAction->setEnabled(true);
		gpsRecordAction->setEnabled(true);
		gpsPauseAction->setEnabled(true);

		theGPS->setGpsDevice(aGps);
		theGPS->resetGpsStatus();
		theGPS->startGps();
	}
}

void MainWindow::on_gpsDisconnectAction_triggered()
{
	gpsConnectAction->setEnabled(true);
	gpsReplayAction->setEnabled(true);
	gpsDisconnectAction->setEnabled(false);
	gpsRecordAction->setEnabled(false);
	gpsPauseAction->setEnabled(false);
	gpsRecordAction->setChecked(false);
	gpsPauseAction->setChecked(false);

	disconnect(theGPS->getGpsDevice(), SIGNAL(updatePosition(float, float, QDateTime, float, float, float)), 
		this, SLOT(updateGpsPosition(float, float, QDateTime, float, float, float)));
	theGPS->stopGps();
	theGPS->resetGpsStatus();
}

void MainWindow::updateGpsPosition(float latitude, float longitude, QDateTime time, float altitude, float speed, float heading)
{
	Q_UNUSED(heading)
	if (theGPS->getGpsDevice()) {
		Coord gpsCoord(angToInt(latitude), angToInt(longitude));
		if (gpsCenterAction->isChecked()) {
			CoordBox vp = theView->projection().viewport();
			QRect vpr = vp.toRect().adjusted(vp.lonDiff() / 4, vp.latDiff() / 4, -vp.lonDiff() / 4, -vp.latDiff() / 4);
			if (!vpr.contains(gpsCoord.toQPoint())) {
				theView->projection().setCenter(gpsCoord, theView->rect());
				theView->invalidate(true, true);
			}
		}

		if (gpsRecordAction->isChecked() && !gpsPauseAction->isChecked()) {
			TrackPoint* pt = new TrackPoint(gpsCoord);
			pt->setTime(time);
			pt->setElevation(altitude);
			pt->setSpeed(speed);
			gpsRecLayer->add(pt);
			curGpsTrackSegment->add(pt);
			theView->invalidate(true, false);
		}
	}
	theView->update();
}

QGPS* MainWindow::gps()
{
	return theGPS;
}

void MainWindow::on_gpsCenterAction_triggered()
{
	M_PREFS->setGpsMapCenter(!M_PREFS->getGpsMapCenter());
	gpsCenterAction->setChecked(M_PREFS->getGpsMapCenter());
	invalidateView();
}

void MainWindow::on_gpsRecordAction_triggered()
{
	if (gpsRecordAction->isChecked()) {
		if (theDocument) {
			QString fn = "log-" + QDateTime::currentDateTime().toString(Qt::ISODate);
			fn.replace(':', '-');

			gpsRecLayer = new TrackMapLayer();
			gpsRecLayer->setName(fn);
			theDocument->add(gpsRecLayer);

			curGpsTrackSegment = new TrackSegment();
			gpsRecLayer->add(curGpsTrackSegment);
		} else {
			gpsRecordAction->setChecked(false);
		}
	} else {
		gpsPauseAction->setChecked(false);
	}
}
void MainWindow::on_gpsPauseAction_triggered()
{
	if (gpsPauseAction->isChecked()) {
		if (!gpsRecordAction->isChecked()) {
			gpsPauseAction->setChecked(false);
		}
	} else {
		if (theDocument && gpsRecordAction->isChecked()) {
			curGpsTrackSegment = new TrackSegment();
			gpsRecLayer->add(curGpsTrackSegment);
		}
	}
}

void MainWindow::on_toolTemplatesSaveAction_triggered()
{
	QString f = QFileDialog::getSaveFileName(this, tr("Save Tag Templates"), "", tr("Merkaartor tag templates (*.mat)"));
	if (!f.isNull()) {
		if (!f.endsWith(".mat"))
			f.append(".mat");
		theProperties->saveTemplates(f);
	}
}

void MainWindow::on_toolTemplatesMergeAction_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(
					this,
					tr("Open Tag Templates"),
					"", "Merkaartor tag templates (*.mat)" );

	if (fileName.isEmpty())
		return;

	theProperties->mergeTemplates(fileName);
	theProperties->resetValues();
}

void MainWindow::on_toolTemplatesLoadAction_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(
					this,
					tr("Open Tag Templates"),
					"", "Merkaartor tag templates (*.mat)" );

	if (fileName.isEmpty())
		return;

	theProperties->loadTemplates(fileName);
	theProperties->resetValues();
}

void MainWindow::updateLanguage()
{
    if (qtTranslator) {
        QCoreApplication::removeTranslator(qtTranslator);
    }
    if (merkaartorTranslator) {
        QCoreApplication::removeTranslator(merkaartorTranslator);
    }
    QString DefaultLanguage = getDefaultLanguage();
    if (DefaultLanguage != "-")
    {
        qtTranslator = new QTranslator;
        bool ret = qtTranslator->load("qt_" + DefaultLanguage
    #ifdef TRANSDIR_SYSTEM
            , TRANSDIR_SYSTEM
    #else
            , QLibraryInfo::location(QLibraryInfo::TranslationsPath)
    #endif
            );
        if (ret)
	        QCoreApplication::installTranslator(qtTranslator);

        merkaartorTranslator = new QTranslator;
        ret &= merkaartorTranslator->load("merkaartor_" + DefaultLanguage
    #ifdef TRANSDIR_MERKAARTOR
            , TRANSDIR_MERKAARTOR
    #else
            , QCoreApplication::applicationDirPath()
    #endif
            );
        if (!ret)
        {
			QMessageBox::warning(this, tr("Error"), tr("Could not load the selected language. Go to Tools, Preferences to select another language or check whether the translation file is missing."));
        	return;   // Language could not be loaded, do nothing (need to restart programme)
		}
        QCoreApplication::installTranslator(merkaartorTranslator);
    }
    retranslateUi(this);
}

void MainWindow::updateMenu()
{
	if (M_PREFS->getOfflineMode()) {
		fileWorkOfflineAction->setChecked(true);
		fileDownloadAction->setEnabled(false);
		fileDownloadMoreAction->setEnabled(false);
		fileUploadAction->setEnabled(false);
	} else {
		fileWorkOfflineAction->setChecked(false);
		fileDownloadAction->setEnabled(true);
		fileDownloadMoreAction->setEnabled(true);
		fileUploadAction->setEnabled(true);
	}
	
	if (M_PREFS->getSeparateMoveMode())
		editMoveAction->setVisible(true);
	else
		editMoveAction->setVisible(false);
}
