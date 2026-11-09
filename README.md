# tFire

A web browser built from its own engine (NetSurf's libhubbub/libcss, QuickJS, own layout), running in a single SDL2 window with mbedTLS for HTTPS. The same engine runs in [tOS](https://github.com/Artfical/tOS).

## What it does

- HTTP/HTTPS with mbedTLS (TLS 1.2/1.3, certificate verification, HSTS, OCSP stapling)
- HTML5 parsing (libhubbub), CSS (libcss), JavaScript (QuickJS, no Atomics), own layout
- WebP, SVG (NanoSVG), WebAssembly (wasm3)
- Cookies, HSTS, redirects, chunked and gzip/deflate bodies
- Keyboard shortcuts: Ctrl+L (address), Ctrl+R/F5 (reload), Alt+←/→ (back/forward), Ctrl+V (paste from clipboard)

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

## Install

Download the latest build from [Releases](https://github.com/Artfical/tFire/releases). It's a single file with no dependencies (on Windows and macOS it includes everything; on Linux it needs SDL2).

## Update

Built-in: check **Settings → Check for updates**. It uses Ed25519 signatures and the same server as tOS (`update.artfical.com/tfire`).

## Licence

AGPL-3.0 (see `LICENSE`). Third-party licences in `THIRD_PARTY_NOTICES.md` and `tfire:licenses` in the app.

## Security

See [SECURITY.md](SECURITY.md). Report vulnerabilities to `security@artfical.com`.
