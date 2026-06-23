qBittorrent-WARP engine
=======================

How the fork routes BitTorrent traffic through Cloudflare WARP, how to configure
and verify it, how to build it, and the options for advanced setups. For an
overview and install instructions see the top-level [README](../README.md).

### How it works:
On first run a self-contained WARP engine downloads its two helper tools (wgcf
and wireproxy) from their official upstream releases at pinned versions, verifies
each one against a hard-coded SHA-256 before running it, registers a free
Cloudflare WARP account and runs a userspace WireGuard tunnel that exposes a
local SOCKS5 endpoint (default `127.0.0.1:40000`). The helpers are stored in the
profile folder and reused on later runs; the engine is supervised by the app and
shut down on exit. Pinned versions, URLs, checksums and licenses are in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

libtorrent is routed through that SOCKS5 endpoint with remote hostname resolution
enabled, so peer connections, HTTP/HTTPS tracker announces and their DNS all go
through the tunnel. Routing is locked and cannot be overridden from the GUI, the
Web UI or the configuration file. UPnP, NAT-PMP, Local Service Discovery, DHT and
uTP are disabled, and anonymous mode is forced on; the only feature left for peer
discovery is Peer Exchange, which rides existing tunnelled connections.

A watchdog pauses all BitTorrent traffic whenever the tunnel is unavailable
(including before it has finished connecting) and resumes once it is up, so
nothing leaks around the tunnel.

The fork-specific code is in `src/base/bittorrent/warpconfig.{h,cpp}`,
`src/base/bittorrent/warpengine.{h,cpp}` and the hooks in
`src/base/bittorrent/sessionimpl.cpp`.

### Modes and fail-closed behaviour:
By default all egress goes through the SOCKS5 endpoint, and the client stays
pointed at it even while it is unreachable, so a dropped tunnel makes connections
fail rather than leak to a direct route. Setups that provide a real `warp`
WireGuard interface (such as `qbt-warp-netns.sh`) can switch to interface binding
with `QBT_WARP_MODE`; a named-but-down interface also fails closed.

### Environment variables (optional):

    QBT_WARP_MODE         interface | socks5 | both    (default: socks5)
    QBT_WARP_INTERFACE    WARP interface name           (default: warp)
    QBT_WARP_SOCKS_HOST   SOCKS5 host                   (default: 127.0.0.1)
    QBT_WARP_SOCKS_PORT   SOCKS5 port                   (default: 40000)
    QBT_WARP_KILLSWITCH   0 to disable the kill switch  (default: 1)
    QBT_WARP_DISABLE      1 to disable enforcement entirely (development only)

On start-up the log shows a line beginning with `[WARP]` describing the active
configuration.

### First run:
On the first launch the engine downloads and verifies the helpers, registers a
free WARP account, starts the tunnel and creates a `profile` folder beside the
binary holding all configuration, data and the engine. Subsequent launches reuse
them. The log shows the progress:

    [WARP] Fetching engine helper (first run only): https://github.com/...
    [WARP] Registering a free Cloudflare WARP account (first run only)...
    [WARP] Userspace WARP tunnel engine started; SOCKS5 on 127.0.0.1:40000.
    [WARP] Tunnel restored. Resuming BitTorrent traffic.

Until the tunnel reports reachable the kill switch keeps all transfers paused.

### Headless Raspberry Pi:
The `aarch64` AppImage on the Releases page is the headless build
(`qbittorrent-nox`). It has no graphical interface and is controlled through its
Web UI in a browser, which suits a Raspberry Pi or any 64-bit ARM board running a
64-bit operating system. Everything else, the WARP tunnel, the kill switch and
the portable `profile` folder, works exactly as on the desktop build.

AppImages need FUSE. Install it once with `sudo apt install libfuse2`, or run the
AppImage with `--appimage-extract-and-run` to skip the dependency.

Download the `aarch64` zip onto the Pi, extract it and start it. Pass
`--confirm-legal-notice` so it does not wait for keyboard input on a machine you
reach over SSH:

    unzip qBittorrent-WARP-*-aarch64.AppImage.zip
    chmod +x qBittorrent-WARP-*-aarch64.AppImage
    ./qBittorrent-WARP-*-aarch64.AppImage --confirm-legal-notice

The Web UI listens on port 8080. On the first start qbittorrent-nox prints a
randomly generated password for the `admin` account to its log:

    ******** Information: The WebUI administrator username is: admin
    ******** Information: A temporary password is provided for this session: <password>

Browse to `http://<pi-ip>:8080`, sign in with that password, then set your own
under Tools, Options, Web UI. Give it a minute on first run while it downloads
and verifies the engine helpers and brings the tunnel up; until then the kill
switch keeps transfers paused.

