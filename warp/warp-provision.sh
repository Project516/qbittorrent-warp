#!/usr/bin/env bash
#
# warp-provision.sh - Provision Cloudflare WARP as a WireGuard profile for the
# qBittorrent-WARP fork, using wgcf (https://github.com/ViRb3/wgcf).
#
# It produces:
#   * ./warp.conf           a wg-quick(8) compatible WireGuard config (interface "warp")
#   * (optional) a wireproxy SOCKS5 endpoint on 127.0.0.1:40000 backed by WARP
#
# The fork binds libtorrent to the "warp" interface AND/OR routes it through the
# SOCKS5 endpoint, depending on QBT_WARP_MODE. This script can set up either or
# both. It does NOT change your system default route by itself - bringing the
# interface up system-wide vs. inside a netns is your choice (see usage below).
#
# THIS TOOL IS FOR PRIVACY, NOT PIRACY.
#
# Requirements: wgcf, wireguard-tools (wg/wg-quick), and optionally wireproxy.
#
set -euo pipefail

IFACE="${QBT_WARP_INTERFACE:-warp}"
SOCKS_HOST="${QBT_WARP_SOCKS_HOST:-127.0.0.1}"
SOCKS_PORT="${QBT_WARP_SOCKS_PORT:-40000}"
WORKDIR="${WARP_WORKDIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
ACCOUNT="${WORKDIR}/wgcf-account.toml"
PROFILE="${WORKDIR}/wgcf-profile.conf"
OUTCONF="${WORKDIR}/${IFACE}.conf"

DO_UP=0          # bring the interface up system-wide with wg-quick
DO_SOCKS=0       # start a wireproxy SOCKS5 endpoint
ROUTE_ALL=0      # if bringing up: route ALL system traffic through WARP

log()  { printf '\033[1;36m[warp]\033[0m %s\n' "$*"; }
err()  { printf '\033[1;31m[warp:err]\033[0m %s\n' "$*" >&2; }
die()  { err "$*"; exit 1; }

usage() {
    cat <<EOF
Usage: $0 [options]

  --up              Bring the "${IFACE}" interface up now via wg-quick (needs root).
                    By default it is created WITHOUT becoming the system default
                    route (Table=off), so only apps that bind to it use it.
  --route-all       With --up: make WARP the system default route (whole machine).
  --socks           Start a wireproxy SOCKS5 endpoint on ${SOCKS_HOST}:${SOCKS_PORT}.
  --force           Re-register / regenerate even if files already exist.
  -h, --help        Show this help.

Typical flows:
  # 1) Generate the config only (then use the netns wrapper - recommended):
  $0

  # 2) Generate + expose a SOCKS5 proxy (QBT_WARP_MODE=socks5 or both):
  $0 --socks

  # 3) Generate + bring up the interface for app binding (QBT_WARP_MODE=interface):
  sudo $0 --up
EOF
}

FORCE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --up) DO_UP=1 ;;
        --route-all) ROUTE_ALL=1 ;;
        --socks) DO_SOCKS=1 ;;
        --force) FORCE=1 ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown option: $1 (see --help)" ;;
    esac
    shift
done

command -v wgcf >/dev/null 2>&1 || die "wgcf not found. Install from https://github.com/ViRb3/wgcf/releases and put it on PATH."

# 1. Register a free WARP account (idempotent).
if [[ ! -f "$ACCOUNT" || "$FORCE" == "1" ]]; then
    log "Registering a new (free) Cloudflare WARP account via wgcf..."
    ( cd "$WORKDIR" && wgcf register --accept-tos )
else
    log "Reusing existing account: $ACCOUNT"
fi

# 2. Generate the WireGuard profile (idempotent unless --force).
if [[ ! -f "$PROFILE" || "$FORCE" == "1" ]]; then
    log "Generating WireGuard profile..."
    ( cd "$WORKDIR" && wgcf generate )
fi
[[ -f "$PROFILE" ]] || die "wgcf did not produce $PROFILE"

# 3. Turn the wgcf profile into our named interface config.
#    By default we disable the catch-all routing table so the interface only
#    carries traffic from apps that explicitly bind to it (our fork does). Use
#    --route-all (or the netns wrapper) for whole-tunnel routing.
log "Writing ${OUTCONF}"
{
    if [[ "$DO_UP" == "1" && "$ROUTE_ALL" == "0" ]]; then
        # Insert "Table = off" into the [Interface] section so wg-quick does not
        # hijack the system default route.
        awk '
            /^\[Interface\]/ { print; print "Table = off"; next }
            { print }
        ' "$PROFILE"
    else
        cat "$PROFILE"
    fi
} > "$OUTCONF"
chmod 600 "$OUTCONF"

ADDR=$(awk -F' *= *' '/^Address/{print $2}' "$OUTCONF" | head -1)
log "Profile ready. Interface=${IFACE} Address=${ADDR}"

# 4. Optionally bring the interface up system-wide.
if [[ "$DO_UP" == "1" ]]; then
    command -v wg-quick >/dev/null 2>&1 || die "wg-quick not found (install wireguard-tools)."
    [[ $EUID -eq 0 ]] || die "--up needs root. Re-run with sudo."
    log "Bringing up ${IFACE} (wg-quick)..."
    # wg-quick uses the file name as the interface name, so point it at OUTCONF.
    wg-quick up "$OUTCONF"
    log "Interface ${IFACE} is up. Verify your egress IP is Cloudflare's:"
    log "  curl --interface ${IFACE} https://www.cloudflare.com/cdn-cgi/trace/ | grep warp="
fi

# 5. Optionally start a SOCKS5 proxy backed by WARP (userspace WireGuard).
if [[ "$DO_SOCKS" == "1" ]]; then
    command -v wireproxy >/dev/null 2>&1 || die "wireproxy not found. Install from https://github.com/pufferffish/wireproxy"
    WPCONF="${WORKDIR}/wireproxy.conf"
    log "Writing ${WPCONF} and starting wireproxy SOCKS5 on ${SOCKS_HOST}:${SOCKS_PORT}"
    {
        cat "$PROFILE"
        echo
        echo "[Socks5]"
        echo "BindAddress = ${SOCKS_HOST}:${SOCKS_PORT}"
    } > "$WPCONF"
    chmod 600 "$WPCONF"
    log "Run:  wireproxy -c ${WPCONF}    (foreground; add to a service for persistence)"
    exec wireproxy -c "$WPCONF"
fi

log "Done."
log "Next: set QBT_WARP_MODE (interface|socks5|both) and start the fork, or use warp/qbt-warp-netns.sh for airtight isolation."
