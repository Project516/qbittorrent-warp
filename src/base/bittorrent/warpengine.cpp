/*
 * qBittorrent-WARP fork.
 * Embedded, self-contained Cloudflare WARP tunnel engine.
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
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include "base/global.h"
#include "base/logger.h"
#include "base/utils/fs.h"
#include "warpconfig.h"

using namespace std::chrono_literals;

// Qt resources compiled into a static library are not auto-registered unless
// something references them. Force initialization explicitly. This helper must
// live in the global namespace so Q_INIT_RESOURCE resolves the generated symbol.
static void initWarpResources()
{
    Q_INIT_RESOURCE(warpengine);
}

namespace
{
    const int MAX_RESTARTS = 5;
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
        initWarpResources();

        LogMsg(tr("[WARP] Starting embedded Cloudflare WARP engine in \"%1\".").arg(m_baseDir.toString()), Log::INFO);

        Utils::Fs::mkpath(m_binDir);

        if (!extractHelpers())
        {
            LogMsg(tr("[WARP] Failed to unpack the bundled WARP engine. The kill switch will keep traffic blocked."), Log::CRITICAL);
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

    bool Engine::extractHelper(const QString &resourcePath, const Path &dest)
    {
        QFile res(resourcePath);
        if (!res.open(QIODevice::ReadOnly))
        {
            LogMsg(tr("[WARP] Missing embedded resource: %1").arg(resourcePath), Log::CRITICAL);
            return false;
        }
        const QByteArray data = res.readAll();
        res.close();

        // Skip rewriting if the on-disk copy already matches.
        if (dest.exists() && (QFile(dest.toString()).size() == data.size()))
            return true;

        QFile out(dest.toString());
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            LogMsg(tr("[WARP] Cannot write helper: %1").arg(dest.toString()), Log::CRITICAL);
            return false;
        }
        out.write(data);
        out.close();
        out.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
            | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
        return true;
    }

    bool Engine::extractHelpers()
    {
        return extractHelper(u":/warp/wgcf"_s, m_wgcf)
            && extractHelper(u":/warp/wireproxy"_s, m_wireproxy);
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
