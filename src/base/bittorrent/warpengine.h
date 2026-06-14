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

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include "base/path.h"

class QProcess;

namespace BitTorrent::Warp
{
    // Manages the userspace Cloudflare WARP tunnel so the application is fully
    // self-contained: no shell scripts, no root, no kernel interface and no
    // system changes. Everything lives in the portable profile directory.
    //
    // The tunnel relies on two upstream tools: wgcf (registers a free WARP
    // account and generates a WireGuard profile) and wireproxy (a userspace
    // WireGuard -> SOCKS5 proxy). Rather than ship opaque prebuilt binaries, the
    // engine downloads them on first run from their official GitHub releases at
    // pinned versions and verifies each one against a hard-coded SHA-256 before
    // it is ever executed. The downloads land in the portable profile directory
    // and are reused on subsequent runs.
    //
    // On first run it then registers a free WARP account and generates a
    // WireGuard profile (wgcf), and runs the userspace WireGuard -> SOCKS5 engine
    // (wireproxy) on the local SOCKS5 endpoint that libtorrent is routed through
    // (see warpconfig.h).
    class Engine final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Engine)

    public:
        // baseDir is the directory (inside the portable profile) where the WARP
        // engine keeps its helpers, account and configuration.
        explicit Engine(const Path &baseDir, QObject *parent = nullptr);
        ~Engine() override;

        // Fetch and verify helpers, ensure a WARP profile exists, and start the tunnel.
        void start();
        // Stop the tunnel process.
        void stop();

    private:
        // A pinned upstream helper to fetch and verify before use.
        struct HelperSpec
        {
            Path dest;              // final on-disk binary inside the profile
            QString url;            // upstream release download URL
            QString downloadSha256; // SHA-256 of the downloaded artifact (verified before unpacking)
            QString binarySha256;   // SHA-256 of the final binary on disk (verified before execution)
            QString archiveMember;  // empty: the artifact is the binary; otherwise the member to extract from the .tar.gz
        };

        bool ensureHelpers();
        bool ensureHelper(const HelperSpec &spec);
        bool downloadToFile(const QString &url, const Path &dest, int timeoutMs);
        static QString fileSha256(const Path &file);
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
        QElapsedTimer m_proxyUptime;
        int m_restartCount = 0;
        bool m_stopping = false;
    };
}
