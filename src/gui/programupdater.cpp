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

<<<<<<< HEAD
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
=======
namespace
{
    bool isVersionMoreRecent(const ProgramUpdater::Version &remoteVersion)
    {
        if (!remoteVersion.isValid())
            return false;

        const ProgramUpdater::Version currentVersion {QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD};
        if (remoteVersion == currentVersion)
        {
            const bool isDevVersion = QStringLiteral(QBT_VERSION_STATUS).contains(
                QRegularExpression(u"(alpha|beta|rc)"_s));
            if (isDevVersion)
                return true;
        }
        return (remoteVersion > currentVersion);
    }
}

void ProgramUpdater::checkForUpdates()
{
    // Don't change this User-Agent. In case our updater goes haywire,
    // the filehost can identify it and contact us.
    const auto USER_AGENT = QStringLiteral("qBittorrent/" QBT_VERSION_2 " ProgramUpdater (www.qbittorrent.org)");
    const auto QBT_MAIN_URL = u"https://www.qbittorrent.org/versions.json"_s;
    const auto QBT_BACKUP_URL = u"https://qbittorrent.github.io/qBittorrent-website/versions.json"_s;

    Net::DownloadManager *netManager = Net::DownloadManager::instance();
    const bool useProxy = Preferences::instance()->useProxyForGeneralPurposes();

    m_pendingRequestCount = 2;
    // don't use the custom user agent for the following requests, disguise as a normal browser instead
    netManager->download(Net::DownloadRequest(QBT_MAIN_URL), useProxy, this, [this](const Net::DownloadResult &result)
    {
        fallbackDownloadFinished(result, m_qbtMainVersion);
    });
    netManager->download(Net::DownloadRequest(QBT_BACKUP_URL), useProxy, this, [this](const Net::DownloadResult &result)
    {
        fallbackDownloadFinished(result, m_qbtBackupVersion);
    });
>>>>>>> 05fcaa8f505f0657d5cd7b7c377282b63855e8ae
}

ProgramUpdater::Version ProgramUpdater::getNewVersion() const
{
<<<<<<< HEAD
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
=======
    switch (getLatestRemoteSource())
    {
    case RemoteSource::QbtMain:
        return m_qbtMainVersion;
    case RemoteSource::QbtBackup:
        return m_qbtBackupVersion;
    }
    Q_UNREACHABLE();
}

void ProgramUpdater::fallbackDownloadFinished(const Net::DownloadResult &result, Version &version)
{
    version = {};

    if (result.status != Net::DownloadStatus::Success)
    {
        LogMsg(tr("Failed to download the program update info. URL: \"%1\". Error: \"%2\"").arg(result.url, result.errorString) , Log::WARNING);
        handleFinishedRequest();
        return;
    }

    const auto json = QJsonDocument::fromJson(result.data);

#if defined(Q_OS_MACOS)
    const QString platformKey = u"macos"_s;
#elif defined(Q_OS_WIN)
    const QString platformKey = u"win"_s;
#endif

    if (const QJsonValue verJSON = json[platformKey][u"version"_s]; verJSON.isString())
    {
        const Version tmpVer {verJSON.toString()};
        if (isVersionMoreRecent(tmpVer))
            version = tmpVer;
    }

    handleFinishedRequest();
>>>>>>> 05fcaa8f505f0657d5cd7b7c377282b63855e8ae
}

bool ProgramUpdater::updateProgram() const
{
<<<<<<< HEAD
    return QDesktopServices::openUrl(m_updateURL);
=======
    switch (getLatestRemoteSource())
    {
    case RemoteSource::QbtMain:
        return QDesktopServices::openUrl(u"https://www.qbittorrent.org/download"_s);
    case RemoteSource::QbtBackup:
        return QDesktopServices::openUrl(u"https://qbittorrent.github.io/qBittorrent-website/download"_s);
    }
    Q_UNREACHABLE();
}

void ProgramUpdater::handleFinishedRequest()
{
    --m_pendingRequestCount;
    if (m_pendingRequestCount == 0)
        emit updateCheckFinished();
}

ProgramUpdater::RemoteSource ProgramUpdater::getLatestRemoteSource() const
{
    const Version max = std::max({m_qbtMainVersion, m_qbtBackupVersion});
    if (max == m_qbtMainVersion)
        return RemoteSource::QbtMain;
    if (max == m_qbtBackupVersion)
        return RemoteSource::QbtBackup;
    Q_UNREACHABLE();
>>>>>>> 05fcaa8f505f0657d5cd7b7c377282b63855e8ae
}
