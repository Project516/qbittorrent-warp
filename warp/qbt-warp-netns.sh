#!/usr/bin/env bash
#
# qbt-warp-netns.sh - Run the qBittorrent-WARP fork inside a network namespace
# whose ONLY egress is the Cloudflare WARP WireGuard interface. This is the
# airtight, leak-proof layer: a process in this namespace physically cannot send
# a packet anywhere except through WARP - including DNS. If WARP is down, nothing
# leaves at all (hard kill switch).
#
# Technique: the WireGuard device is created in the init namespace (so its
# encrypted UDP socket egresses via your real NIC) and then *moved* into the
# namespace, where it becomes the default route. See WireGuard's documentation
# on "Routing & Namespace Integration".
#
# THIS TOOL IS FOR PRIVACY, NOT PIRACY.
#
# Requirements: root, iproute2, wireguard-tools (wg), a WARP config produced by
# warp-provision.sh (warp.conf), and qbittorrent-nox (or qbittorrent for --gui).
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NETNS="${QBT_WARP_NETNS:-qbtwarp}"
IFACE="${QBT_WARP_INTERFACE:-warp}"
CONF="${WARP_CONF:-${HERE}/${IFACE}.conf}"

# Link subnet for the host <-> netns veth used ONLY to reach the WebUI from the
# host. It carries no default route, so torrent traffic never uses it.
VETH_HOST="vethwarp0"
VETH_NS="vethwarp1"
HOST_IP="10.213.213.1"
NS_IP="10.213.213.2"
PREFIX="30"

WEBUI_PORT="${QBT_WARP_WEBUI_PORT:-8080}"
RUN_GUI=0
NO_VETH=0

# WARP / Cloudflare resolvers used inside the namespace.
DNS4_1="1.1.1.1"; DNS4_2="1.0.0.1"; DNS6_1="2606:4700:4700::1111"

log() { printf '\033[1;36m[warp-netns]\033[0m %s\n' "$*"; }
err() { printf '\033[1;31m[warp-netns:err]\033[0m %s\n' "$*" >&2; }
die() { err "$*"; exit 1; }

usage() {
    cat <<EOF
Usage: sudo $0 [options] [-- <extra args passed to qbittorrent>]

  --gui          Launch the GUI qbittorrent instead of qbittorrent-nox.
                 (Best-effort: X must be reachable from the namespace; if it
                 fails, prefer nox + WebUI.)
  --no-veth      Do not create the host<->namespace veth (WebUI unreachable
                 from the host, but maximum isolation).
  --conf PATH    WireGuard config to use (default: ${CONF}).
  -h, --help     Show this help.

Examples:
  sudo $0                       # qbittorrent-nox in the namespace; WebUI at http://${NS_IP}:${WEBUI_PORT}
  sudo $0 --gui                 # GUI client routed through WARP
EOF
}

EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --gui) RUN_GUI=1 ;;
        --no-veth) NO_VETH=1 ;;
        --conf) CONF="$2"; shift ;;
        --) shift; EXTRA_ARGS=("$@"); break ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown option: $1 (see --help)" ;;
    esac
    shift
done

[[ $EUID -eq 0 ]] || die "Must run as root (sudo)."
command -v ip >/dev/null 2>&1 || die "iproute2 (ip) not found."
command -v wg >/dev/null 2>&1 || die "wireguard-tools (wg) not found."
[[ -f "$CONF" ]] || die "WireGuard config not found: $CONF (run warp-provision.sh first)."

RUN_USER="${SUDO_USER:-root}"