To start it automatically on boot, create a systemd service. Put the AppImage in
its own directory (the `profile` folder is created next to it) and rename it to a
stable name so updates do not change the path:

    mkdir -p ~/qbittorrent-warp
    mv qBittorrent-WARP-*-aarch64.AppImage ~/qbittorrent-warp/qBittorrent-WARP.AppImage

Write `/etc/systemd/system/qbittorrent-warp.service`, adjusting the user and path
to match your system:

    [Unit]
    Description=qBittorrent-WARP (headless)
    Wants=network-online.target
    After=network-online.target nss-lookup.target

    [Service]
    Type=simple
    User=pi
    WorkingDirectory=/home/pi/qbittorrent-warp
    ExecStart=/home/pi/qbittorrent-warp/qBittorrent-WARP.AppImage --confirm-legal-notice
    Restart=on-failure
    TimeoutStopSec=1800

    [Install]
    WantedBy=multi-user.target

Enable and start it, then read the first-run password from the journal:

    sudo systemctl enable --now qbittorrent-warp
    journalctl -u qbittorrent-warp | grep -i password

The same peer-connectivity and magnet-link limitations below apply to the
headless build. Use torrents with HTTP/HTTPS trackers.

### Verifying the tunnel:
Confirm the SOCKS5 endpoint exits through WARP:

    curl --socks5-hostname 127.0.0.1:40000 https://www.cloudflare.com/cdn-cgi/trace

The reply should contain `warp=on` and a Cloudflare address. With a torrent
active, `ss -tnp | grep 40000` should show the client connected only to
`127.0.0.1:40000`, never directly to a peer. If a system WARP client such as
`warp-cli` is running, disconnect it first so the check reflects only the fork's
tunnel.

### Limitations:
* WARP does not support inbound connections or port forwarding. The client is
  connect-only: it can reach peers that accept incoming connections, but remote
  peers cannot connect to it. Transfers still work, with a smaller peer set.
* Magnet links and UDP trackers do not work: they rely on DHT and UDP, which the
  TCP-only SOCKS5 tunnel cannot carry. Use torrents with HTTP/HTTPS trackers, or
  the netns wrapper below for full UDP tunnelling.
* Running BitTorrent over the free WARP tier may conflict with Cloudflare's terms
  of service.
* WARP hides your address from peers and trackers and your traffic from the local
  network and ISP. It is not an anonymity network; Cloudflare can see your traffic
  at its edge.
* The opt-in interface mode (`QBT_WARP_MODE=interface`) does not bind the
  resolver, so use it only with system DNS already routed through WARP.
* The engine helpers and the helper scripts are Linux-only. The in-client
  enforcement itself is cross-platform.

### Building from source:
The build dependencies are the same as upstream qBittorrent (Qt 6, libtorrent
2.0, Boost, OpenSSL, zlib) plus CMake and Ninja. See [INSTALL](../INSTALL) for the
full list and platform notes.

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

Add `-DGUI=OFF` to build `qbittorrent-nox` (headless, Web UI only). The engine
helpers are not bundled; they are downloaded and verified on first run, so the
build needs no extra steps.

### Advanced (optional):
The built-in engine makes the helper scripts in this folder unnecessary for
normal use. They remain for advanced setups: `warp-provision.sh` provisions WARP
with an external `wgcf`/`wireproxy`, and `qbt-warp-netns.sh` runs the client
inside a network namespace whose only route is WARP - the strongest, fully
kernel-level isolation, and the only way to tunnel UDP (so magnet links and DHT
work). Point the client at an external endpoint with the environment variables
above.

### Updating to a new qBittorrent release:
This is handled automatically. A scheduled workflow
(`.github/workflows/auto_update.yaml`) checks weekly for the latest upstream
stable release, opens a pull request that merges it into the `warp` branch, and
merges it on its own when there are no conflicts. When a release would conflict
(normally only in `src/base/bittorrent/sessionimpl.cpp`) the pull request is left
open for manual resolution. It can also be run on demand from the Actions tab.

Each change produces a new release. Versions follow the upstream release the build
is based on plus a fork patch number, written `X.Y.Z.W`: `X.Y.Z` is the upstream
base and `W` is incremented for each fork build of that base, so a new upstream
release restarts `W` at 1. `.github/workflows/release.yaml` builds the AppImage
and publishes it automatically, and the client's built-in Check for Updates
tracks this fork's releases.

For a manual, rebase-based update instead of a merge, `update-from-upstream.sh`
rebases the fork onto a release tag.
