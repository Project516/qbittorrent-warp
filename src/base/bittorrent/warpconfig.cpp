/*
 * qBittorrent-WARP fork.
 * Routes all BitTorrent traffic through Cloudflare WARP.
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

#include "warpconfig.h"

#include <QNetworkInterface>
#include <QTcpSocket>

#include "base/global.h"

namespace
{
    const QString DEFAULT_INTERFACE = u"warp"_s;
    const QString DEFAULT_SOCKS_HOST = u"127.0.0.1"_s;
    const int DEFAULT_SOCKS_PORT = 40000;

    QString envOr(const char *name, const QString &fallback)
    {
        const QString value = qEnvironmentVariable(name).trimmed();
        return value.isEmpty() ? fallback : value;
    }
}

namespace BitTorrent::Warp
{
    bool isEnforced()
    {
        // Enforcement is on unless explicitly disabled for a dev/test build.
        return qEnvironmentVariable("QBT_WARP_DISABLE") != u"1"_s;
    }

    QString interfaceName()
    {
        return envOr("QBT_WARP_INTERFACE", DEFAULT_INTERFACE);
    }

    QString socksHost()
    {
        return envOr("QBT_WARP_SOCKS_HOST", DEFAULT_SOCKS_HOST);
    }

    int socksPort()
    {
        bool ok = false;
        const int port = qEnvironmentVariable("QBT_WARP_SOCKS_PORT").toInt(&ok);
        return (ok && (port > 0) && (port <= 65535)) ? port : DEFAULT_SOCKS_PORT;
    }

    Mode mode()
    {
        const QString value = qEnvironmentVariable("QBT_WARP_MODE").trimmed().toLower();
        if (value == u"interface"_s)
            return Mode::Interface;
        if (value == u"both"_s)
            return Mode::Both;
        // Default to SOCKS5: it is the only egress path the self-contained build
        // has, and it fails closed when the tunnel is down (see useSocks()).
        return Mode::Socks5;
    }

    bool killSwitchEnabled()
    {
        return qEnvironmentVariable("QBT_WARP_KILLSWITCH") != u"0"_s;
    }

    bool isInterfaceUp()
    {
        const QNetworkInterface iface = QNetworkInterface::interfaceFromName(interfaceName());
        if (!iface.isValid())
            return false;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            return false;
        // A WireGuard interface that is up but has not been assigned an address
        // is not usable yet.
        return !iface.addressEntries().isEmpty();
    }

    bool isSocksReachable()
    {
        QTcpSocket socket;
        socket.connectToHost(socksHost(), static_cast<quint16>(socksPort()));
        const bool connected = socket.waitForConnected(300);
        socket.abort();
        return connected;
    }

    bool useSocks()
    {
        switch (mode())
        {
        case Mode::Socks5:
            // The SOCKS5 proxy is the committed egress path: route through it even
            // while it is (re)starting or down, so libtorrent fails closed on an
            // unreachable proxy instead of being handed proxy_type=none and going
            // out directly.
            return true;
        case Mode::Both:
            // Prefer the proxy when reachable; otherwise fall back to binding the
            // WARP interface (see applyWarpProxy / applyNetworkInterfacesSettings).
            return isSocksReachable();
        case Mode::Interface:
        default:
            return false;
        }
    }

    bool isHealthy()
    {
        switch (mode())
        {
        case Mode::Socks5:
            return isSocksReachable();
        case Mode::Interface:
            return isInterfaceUp();
        case Mode::Both:
        default:
            // Healthy if at least one WARP path is currently usable.
            return isInterfaceUp() || isSocksReachable();
        }
    }

    QString describe()
    {
        QString modeStr;
        switch (mode())
        {
        case Mode::Interface: modeStr = u"interface"_s; break;
        case Mode::Socks5:    modeStr = u"socks5"_s; break;
        case Mode::Both:      modeStr = u"both"_s; break;
        }

        return u"mode=%1 iface=%2(%3) socks=%4:%5(%6) killswitch=%7"_s
            .arg(modeStr
                , interfaceName()
                , isInterfaceUp() ? u"up"_s : u"down"_s
                , socksHost()
                , QString::number(socksPort())
                , isSocksReachable() ? u"reachable"_s : u"unreachable"_s
                , killSwitchEnabled() ? u"on"_s : u"off"_s);
    }
}
