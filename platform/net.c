/*
 * tFire's fetching on a desktop: name lookup and sockets from the system, TLS from mbedTLS, one thread per request
 * (the engine polls the state), redirects, cookies, chunked and compressed (gzip, deflate) bodies.
 * The same interface as tOS's own kernel/net/webfetch.c (engine/webfetch.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <SDL.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_BAD INVALID_SOCKET
#define sock_close closesocket
#else
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
typedef int sock_t;
#define SOCK_BAD (-1)
#define sock_close close
#endif

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

#include "webfetch.h"
#include "png.h"                 /* inflate_raw_buffer_ex */

extern const unsigned char tf_cacert[];
extern const unsigned long tf_cacert_len;

#define WEBFETCH_MAX   (64 * 1024 * 1024)
#define WEBFETCH_HOPS  10
#define IO_TIMEOUT_MS  25000
#define HDR_MAX        65536

static SDL_mutex *state_lock;          /* webfetch_t.state / abandoned, and the cookie jar */
static SDL_mutex *tls_lock;            /* the shared TLS configuration */

static void lock(SDL_mutex *m) { SDL_LockMutex(m); }
static void unlock(SDL_mutex *m) { SDL_UnlockMutex(m); }

static void net_init_once(void)
{
    static int done;
    if (done) return;
    done = 1;
    state_lock = SDL_CreateMutex();
    tls_lock = SDL_CreateMutex();
#ifdef _WIN32
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

/* ------------------------------------------------------------------ small string helpers */

static int lcase(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static void cat(char *dst, const char *src, int cap)
{
    int l = (int)strlen(dst);
    while (*src && l < cap - 1) dst[l++] = *src++;
    dst[l] = 0;
}

static void dbg(const char *what, int a, int b)
{
    if (getenv("TFIRE_DEBUG")) fprintf(stderr, "[net] %s %d %d\n", what, a, b);
}

static void fail(webfetch_t *f, const char *a, const char *b)
{
    strncpy(f->error, a, sizeof(f->error) - 1);
    if (b) {
        int l = (int)strlen(f->error);
        strncpy(f->error + l, b, sizeof(f->error) - 1 - l);
    }
    f->error[sizeof(f->error) - 1] = 0;
}

/* "http(s)://host[:port]/path" -> its parts; 0 = fine */
static int split(const char *url, int *tls, char *host, int hcap, int *port, char *path, int pcap)
{
    if (!strncmp(url, "https://", 8)) { *tls = 1; url += 8; }
    else if (!strncmp(url, "http://", 7)) { *tls = 0; url += 7; }
    else return -1;
    int i = 0;
    if (*url == '[') {                                   /* [::1]:8080 */
        url++;
        while (*url && *url != ']' && i < hcap - 1) host[i++] = *url++;
        if (*url == ']') url++;
    } else {
        while (*url && *url != '/' && *url != ':' && *url != '?' && *url != '#' && i < hcap - 1) host[i++] = *url++;
    }
    host[i] = 0;
    if (!i) return -1;
    *port = *tls ? 443 : 80;
    if (*url == ':') {
        url++;
        int v = 0, d = 0;
        while (*url >= '0' && *url <= '9') { v = v * 10 + (*url++ - '0'); if (++d > 5) return -1; }
        if (!d || !v || v > 65535) return -1;
        *port = v;
    }
    int j = 0;
    if (*url != '/') path[j++] = '/';
    while (*url && *url != '#' && j < pcap - 1) path[j++] = *url++;
    path[j] = 0;
    return 0;
}

/* ------------------------------------------------------------------ cookies */

#define COOKIES 400
typedef struct {
    char name[64];
    char domain[128];                  /* lower case, without a leading dot */
    char path[128];
    char *value;                       /* malloc'd */
    uint8_t host_only, secure, http_only;
} cookie_t;
static cookie_t jar[COOKIES];
static int njar;

static void url_parts(const char *url, char *host, int hcap, char *path, int pcap, int *secure)
{
    *secure = !strncmp(url, "https://", 8);
    const char *p = strstr(url, "://");
    p = p ? p + 3 : url;
    int i = 0;
    while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#' && i < hcap - 1) host[i++] = (char)lcase(*p++);
    host[i] = 0;
    while (*p && *p != '/' && *p != '?' && *p != '#') p++;
    int j = 0;
    if (*p != '/') path[j++] = '/';
    while (*p && *p != '?' && *p != '#' && j < pcap - 1) path[j++] = *p++;
    path[j] = 0;
}

static int domain_match(const char *host, const cookie_t *c)
{
    if (!strcmp(host, c->domain)) return 1;
    if (c->host_only) return 0;
    int hl = (int)strlen(host), dl = (int)strlen(c->domain);
    return hl > dl && host[hl - dl - 1] == '.' && !strcmp(host + hl - dl, c->domain);
}

static int path_match(const char *path, const char *cp)
{
    int l = (int)strlen(cp);
    if (strncmp(path, cp, (size_t)l)) return 0;
    return path[l] == 0 || path[l] == '/' || (l && cp[l - 1] == '/');
}

static int cookie_attr(const char *a, const char *e, const char *name, char *out, int cap)
{
    int nl = (int)strlen(name);
    const char *p = a;
    while (p < e) {
        while (p < e && (*p == ';' || *p == ' ')) p++;
        const char *s = p;
        while (p < e && *p != ';') p++;
        int k = 0;
        while (k < nl && s + k < p && lcase(s[k]) == name[k]) k++;
        if (k == nl && (s + k == p || s[k] == '=' || s[k] == ' ')) {
            const char *v = s + k;
            while (v < p && (*v == ' ' || *v == '=')) v++;
            int o = 0;
            while (v < p && o < cap - 1) out[o++] = *v++;
            while (o && out[o - 1] == ' ') o--;
            out[o] = 0;
            return 1;
        }
    }
    return 0;
}

void webfetch_cookie_set(const char *url, const char *line, int for_script)
{
    net_init_once();
    char host[128], path[128], tmp[128];
    int secure;
    url_parts(url, host, sizeof(host), path, sizeof(path), &secure);
    const char *e = line;
    while (*e && *e != '\r' && *e != '\n') e++;
    while (*line == ' ') line++;
    const char *eq = line;
    while (eq < e && *eq != '=' && *eq != ';') eq++;
    if (eq >= e || *eq != '=') return;
    cookie_t c;
    memset(&c, 0, sizeof(c));
    int nl = (int)(eq - line);
    while (nl && line[nl - 1] == ' ') nl--;
    if (nl <= 0 || nl >= (int)sizeof(c.name)) return;
    memcpy(c.name, line, (size_t)nl);
    const char *v = eq + 1, *ve = v;
    while (ve < e && *ve != ';') ve++;
    if (ve - v > 4000) return;
    int remove = 0;
    if (cookie_attr(ve, e, "max-age", tmp, sizeof(tmp)) && (tmp[0] == '-' || (tmp[0] == '0' && !tmp[1]))) remove = 1;
    if (cookie_attr(ve, e, "expires", tmp, sizeof(tmp)) && (strstr(tmp, "1970") || strstr(tmp, "1969"))) remove = 1;
    c.http_only = (uint8_t)cookie_attr(ve, e, "httponly", tmp, sizeof(tmp));
    c.secure = (uint8_t)cookie_attr(ve, e, "secure", tmp, sizeof(tmp));
    if (for_script && c.http_only) return;
    if (cookie_attr(ve, e, "domain", tmp, sizeof(tmp)) && tmp[0]) {
        const char *d = tmp[0] == '.' ? tmp + 1 : tmp;
        int i = 0;
        for (; d[i] && i < (int)sizeof(c.domain) - 1; i++) c.domain[i] = (char)lcase(d[i]);
        c.domain[i] = 0;
        cookie_t probe = c;
        if (!domain_match(host, &probe) || !strchr(c.domain, '.')) return;    /* not for another site, not for a whole TLD */
    } else {
        strcpy(c.domain, host);
        c.host_only = 1;
    }
    if (cookie_attr(ve, e, "path", tmp, sizeof(tmp)) && tmp[0] == '/') strcpy(c.path, tmp);
    else {
        strcpy(c.path, path);
        char *sl = 0;
        for (char *q = c.path; *q; q++) if (*q == '/') sl = q;
        if (sl && sl != c.path) *sl = 0; else strcpy(c.path, "/");
    }
    char *val = remove ? 0 : malloc((size_t)(ve - v) + 1);
    if (val) { memcpy(val, v, (size_t)(ve - v)); val[ve - v] = 0; }
    if (!remove && !val) return;
    char *old = 0;
    lock(state_lock);
    int i;
    for (i = 0; i < njar; i++)
        if (!strcmp(jar[i].name, c.name) && !strcmp(jar[i].domain, c.domain) && !strcmp(jar[i].path, c.path)) break;
    if (i < njar) {
        if (for_script && jar[i].http_only) { unlock(state_lock); free(val); return; }
        old = jar[i].value;
        if (remove) { jar[i] = jar[--njar]; }
        else { c.value = val; jar[i] = c; }
    } else if (!remove) {
        if (njar == COOKIES) { old = jar[0].value; memmove(&jar[0], &jar[1], sizeof(cookie_t) * (COOKIES - 1)); njar--; }
        c.value = val;
        jar[njar++] = c;
    }
    unlock(state_lock);
    free(old);
}

int webfetch_cookie_header(const char *url, char *out, int cap, int for_script)
{
    net_init_once();
    char host[128], path[WEBFETCH_URL_LEN];
    int secure, o = 0;
    url_parts(url, host, sizeof(host), path, sizeof(path), &secure);
    out[0] = 0;
    lock(state_lock);
    for (int i = 0; i < njar; i++) {
        cookie_t *c = &jar[i];
        if (!domain_match(host, c) || !path_match(path, c->path) || (c->secure && !secure) || (for_script && c->http_only)) continue;
        int need = (int)strlen(c->name) + 1 + (int)strlen(c->value) + 2;
        if (o + need >= cap) break;
        if (o) { out[o++] = ';'; out[o++] = ' '; }
        strcpy(out + o, c->name); o += (int)strlen(c->name);
        out[o++] = '=';
        strcpy(out + o, c->value); o += (int)strlen(c->value);
        out[o] = 0;
    }
    unlock(state_lock);
    return o;
}

static void take_cookies(const char *url, const uint8_t *r, int hlen)
{
    for (int i = 0; i + 11 < hlen; i++) {
        if (i && r[i - 1] != '\n') continue;
        static const char sc[] = "set-cookie:";
        int k = 0;
        while (k < 11 && lcase(r[i + k]) == sc[k]) k++;
        if (k < 11) continue;
        webfetch_cookie_set(url, (const char *)r + i + 11, 0);
    }
}

/* ------------------------------------------------------------------ the connection: TCP, then TLS */

typedef struct {
    sock_t s;
    int tls;
    mbedtls_ssl_context ssl;
} conn_t;

static mbedtls_ssl_config tls_conf;
static mbedtls_x509_crt tls_ca;
static mbedtls_entropy_context tls_entropy;
static mbedtls_ctr_drbg_context tls_drbg;
static int tls_ready;                  /* 1 = configured, -1 = it failed */

static int tls_setup(void)
{
    lock(tls_lock);
    if (!tls_ready) {
        mbedtls_ssl_config_init(&tls_conf);
        mbedtls_x509_crt_init(&tls_ca);
        mbedtls_entropy_init(&tls_entropy);
        mbedtls_ctr_drbg_init(&tls_drbg);
        int ok = mbedtls_ctr_drbg_seed(&tls_drbg, mbedtls_entropy_func, &tls_entropy, (const unsigned char *)"tfire", 5) == 0 &&
                 mbedtls_x509_crt_parse(&tls_ca, tf_cacert, (size_t)tf_cacert_len + 1) >= 0 &&
                 mbedtls_ssl_config_defaults(&tls_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0;
        if (ok) {
            mbedtls_ssl_conf_authmode(&tls_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
            mbedtls_ssl_conf_ca_chain(&tls_conf, &tls_ca, 0);
            mbedtls_ssl_conf_rng(&tls_conf, mbedtls_ctr_drbg_random, &tls_drbg);
            mbedtls_ssl_conf_min_tls_version(&tls_conf, MBEDTLS_SSL_VERSION_TLS1_2);
        }
        tls_ready = ok ? 1 : -1;
    }
    int r = tls_ready;
    unlock(tls_lock);
    return r == 1 ? 0 : -1;
}

static int wait_sock(sock_t s, int write, int ms)
{
#ifdef _WIN32
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    return select(0, write ? 0 : &fds, write ? &fds : 0, 0, &tv);
#else
    struct pollfd p = { s, (short)(write ? POLLOUT : POLLIN), 0 };
    return poll(&p, 1, ms);
#endif
}

static void set_nonblocking(sock_t s, int on)
{
#ifdef _WIN32
    u_long m = on;
    ioctlsocket(s, FIONBIO, &m);
#else
    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
#endif
}

static int tcp_open(sock_t *out, const char *host, int port, char *err, int ecap)
{
    char ps[8];
    snprintf(ps, sizeof(ps), "%d", port);
    struct addrinfo hints, *res = 0, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, ps, &hints, &res);
    if (rc != 0 || !res) { snprintf(err, (size_t)ecap, "Can't find the server %s", host); return -1; }
    sock_t s = SOCK_BAD;
    for (ai = res; ai; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == SOCK_BAD) continue;
        set_nonblocking(s, 1);
        int c = connect(s, ai->ai_addr, (int)ai->ai_addrlen);
        int ok = c == 0;
        if (!ok) {
#ifdef _WIN32
            int pending = WSAGetLastError() == WSAEWOULDBLOCK;
#else
            int pending = errno == EINPROGRESS;
#endif
            if (pending && wait_sock(s, 1, 12000) > 0) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &sl);
                ok = soerr == 0;
            }
        }
        if (ok) break;
        sock_close(s);
        s = SOCK_BAD;
    }
    freeaddrinfo(res);
    if (s == SOCK_BAD) { snprintf(err, (size_t)ecap, "Couldn't connect to %s", host); return -1; }
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    *out = s;
    return 0;
}

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    conn_t *c = ctx;
    for (;;) {
        int n = (int)send(c->s, (const char *)buf, (int)len, 0);
        if (n >= 0) return n;
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) { if (wait_sock(c->s, 1, IO_TIMEOUT_MS) > 0) continue; return MBEDTLS_ERR_SSL_TIMEOUT; }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) { if (wait_sock(c->s, 1, IO_TIMEOUT_MS) > 0) continue; return MBEDTLS_ERR_SSL_TIMEOUT; }
        if (errno == EINTR) continue;
