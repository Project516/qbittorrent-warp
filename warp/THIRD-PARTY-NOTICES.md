Third-party notices
===================

qBittorrent-WARP relies on two external, upstream tools to bring up its
userspace Cloudflare WARP tunnel. They are not bundled in this repository or in
the qBittorrent binary. On first run the engine downloads them from their
official GitHub releases at the pinned versions below and verifies each one
against a hard-coded SHA-256 before it is executed (see
src/base/bittorrent/warpengine.cpp). The downloads are stored in the portable
profile directory and reused on later runs.

The fork ships an x86_64 desktop build and an arm64 build (the headless
Raspberry Pi release), so each tool is pinned for both architectures and the
matching asset is chosen at runtime.

To update a tool, bump its version, download URLs and checksums together; the
checksums are taken from each release's published checksums.txt.


wgcf
----
Registers a free Cloudflare WARP account and generates a WireGuard profile.

* Project:  https://github.com/ViRb3/wgcf
* Version:  v2.2.32
* Asset (amd64):   wgcf_2.2.32_linux_amd64
* SHA-256 (amd64): 2ff97f2201972ce582a424455d50a3719a380eef0cd1f3144f7779348e122a2c
* Asset (arm64):   wgcf_2.2.32_linux_arm64
* SHA-256 (arm64): 21fe21d9f61db9b381d71200f6f59c7949e0bb455446edcb33dda6ad6a8fcf8f
* License:  MIT

MIT License

Copyright (c) 2020 ViRb3

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


wireproxy
---------
A userspace WireGuard client that exposes a local SOCKS5 proxy.

* Project:  https://github.com/pufferffish/wireproxy
* Version:  v1.1.3
* Asset (amd64):            wireproxy_linux_amd64.tar.gz
* SHA-256 (amd64, archive): e88c1d090740373fc606c1bafd81d9a5eadc642cce5667616e20e9d7a444f51c
* SHA-256 (amd64, binary):  70ae5e52223dac7974af8d98a321f14a0e1689d2b14655ebc8dadfa1ec69466d
* Asset (arm64):            wireproxy_linux_arm64.tar.gz
* SHA-256 (arm64, archive): 370e00bd2167960d1ecd1c3c1439715bbaa94a0a110a2040468670c9af6021b6
* SHA-256 (arm64, binary):  5852e32671afb8918c39c59330b85f833c187ed41b6b1f683c90b6bfd320f3fa
* License:  ISC

Copyright (c) 2026 Tsz Fung Wong <im@windtfw.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
