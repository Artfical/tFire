#!/usr/bin/env python3
"""Writes THIRD_PARTY_NOTICES.md: every piece of other people's work in tFire, its licence, where it came from and
what was changed, followed by the full text of each permissive licence (MIT, BSD, zlib, ISC, Apache, OFL), which
must go with copies of the software, binaries included. tOS's kernel carries the file (tFire shows it at
tfire:licenses), so the ISO has it too.

    python3 tools/gen_notices.py      (run from the top of the tree; the Makefile does it when a licence changes)
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# name, version, upstream, licence, where it is in the tree, what it is for, local changes, licence files
COMPONENTS = [
    ("libcss (NetSurf)", "0.9.2", "https://source.netsurf-browser.org/libcss.git", "MIT",
     "third_party/netsurf/libcss/", "tFire: CSS parsing, selection and cascade",
     "src/stylesheet.h defines _ALIGNED (see third_party/netsurf/README.tOS)",
     ["third_party/netsurf/libcss/COPYING"]),
    ("libhubbub (NetSurf)", "0.3.8", "https://source.netsurf-browser.org/libhubbub.git", "MIT",
     "third_party/netsurf/libhubbub/", "tFire: HTML5 parsing", "none",
     ["third_party/netsurf/libhubbub/COPYING"]),
    ("libparserutils (NetSurf)", "0.2.5", "https://source.netsurf-browser.org/libparserutils.git", "MIT",
     "third_party/netsurf/libparserutils/", "tFire: input streams and character sets for the above", "none",
     ["third_party/netsurf/libparserutils/COPYING"]),
    ("libwapcaplet (NetSurf)", "0.4.3", "https://source.netsurf-browser.org/libwapcaplet.git", "MIT",
     "third_party/netsurf/libwapcaplet/", "tFire: interned strings for the above", "none",
     ["third_party/netsurf/libwapcaplet/COPYING"]),
    ("QuickJS", "2025-09-13", "https://bellard.org/quickjs/", "MIT",
     "third_party/quickjs/", "tFire: the JavaScript engine",
     "two changes behind TOS_TFIRE: no Atomics, UTC time zone (see third_party/quickjs/README.tOS)",
     ["third_party/quickjs/LICENSE"]),
    ("NanoSVG", "master (2026-10)", "https://github.com/memononen/nanosvg", "Zlib",
     "third_party/nanosvg/", "tFire: drawing SVG pictures", "none",
     ["third_party/nanosvg/LICENSE.txt"]),
    ("libwebp (decoder)", "1.4.0", "https://chromium.googlesource.com/webm/libwebp", "BSD-3-Clause (+ patent grant)",
     "third_party/libwebp/", "tFire: decoding WebP pictures", "none (the decoder's files only)",
     ["third_party/libwebp/COPYING", "third_party/libwebp/PATENTS"]),
    ("wasm3", "0.5.0", "https://github.com/wasm3/wasm3", "MIT",
     "third_party/wasm3/", "tFire: WebAssembly (an interpreter)",
     "an imported table's type is read past; an imported memory is allocated by the instance (see third_party/wasm3/README.tOS)",
     ["third_party/wasm3/LICENSE"]),
    ("Inter (font)", "", "https://github.com/rsms/inter", "OFL-1.1",
     "third_party/ui/Inter-*.ttf", "the text of the window", "rasterised into bitmaps at build time (ui/ui_assets.c)",
     ["third_party/ui/Inter-LICENSE.txt"]),
    ("JetBrains Mono (font)", "", "https://github.com/JetBrains/JetBrainsMono", "OFL-1.1",
     "third_party/ui/JetBrainsMono-Regular.ttf", "monospaced text", "none",
     ["third_party/ui/JetBrainsMono-OFL.txt"]),
    ("Lucide (icons)", "", "https://lucide.dev/", "ISC",
     "third_party/ui/lucide.ttf", "the icons of the window", "rasterised into bitmaps at build time",
     ["third_party/ui/Lucide-LICENSE.txt"]),
    ("SDL2", "2.30.9", "https://libsdl.org/", "Zlib",
     "built from source by CMake (not in the tree)", "the window, keyboard, mouse and threads", "none",
     ["third_party/sdl2/LICENSE.txt"]),
    ("Mbed TLS", "3.6.2", "https://github.com/Mbed-TLS/mbedtls", "Apache-2.0 (it is dual licensed with GPL-2.0-or-later; tFire uses it under Apache-2.0)",
     "third_party/mbedtls/", "HTTPS (TLS 1.2 and 1.3, certificate checks)", "the test framework and docs were left out",
     ["third_party/mbedtls/LICENSE"]),
    ("Monocypher", "4.0.2", "https://monocypher.org/", "BSD-2-Clause or CC0-1.0",
     "platform/monocypher*.c, platform/monocypher*.h", "checking the signature of an update (Ed25519)", "none",
     ["third_party/monocypher/LICENCE.md"]),
    ("Mozilla CA certificate bundle", "from Ubuntu ca-certificates 20260816", "https://wiki.mozilla.org/CA", "MPL-2.0",
     "third_party/cacert/cacert.pem", "the certificate authorities HTTPS sites are checked against", "none",
     ["third_party/cacert/LICENSE-MPL-2.0"]),
]

# GPL code in the tree: named here, its licence texts are next to it (not repeated in full)
GPL = []


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as f:
        return f.read().rstrip() + "\n"


def main():
    out = []
    out.append("# Third-party notices\n")
    out.append("tFire is distributed under the GNU Affero General Public License v3.0 (see `LICENSE`). It contains or is built with the\n"
               "following work of others, under their own licences. This file is made by `tools/gen_notices.py`;\n"
               "tFire shows it at `tfire:licenses`.\n")
    out.append("## Components\n")
    out.append("| Component | Version | Licence | In the tree | Used for | Local changes |")
    out.append("|---|---|---|---|---|---|")
    for name, ver, url, lic, where, what, changes, _ in COMPONENTS:
        out.append(f"| [{name}]({url}) | {ver} | {lic} | `{where}` | {what} | {changes} |")
    out.append("")
    out.append("The same engine runs in [tOS](https://github.com/Artfical/tOS), where it is built with tOS's own tools.\n")
    out.append("## Licence texts\n")
    seen = set()
    for name, ver, url, lic, where, what, changes, files in COMPONENTS:
        for path in files:
            text = read(path)
            out.append(f"### {name} — `{path}`\n")
            if text in seen:
                out.append("(The same text as above.)\n")
                continue
            seen.add(text)
            out.append("```")
            out.append(text.rstrip("\n"))
            out.append("```\n")
    with open(os.path.join(ROOT, "THIRD_PARTY_NOTICES.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    write_html()


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def inline(t):
    """markdown's `code` as <code> (the rest escaped)"""
    parts = esc(t).split("`")
    return "".join(p if i % 2 == 0 else "<code>" + p + "</code>" for i, p in enumerate(parts))


