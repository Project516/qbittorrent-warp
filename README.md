qBittorrent-WARP - qBittorrent routed through Cloudflare WARP
-------------------------------------------------------------

### Description:
qBittorrent-WARP is a fork of qBittorrent that forces all BitTorrent traffic
through Cloudflare WARP. The peer, tracker (HTTP and UDP), DHT and uTP sockets
are routed through the WARP tunnel, and a kill switch pauses all transfers
whenever the tunnel is unavailable.

It is self-contained: a single binary with no setup. On first run it registers a
free WARP account and starts a bundled userspace WireGuard tunnel by itself. No
shell scripts, no root, no kernel interface and no system changes are required.
All configuration, data and the bundled engine live in a portable `profile`
folder beside the binary; nothing is written to system locations.

This fork exists for privacy. It is not intended for, and does not condone,
copyright infringement.

It tracks upstream qBittorrent and is rebased onto upstream stable releases as
they are published (see *Updating to a new qBittorrent release*). The exact base
release is whatever the git history is currently rebased onto.

### What the fork changes:
* An embedded WARP engine is bundled into the binary. On first run it extracts
  its helpers into the profile folder, registers a free Cloudflare WARP account
  and runs a userspace WireGuard tunnel that exposes a local SOCKS5 endpoint
  (default `127.0.0.1:40000`). It is supervised by the app and shut down on exit.
* libtorrent is routed through that SOCKS5 endpoint with remote hostname
  resolution enabled, so peer, tracker, DHT and DNS traffic all go through the
  tunnel. Routing is locked and cannot be overridden from the GUI, the Web UI or
  the configuration file.
* A watchdog pauses all BitTorrent traffic whenever the tunnel is unavailable
  (including before it has finished connecting) and resumes once it is up, so
  nothing leaks around the tunnel.
* UPnP, NAT-PMP and Local Service Discovery are disabled so the client does not
  contact the local router or announce activity on the LAN.
* Portable by default: configuration, data, logs and the engine are stored in a
  `profile` folder next to the binary instead of system locations.

As a fallback, if the SOCKS5 endpoint is not reachable the client can instead
bind directly to a WARP WireGuard interface named `warp` (see `QBT_WARP_MODE`);
when an interface is named but down, libtorrent fails closed rather than leaking.
The fork-specific code is in `src/base/bittorrent/warpconfig.{h,cpp}`,
`src/base/bittorrent/warpengine.{h,cpp}` and the hooks in
`src/base/bittorrent/sessionimpl.cpp`.

### Limitations:
* WARP does not support inbound connections or port forwarding. The client is
  connect-only: it can reach peers that accept incoming connections, but remote
  peers cannot connect to it. Transfers still work, with a smaller peer set.
* Running BitTorrent over the free WARP tier may conflict with Cloudflare's
  terms of service.
* WARP hides your address from peers and trackers and your traffic from the
  local network and ISP. It is not an anonymity network; Cloudflare can see your
  traffic at its edge.
* The default SOCKS5 path resolves hostnames through the tunnel, so DNS does not
  leak. The manual interface fallback (`QBT_WARP_MODE=interface`) does not bind
  the resolver, so use it only with system DNS already routed through WARP.
* The bundled engine and the optional helper scripts are Linux-only. The
  in-client enforcement itself is cross-platform.

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

The build embeds the prebuilt Linux engine helpers from `warp/engine/` (wgcf and
wireproxy) into the binary; no extra build steps are needed.

### Usage:
Run the binary. There is nothing to configure:

    ./qbittorrent

On the first launch it registers a free Cloudflare WARP account, starts the
bundled tunnel and creates a `profile` folder beside the binary holding all
configuration, data and the engine. Subsequent launches reuse them. The log
shows the progress:

    [WARP] Registering a free Cloudflare WARP account (first run only)...
    [WARP] Userspace WARP tunnel engine started; SOCKS5 on 127.0.0.1:40000.
    [WARP] Tunnel restored. Resuming BitTorrent traffic.

Until the tunnel reports reachable the kill switch keeps all transfers paused.

Environment variables (optional):

    QBT_WARP_MODE         interface | socks5 | both    (default: both)
    QBT_WARP_INTERFACE    WARP interface name           (default: warp)
    QBT_WARP_SOCKS_HOST   SOCKS5 host                   (default: 127.0.0.1)
    QBT_WARP_SOCKS_PORT   SOCKS5 port                   (default: 40000)
    QBT_WARP_KILLSWITCH   0 to disable the kill switch  (default: 1)
    QBT_WARP_DISABLE      1 to disable enforcement entirely (development only)

On start-up the log shows a line beginning with `[WARP]` describing the active
configuration.

### Advanced (optional):
The bundled engine makes the helper scripts in `warp/` unnecessary for normal
use. They remain for advanced setups: `warp-provision.sh` provisions WARP with an
external `wgcf`/`wireproxy`, and `qbt-warp-netns.sh` runs the client inside a
network namespace whose only route is WARP (the strongest, fully kernel-level
isolation). Point the client at an external endpoint with the environment
variables above.

### Updating to a new qBittorrent release:
The fork is a small set of changes applied on top of an upstream release tag. To
move it onto a newer release, run:

    warp/update-from-upstream.sh                  # rebase onto the latest stable release tag
    warp/update-from-upstream.sh release-X.Y.Z    # or onto a specific tag

Conflicts, if any, are normally confined to `src/base/bittorrent/sessionimpl.cpp`.
Resolve them, finish the rebase, then rebuild.

### Misc:
This is an unofficial fork and is not affiliated with or endorsed by the
qBittorrent project. Report problems with the fork to this repository, not to
the upstream qBittorrent trackers.

Upstream project: https://www.qbittorrent.org
Upstream wiki:    https://wiki.qbittorrent.org
