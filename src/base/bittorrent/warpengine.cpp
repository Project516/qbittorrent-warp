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
#include <optional>

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
#include <QSysInfo>
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

    // Pinned upstream engine helpers, kept per CPU architecture. They are
    // downloaded from their official GitHub releases on first run and verified
    // against these SHA-256 sums before they are ever executed. The matching
    // asset is chosen at runtime from QSysInfo::currentCpuArchitecture(): the
    // x86_64 desktop build and the arm64 build (used by the headless Raspberry
    // Pi release) each fetch their own binary. Bump the version, URLs and
    // checksums together when updating; the sums come from each release's
    // published checksums.txt.
    //
    // wgcf 2.2.31 - MIT - https://github.com/ViRb3/wgcf
    const QString WGCF_URL_AMD64 =
        u"https://github.com/ViRb3/wgcf/releases/download/v2.2.31/wgcf_2.2.31_linux_amd64"_s;
    const QString WGCF_SHA256_AMD64 =
        u"69147e1a517c66129edd8ac8cb60484d6c9515178d7b4a2f95e3c925f225572a"_s;
    const QString WGCF_URL_ARM64 =
        u"https://github.com/ViRb3/wgcf/releases/download/v2.2.31/wgcf_2.2.31_linux_arm64"_s;
    const QString WGCF_SHA256_ARM64 =
        u"b9bdbdeaa3f9f4ba741ba55b8bd94c24f7166c27668eb7e8192ccf9746961182"_s;

    // wireproxy 1.1.3 - ISC - https://github.com/pufferffish/wireproxy
    // For each architecture, the SHA-256 of the downloaded .tar.gz (verified
    // before unpacking) and of the single binary it holds (verified after
    // extraction, before execution).
    const QString WIREPROXY_URL_AMD64 =
        u"https://github.com/pufferffish/wireproxy/releases/download/v1.1.3/wireproxy_linux_amd64.tar.gz"_s;
    const QString WIREPROXY_ARCHIVE_SHA256_AMD64 =
        u"e88c1d090740373fc606c1bafd81d9a5eadc642cce5667616e20e9d7a444f51c"_s;
    const QString WIREPROXY_BINARY_SHA256_AMD64 =
        u"70ae5e52223dac7974af8d98a321f14a0e1689d2b14655ebc8dadfa1ec69466d"_s;
    const QString WIREPROXY_URL_ARM64 =
        u"https://github.com/pufferffish/wireproxy/releases/download/v1.1.3/wireproxy_linux_arm64.tar.gz"_s;
    const QString WIREPROXY_ARCHIVE_SHA256_ARM64 =
        u"370e00bd2167960d1ecd1c3c1439715bbaa94a0a110a2040468670c9af6021b6"_s;
    const QString WIREPROXY_BINARY_SHA256_ARM64 =
        u"5852e32671afb8918c39c59330b85f833c187ed41b6b1f683c90b6bfd320f3fa"_s;

    // The pinned helper set for one CPU architecture.
    struct HelperPins
    {
        QString wgcfUrl;
        QString wgcfSha256;
        QString wireproxyUrl;
        QString wireproxyArchiveSha256;
        QString wireproxyBinarySha256;
    };

    // Returns the helper pins for the architecture the application is running on,
    // or nullopt on an architecture the fork ships no helpers for, so the kill
    // switch keeps traffic blocked rather than running an unverified tool.
    std::optional<HelperPins> pinsForCurrentArch()
    {
        const QString arch = QSysInfo::currentCpuArchitecture();
        if (arch == u"x86_64"_s)
            return HelperPins {WGCF_URL_AMD64, WGCF_SHA256_AMD64, WIREPROXY_URL_AMD64,
                WIREPROXY_ARCHIVE_SHA256_AMD64, WIREPROXY_BINARY_SHA256_AMD64};
        if (arch == u"arm64"_s)
            return HelperPins {WGCF_URL_ARM64, WGCF_SHA256_ARM64, WIREPROXY_URL_ARM64,
                WIREPROXY_ARCHIVE_SHA256_ARM64, WIREPROXY_BINARY_SHA256_ARM64};
        return std::nullopt;
    }
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
        QNetworkAccessManager manager;
        QNetworkRequest request {QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, u"qBittorrent-WARP"_s);

        QNetworkReply *reply = manager.get(request);

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
        const std::optional<HelperPins> pins = pinsForCurrentArch();
        if (!pins)
        {
            LogMsg(tr("[WARP] No verified WARP engine helpers are pinned for this CPU architecture (%1).")
                .arg(QSysInfo::currentCpuArchitecture()), Log::CRITICAL);
            return false;
        }

        const HelperSpec wgcf {m_wgcf, pins->wgcfUrl, pins->wgcfSha256, pins->wgcfSha256, {}};
        const HelperSpec wireproxy {m_wireproxy, pins->wireproxyUrl, pins->wireproxyArchiveSha256,
            pins->wireproxyBinarySha256, u"wireproxy"_s};

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
        // A profile left behind by an interrupted first run can exist but be
        // empty or truncated. Feeding that to wireproxy yields a tunnel that
        // never comes up, which the kill switch then turns into permanently
        // blocked traffic. Treat anything without a WireGuard [Interface] section
        // as missing so it is regenerated cleanly.
        if (m_profileConf.exists())
        {
            QFile profile(m_profileConf.toString());
            if (profile.open(QIODevice::ReadOnly | QIODevice::Text) && profile.readAll().contains("[Interface]"))
                return true;

            LogMsg(tr("[WARP] The stored WARP profile is missing or invalid; regenerating it."), Log::WARNING);
            Utils::Fs::removeFile(m_profileConf);
        }

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

        // Write atomically: a crash mid-write must not leave wireproxy a
        // truncated config to choke on.
        QSaveFile out(m_wireproxyConf.toString());
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return false;
        out.write(conf);
        if (!out.commit())
            return false;
        QFile::setPermissions(m_wireproxyConf.toString(), QFile::ReadOwner | QFile::WriteOwner);
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

            // A tunnel that ran healthily for a while before dying is a transient
            // blip, not a crash loop, so refill the restart budget. Without this
            // the few lifetime restarts would eventually be exhausted by the odd
            // hiccup and the kill switch would then block traffic permanently.
            if (m_proxyUptime.isValid() && (m_proxyUptime.elapsed() > 60000))
                m_restartCount = 0;

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
                {
                    m_proxyUptime.restart();
                    m_proxy->start(m_wireproxy.toString(), {u"-c"_s, m_wireproxyConf.toString()});
                }
            });
        });

        m_proxyUptime.start();
        m_proxy->start(m_wireproxy.toString(), {u"-c"_s, m_wireproxyConf.toString()});
        LogMsg(tr("[WARP] Userspace WARP tunnel engine started; SOCKS5 on %1:%2.")
            .arg(Warp::socksHost(), QString::number(Warp::socksPort())), Log::INFO);
    }
}
