/*
 * What tFire's engine and its app (the same ones tOS runs) ask of the desktop around them, on a desktop that is
 * not tOS: the clock, files, the clipboard of the system, notifications as dialogs, pictures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "uiapp.h"
#include "uihost.h"
#include "imgload.h"

uint32_t uihost_ms(void) { return (uint32_t)SDL_GetTicks(); }

void uihost_time(int *y, int *mo, int *d, int *h, int *mi, int *s)
{
    time_t now = time(0);
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    *y = tm.tm_year + 1900; *mo = tm.tm_mon + 1; *d = tm.tm_mday; *h = tm.tm_hour; *mi = tm.tm_min; *s = tm.tm_sec;
}

void serial_write(const char *s)
{
    if (getenv("TFIRE_DEBUG")) fputs(s, stderr);
}

void csprng_fill(uint8_t *b, int n)
{
#ifdef _WIN32
    BCryptGenRandom(0, b, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) { size_t got = fread(b, 1, (size_t)n, f); (void)got; fclose(f); }
    else for (int i = 0; i < n; i++) b[i] = (uint8_t)rand();
#endif
}

void *krealloc(void *p, size_t n) { return realloc(p, n); }
void tf_free(void *p) { free(p); }
void tf_alloc_usage(uint32_t *s, uint32_t *b) { *s = *b = 0; }
void memory_get_usage(uint32_t *t, uint32_t *u) { *t = *u = 0; }

/* ------------------------------------------------------------------ files */

const char *uihost_home(void)
{
    static char home[512];
    if (!home[0]) {
        const char *h = getenv("HOME");
#ifdef _WIN32
        if (!h || !*h) h = getenv("USERPROFILE");
#endif
        snprintf(home, sizeof(home), "%s", h && *h ? h : ".");
        for (char *c = home; *c; c++) if (*c == '\\') *c = '/';
    }
    return home;
}

int uihost_list(const char *dir, uihost_entry_t *out, int max)
{
#ifdef _WIN32
    char pat[600];
    snprintf(pat, sizeof(pat), "%s/*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    int n = 0;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..") || n >= max) continue;
        memset(&out[n], 0, sizeof(out[n]));
        snprintf(out[n].name, sizeof(out[n].name), "%s", fd.cFileName);
        out[n].size = fd.nFileSizeLow;
        out[n].is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
#else
    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        memset(&out[n], 0, sizeof(out[n]));
        snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
        if (stat(full, &st) == 0) { out[n].size = (uint32_t)st.st_size; out[n].is_dir = S_ISDIR(st.st_mode) != 0; }
        n++;
    }
    closedir(d);
    return n;
#endif
}

int uihost_read(const char *path, uint8_t **data, uint32_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 0x7FFFFFFF) { fclose(f); return -1; }
    uint8_t *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return -1; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = 0;
    *data = b;
    *len = (uint32_t)got;
    return 0;
}

int uihost_write(const char *path, const void *data, uint32_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t put = fwrite(data, 1, len, f);
    int rc = fclose(f);
    return put == len && rc == 0 ? 0 : -1;
}

int uihost_mkdir(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || uihost_exists(path) == 2 ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 || uihost_exists(path) == 2 ? 0 : -1;
#endif
}

int uihost_delete(const char *path) { return remove(path); }
int uihost_rename(const char *from, const char *to) { return rename(from, to); }

int uihost_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 2 : 1;
}

void uihost_drag_files(const char *paths) { (void)paths; }
unsigned uihost_files_gen(void) { return 0; }
void uihost_files_touch(void) {}

int uihost_keys_held(void)
{
    SDL_Keymod m = SDL_GetModState();
    return ((m & KMOD_SHIFT) ? 1 : 0) | ((m & KMOD_CTRL) ? 2 : 0) | ((m & KMOD_ALT) ? 4 : 0);
}

/* a folder or file in the system's own program ("files" is the folder of a download) */
void uihost_open(const char *app_id, const char *path)
{
    (void)app_id;
    if (!path || !*path) return;
    char url[1100];
    snprintf(url, sizeof(url), "file://%s%s", path[0] == '/' ? "" : "/", path);
    SDL_OpenURL(url);
}

int uihost_set_wallpaper(const char *path) { (void)path; return -1; }

int uihost_notify(const char *app, const char *icon, const char *title, const char *body, const char *act1, const char *act2)
{
    (void)app; (void)icon; (void)act1; (void)act2;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title ? title : "tFire", body ? body : "", 0);
    return 0;
}
int uihost_notify_result(int id) { (void)id; return 1; }

/* ------------------------------------------------------------------ the rest of what a desktop may be asked */

int uihost_tasks(uihost_task_t *out, int max) { (void)out; (void)max; return 0; }
int uihost_kill(uint32_t pid) { (void)pid; return -1; }
void uihost_memory(uint32_t *total_kb, uint32_t *used_kb) { *total_kb = *used_kb = 0; }
void uihost_net(uihost_net_t *n) { memset(n, 0, sizeof(*n)); n->up = 1; }
int uihost_net_start(void) { return 0; }
const char *uihost_system_name(void) { return SDL_GetPlatform(); }
const char *uihost_hostname(void) { return "localhost"; }
int uihost_get(const char *key) { (void)key; return 0; }
void uihost_set(const char *key, int value) { (void)key; (void)value; }
int uihost_users(uihost_user_t *out, int max) { (void)out; (void)max; return 0; }
int uihost_services(uihost_service_t *out, int max) { (void)out; (void)max; return 0; }
int uihost_service_set(const char *name, int on) { (void)name; (void)on; return -1; }
int uihost_add_user(const char *a, const char *b, const char *c, int d) { (void)a; (void)b; (void)c; (void)d; return -1; }
int uihost_set_password(const char *a, const char *b) { (void)a; (void)b; return -1; }
void uihost_authorize(const char *reason) { (void)reason; }
int uihost_authorized(void) { return -1; }
int uihost_volumes(uihost_volume_t *out, int max) { (void)out; (void)max; return 0; }
int uihost_eject(const char *path) { (void)path; return -1; }
int uihost_disks(uihost_disk_t *out, int max) { (void)out; (void)max; return 0; }
int uihost_disk_mount(const char *dev) { (void)dev; return -1; }
int uihost_disk_unmount(const char *dev) { (void)dev; return -1; }

/* ------------------------------------------------------------------ pictures for the engine */

int uihost_image_decode(const uint8_t *data, uint32_t len, int max_w, int max_h, uint8_t **rgba, int *w, int *h)
{
    uint8_t *px;
    uint32_t iw, ih;
    if (img_decode(data, len, &px, &iw, &ih, 0) != 0) return -1;
    if ((uint64_t)iw * ih > 16000000u) { free(px); return -1; }
    uint32_t tw = iw, th = ih;
    if (max_w > 0 && tw > (uint32_t)max_w) { th = th * (uint32_t)max_w / tw; tw = (uint32_t)max_w; }
    if (max_h > 0 && th > (uint32_t)max_h) { tw = tw * (uint32_t)max_h / th; th = (uint32_t)max_h; }
    if (!tw) tw = 1;
    if (!th) th = 1;
    if (tw != iw || th != ih) {
        uint8_t *sc = img_scale(px, iw, ih, tw, th);
        free(px);
        if (!sc) return -1;
        px = sc;
    }
    *rgba = px; *w = (int)tw; *h = (int)th;
    return 0;
}
