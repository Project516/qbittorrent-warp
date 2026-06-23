qBittorrent-WARP - qBittorrent routed through Cloudflare WARP
-------------------------------------------------------------

### Description:
qBittorrent-WARP is a fork of qBittorrent that forces all BitTorrent traffic
through Cloudflare WARP. Peer connections, tracker announces and their DNS go
through the tunnel, and a kill switch pauses all transfers whenever the tunnel is
unavailable.

It is self-contained: a single binary with no setup. On first run it fetches and
verifies its tunnel helpers, registers a free WARP account and starts a userspace
WireGuard tunnel by itself - no shell scripts, no root, no system changes. All
configuration, data and the engine live in a portable `profile` folder beside the
binary.

This fork exists for privacy. It is not intended for, and does not condone,
copyright infringement.

### What the fork changes:
* Routes libtorrent through a self-contained userspace WARP tunnel (SOCKS5), with
  DNS resolved through the tunnel. Routing is locked and cannot be overridden from
  the GUI, the Web UI or the configuration file.
* Fails closed: a kill switch pauses traffic whenever the tunnel is down, and the
  client never falls back to a direct connection.
* Disables everything that touches the local network or leaks identity - UPnP,
  NAT-PMP, Local Service Discovery, DHT and uTP - and forces anonymous mode on.
* Portable: configuration, data, logs and the engine live in a `profile` folder
  beside the binary, not in system locations.

See [warp/README.md](warp/README.md) for how it works, configuration, verification
and advanced setups.

### Installation:
Prebuilt binaries are on the
[Releases](https://github.com/Project516/qbittorrent-warp/releases) page as
self-contained AppImages. Two builds are published:

* `x86_64` - desktop build with the graphical interface, for 64-bit Linux PCs.
* `aarch64` - headless build (`qbittorrent-nox`) for 64-bit ARM boards such as
  the Raspberry Pi. It has no graphical interface and is used through its Web UI
  in a browser. See [warp/README.md](warp/README.md#headless-raspberry-pi) for
  headless setup.

Download the one for your machine, make it executable and run it:

    chmod +x qBittorrent-WARP-*-x86_64.AppImage
    ./qBittorrent-WARP-*-x86_64.AppImage

Nothing needs to be installed; a `profile` folder is created beside the AppImage,
and each release attaches a `SHA256SUMS` file for verification. To build from
source instead, see [warp/README.md](warp/README.md#building-from-source).

### Documentation:
* [warp/README.md](warp/README.md) - the WARP engine, configuration, verification,
  building and advanced use.
* [warp/THIRD-PARTY-NOTICES.md](warp/THIRD-PARTY-NOTICES.md) - bundled helper
  versions, checksums and licenses.
* `doc/` - qBittorrent man pages.

### Misc:
This is an unofficial fork and is not affiliated with or endorsed by the
qBittorrent project. Report problems with the fork to this repository, not to the
upstream qBittorrent trackers.

Upstream project: https://www.qbittorrent.org
Upstream wiki:    https://wiki.qbittorrent.org
