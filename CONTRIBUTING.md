# Contributing to tFire

Thank you for helping. This page tells you how.

## Before you start

- **Bugs and ideas:** open an issue. For a bug, say what you did, what you expected, what happened, and the build (`tfire --version`). A log (`TFIRE_DEBUG=1 tfire <url>`) helps a lot.
- **Bigger changes:** open an issue first so we can agree on the direction before you spend time on it.
- **Security problems:** do not use issues. See [SECURITY.md](SECURITY.md).

## Building and testing

```bash
cmake -B build && cmake --build build -j
./build/tfire https://example.com
```

For a screenshot (no window):
```bash
./build/tfire --screenshot shot.png --size 1000x700 --wait 15 https://example.com
```

## Cross-platform builds

The CI builds for Linux (x64, arm64), macOS (arm64, x64) and Windows (x64, arm64) on every push. See `.github/workflows/release.yml`.

## What to work on

The engine and app are in `engine/` and `app/` (the same code tOS uses). The platform layer is in `platform/` (SDL2, mbedTLS, SDL_mixer). The third-party libraries are in `third_party/`.

If you touch a third-party library, keep its licence file next to it and run `python3 tools/gen_notices.py` to update `THIRD_PARTY_NOTICES.md` and `.html`.