#endif
        return (-0x004E);
    }
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len)
{
    conn_t *c = ctx;
    for (;;) {
        int n = (int)recv(c->s, (char *)buf, (int)len, 0);
        if (n > 0) return n;
        if (n == 0) return 0;
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) { if (wait_sock(c->s, 0, IO_TIMEOUT_MS) > 0) continue; return MBEDTLS_ERR_SSL_TIMEOUT; }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) { if (wait_sock(c->s, 0, IO_TIMEOUT_MS) > 0) continue; return MBEDTLS_ERR_SSL_TIMEOUT; }
        if (errno == EINTR) continue;
#endif
        return (-0x004C);
    }
}

static int conn_open(conn_t *c, int tls, const char *host, int port, char *err, int ecap)
{
    memset(c, 0, sizeof(*c));
    c->s = SOCK_BAD;
    if (tcp_open(&c->s, host, port, err, ecap)) return -1;
    c->tls = tls;
    if (!tls) return 0;
    if (tls_setup()) { snprintf(err, (size_t)ecap, "TLS could not start"); sock_close(c->s); return -1; }
    mbedtls_ssl_init(&c->ssl);
    if (mbedtls_ssl_setup(&c->ssl, &tls_conf) != 0 || mbedtls_ssl_set_hostname(&c->ssl, host) != 0) {
        snprintf(err, (size_t)ecap, "TLS could not start");
        mbedtls_ssl_free(&c->ssl);
        sock_close(c->s);
        return -1;
    }
    mbedtls_ssl_set_bio(&c->ssl, c, bio_send, bio_recv, 0);
    int rc;
    while ((rc = mbedtls_ssl_handshake(&c->ssl)) != 0) {
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
            uint32_t fl = mbedtls_ssl_get_verify_result(&c->ssl);
            char why[128];
            mbedtls_x509_crt_verify_info(why, sizeof(why), "", fl);
            for (char *q = why; *q; q++) if (*q == '\n') *q = 0;
            snprintf(err, (size_t)ecap, "The certificate of %s can't be trusted (%s)", host, why);
        } else {
            char why[80];
            mbedtls_strerror(rc, why, sizeof(why));
            snprintf(err, (size_t)ecap, "Couldn't connect securely to %s: %s", host, why);
        }
        mbedtls_ssl_free(&c->ssl);
        sock_close(c->s);
        return -1;
    }
    return 0;
}

