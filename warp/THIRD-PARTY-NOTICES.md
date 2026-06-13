Third-party notices
===================

qBittorrent-WARP relies on two external, upstream tools to bring up its
userspace Cloudflare WARP tunnel. They are not bundled in this repository or in
the qBittorrent binary. On first run the engine downloads them from their
official GitHub releases at the pinned versions below and verifies each one
against a hard-coded SHA-256 before it is executed (see
src/base/bittorrent/warpengine.cpp). The downloads are stored in the portable
profile directory and reused on later runs.

To update a tool, bump its version, download URL and checksums together; the
checksums are taken from each release's published checksums.txt.


wgcf
----
Registers a free Cloudflare WARP account and generates a WireGuard profile.

* Project:  https://github.com/ViRb3/wgcf
* Version:  v2.2.31
* Asset:    wgcf_2.2.31_linux_amd64
* SHA-256:  69147e1a517c66129edd8ac8cb60484d6c9515178d7b4a2f95e3c925f225572a
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
* Version:  v1.1.2
* Asset:    wireproxy_linux_amd64.tar.gz
* SHA-256 (archive): b7dcff8f6e9d3410364e432aff24154eaa8db8206e0c6faac35d6c6ab06dac51
* SHA-256 (binary):  b5a729f3606753ce4d4bfeb0f56d522e4aa0908aff8c7d55960fd4301cc58b11
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
