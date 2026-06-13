/*
 * qBittorrent-WARP fork.
 * Self-contained Cloudflare WARP tunnel engine.
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
 */

#include "warpengine.h"

#include <chrono>

#include <QByteArray>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/utils/fs.h"
#include "warpconfig.h"

using namespace std::chrono_literals;

namespace
{
    const int MAX_RESTARTS = 5;

    // Pinned upstream engine helpers. They are downloaded from their official
    // GitHub releases on first run and verified against these SHA-256 sums before
    // they are ever executed. Bump the version, URL and checksums together when
    // updating; the checksums come from each release's published checksums.txt.
    //
    // wgcf 2.2.31 - MIT - https://github.com/ViRb3/wgcf
    const QString WGCF_URL =
        u"https://github.com/ViRb3/wgcf/releases/download/v2.2.31/wgcf_2.2.31_linux_amd64"_s;
    const QString WGCF_SHA256 =
        u"69147e1a517c66129edd8ac8cb60484d6c9515178d7b4a2f95e3c925f225572a"_s;

    // wireproxy 1.1.2 - ISC - https://github.com/pufferffish/wireproxy
    const QString WIREPROXY_URL =
        u"https://github.com/pufferffish/wireproxy/releases/download/v1.1.2/wireproxy_linux_amd64.tar.gz"_s;
    // SHA-256 of the downloaded .tar.gz (from the release's checksums.txt)...
    const QString WIREPROXY_ARCHIVE_SHA256 =
        u"b7dcff8f6e9d3410364e432aff24154eaa8db8206e0c6faac35d6c6ab06dac51"_s;
    // ...and of the single binary it contains, verified after extraction.
    const QString WIREPROXY_BINARY_SHA256 =
        u"b5a729f3606753ce4d4bfeb0f56d522e4aa0908aff8c7d55960fd4301cc58b11"_s;
}

namespace BitTorrent::Warp
{
    Engine::Engine(const Path &baseDir, QObject *parent)
        : QObject(parent)
        , m_baseDir(baseDir)
        , m_binDir(baseDir / Path(u"bin"_s))
        , m_wgcf(m_binDir / Path(u"wgcf"_s))
        , m_wireproxy(m_binDir / Path(u"wireproxy"_s))
        , m_accountConf(baseDir / Path(u"wgcf-account.toml"_s))
        , m_profileConf(baseDir / Path(u"wgcf-profile.conf"_s))
        , m_wireproxyConf(baseDir / Path(u"wireproxy.conf"_s))
    {
    }

    Engine::~Engine()
    {
        stop();
    }

    void Engine::start()
    {
        LogMsg(tr("[WARP] Starting Cloudflare WARP engine in \"%1\".").arg(m_baseDir.toString()), Log::INFO);

        Utils::Fs::mkpath(m_binDir);

        if (!ensureHelpers())
        {
            LogMsg(tr("[WARP] Could not obtain the verified WARP engine helpers. The kill switch will keep traffic blocked."), Log::CRITICAL);
            return;
        }

        if (!ensureProfile())
        {
            LogMsg(tr("[WARP] Could not obtain a Cloudflare WARP profile. The kill switch will keep traffic blocked."), Log::CRITICAL);
            return;
        }

        if (!writeWireproxyConfig())
        {
            LogMsg(tr("[WARP] Failed to write the WARP tunnel configuration."), Log::CRITICAL);
            return;
        }

        launchProxy();
    }

    void Engine::stop()
    {
        m_stopping = true;
        if (!m_proxy)
            return;

        if (m_proxy->state() != QProcess::NotRunning)
        {
            m_proxy->terminate();
            if (!m_proxy->waitForFinished(3000))
                m_proxy->kill();
            m_proxy->waitForFinished(2000);
        }
        delete m_proxy;
        m_proxy = nullptr;
    }

