/*
 * tFire for desktops: one window around tFire's app (app/tfire.c, the browser tOS runs), drawn with the same
 * software drawing code into a picture that SDL puts on the screen.
 *
 *   tfire [address]
 *   tfire --screenshot out.png [--size WxH] [--wait seconds] address     (no window: draws the page to a PNG)
 *   tfire --version
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <SDL.h>

#include "uiapp.h"
#include "uihost.h"
#include "gfx.h"
#include "updater.h"

/* keys the desktop version adds (app/tfire.c) */
#define TF_KEY_URL      0x200001
#define TF_KEY_RELOAD   0x200002
#define TF_KEY_BACK     0x200003
#define TF_KEY_FORWARD  0x200004

extern void tf_free(void *p);

/* ------------------------------------------------------------------ a PNG for --screenshot (stored, not compressed) */

static uint32_t crc_table[256];
static uint32_t crc32_of(uint32_t crc, const uint8_t *p, size_t n)
{
    if (!crc_table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_table[i] = c; }
    crc = ~crc;
    while (n--) crc = crc_table[(crc ^ *p++) & 255] ^ (crc >> 8);
    return ~crc;
}
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8], c[4];
    put32(h, n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(d, 1, n, f);
    uint32_t crc = crc32_of(0, (const uint8_t *)type, 4);
    if (n) { crc = ~crc; crc = crc32_of(~crc, d, n); }
    put32(c, crc);
    fwrite(c, 1, 4, f);
}
static int write_png(const char *path, const uint32_t *px, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    put32(ihdr, (uint32_t)w); put32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t row = 1 + (size_t)w * 3, raw = row * (size_t)h;
    uint8_t *img = malloc(raw);
    for (int y = 0; y < h; y++) {
        img[y * row] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            img[y * row + 1 + x * 3] = (uint8_t)(c >> 16); img[y * row + 2 + x * 3] = (uint8_t)(c >> 8); img[y * row + 3 + x * 3] = (uint8_t)c;
        }
    }
    size_t zl = raw + (raw / 65535 + 1) * 5 + 6;
    uint8_t *z = malloc(zl);
    size_t o = 0;
    z[o++] = 0x78; z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + img[i]) % 65521; b = (b + a) % 65521; }
    for (size_t pos = 0; pos < raw;) {
        size_t n = raw - pos > 65535 ? 65535 : raw - pos;
        z[o++] = pos + n == raw; z[o++] = (uint8_t)n; z[o++] = (uint8_t)(n >> 8); z[o++] = (uint8_t)~n; z[o++] = (uint8_t)(~n >> 8);
        memcpy(z + o, img + pos, n); o += n; pos += n;
    }
    put32(z + o, (b << 16) | a); o += 4;
    chunk(f, "IDAT", z, (uint32_t)o);
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(img); free(z);
    return 0;
}

/* ------------------------------------------------------------------ the app */

static uiapp_t *app_new(const uiapp_class_t *cls, int w, int h, const char *arg)
{
    uiapp_t *a = calloc(1, sizeof(*a));
    a->cls = cls;
    a->st = calloc(1, (size_t)cls->state_size);
    a->w = w; a->h = h;
    snprintf(a->title, sizeof(a->title), "%s", cls->title);
    cls->init(a, arg);
    return a;
}

static void usage(void)
{
    puts("usage: tfire [address]\n"
         "       tfire --screenshot out.png [--size WxH] [--wait seconds] address\n"
         "       tfire --version");
}

static int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int c = *p, n = 0;
    if (c < 0x80) { n = 1; }
    else if ((c & 0xE0) == 0xC0) { c &= 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; n = 3; }
    else { c &= 0x07; n = 4; }
    for (int i = 1; i < n; i++) c = (c << 6) | (p[i] & 0x3F);
    *s += n;
    return c;
}

