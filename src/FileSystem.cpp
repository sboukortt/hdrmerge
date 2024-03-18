/*
 *  HDRMerge - HDR exposure merging software.
 *  Copyright 2018 Jean-Christophe FRISCH
 *  natureh.510@gmail.com
 *
 *  This file is part of HDRMerge.
 *
 *  HDRMerge is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  HDRMerge is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with HDRMerge. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "FileSystem.hpp"
#include <QString>
#include <QStandardPaths>
#include <QStorageInfo>

namespace hdrmerge {

QList<QUrl> getStdUrls(const QString additionalPath)
{

    QList<QUrl> urls;
    QStringList stdLocations;
    if (!additionalPath.isEmpty()) {
        urls.append(QUrl::fromLocalFile(additionalPath));
    }

    stdLocations = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::HomeLocation);
    for (auto & i : stdLocations) {
        urls.append(QUrl::fromLocalFile(i));
    }
    stdLocations = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::DesktopLocation);
    for (auto & i : stdLocations) {
        urls.append(QUrl::fromLocalFile(i));
    }
    stdLocations = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::DocumentsLocation);
    for (auto & i : stdLocations) {
        urls.append(QUrl::fromLocalFile(i));
    }
    stdLocations = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::PicturesLocation);
    for (auto & i : stdLocations) {
        urls.append(QUrl::fromLocalFile(i));
    }

    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (auto & volume : volumes) {
        if (volume.isValid() && volume.isReady() && !volume.isReadOnly()) {
            urls.append(QUrl::fromLocalFile(volume.rootPath()));
        }
    }

    return urls;
}

}
