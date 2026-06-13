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

#pragma once

#include <QString>

// WARP routing enforcement for the qBittorrent-WARP fork.
//
// Every libtorrent socket (peers, HTTP/UDP tracker announces, DHT, uTP) is
// forced through Cloudflare WARP. Two complementary mechanisms are supported,
// selectable via the WARP mode:
//
//   * Interface bind - libtorrent's listen_interfaces / outgoing_interfaces are
//     pinned to the WARP WireGuard interface (default "warp"). When the
//     interface is down, libtorrent fails closed and never falls back to a
//     different NIC, so there is no leak.
//
//   * SOCKS5 proxy   - libtorrent is pointed at the SOCKS5 endpoint exposed by
//     WARP (warp-cli proxy mode or wireproxy, default 127.0.0.1:40000) with
//     proxy_hostnames enabled so even DNS for trackers resolves through WARP.
//
// Note: binding egress to the WARP interface and connecting to a loopback SOCKS5
// proxy are mutually exclusive on the same socket (binding the local endpoint to
// the WARP IP breaks the connection to 127.0.0.1). Therefore in "both" mode the
// SOCKS5 proxy is preferred as the egress path when reachable (it is also the
// DNS-safe path), and interface binding acts as the automatic fallback. The
// listener is always pinned to the WARP interface and a kill switch pauses all
// traffic whenever WARP is unavailable.
namespace BitTorrent::Warp
{
    enum class Mode
    {
        Interface,  // bind libtorrent to the WARP WireGuard interface only
        Socks5,     // route libtorrent through the WARP SOCKS5 proxy only
        Both        // prefer SOCKS5 when reachable, fall back to interface bind
    };

    // Whether WARP enforcement is active. Defaults to true. It can only be
    // disabled for development/testing builds by setting QBT_WARP_DISABLE=1 in
    // the environment; normal builds always enforce.
    bool isEnforced();

    // Name of the WARP WireGuard interface to bind to. Default "warp".
    // Override with env QBT_WARP_INTERFACE.
    QString interfaceName();

    // SOCKS5 endpoint exposed by WARP. Default host 127.0.0.1, port 40000.
    // Override with env QBT_WARP_SOCKS_HOST / QBT_WARP_SOCKS_PORT.
    QString socksHost();
    int socksPort();

    // Enforcement mode. Default Both. Override with
    // env QBT_WARP_MODE = interface | socks5 | both.
    Mode mode();

    // Kill switch: block all traffic while WARP is unavailable. Default true.
    // Override (disable) with env QBT_WARP_KILLSWITCH=0.
    bool killSwitchEnabled();

    // Runtime health checks.
    bool isInterfaceUp();    // the WARP interface exists, is up and has an address
    bool isSocksReachable(); // a TCP connection to the SOCKS5 endpoint succeeds
    bool useSocks();         // mode includes SOCKS5 and the endpoint is reachable
    bool isHealthy();        // at least one configured WARP path is currently usable

    // One-line, human-readable summary of the current WARP configuration/state.
    QString describe();
}
