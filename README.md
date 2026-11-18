# tFire

A web browser built from its own engine (NetSurf's libhubbub/libcss, QuickJS, own layout), running in a single SDL2 window with mbedTLS for HTTPS. The same engine runs in [tOS](https://github.com/Artfical/tOS).

## What it does

- HTTP/HTTPS with mbedTLS (TLS 1.2/1.3, certificate verification, HSTS, OCSP stapling)
- HTML5 parsing (libhubbub), CSS (libcss), JavaScript (QuickJS, no Atomics), own layout
- WebP, SVG (NanoSVG), WebAssembly (wasm3)
- Cookies, HSTS, redirects, chunked and gzip/deflate bodies
- Tabs: Ctrl+T new, Ctrl+W close, Ctrl+Shift+T reopen the last closed one, Ctrl+Tab / Ctrl+Shift+Tab (or Ctrl+PageDown/PageUp) to switch, Ctrl+1..9 to jump; middle-click a link to open it in a new tab, middle-click a tab to close it. The tabs that were open come back the next time tFire starts without an address (`~/.tfire/session.txt`)
- Find in page: Ctrl+F, Enter / Shift+Enter for the next / previous match, Esc to close (ASCII letters are matched without regard to case; a match lights up the whole word)
- Bookmarks: Ctrl+D or the star in the address bar; Ctrl+Shift+O (`tfire:bookmarks`) lists and removes them; the home page shows them
- History: every page you visit is kept (newest first, 300 at most) in `tfire:history` (Ctrl+H), where entries can be removed or all cleared; the home page lists the latest
- Address bar: a caret (Left/Right/Home/End/Delete), Ctrl+A / Ctrl+C / Ctrl+X / Ctrl+V, click to place the caret, and suggestions from bookmarks and history (Up/Down to choose, Enter to go)
- Mouse back and forward buttons, PageUp/PageDown/Home/End to scroll
- Other shortcuts: Ctrl+L (address), Ctrl+R/F5 (reload), Alt+←/→ (back/forward)

Bookmarks, history and the session are plain text files in `~/.tfire/` (`bookmarks.txt`, `history.txt`, `session.txt`: one `address<TAB>title` per line).

## Build

```bash
cmake -B build -G Ninja -DTFIRE_SDL_STATIC=OFF
cmake --build build -j
./build/tfire https://example.com
```

Screenshot (no window, for CI):

```bash
./build/tfire --screenshot shot.png --size 1000x700 --wait 15 https://example.com
```

`--keys` does things first, as a person would, so a screenshot (or a test) can show tabs, the find bar, and so on. Steps are separated by spaces: `key:ctrl+t`, `key:enter`, `type:hello%20world` (`%20` is a space), `click:x,y[,button]`, `hover:x,y`, `wheel:n`, `wait:milliseconds`.

```bash
./build/tfire --screenshot tabs.png --keys "key:ctrl+t type:example.com key:enter wait:3000 key:ctrl+f type:domain"
```

## Install

Download the latest build from [Releases](https://github.com/Artfical/tFire/releases). It's a single file with no dependencies (on Windows and macOS it includes everything; on Linux it needs SDL2).

## Update

Built-in: check **Settings → Check for updates**. It uses Ed25519 signatures and the same server as tOS (`update.artfical.com/tfire`).

## Licence

AGPL-3.0 (see `LICENSE`). Third-party licences in `THIRD_PARTY_NOTICES.md` and `tfire:licenses` in the app.

## Security

See [SECURITY.md](SECURITY.md). Report vulnerabilities to `security@artfical.com`.
