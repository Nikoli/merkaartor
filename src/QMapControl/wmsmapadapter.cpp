/***************************************************************************
 *   Copyright (C) 2007 by Kai Winter   *
 *   kaiwinter@gmx.de   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "wmsmapadapter.h"

WMSMapAdapter::WMSMapAdapter(WmsServer aServer)
 : theServer(aServer)
{
    loc = QLocale(QLocale::English);
    loc.setNumberOptions(QLocale::OmitGroupSeparator);
}

QString	WMSMapAdapter::getName() const
{
    return theServer.WmsName;
}

QString	WMSMapAdapter::getHost() const
{
    return theServer.WmsAdress;
}

IImageManager* WMSMapAdapter::getImageManager()
{
    return theImageManager;
}

void WMSMapAdapter::setImageManager(IImageManager* anImageManager)
{
    theImageManager = anImageManager;
}

QString WMSMapAdapter::projection() const
{
    if (theServer.WmsProjections == "OSGEO:41001")
        return "EPSG:3857";
    return theServer.WmsProjections;
}

WMSMapAdapter::~WMSMapAdapter()
{
}

QUuid WMSMapAdapter::getId() const
{
    return QUuid("{E238750A-AC27-429e-995C-A60C17B9A1E0}");
}

IMapAdapter::Type WMSMapAdapter::getType() const
{
    if (theServer.WmsImgFormat.startsWith("image"))
        return IMapAdapter::NetworkBackground;
    else
        return IMapAdapter::NetworkDataBackground;
}

QString WMSMapAdapter::getQuery(const QRectF& /*wgs84Bbox*/, const QRectF& projBbox, const QRect& size) const
{
    QUrl theUrl(theServer.WmsPath);
    if (!theUrl.hasQueryItem("VERSION"))
        theUrl.addQueryItem("VERSION", "1.1.1");
    if (!theUrl.hasQueryItem("SERVICE"))
        theUrl.addQueryItem("SERVICE", "WMS");
    theUrl.addQueryItem("REQUEST", "GetMap");

    if (!theUrl.hasQueryItem("TRANSPARENT"))
        theUrl.addQueryItem("TRANSPARENT", "TRUE");
    if (!theUrl.hasQueryItem("LAYERS"))
        theUrl.addQueryItem("LAYERS", theServer.WmsLayers);
    if (!theUrl.hasQueryItem("SRS"))
        theUrl.addQueryItem("SRS", theServer.WmsProjections);
    if (!theUrl.hasQueryItem("STYLES"))
        theUrl.addQueryItem("STYLES", theServer.WmsStyles);
    if (!theUrl.hasQueryItem("FORMAT"))
        theUrl.addQueryItem("FORMAT", theServer.WmsImgFormat);
    theUrl.addQueryItem("WIDTH", QString::number(size.width()));
    theUrl.addQueryItem("HEIGHT", QString::number(size.height()));
    theUrl.addQueryItem("BBOX", loc.toString(projBbox.bottomLeft().x(),'f',6).append(",")
            .append(loc.toString(projBbox.bottomLeft().y(),'f',6)).append(",")
            .append(loc.toString(projBbox.topRight().x(),'f',6)).append(",")
            .append(loc.toString(projBbox.topRight().y(),'f',6))
            );

    return theUrl.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority);
}

QString WMSMapAdapter::getSourceTag() const
{
    return theServer.WmsSourceTag;
}

QString WMSMapAdapter::getLicenseUrl() const
{
    return theServer.WmsLicenseUrl;
}