int main(int argc, char **argv)
{
    const char *shot = 0, *addr = 0;
    int W = 1100, H = 760, wait_s = 12;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--version")) { printf("tFire %s (%s)\n", TFIRE_VERSION, TFIRE_PLATFORM); return 0; }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &W, &H);
        else if (!strcmp(argv[i], "--wait") && i + 1 < argc) wait_s = atoi(argv[++i]);
        else if (argv[i][0] != '-') addr = argv[i];
    }
    if (gfx_init() != 0) { fprintf(stderr, "tfire: the fonts and icons did not load\n"); return 1; }
    ui_set_dark(getenv("TFIRE_LIGHT") ? 0 : 1);
    const uiapp_class_t *cls = uiapp_find("tfire");

    if (shot) {                                           /* no window: the page drawn once it has loaded */
        SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS);
        uiapp_t *a = app_new(cls, W, H, addr);
        uint32_t *px = calloc((size_t)W * H, 4);
        gfx_surface_t s;
        gfx_surface_init(&s, px, W, H);
        uint32_t t0 = SDL_GetTicks();
        for (;;) {
            cls->draw(a, &s, 0, 0, W, H);
            int redraw = cls->tick(a);
            SDL_Delay(30);
            if (!redraw && SDL_GetTicks() - t0 > 2500) break;
            if (SDL_GetTicks() - t0 > (uint32_t)wait_s * 1000u) break;
        }
        cls->draw(a, &s, 0, 0, W, H);
        int rc = write_png(shot, px, W, H);
        printf("tfire: %s [%s]\n", rc == 0 ? "wrote" : "could not write", shot);
        return rc ? 1 : 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "tfire: %s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    SDL_Window *win = SDL_CreateWindow("tFire", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "tfire: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "tfire: %s\n", SDL_GetError()); return 1; }
    SDL_GetWindowSize(win, &W, &H);
    uiapp_t *a = app_new(cls, W, H, addr);
    SDL_Texture *tex = 0;
    uint32_t *px = 0;
    gfx_surface_t surf;
    int tw = 0, th = 0, running = 1, need = 1, down = 0;
    char title[sizeof(a->title)] = "";
    double wheel_acc = 0;
    updater_start();
    SDL_StartTextInput();

    while (running) {
        SDL_Event e;
        int idle = 1;
        while (SDL_PollEvent(&e)) {
            idle = 0;
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_EXPOSED) need = 1;
                else if (e.window.event == SDL_WINDOWEVENT_LEAVE && cls->hover) cls->hover(a, -1, -1);
                break;
            case SDL_MOUSEBUTTONDOWN:
                down = 1;
                if (cls->press) cls->press(a, e.button.x, e.button.y, e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 3);
                need = 1;
                break;
            case SDL_MOUSEBUTTONUP:
                down = 0;
                if (cls->release) cls->release(a, e.button.x, e.button.y);
                need = 1;
                break;
            case SDL_MOUSEMOTION:
                if (down && cls->drag) cls->drag(a, e.motion.x, e.motion.y);
                else if (!down && cls->hover) cls->hover(a, e.motion.x, e.motion.y);
                break;
            case SDL_MOUSEWHEEL: {
                wheel_acc += e.wheel.preciseY * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1 : -1);
                int d = (int)wheel_acc;
                if (d) { wheel_acc -= d; if (cls->wheel) cls->wheel(a, d); need = 1; }
                break;
            }
            case SDL_TEXTINPUT:
                if (!(SDL_GetModState() & (KMOD_CTRL | KMOD_ALT))) {
                    for (const char *s = e.text.text; *s;) { int cp = utf8_next(&s); if (cp >= 32 && cp != 127 && cls->key) cls->key(a, cp); }
                    need = 1;
                }
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode k = e.key.keysym.sym;
                SDL_Keymod m = SDL_GetModState();
                int ctrl = (m & (KMOD_CTRL | KMOD_GUI)) != 0, alt = (m & KMOD_ALT) != 0, key = 0;
                if (ctrl && k == SDLK_q) { running = 0; break; }
                if (ctrl && k == SDLK_l) key = TF_KEY_URL;
                else if ((ctrl && k == SDLK_r) || k == SDLK_F5) key = TF_KEY_RELOAD;
                else if (alt && k == SDLK_LEFT) key = TF_KEY_BACK;
                else if (alt && k == SDLK_RIGHT) key = TF_KEY_FORWARD;
                else if (ctrl && k == SDLK_v) {                       /* the system's clipboard, as typing */
                    char *clip = SDL_GetClipboardText();
                    if (clip) { for (const char *s = clip; *s;) { int cp = utf8_next(&s); if (cp >= 32 && cp != 127) cls->key(a, cp); } SDL_free(clip); }
                    need = 1;
                    break;
                }
                else if (ctrl || alt) break;
                else switch (k) {
                case SDLK_UP: key = UI_KEY_UP; break;
                case SDLK_DOWN: key = UI_KEY_DOWN; break;
                case SDLK_LEFT: key = UI_KEY_LEFT; break;
                case SDLK_RIGHT: key = UI_KEY_RIGHT; break;
                case SDLK_RETURN: case SDLK_KP_ENTER: key = '\n'; break;
                case SDLK_BACKSPACE: key = '\b'; break;
                case SDLK_ESCAPE: key = UI_KEY_ESC; break;
                case SDLK_DELETE: key = UI_KEY_DEL; break;
                case SDLK_TAB: key = '\t'; break;
                case SDLK_PAGEDOWN: key = ' '; break;
                case SDLK_PAGEUP: if (cls->wheel) cls->wheel(a, -(H / 64)); need = 1; break;
                case SDLK_HOME: if (cls->wheel) cls->wheel(a, -100000); need = 1; break;
                case SDLK_END: if (cls->wheel) cls->wheel(a, 100000); need = 1; break;
                default: break;
                }
                if (key && cls->key) { cls->key(a, key); need = 1; }
                break;
            }
            default: break;
            }
        }
        int redraw = cls->tick ? cls->tick(a) : 0;
        if (redraw || a->dirty || need) {
            int ww, wh, ow, oh;
            SDL_GetWindowSize(win, &ww, &wh);
            SDL_GetRendererOutputSize(ren, &ow, &oh);
            if (ww != tw || wh != th || !tex) {
                if (tex) SDL_DestroyTexture(tex);
                free(px);
                tw = ww; th = wh;
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tw, th);
                px = calloc((size_t)tw * th, 4);
                gfx_surface_init(&surf, px, tw, th);
                a->w = tw; a->h = th;
            }
            cls->draw(a, &surf, 0, 0, tw, th);
            a->dirty = 0;
            need = 0;
            SDL_UpdateTexture(tex, 0, px, tw * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, 0, 0);
            SDL_RenderPresent(ren);
            if (strcmp(title, a->title)) {
                snprintf(title, sizeof(title), "%s", a->title);
                char full[sizeof(title) + 16];
                snprintf(full, sizeof(full), "%s - tFire", title);
                SDL_SetWindowTitle(win, a->title[0] ? full : "tFire");
            }
            idle = 0;
        }
        updater_poll(a);
        if (idle) SDL_Delay(8);
    }
    if (cls->close) cls->close(a);
    SDL_Quit();
    return 0;
}