static int conn_write(conn_t *c, const void *p, int n)
{
    const char *b = p;
    while (n > 0) {
        int k = c->tls ? mbedtls_ssl_write(&c->ssl, (const unsigned char *)b, (size_t)n) : bio_send(c, (const unsigned char *)b, (size_t)n);
        if (k == MBEDTLS_ERR_SSL_WANT_READ || k == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (k <= 0) return -1;
        b += k; n -= k;
    }
    return 0;
}

static int conn_read(conn_t *c, uint8_t *p, int n)
{
    for (;;) {
        int k = c->tls ? mbedtls_ssl_read(&c->ssl, p, (size_t)n) : bio_recv(c, p, (size_t)n);
        if (k == MBEDTLS_ERR_SSL_WANT_READ || k == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (k == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
        return k;
    }
}

static void conn_close(conn_t *c)
{
    if (c->tls) {
        mbedtls_ssl_close_notify(&c->ssl);
        mbedtls_ssl_free(&c->ssl);
    }
    if (c->s != SOCK_BAD) sock_close(c->s);
}

/* ------------------------------------------------------------------ HTTP */

static int hdr_end(const uint8_t *r, int total)
{
    for (int i = 0; i + 3 < total; i++)
        if (r[i] == '\r' && r[i + 1] == '\n' && r[i + 2] == '\r' && r[i + 3] == '\n') return i + 4;
    return -1;
}

static const char *hdr_value(const char *r, int hlen, const char *name)
{
    int nl = (int)strlen(name);
    for (int i = 0; i + nl < hlen; i++) {
        if (i != 0 && r[i - 1] != '\n') continue;
        int k = 0;
        while (k < nl && lcase(r[i + k]) == name[k]) k++;
        if (k == nl) {
            const char *v = r + i + nl;
            while (v < r + hlen && (*v == ' ' || *v == '\t')) v++;
            return v;
        }
    }
    return 0;
}

static const char *ua(void)
{
    const char *e = getenv("TFIRE_UA");
    return e && *e ? e : "Mozilla/5.0 (compatible; tFire/" TFIRE_VERSION "; +https://github.com/Artfical/tfire)";
}

/* the request's lines; returns its length or -1 */
static int build_request(char *req, int cap, const char *method, const char *host, int port, int tls, const char *path,
                         const char *extra, int body_len)
{
    char hostport[300];
    if (port != (tls ? 443 : 80)) snprintf(hostport, sizeof(hostport), "%s:%d", host, port);
    else snprintf(hostport, sizeof(hostport), "%s", host);
    for (const char *q = path; *q; q++) if ((unsigned char)*q <= 0x20 || *q == 0x7F) return -1;
    int n = snprintf(req, (size_t)cap, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: */*\r\n"
                     "Accept-Encoding: gzip, deflate\r\nConnection: close\r\n",
                     method, path, hostport, ua());
    if (n <= 0 || n >= cap) return -1;
    if (extra) {
        for (const char *h = extra; *h; h++)
            if ((*h == '\n' && (h == extra || h[-1] != '\r')) || (*h == '\r' && h[1] != '\n')) return -1;
        int l = (int)strlen(extra);
        if (n + l >= cap) return -1;
        memcpy(req + n, extra, (size_t)l);
        n += l;
    }
    if (body_len > 0) {
        int k = snprintf(req + n, (size_t)(cap - n), "Content-Length: %d\r\n", body_len);
        if (k <= 0 || n + k >= cap) return -1;
        n += k;
    }
    if (n + 2 >= cap) return -1;
    req[n++] = '\r'; req[n++] = '\n';
    req[n] = 0;
    return n;
}

/* rewrites a chunked body (after the header block) into a plain one in place; the new total, or -1 */
static int dechunk(uint8_t *r, int total, int he)
{
    int in = he, out = he;
    for (;;) {
        long sz = 0;
        int digits = 0;
        while (in < total) {
            int c = lcase(r[in]);
            int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
            if (d < 0) break;
            sz = sz * 16 + d;
            if (++digits > 8) return -1;
            in++;
        }
        if (digits == 0) return -1;
        while (in < total && r[in] != '\n') in++;
        if (in >= total) return out;                       /* cut short: what there is */
        in++;
        if (sz == 0) return out;
        if (sz > total - in) sz = total - in;
        memmove(r + out, r + in, (size_t)sz);
        out += (int)sz;
        in += (int)sz;
        if (in + 1 < total && r[in] == '\r' && r[in + 1] == '\n') in += 2;
        else if (in >= total) return out;
        else return -1;
    }
}

/* the whole response (headers, then the body made plain: dechunked and decompressed) into *raw (malloc'd, 0-terminated);
 * its length, or a negative number with err filled */
static int http_exchange(const char *host, int port, int tls, const char *method, const char *path, const char *extra,
                         const uint8_t *body, int body_len, uint8_t **raw, char *err, int ecap)
{
    conn_t c;
    if (conn_open(&c, tls, host, port, err, ecap)) return -1;
    char req[8192];
    int rn = build_request(req, sizeof(req), method, host, port, tls, path, extra, body_len);
    if (rn < 0) { snprintf(err, (size_t)ecap, "The address or a header is not valid"); conn_close(&c); return -1; }
    if (conn_write(&c, req, rn) || (body_len > 0 && conn_write(&c, body, body_len))) {
        snprintf(err, (size_t)ecap, "The connection to %s dropped", host);
        conn_close(&c);
        return -1;
    }
    int cap = 65536, total = 0;
    uint8_t *buf = malloc((size_t)cap + 1);
    if (!buf) { snprintf(err, (size_t)ecap, "Not enough memory"); conn_close(&c); return -1; }
    int he = -1, want = -1, chunked = 0;
    for (;;) {
        if (total == cap) {
            if (cap >= WEBFETCH_MAX) break;
            int ncap = cap * 2 > WEBFETCH_MAX ? WEBFETCH_MAX : cap * 2;
            uint8_t *nb = realloc(buf, (size_t)ncap + 1);
            if (!nb) break;
            buf = nb; cap = ncap;
        }
        int n = conn_read(&c, buf + total, cap - total);
        if (n <= 0) {
            if (n < 0 && total == 0) { snprintf(err, (size_t)ecap, "The connection to %s dropped", host); free(buf); conn_close(&c); return -1; }
            break;
        }
        total += n;
        if (he < 0) {
            he = hdr_end(buf, total);
            if (he < 0 && total > HDR_MAX) { snprintf(err, (size_t)ecap, "The server sent a bad header"); free(buf); conn_close(&c); return -1; }
            if (he >= 0) {
                const char *cl = hdr_value((const char *)buf, he, "content-length:");
                if (cl) { want = atoi(cl); if (want < 0) want = -1; }
                const char *te = hdr_value((const char *)buf, he, "transfer-encoding:");
                chunked = te && lcase(te[0]) == 'c';
                if (!strncmp((const char *)buf + 9, "204", 3) || !strncmp((const char *)buf + 9, "304", 3) || !strcmp(method, "HEAD")) want = 0;
            }
        }
        if (he >= 0 && !chunked && want >= 0 && total >= he + want) { total = he + want; break; }
        if (he >= 0 && chunked && total >= 5 && memcmp(buf + total - 5, "0\r\n\r\n", 5) == 0) break;
    }
    conn_close(&c);
    if (he < 0) { snprintf(err, (size_t)ecap, "The server sent no web page"); free(buf); return -1; }
    buf[total] = 0;
    if (chunked) {
        int t = dechunk(buf, total, he);
        if (t < 0) { dbg("dechunk failed", total, he); snprintf(err, (size_t)ecap, "The server sent a broken page"); free(buf); return -1; }
        total = t;
        buf[total] = 0;
    }
    /* Content-Encoding: gzip or deflate */
    const char *ce = hdr_value((const char *)buf, he, "content-encoding:");
    if (ce && total > he && (!strncmp(ce, "gzip", 4) || !strncmp(ce, "x-gzip", 6) || !strncmp(ce, "deflate", 7))) {
        const uint8_t *z = buf + he;
        uint32_t zl = (uint32_t)(total - he), skip = 0;
        if (ce[0] != 'd') {                                /* gzip: a header of its own, then the deflate stream */
            if (zl < 18 || z[0] != 0x1F || z[1] != 0x8B) { dbg("not gzip", (int)zl, z[0]); snprintf(err, (size_t)ecap, "The server sent a broken page"); free(buf); return -1; }
            uint8_t flg = z[3];
            skip = 10;
            if (flg & 4) { if (skip + 2 > zl) goto bad; skip += 2 + (uint32_t)(z[skip] | (z[skip + 1] << 8)); }
            if (flg & 8) { while (skip < zl && z[skip]) skip++; skip++; }
            if (flg & 16) { while (skip < zl && z[skip]) skip++; skip++; }
            if (flg & 2) skip += 2;
            if (skip >= zl) goto bad;
            zl -= skip + 8;                                 /* (the trailer: crc32 and size) */
        } else if (zl >= 2 && (z[0] & 0x0F) == 8) {          /* deflate in a zlib wrapper */
            skip = 2;
            zl -= 6;
        }
        uint32_t ocap = (uint32_t)(total - he) * 6 + 65536;
        for (;;) {
            if (ocap > WEBFETCH_MAX) ocap = WEBFETCH_MAX;
            uint8_t *out = malloc((size_t)he + ocap + 1);
            if (!out) goto bad;
            uint32_t got = 0;
            uint32_t used = 0;
            int rc = inflate_raw_buffer_ex(z + skip, zl, out + he, ocap, &got, &used);
            if (rc == 0 || (rc != 0 && got > 0 && ocap >= WEBFETCH_MAX)) {
                memcpy(out, buf, (size_t)he);
                free(buf);
                buf = out;
                total = he + (int)got;
                buf[total] = 0;
                break;
            }
            dbg("inflate failed rc/ocap", rc, (int)ocap);
            free(out);
            if (ocap >= WEBFETCH_MAX) goto bad;
            ocap *= 4;
        }
    }
    *raw = buf;
    return total;
bad:
    snprintf(err, (size_t)ecap, "The server sent a broken page");
    free(buf);
    return -1;
}

/* 1 = the page is in f, 0 = f->error says why not */
static int fetch(webfetch_t *f)
{
    char url[WEBFETCH_URL_LEN];
    strncpy(url, f->url, sizeof(url) - 1);
    url[sizeof(url) - 1] = 0;
    for (int hop = 0; hop <= WEBFETCH_HOPS; hop++) {
        int tls, port;
        char host[256], path[WEBFETCH_URL_LEN];
        if (split(url, &tls, host, sizeof(host), &port, path, sizeof(path))) { fail(f, "Not a web address: ", url); return 0; }
        char hdrs[6144];
        hdrs[0] = 0;
        if (f->headers) cat(hdrs, f->headers, (int)sizeof(hdrs) - 4400);
        char ck[4096];
        if (webfetch_cookie_header(url, ck, sizeof(ck), 0)) { cat(hdrs, "Cookie: ", sizeof(hdrs)); cat(hdrs, ck, sizeof(hdrs) - 2); cat(hdrs, "\r\n", sizeof(hdrs)); }
        uint8_t *raw = 0;
        char err[120] = "";
        int n = http_exchange(host, port, tls, f->method[0] ? f->method : "GET", path, hdrs, f->req_body_len ? f->req_body : 0,
                              f->req_body_len, &raw, err, sizeof(err));
        if (n < 0) { fail(f, err, 0); return 0; }
        if (n < 12 || strncmp((const char *)raw, "HTTP/1.", 7)) { fail(f, "The server sent no web page", 0); free(raw); return 0; }
        free(f->raw);
        f->raw = raw;
        f->status = (raw[9] - '0') * 100 + (raw[10] - '0') * 10 + (raw[11] - '0');
        strncpy(f->final_url, url, sizeof(f->final_url) - 1);
        f->final_url[sizeof(f->final_url) - 1] = 0;
        int hdr = hdr_end(raw, n);
        if (hdr < 0) hdr = n;
        take_cookies(url, raw, hdr);
        const char *loc = hdr_value((const char *)raw, hdr, "location:");
        if (f->status >= 300 && f->status <= 399 && loc && hop < WEBFETCH_HOPS) {
            if (f->status != 307 && f->status != 308) {      /* a form sent with POST: the answer is fetched with GET */
                f->method[0] = 0;
                f->req_body_len = 0;
            }
            char next[WEBFETCH_URL_LEN];
            int l = 0;
            while (loc[l] && loc[l] != '\r' && loc[l] != '\n' && l < (int)sizeof(next) - 1) { next[l] = loc[l]; l++; }
            next[l] = 0;
            if (!strncmp(next, "http://", 7) || !strncmp(next, "https://", 8)) {
                strcpy(url, next);
            } else if (next[0] == '/' && next[1] == '/') {
                strcpy(url, tls ? "https:" : "http:");
                cat(url, next, sizeof(url));
            } else {
                char base[WEBFETCH_URL_LEN];
                snprintf(base, sizeof(base), "%s://%s", tls ? "https" : "http", host);
                if (port != (tls ? 443 : 80)) { char pb[8]; snprintf(pb, sizeof(pb), ":%d", port); cat(base, pb, sizeof(base)); }
                if (next[0] != '/') {
                    char dir[WEBFETCH_URL_LEN];
                    strcpy(dir, path);
                    char *q = dir;
                    while (*q && *q != '?') q++;
                    *q = 0;
                    char *slash = dir;
                    for (char *c = dir; *c; c++) if (*c == '/') slash = c;
                    slash[1] = 0;
                    cat(base, dir, sizeof(base));
                }
                cat(base, next, sizeof(base));
                strcpy(url, base);
            }
            continue;
        }
        f->body = f->raw + hdr;
        f->body_len = (uint32_t)(n - hdr);
        f->head_len = (uint32_t)hdr;
        const char *ct = hdr_value((const char *)raw, hdr, "content-type:");
        int c = 0;
        if (ct)
            while (ct[c] && ct[c] != ';' && ct[c] != '\r' && ct[c] != '\n' && c < (int)sizeof(f->ctype) - 1) {
                char ch = ct[c];
                f->ctype[c++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
            }
        f->ctype[c] = 0;
        return 1;
    }
    fail(f, "Too many redirects", 0);
    return 0;
}

static int worker(void *arg)
{
    webfetch_t *f = arg;
    int ok = fetch(f);
    lock(state_lock);
    int gone = f->abandoned;
    f->state = ok ? WEBFETCH_DONE : WEBFETCH_FAILED;
    unlock(state_lock);
    if (gone) { free(f->raw); free(f->headers); free(f->req_body); free(f); }
    return 0;
}

webfetch_t *webfetch_start(const char *url) { return webfetch_request(url, 0, 0, 0, 0); }

webfetch_t *webfetch_request(const char *url, const char *method, const char *headers, const void *body, int body_len)
{
    net_init_once();
    webfetch_t *f = calloc(1, sizeof(*f));
    if (!f) return 0;
    strncpy(f->url, url, sizeof(f->url) - 1);
    if (method && strcmp(method, "GET")) {
        int i = 0;
        for (; method[i] && i < (int)sizeof(f->method) - 1; i++) f->method[i] = method[i] >= 'a' && method[i] <= 'z' ? (char)(method[i] - 32) : method[i];
        f->method[i] = 0;
    }
    if (headers && *headers) {
        f->headers = malloc(strlen(headers) + 1);
        if (!f->headers) { free(f); return 0; }
        strcpy(f->headers, headers);
    }
    if (body && body_len > 0) {
        f->req_body = malloc((size_t)body_len);
        if (!f->req_body) { free(f->headers); free(f); return 0; }
        memcpy(f->req_body, body, (size_t)body_len);
        f->req_body_len = body_len;
    }
    SDL_Thread *t = SDL_CreateThread(worker, "tfire-fetch", f);
    if (!t) { free(f->headers); free(f->req_body); free(f); return 0; }
    SDL_DetachThread(t);
    return f;
}

void webfetch_free(webfetch_t *f)
{
    if (!f) return;
    lock(state_lock);
    int running = f->state == WEBFETCH_RUNNING;
    if (running) f->abandoned = 1;
    unlock(state_lock);
    if (!running) { free(f->raw); free(f->headers); free(f->req_body); free(f); }
}

int uihost_fetch_poll(void *h)
{
    webfetch_t *f = h;
    return f->state == WEBFETCH_RUNNING ? 0 : f->state == WEBFETCH_DONE ? 1 : -1;
}

/* a plain GET for the updater: the whole response body (malloc'd) and the status */
int tfire_http_get(const char *url, uint8_t **body, uint32_t *len, int *status, char *err, int ecap)
{
    net_init_once();
    webfetch_t *f = webfetch_start(url);
    if (!f) { snprintf(err, (size_t)ecap, "no memory"); return -1; }
    while (f->state == WEBFETCH_RUNNING) SDL_Delay(20);
    int rc = -1;
    if (f->state == WEBFETCH_DONE) {
        *body = malloc(f->body_len + 1);
        if (*body) {
            memcpy(*body, f->body, f->body_len);
            (*body)[f->body_len] = 0;
            *len = f->body_len;
            *status = f->status;
            rc = 0;
        }
    } else snprintf(err, (size_t)ecap, "%s", f->error);
    webfetch_free(f);
    return rc;
}