    QString Engine::fileSha256(const Path &file)
    {
        QFile f(file.toString());
        if (!f.open(QIODevice::ReadOnly))
            return {};

        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&f))
            return {};

        return QString::fromLatin1(hash.result().toHex());
    }

    bool Engine::downloadToFile(const QString &url, const Path &dest, const int timeoutMs)
    {
        QNetworkAccessManager nam;
        QNetworkRequest request {QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, u"qBittorrent-WARP"_s);

        QNetworkReply *reply = nam.get(request);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        bool timedOut = false;
        connect(&timer, &QTimer::timeout, &loop, [reply, &timedOut]()
        {
            timedOut = true;
            reply->abort();
        });
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();

        if (timedOut)
        {
            LogMsg(tr("[WARP] Timed out downloading %1.").arg(url), Log::CRITICAL);
            reply->deleteLater();
            return false;
        }
        if (reply->error() != QNetworkReply::NoError)
        {
            LogMsg(tr("[WARP] Download failed for %1: %2").arg(url, reply->errorString()), Log::CRITICAL);
            reply->deleteLater();
            return false;
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QSaveFile out(dest.toString());
        if (!out.open(QIODevice::WriteOnly))
        {
            LogMsg(tr("[WARP] Cannot write %1.").arg(dest.toString()), Log::CRITICAL);
            return false;
        }
        out.write(data);
        if (!out.commit())
        {
            LogMsg(tr("[WARP] Failed to save %1.").arg(dest.toString()), Log::CRITICAL);
            return false;
        }
        return true;
    }

    bool Engine::ensureHelper(const HelperSpec &spec)
    {
        // Reuse the binary from a previous run if it is present and still verifies.
        if (spec.dest.exists() && (fileSha256(spec.dest) == spec.binarySha256))
            return true;

        LogMsg(tr("[WARP] Fetching engine helper (first run only): %1").arg(spec.url), Log::INFO);

        const Path downloadPath {spec.dest.toString() + u".download"_s};
        if (!downloadToFile(spec.url, downloadPath, 180000))
            return false;

        const QString gotSha = fileSha256(downloadPath);
        if (gotSha != spec.downloadSha256)
        {
            LogMsg(tr("[WARP] Checksum mismatch for %1 (expected %2, got %3); refusing to use it.")
                .arg(spec.url, spec.downloadSha256, gotSha), Log::CRITICAL);
            Utils::Fs::removeFile(downloadPath);
            return false;
        }

        if (spec.archiveMember.isEmpty())
        {
            // The downloaded artifact is the binary itself.
            Utils::Fs::removeFile(spec.dest);
            if (!Utils::Fs::renameFile(downloadPath, spec.dest))
            {
                LogMsg(tr("[WARP] Failed to install %1.").arg(spec.dest.toString()), Log::CRITICAL);
                Utils::Fs::removeFile(downloadPath);
                return false;
            }
        }
        else
        {
            // Extract the single member from the verified tarball into the bin dir.
            Utils::Fs::removeFile(spec.dest);
            const bool extracted = runBlocking(Path(u"tar"_s),
                {u"-xzf"_s, downloadPath.toString(), u"-C"_s, m_binDir.toString(), spec.archiveMember}, 60000);
            Utils::Fs::removeFile(downloadPath);
            if (!extracted || !spec.dest.exists())
            {
                LogMsg(tr("[WARP] Failed to unpack %1.").arg(spec.url), Log::CRITICAL);
                return false;
            }
        }

        QFile::setPermissions(spec.dest.toString(),
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
            | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);

        // Final defence: verify the on-disk binary before it is ever executed.
        const QString binSha = fileSha256(spec.dest);
        if (binSha != spec.binarySha256)
        {
            LogMsg(tr("[WARP] Verification of %1 failed (expected %2, got %3); refusing to use it.")
                .arg(spec.dest.toString(), spec.binarySha256, binSha), Log::CRITICAL);
            Utils::Fs::removeFile(spec.dest);
            return false;
        }

        return true;
    }

    bool Engine::ensureHelpers()
    {
        const HelperSpec wgcf {m_wgcf, WGCF_URL, WGCF_SHA256, WGCF_SHA256, {}};
        const HelperSpec wireproxy {m_wireproxy, WIREPROXY_URL, WIREPROXY_ARCHIVE_SHA256,
            WIREPROXY_BINARY_SHA256, u"wireproxy"_s};

        return ensureHelper(wgcf) && ensureHelper(wireproxy);
    }

    bool Engine::runBlocking(const Path &program, const QStringList &args, const int timeoutMs)
    {
        QProcess proc;
        proc.setWorkingDirectory(m_baseDir.toString());
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(program.toString(), args);
        if (!proc.waitForStarted(5000))
        {
            LogMsg(tr("[WARP] Failed to start %1.").arg(program.toString()), Log::CRITICAL);
            return false;
        }
        if (!proc.waitForFinished(timeoutMs))
        {
            proc.kill();
            LogMsg(tr("[WARP] %1 timed out.").arg(program.toString()), Log::CRITICAL);
            return false;
        }
        const bool ok = (proc.exitStatus() == QProcess::NormalExit) && (proc.exitCode() == 0);
        if (!ok)
            LogMsg(tr("[WARP] %1 failed: %2").arg(program.toString(), QString::fromLocal8Bit(proc.readAll())), Log::WARNING);
        return ok;
    }

    bool Engine::ensureProfile()
    {
        if (m_profileConf.exists())
            return true;

        LogMsg(tr("[WARP] Registering a free Cloudflare WARP account (first run only)..."), Log::INFO);
        if (!m_accountConf.exists())
        {
            if (!runBlocking(m_wgcf, {u"register"_s, u"--accept-tos"_s}, 60000))
                return false;
        }
        if (!runBlocking(m_wgcf, {u"generate"_s}, 60000))
            return false;

        return m_profileConf.exists();
    }

    bool Engine::writeWireproxyConfig()
    {
        QFile profile(m_profileConf.toString());
        if (!profile.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QByteArray conf = profile.readAll();
        profile.close();

        conf.append(u"\n[Socks5]\nBindAddress = %1:%2\n"_s
            .arg(Warp::socksHost(), QString::number(Warp::socksPort())).toUtf8());

        QFile out(m_wireproxyConf.toString());
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return false;
        out.write(conf);
        out.close();
        out.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        return true;
    }

    void Engine::launchProxy()
    {
        m_proxy = new QProcess(this);
        m_proxy->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_proxy, &QProcess::finished, this, [this](const int code, QProcess::ExitStatus)
        {
            if (m_stopping)
                return;

            if (m_restartCount >= MAX_RESTARTS)
            {
                LogMsg(tr("[WARP] Tunnel engine keeps exiting (last code %1); giving up. The kill switch will keep traffic blocked.")
                    .arg(code), Log::CRITICAL);
                return;
            }

            ++m_restartCount;
            LogMsg(tr("[WARP] Tunnel engine exited (code %1); restarting (%2/%3)...")
                .arg(QString::number(code), QString::number(m_restartCount), QString::number(MAX_RESTARTS)), Log::WARNING);
            QTimer::singleShot(3s, this, [this]()
            {
                if (!m_stopping && m_proxy)
                    m_proxy->start(m_wireproxy.toString(), {u"-c"_s, m_wireproxyConf.toString()});
            });
        });

        m_proxy->start(m_wireproxy.toString(), {u"-c"_s, m_wireproxyConf.toString()});
        LogMsg(tr("[WARP] Userspace WARP tunnel engine started; SOCKS5 on %1:%2.")
            .arg(Warp::socksHost(), QString::number(Warp::socksPort())), Log::INFO);
    }
}
