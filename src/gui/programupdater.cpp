/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Mike Tzou (Chocobo1)
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "programupdater.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/version.h"

// qBittorrent-WARP: this fork publishes its own builds as GitHub releases tagged
// vX.Y.Z.W (W being the fork patch number). The update check asks the GitHub API
// for the latest stable release of the fork and compares it with the running
// version, instead of querying upstream qBittorrent's release feeds. The request
// uses general-purpose networking (not the WARP-routed BitTorrent path).

void ProgramUpdater::checkForUpdates()
{
    const auto USER_AGENT = QStringLiteral("qBittorrent-WARP/" QBT_VERSION_2 " ProgramUpdater");
    // "releases/latest" returns the most recent non-prerelease, non-draft release.
    const auto RELEASES_API_URL = u"https://api.github.com/repos/Project516/qbittorrent-warp/releases/latest"_s;

    Net::DownloadManager *netManager = Net::DownloadManager::instance();
    const bool useProxy = Preferences::instance()->useProxyForGeneralPurposes();
    netManager->download(Net::DownloadRequest(RELEASES_API_URL).userAgent(USER_AGENT)
        , useProxy, this, &ProgramUpdater::downloadFinished);
}

ProgramUpdater::Version ProgramUpdater::getNewVersion() const
{
    return m_remoteVersion;
}

void ProgramUpdater::downloadFinished(const Net::DownloadResult &result)
{
    if (result.status != Net::DownloadStatus::Success)
    {
        LogMsg(tr("Failed to download the program update info. URL: \"%1\". Error: \"%2\"")
            .arg(result.url, result.errorString), Log::WARNING);
        emit updateCheckFinished();
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson(result.data).object();

    QString tagName = release.value(u"tag_name"_s).toString();
    if (tagName.startsWith(u'v') || tagName.startsWith(u'V'))
        tagName.remove(0, 1);

    const Version remoteVersion {tagName};
    const Version currentVersion {QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD};
    if (remoteVersion.isValid() && (remoteVersion > currentVersion))
    {
        m_remoteVersion = remoteVersion;
        const QString htmlURL = release.value(u"html_url"_s).toString();
        m_updateURL = !htmlURL.isEmpty()
            ? QUrl(htmlURL)
            : QUrl(u"https://github.com/Project516/qbittorrent-warp/releases/latest"_s);
    }

    emit updateCheckFinished();
}

bool ProgramUpdater::updateProgram() const
{
    return QDesktopServices::openUrl(m_updateURL);
}
