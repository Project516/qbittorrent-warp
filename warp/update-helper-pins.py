#!/usr/bin/env python3
# Rewrites the pinned WARP engine helper versions, download URLs and SHA-256
# sums in src/base/bittorrent/warpengine.cpp and warp/THIRD-PARTY-NOTICES.md.
#
# Run from the repository root. The new values are read from the environment:
#   WGCF_TAG WGCF_VER WGCF_SHA   WP_TAG WP_VER WP_ARC WP_BIN
# Each replacement must match exactly once; the script exits non-zero otherwise
# so a layout change upstream fails the update instead of silently corrupting a
# checksum. It is used by .github/workflows/update_warp_helpers.yaml.

import os
import re
import sys

CPP = "src/base/bittorrent/warpengine.cpp"
NOTICES = "warp/THIRD-PARTY-NOTICES.md"


def env(name):
    value = os.environ.get(name, "")
    if not value:
        sys.exit("missing environment variable: " + name)
    return value


def sub_once(pattern, repl, text, what, flags=0):
    result, count = re.subn(pattern, repl, text, count=1, flags=flags)
    if count != 1:
        sys.exit("failed to update %s (matched %d times, expected 1)" % (what, count))
    return result


def main():
    wgcf_tag = env("WGCF_TAG")
    wgcf_ver = env("WGCF_VER")
    wgcf_sha = env("WGCF_SHA")
    wp_tag = env("WP_TAG")
    wp_ver = env("WP_VER")
    wp_arc = env("WP_ARC")
    wp_bin = env("WP_BIN")

    with open(CPP, encoding="utf-8") as handle:
        cpp = handle.read()

    cpp = sub_once(r"// wgcf \S+ - MIT", "// wgcf %s - MIT" % wgcf_ver, cpp, "wgcf comment")
    cpp = sub_once(r"// wireproxy \S+ - ISC", "// wireproxy %s - ISC" % wp_ver, cpp, "wireproxy comment")

    def set_const(name, value, text):
        pattern = r'(const QString ' + name + r'\s*=\s*u")[^"]*(")'
        return sub_once(pattern, lambda m: m.group(1) + value + m.group(2), text, name)

    cpp = set_const(
        "WGCF_URL",
        "https://github.com/ViRb3/wgcf/releases/download/%s/wgcf_%s_linux_amd64" % (wgcf_tag, wgcf_ver),
        cpp)
    cpp = set_const("WGCF_SHA256", wgcf_sha, cpp)
    cpp = set_const(
        "WIREPROXY_URL",
        "https://github.com/pufferffish/wireproxy/releases/download/%s/wireproxy_linux_amd64.tar.gz" % wp_tag,
        cpp)
    cpp = set_const("WIREPROXY_ARCHIVE_SHA256", wp_arc, cpp)
    cpp = set_const("WIREPROXY_BINARY_SHA256", wp_bin, cpp)

    with open(CPP, "w", encoding="utf-8") as handle:
        handle.write(cpp)

    with open(NOTICES, encoding="utf-8") as handle:
        notices = handle.read()

    notices = sub_once(
        r"(https://github\.com/ViRb3/wgcf\n\* Version:  )v\S+",
        r"\g<1>v" + wgcf_ver, notices, "notices wgcf version", flags=re.S)
    notices = sub_once(
        r"(\* Asset:    wgcf_)\S+(_linux_amd64)",
        r"\g<1>" + wgcf_ver + r"\g<2>", notices, "notices wgcf asset")
    notices = sub_once(
        r"(\* SHA-256:  )[0-9a-f]{64}",
        r"\g<1>" + wgcf_sha, notices, "notices wgcf sha")
    notices = sub_once(
        r"(https://github\.com/pufferffish/wireproxy\n\* Version:  )v\S+",
        r"\g<1>v" + wp_ver, notices, "notices wireproxy version", flags=re.S)
    notices = sub_once(
        r"(\* SHA-256 \(archive\): )[0-9a-f]{64}",
        r"\g<1>" + wp_arc, notices, "notices wireproxy archive sha")
    notices = sub_once(
        r"(\* SHA-256 \(binary\):  )[0-9a-f]{64}",
        r"\g<1>" + wp_bin, notices, "notices wireproxy binary sha")

    with open(NOTICES, "w", encoding="utf-8") as handle:
        handle.write(notices)


if __name__ == "__main__":
    main()
