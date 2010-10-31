//***************************************************************
// CLass: SpatialiteAdapter
//
// Description:
//
//
// Author: Chris Browet <cbro@semperpax.com> (C) 2010
//
// Copyright: See COPYING file that comes with this distribution
//
//******************************************************************

#ifndef SPATIALITEADAPTER_H
#define SPATIALITEADAPTER_H

#include "IMapAdapterFactory.h"
#include "IMapAdapter.h"

#include <QLocale>
#include <QByteArray>
#include <QHash>

/*
these headers are required in order to support
SQLite/SpatiaLite
*/
#include <spatialite/sqlite3.h>
#include <spatialite/gaiageo.h>
#include <spatialite.h>

#include "PaintStyle/PrimitivePainter.h"
class MasPaintStyle;

class SpatialiteAdapter : public IMapAdapter
{
    Q_OBJECT
    Q_INTERFACES(IMapAdapter)

public:
    SpatialiteAdapter();
    virtual ~SpatialiteAdapter();

    //! returns the unique identifier (Uuid) of this MapAdapter
    /*!
     * @return  the unique identifier (Uuid) of this MapAdapter
     */
    virtual QUuid	getId		() const;

    //! returns the type of this MapAdapter
    /*!
     * @return  the type of this MapAdapter
     */
    virtual IMapAdapter::Type	getType		() const;

    //! returns the name of this MapAdapter
    /*!
     * @return  the name of this MapAdapter
     */
    virtual QString	getName		() const;

    //! returns the host of this MapAdapter
    /*!
     * @return  the host of this MapAdapter
     */
    virtual QString	getHost		() const;

    //! returns the size of the tiles
    /*!
     * @return the size of the tiles
     */
    virtual int		getTileSizeW	() const { return -1; }
    virtual int		getTileSizeH	() const { return -1; }

    //! returns the min zoom value
    /*!
     * @return the min zoom value
     */
    virtual int 		getMinZoom	() const { return -1; }

    //! returns the max zoom value
    /*!
     * @return the max zoom value
     */
    virtual int		getMaxZoom	() const { return -1; }

    //! returns the current zoom
    /*!
     * @return the current zoom
     */
    virtual int 		getZoom		() const { return -1; }

    //! returns the source tag to be applied when drawing over this map
    /*!
     * @return the source tag
     */
    virtual QString	getSourceTag		() const { return ""; }

    //! returns the Url of the usage license
    /*!
     * @return the Url of the usage license
     */
    virtual QString	getLicenseUrl() const {return "";}


    virtual int		getAdaptedZoom() const { return -1; }
    virtual int 	getAdaptedMinZoom() const { return -1; }
    virtual int		getAdaptedMaxZoom() const { return -1; }

    virtual void	zoom_in() {}
    virtual void	zoom_out() {}

    virtual bool	isValid(int, int, int) const { return true; }
    virtual QString getQuery(int, int, int)  const { return ""; }
    virtual QString getQuery(const QRectF& , const QRectF& , const QRect& ) const { return ""; }
    virtual QPixmap getPixmap(const QRectF& wgs84Bbox, const QRectF& projBbox, const QRect& size) const ;

    virtual QString projection() const;
    virtual QRectF	getBoundingbox() const;

    virtual bool isTiled() const { return false; }
    virtual int getTilesWE(int) const { return -1; }
    virtual int getTilesNS(int) const { return -1; }

    virtual QMenu* getMenu() const;

    virtual IImageManager* getImageManager();
    virtual void setImageManager(IImageManager* anImageManager);

    virtual void cleanup();

    virtual bool toXML(QDomElement xParent);
    virtual void fromXML(const QDomElement xParent);
    virtual QString toPropertiesHtml();

    virtual void setSettings(QSettings* /*aSet*/) {}

public:
    void setFile(const QString& fn);
    void render(QPainter* P, const QRectF& fullbox, const QRectF& selbox, const QRect& src) const;

public slots:
    void onLoadFile();

protected:
    bool alreadyLoaded(QString fn) const;

private:
    QMenu* theMenu;
    bool m_loaded;

    QHash<QString, PrimitivePainter* > myStyles;
    QList<PrimitivePainter> thePrimitivePainters;

    QString m_dbName;
    sqlite3 *m_handle;
    sqlite3_stmt *m_highwayStmt;
};

class SpatialiteAdapterFactory : public QObject, public IMapAdapterFactory
{
    Q_OBJECT
    Q_INTERFACES(IMapAdapterFactory)

public:
    //! Creates an instance of the actual plugin
    /*!
     * @return  a pointer to the MapAdapter
     */
    IMapAdapter* CreateInstance() {return new SpatialiteAdapter(); }

    //! returns the unique identifier (Uuid) of this MapAdapter
    /*!
     * @return  the unique identifier (Uuid) of this MapAdapter
     */
    virtual QUuid	getId		() const;

    //! returns the name of this MapAdapter
    /*!
     * @return  the name of this MapAdapter
     */
    virtual QString	getName		() const;
};


#endif // NAVITADAPTER_H
