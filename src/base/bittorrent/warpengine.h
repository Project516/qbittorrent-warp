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

#pragma once

#include <QObject>

#include "base/path.h"

class QProcess;

namespace BitTorrent::Warp
{
    // Manages the embedded, userspace Cloudflare WARP tunnel so the application
    // is fully self-contained: no shell scripts, no root, no kernel interface
    // and no system changes. Everything lives in the portable profile directory.
    //
    // On first run it registers a free WARP account and generates a WireGuard
    // profile (bundled wgcf), then runs a userspace WireGuard -> SOCKS5 engine
    // (bundled wireproxy) on the local SOCKS5 endpoint that libtorrent is routed
    // through (see warpconfig.h). The helper binaries are embedded in the
    // executable and extracted into the profile directory on first run.
    class Engine final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Engine)

    public:
        // baseDir is the directory (inside the portable profile) where the WARP
        // engine keeps its helpers, account and configuration.
        explicit Engine(const Path &baseDir, QObject *parent = nullptr);
        ~Engine() override;

        // Extract helpers, ensure a WARP profile exists, and start the tunnel.
        void start();
        // Stop the tunnel process.
        void stop();

    private:
        bool extractHelper(const QString &resourcePath, const Path &dest);
        bool extractHelpers();
        bool ensureProfile();
        bool writeWireproxyConfig();
        void launchProxy();
        bool runBlocking(const Path &program, const QStringList &args, int timeoutMs);

        const Path m_baseDir;
        const Path m_binDir;
        const Path m_wgcf;
        const Path m_wireproxy;
        const Path m_accountConf;
        const Path m_profileConf;
        const Path m_wireproxyConf;

        QProcess *m_proxy = nullptr;
        int m_restartCount = 0;
        bool m_stopping = false;
    };
}
