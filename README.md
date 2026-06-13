qBittorrent-WARP - qBittorrent routed through Cloudflare WARP
-------------------------------------------------------------

### Description:
qBittorrent-WARP is a fork of qBittorrent that forces all BitTorrent traffic
through Cloudflare WARP. The peer, tracker (HTTP and UDP), DHT and uTP sockets
are bound to the WARP tunnel, and a kill switch pauses all transfers whenever the
tunnel is unavailable.

This fork exists for privacy. It is not intended for, and does not condone,
copyright infringement.

It is based on qBittorrent 5.2.1 (upstream tag `release-5.2.1`).

### What the fork changes:
Routing is locked to WARP and cannot be overridden from the GUI, the Web UI or
the configuration file while enforcement is active:

* The BitTorrent listen and outgoing sockets are bound to the WARP WireGuard
  interface (default name `warp`). If that interface is down, libtorrent does
  not fall back to another adapter, so traffic is not leaked.
* When SOCKS5 is in use, libtorrent is pointed at the WARP SOCKS5 endpoint
  (default `127.0.0.1:40000`) with remote hostname resolution enabled, so DNS
  lookups also go through the tunnel.
* UPnP, NAT-PMP and Local Service Discovery are disabled so the client does not
  contact the local router or announce activity on the LAN.
* A watchdog pauses the session when the tunnel goes down and resumes it when
  the tunnel returns.

The behaviour is selected at runtime with environment variables (see Usage). The
fork-specific code is in `src/base/bittorrent/warpconfig.{h,cpp}` and the hooks
in `src/base/bittorrent/sessionimpl.cpp`.

### Limitations:
* WARP does not support inbound connections or port forwarding. The client is
  connect-only: it can reach peers that accept incoming connections, but remote
  peers cannot connect to it. Transfers still work, with a smaller peer set.
* In interface mode, libtorrent's hostname resolver is not bound to the tunnel.
  Use SOCKS5 mode, or run inside the supplied network namespace, to prevent DNS
  leaks.
* Running BitTorrent over the free WARP tier may conflict with Cloudflare's
  terms of service.
* WARP hides your address from peers and trackers and your traffic from the
  local network and ISP. It is not an anonymity network; Cloudflare can see your
  traffic at its edge.
* The WARP provisioning and isolation scripts are Linux-only. The in-client
  enforcement itself is cross-platform.

### Installation:
The build dependencies are the same as upstream qBittorrent (Qt 6, libtorrent
2.0, Boost, OpenSSL, zlib) plus CMake and Ninja. Refer to the [INSTALL](INSTALL)
file for the full list and platform notes.

Fedora:

    sudo dnf install -y gcc-c++ cmake ninja-build openssl-devel zlib-devel \
        zlib-ng-compat-static boost-devel qt6-qtbase-devel qt6-qtbase-private-devel \
        qt6-qttools-devel qt6-qtsvg-devel rb_libtorrent-devel

Debian / Ubuntu:

    sudo apt install -y build-essential cmake ninja-build libssl-dev zlib1g-dev \
        libboost-dev qt6-base-dev qt6-base-private-dev qt6-tools-dev libqt6svg6-dev \
        libtorrent-rasterbar-dev

Configure and build:

    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Add `-DGUI=OFF` to build `qbittorrent-nox` (headless, Web UI only).

### Usage:
WARP must be provisioned first. The scripts in `warp/` use
[wgcf](https://github.com/ViRb3/wgcf) to create a free WARP WireGuard profile
and, optionally, a SOCKS5 endpoint:

    cd warp
    ./warp-provision.sh            # generate warp/warp.conf
    ./warp-provision.sh --socks    # also expose SOCKS5 on 127.0.0.1:40000

For leak-proof isolation, run the client inside the WARP network namespace. The
namespace has no route other than WARP, and its resolver is set to Cloudflare,
so nothing (including DNS) can leave the tunnel:

    sudo ./qbt-warp-netns.sh       # qbittorrent-nox; Web UI at http://10.213.213.2:8080
    sudo ./qbt-warp-netns.sh --gui # GUI client

Alternatively, run the client directly and select the enforcement mode:

    QBT_WARP_MODE=both      qbittorrent   # SOCKS5 when reachable, else interface (default)
    QBT_WARP_MODE=socks5    qbittorrent
    QBT_WARP_MODE=interface qbittorrent

Environment variables:

    QBT_WARP_MODE         interface | socks5 | both    (default: both)
    QBT_WARP_INTERFACE    WARP interface name           (default: warp)
    QBT_WARP_SOCKS_HOST   SOCKS5 host                   (default: 127.0.0.1)
    QBT_WARP_SOCKS_PORT   SOCKS5 port                   (default: 40000)
    QBT_WARP_KILLSWITCH   0 to disable the kill switch  (default: 1)
    QBT_WARP_DISABLE      1 to disable enforcement entirely (development only)

On start-up the log shows a line beginning with `[WARP]` describing the active
configuration.

### Updating to a new qBittorrent release:
The fork is a small set of changes applied on top of an upstream release tag. To
move it onto a newer release, run:

    warp/update-from-upstream.sh                 # rebase onto the latest release-* tag
    warp/update-from-upstream.sh release-5.3.0   # or onto a specific tag

Conflicts, if any, are normally confined to `src/base/bittorrent/sessionimpl.cpp`.
Resolve them, finish the rebase, then rebuild.

### Misc:
This is an unofficial fork and is not affiliated with or endorsed by the
qBittorrent project. Report problems with the fork to this repository, not to
the upstream qBittorrent trackers.

Upstream project: https://www.qbittorrent.org
Upstream wiki:    https://wiki.qbittorrent.org