cleanup() {
    log "Tearing down namespace ${NETNS}..."
    ip netns pids "$NETNS" 2>/dev/null | xargs -r kill 2>/dev/null || true
    # Deleting the netns destroys the moved wg device and the veth peer in it.
    ip netns del "$NETNS" 2>/dev/null || true
    ip link del "$VETH_HOST" 2>/dev/null || true
    ip link del "$IFACE" 2>/dev/null || true   # in case it never got moved
    rm -rf "/etc/netns/${NETNS}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Clean any stale state from a previous crash.
ip netns del "$NETNS" 2>/dev/null || true
ip link del "$IFACE" 2>/dev/null || true
ip link del "$VETH_HOST" 2>/dev/null || true

log "Creating namespace ${NETNS}"
ip netns add "$NETNS"
ip -n "$NETNS" link set lo up

# 1. Create the WireGuard device in the INIT namespace so its encrypted UDP
#    socket uses your real network, then load keys/peer from the config.
log "Creating WireGuard device ${IFACE} and loading config"
ip link add "$IFACE" type wireguard
wg setconf "$IFACE" <(wg-quick strip "$CONF" 2>/dev/null || sed -n '/\[Interface\]/,$p' "$CONF" | grep -viE '^(Address|DNS|MTU|Table|PreUp|PostUp|PreDown|PostDown)')

# 2. Move the device into the namespace (its UDP socket stays in init ns).
ip link set "$IFACE" netns "$NETNS"

# 3. Configure addresses / MTU / routes inside the namespace.
MTU="$(awk -F' *= *' 'tolower($1)=="mtu"{print $2}' "$CONF" | head -1)"; MTU="${MTU:-1280}"
ip -n "$NETNS" link set "$IFACE" mtu "$MTU"

HAS_V6=0
# Collect every address from all "Address =" lines (may be comma-separated).
while IFS= read -r line; do
    addrs="${line#*=}"
    IFS=',' read -ra parts <<< "$addrs"
    for a in "${parts[@]}"; do
        a="$(echo "$a" | xargs)"   # trim
        [[ -z "$a" ]] && continue
        ip -n "$NETNS" addr add "$a" dev "$IFACE"
        [[ "$a" == *:* ]] && HAS_V6=1
    done
done < <(grep -iE '^Address' "$CONF")

ip -n "$NETNS" link set "$IFACE" up
ip -n "$NETNS" route add default dev "$IFACE"
[[ "$HAS_V6" == "1" ]] && ip -n "$NETNS" -6 route add default dev "$IFACE" || true

# 4. DNS inside the namespace -> Cloudflare resolvers (reached via WARP).
mkdir -p "/etc/netns/${NETNS}"
{
    echo "nameserver ${DNS4_1}"
    echo "nameserver ${DNS4_2}"
    [[ "$HAS_V6" == "1" ]] && echo "nameserver ${DNS6_1}"
} > "/etc/netns/${NETNS}/resolv.conf"

# 5. Optional veth so the host can reach the WebUI (no default route over it).
if [[ "$NO_VETH" == "0" ]]; then
    log "Creating veth for WebUI access: host ${HOST_IP} <-> ns ${NS_IP}"
    ip link add "$VETH_HOST" type veth peer name "$VETH_NS"
    ip link set "$VETH_NS" netns "$NETNS"
    ip addr add "${HOST_IP}/${PREFIX}" dev "$VETH_HOST"
    ip link set "$VETH_HOST" up
    ip -n "$NETNS" addr add "${NS_IP}/${PREFIX}" dev "$VETH_NS"
    ip -n "$NETNS" link set "$VETH_NS" up
fi

# 6. Sanity check: confirm egress IP belongs to WARP/Cloudflare.
log "Verifying egress goes through WARP..."
if ip netns exec "$NETNS" sh -c 'command -v curl >/dev/null 2>&1'; then
    ip netns exec "$NETNS" curl -s --max-time 10 https://www.cloudflare.com/cdn-cgi/trace/ \
        | grep -E 'warp=|ip=' || err "Could not verify trace (continuing anyway)."
fi

# 7. Launch the fork inside the namespace, as the invoking user.
#    Inside the netns the WARP interface is the only route, so interface mode is
#    sufficient and DNS is already forced through WARP.
export QBT_WARP_MODE="${QBT_WARP_MODE:-interface}"
export QBT_WARP_INTERFACE="$IFACE"

if [[ "$RUN_GUI" == "1" ]]; then
    BIN="$(command -v qbittorrent || true)"
    [[ -n "$BIN" ]] || die "qbittorrent (GUI) not found on PATH."
    log "Launching GUI qbittorrent in ${NETNS} (DISPLAY=${DISPLAY:-unset})"
    ip netns exec "$NETNS" sudo -u "$RUN_USER" \
        env DISPLAY="${DISPLAY:-:0}" XAUTHORITY="${XAUTHORITY:-/home/$RUN_USER/.Xauthority}" \
            QBT_WARP_MODE="$QBT_WARP_MODE" QBT_WARP_INTERFACE="$QBT_WARP_INTERFACE" \
        "$BIN" "${EXTRA_ARGS[@]}"
else
    BIN="$(command -v qbittorrent-nox || true)"
    [[ -n "$BIN" ]] || die "qbittorrent-nox not found on PATH."
    [[ "$NO_VETH" == "0" ]] && log "WebUI will be reachable from the host at: http://${NS_IP}:${WEBUI_PORT}"
    log "Launching qbittorrent-nox in ${NETNS} (Ctrl-C to stop and tear down)"
    ip netns exec "$NETNS" sudo -u "$RUN_USER" \
        env QBT_WARP_MODE="$QBT_WARP_MODE" QBT_WARP_INTERFACE="$QBT_WARP_INTERFACE" \
        "$BIN" "${EXTRA_ARGS[@]}"
fi