def write_html():
    """the same as a page: what tFire shows at tfire:licenses (kernel/display/apps/tfire_notices.s carries it)"""
    h = ['<!doctype html><html><head><meta charset="utf-8"><title>Licences</title><style>',
         "body{font-family:sans-serif;margin:32px 40px;color:#1f2937;line-height:1.5}",
         "h1{font-size:28px;margin:0 0 8px}h2{font-size:20px;margin:28px 0 10px}h3{font-size:15px;margin:22px 0 6px}",
         "table{border-collapse:collapse;width:100%;font-size:13px}th,td{border:1px solid #d1d5db;padding:5px 8px;"
         "text-align:left;vertical-align:top}th{background-color:#f3f4f6}",
         "pre{white-space:pre-wrap;font-size:12px;background-color:#f9fafb;border:1px solid #e5e7eb;padding:10px;color:#374151}",
         "p.note{color:#4b5563}a{color:#1a56db}code{color:#374151}small{font-size:11px;color:#6b7280}</style></head><body>",
         "<h1>Licences</h1>",
         "<p>tFire is free software under the GNU Affero General Public License v3.0. These are the works of others it "
         "is built from, their licences, and what was changed in them.</p>",
         "<h2>Components</h2><table><tr><th>Component</th><th>Version</th><th>Licence</th>"
         "<th>Used for</th><th>Local changes</th></tr>"]
    for name, ver, url, lic, where, what, changes, _ in COMPONENTS:
        h.append(f'<tr><td><a href="{esc(url)}">{esc(name)}</a><br><small><code>{esc(where)}</code></small></td>'
                 f"<td>{esc(ver)}</td><td>{esc(lic)}</td><td>{inline(what)}</td><td>{inline(changes)}</td></tr>")
    h.append("</table>")
    h.append("<h2>Licence texts</h2>")
    seen = set()
    for name, ver, url, lic, where, what, changes, files in COMPONENTS:
        for path in files:
            text = read(path)
            h.append(f"<h3>{esc(name)} &mdash; <code>{esc(path)}</code></h3>")
            if text in seen:
                h.append("<p class=note>The same text as above.</p>")
                continue
            seen.add(text)
            h.append(f"<pre>{esc(text)}</pre>")
    h.append("</body></html>")
    with open(os.path.join(ROOT, "THIRD_PARTY_NOTICES.html"), "w", encoding="utf-8") as f:
        f.write("\n".join(h))


if __name__ == "__main__":
    main()
