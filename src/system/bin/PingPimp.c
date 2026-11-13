#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <net/if.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PP_MODDIR_DEFAULT "/data/adb/modules/PingPimp"

static char g_moddir[PATH_MAX];
static char g_logfile[PATH_MAX + 64], g_statefile[PATH_MAX + 64], g_lockdir[PATH_MAX + 64];
static char g_gamelist[PATH_MAX + 64], g_pidfile[PATH_MAX + 64], g_modprop[PATH_MAX + 64];
static const char *g_mode = "system";
static char g_self[PATH_MAX];
static const char *g_procroot; /* PP_PROC_ROOT, NULL di produksi */

/* ---------- util dasar ---------- */

static void pp_path(char *out, size_t n, const char *path) {
    if (g_procroot && strncmp(path, "/proc/", 6) == 0)
        snprintf(out, n, "%s%s", g_procroot, path);
    else
        snprintf(out, n, "%s", path);
}

static char *pp_strcasestr(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (!nl) return (char *)hay;
    for (; *hay; hay++)
        if (strncasecmp(hay, needle, nl) == 0) return (char *)hay;
    return NULL;
}

static void trim_nl(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
}

static void trim_ws(char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    char *d = s;
    while (*d && isspace((unsigned char)*d)) d++;
    if (d != s) memmove(s, d, strlen(d) + 1);
}

static int read_first_line(const char *path, char *out, size_t n) {
    out[0] = 0;
    char p[PATH_MAX];
    pp_path(p, sizeof p, path);
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    if (fgets(out, (int)n, f)) trim_nl(out);
    fclose(f);
    return 0;
}

static int write_sysfile(const char *path, const char *val) {
    char p[PATH_MAX];
    pp_path(p, sizeof p, path);
    /* padanan `echo val > path`: truncate + create bila diijinkan fs */
    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    int rc = (dprintf(fd, "%s\n", val) < 0) ? -1 : 0;
    close(fd);
    return rc;
}

/* system() dengan exit-code shell: 0 = sukses. */
static int sh(const char *cmd) {
    int rc = system(cmd);
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc) == 0 ? 0 : -1;
    return -1;
}

static int shf(const char *fmt, ...) {
    char cmd[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    return sh(cmd);
}

/* quoting aman untuk nilai yang diinterpolasi ke command sh. */
static const char *shq(char *buf, size_t n, const char *s) {
    size_t o = 0;
    buf[o++] = '\'';
    for (; *s && o + 4 < n; s++) {
        if (*s == '\'') {
            buf[o++] = '\''; buf[o++] = '\\'; buf[o++] = '\''; buf[o++] = '\'';
        } else {
            buf[o++] = *s;
        }
    }
    buf[o++] = '\'';
    buf[o] = 0;
    return buf;
}

static int command_exists(const char *name) {
    if (strchr(name, '/')) return access(name, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path || !path[0]) return 0;
    char dirs[8192];
    snprintf(dirs, sizeof dirs, "%s", path);
    char *save = NULL;
    for (char *d = strtok_r(dirs, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
        char full[PATH_MAX];
        snprintf(full, sizeof full, "%s/%s", d, name);
        if (access(full, X_OK) == 0) return 1;
    }
    return 0;
}

static void popen_first_line(const char *cmd, char *out, size_t n) {
    out[0] = 0;
    FILE *f = popen(cmd, "r");
    if (!f) return;
    if (fgets(out, (int)n, f)) trim_nl(out);
    pclose(f);
}

/* token ke-n (1-based) dengan separator whitespace-run — padanan awk '{print $n}'. */
static int ws_token(const char *line, int want, char *out, size_t n) {
    out[0] = 0;
    const char *c = line;
    while (isspace((unsigned char)*c)) c++;
    int f = 0;
    while (*c) {
        const char *start = c;
        while (*c && !isspace((unsigned char)*c)) c++;
        if (++f == want) {
            size_t len = (size_t)(c - start);
            if (len >= n) len = n - 1;
            memcpy(out, start, len);
            out[len] = 0;
            return 1;
        }
        while (isspace((unsigned char)*c)) c++;
    }
    return 0;
}

/* field ke-n dengan delimiter per-karakter " /" — padanan awk -F'[ /]'
 * (field kosong dihitung, termasuk yang di depan). */
static int awk_fs_field(const char *line, int want, char *out, size_t n) {
    out[0] = 0;
    const char *start = line;
    int f = 0;
    for (const char *c = line; ; c++) {
        if (*c == ' ' || *c == '/' || *c == '\0') {
            f++;
            if (f == want) {
                size_t len = (size_t)(c - start);
                if (len >= n) len = n - 1;
                memcpy(out, start, len);
                out[len] = 0;
                return 1;
            }
            start = c + 1;
            if (*c == '\0') break;
        }
    }
    return 0;
}

static void init_paths(void) {
    const char *md = getenv("PP_MODDIR");
    if (!md || !md[0]) md = PP_MODDIR_DEFAULT;
    snprintf(g_moddir, sizeof g_moddir, "%s", md);
    snprintf(g_logfile, sizeof g_logfile, "%s/log.txt", g_moddir);
    snprintf(g_statefile, sizeof g_statefile, "%s/state.json", g_moddir);
    snprintf(g_lockdir, sizeof g_lockdir, "%s/.engine.lock", g_moddir);
    snprintf(g_gamelist, sizeof g_gamelist, "%s/game_list.txt", g_moddir);
    snprintf(g_pidfile, sizeof g_pidfile, "%s/daemon.pid", g_moddir);
    snprintf(g_modprop, sizeof g_modprop, "%s/module.prop", g_moddir);
    g_procroot = getenv("PP_PROC_ROOT");
    ssize_t r = readlink("/proc/self/exe", g_self, sizeof g_self - 1);
    if (r > 0) g_self[r] = 0;
    else snprintf(g_self, sizeof g_self, "%s", "/system/bin/PingPimp");
}

/* ---------- log ---------- */

static void vwrite_log(const char *tweak, const char *status) {
    char ts[32];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
    FILE *f = fopen(g_logfile, "a");
    if (!f) return;
    fprintf(f, "[%s] [%s] Tweak: %s | Status: %s\n", ts, g_mode, tweak, status ? status : "");
    fclose(f);
}

static void write_log(const char *tweak, const char *status) {
    vwrite_log(tweak, status);
}

static void write_logf(const char *tweak_fmt, const char *status, ...) {
    char tweak[1024];
    va_list ap;
    va_start(ap, status);
    vsnprintf(tweak, sizeof tweak, tweak_fmt, ap);
    va_end(ap);
    vwrite_log(tweak, status);
}

/* ---------- state.json: satu file flat untuk semua state/backup ----------
 * Pretty 1 key per baris biar gampang dibaca saat debugging; ditulis atomic
 * (tmp + rename). Nilai di-escape (\ dan "), dibaca dengan unescape.
 */

#define PP_MAX_KEYS 512
#define PP_KEY_LEN 96
#define PP_VAL_LEN 512

static void json_unescape(char *s) {
    /* pass 1: \" -> " ; pass 2: \\ -> \ (urutan sama dengan sed pp_get) */
    char *w = s, *r = s;
    while (*r) {
        if (r[0] == '\\' && r[1] == '"') { *w++ = '"'; r += 2; }
        else *w++ = *r++;
    }
    *w = 0;
    w = s; r = s;
    while (*r) {
        if (r[0] == '\\' && r[1] == '\\') { *w++ = '\\'; r += 2; }
        else *w++ = *r++;
    }
    *w = 0;
}

static void json_escape(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (; *in && o + 4 < n; in++) {
        if (*in == '\\') out[o++] = '\\', out[o++] = '\\';
        else if (*in == '"') out[o++] = '\\', out[o++] = '"';
        else if (*in == '\r' || *in == '\n') continue; /* tr -d '\r\n' */
        else out[o++] = *in;
    }
    out[o] = 0;
}

static int state_parse(char keys[][PP_KEY_LEN], char vals[][PP_VAL_LEN]) {
    int n = 0;
    FILE *f = fopen(g_statefile, "r");
    if (!f) return 0;
    char line[PP_VAL_LEN + 128];
    while (fgets(line, sizeof line, f)) {
        trim_nl(line);
        trim_ws(line);
        if (line[0] != '"') continue;                 /* hanya baris "key": "val" */
        char *k = line + 1;
        char *kq = strchr(k, '"');
        if (!kq) continue;
        *kq = 0;
        char *c = kq + 1;
        if (*c != ':') continue;
        c++;
        while (*c == ' ' || *c == '\t') c++;
        if (*c != '"') continue;
        c++;
        size_t L = strlen(c);
        while (L && (c[L - 1] == ' ' || c[L - 1] == '\t')) c[--L] = 0;
        if (L && c[L - 1] == ',') c[--L] = 0;
        if (L && c[L - 1] == '"') c[--L] = 0;
        if (n < PP_MAX_KEYS) {
            size_t kl = strlen(k);
            if (kl >= PP_KEY_LEN) kl = PP_KEY_LEN - 1;
            memcpy(keys[n], k, kl);
            keys[n][kl] = 0;
            snprintf(vals[n], PP_VAL_LEN, "%s", c);
            n++;
        }
    }
    fclose(f);
    return n;
}

static int state_write(char keys[][PP_KEY_LEN], char vals[][PP_VAL_LEN], int n) {
    char tmp[PATH_MAX + 128];
    snprintf(tmp, sizeof tmp, "%s.tmp.%d", g_statefile, (int)getpid());
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "{\n");
    for (int i = 0; i < n; i++)
        fprintf(f, "  \"%s\": \"%s\"%s\n", keys[i], vals[i], (i < n - 1) ? "," : "");
    fprintf(f, "}\n");
    fclose(f);
    if (rename(tmp, g_statefile) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* key -> value (return 1 jika ada); nilai sudah di-unescape. */
static int pp_get(const char *key, char *out, size_t outsz) {
    out[0] = 0;
    if (access(g_statefile, F_OK) != 0) return 0;
    char keys[PP_MAX_KEYS][PP_KEY_LEN], vals[PP_MAX_KEYS][PP_VAL_LEN];
    int n = state_parse(keys, vals);
    for (int i = 0; i < n; i++) {
        if (strcmp(keys[i], key) == 0) {
            json_unescape(vals[i]);
            snprintf(out, outsz, "%s", vals[i]);
            return 1;
        }
    }
    return 0;
}

/* update/append lalu tulis ulang JSON valid (atomic). */
static int pp_set(const char *key, const char *val) {
    char esc[PP_VAL_LEN];
    json_escape(val ? val : "", esc, sizeof esc);
    char keys[PP_MAX_KEYS][PP_KEY_LEN], vals[PP_MAX_KEYS][PP_VAL_LEN];
    int n = 0;
    if (access(g_statefile, F_OK) == 0) n = state_parse(keys, vals);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(keys[i], key) == 0) {
            snprintf(vals[i], PP_VAL_LEN, "%s", esc);
            found = 1;
            break;
        }
    }
    if (!found) {
        if (n >= PP_MAX_KEYS) return -1;
        snprintf(keys[n], PP_KEY_LEN, "%s", key);
        snprintf(vals[n], PP_VAL_LEN, "%s", esc);
        n++;
    }
    return state_write(keys, vals, n);
}

/* hapus key persis ATAU prefix "pat_*" (padanan pp_del shell). */
static int pp_del(const char *pat) {
    if (access(g_statefile, F_OK) != 0) return 0;
    char keys[PP_MAX_KEYS][PP_KEY_LEN], vals[PP_MAX_KEYS][PP_VAL_LEN];
    int n = state_parse(keys, vals);
    char pref[PP_KEY_LEN + 2];
    snprintf(pref, sizeof pref, "%s_", pat);
    int w = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(keys[i], pat) == 0 || strncmp(keys[i], pref, strlen(pref)) == 0) continue;
        if (w != i) {
            memcpy(keys[w], keys[i], PP_KEY_LEN);
            memcpy(vals[w], vals[i], PP_VAL_LEN);
        }
        w++;
    }
    if (w == n) return state_write(keys, vals, n); /* shell juga rewrite walau tak ada hapusan */
    return state_write(keys, vals, w);
}

/* ---------- lock + debounce ---------- */

static int acquire_lock(void) {
    for (int tries = 0; ; ) {
        if (mkdir(g_lockdir, 0777) == 0) return 1;
        if (++tries >= 10) return 0;
        sleep(1);
    }
}

static void release_lock(void) {
    rmdir(g_lockdir);
}

static int debounce_ok(const char *key, int min_gap) {
    char k[288], v[64];
    snprintf(k, sizeof k, "debounce_%s", key);
    pp_get(k, v, sizeof v);
    long last = v[0] ? strtol(v, NULL, 10) : 0;
    long now = (long)time(NULL);
    if (now - last < (long)min_gap) return 0;
    char nv[32];
    snprintf(nv, sizeof nv, "%ld", now);
    pp_set(k, nv);
    return 1;
}

/* ---------- spawn latar: padanan "$0 --mode ... &" ---------- */

static void spawn_bg(const char *arg1, const char *arg2) {
    pid_t pid = fork();
    if (pid != 0) return;
    int dn = open("/dev/null", O_RDWR);
    if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); if (dn > 2) close(dn); }
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    if (arg2) execl(g_self, g_self, arg1, arg2, (char *)NULL);
    else execl(g_self, g_self, arg1, (char *)NULL);
    _exit(127);
}

/* ---------- kategori aktivitas + app foreground ---------- */

static int classify_pkg(const char *pkg, char *out, size_t n) {
    char p[512];
    size_t o = 0;
    for (; pkg[o] && o < sizeof p - 1; o++) p[o] = (char)tolower((unsigned char)pkg[o]);
    p[o] = 0;
    static const char *cats[][12] = {
        {"tube", "netflix", "twitch", "disney", "hulu", "spotify", "bilibili",
         "viu", "iqiyi", "music", NULL},
        {"chrome", "firefox", "opera", "edge", "brave", "ucbrowser", NULL},
        {"whatsapp", "insta", "facebook", "twitter", "tiktok", "gram",
         "snap", "reddit", NULL},
    };
    static const char *names[] = {"streaming", "browsing", "social"};
    for (int c = 0; c < 3; c++)
        for (int i = 0; cats[c][i]; i++)
            if (strstr(p, cats[c][i])) {
                snprintf(out, n, "%s", names[c]);
                return 1;
            }
    return 0;
}

/* cmd activity stack info | grep ',0.*=t' | awk -F'[ /]' '{print $4}' */
static void current_fg_pkg(char *out, size_t n) {
    out[0] = 0;
    FILE *f = popen("cmd activity stack info 1 0 2>/dev/null", "r");
    if (!f) return;
    char line[1024], field[256];
    size_t used = 0;
    while (fgets(line, sizeof line, f)) {
        trim_nl(line);
        const char *c0 = strstr(line, ",0");
        if (!c0) continue;
        if (!strstr(c0, "=t")) continue;
        if (awk_fs_field(line, 4, field, sizeof field) && field[0]) {
            int len = snprintf(out + used, n - used, "%s%s", used ? "\n" : "", field);
            if (len > 0) used += (size_t)len;
            if (used >= n - 1) break;
        }
    }
    pclose(f);
}

/* ---------- snapshot iface / route / vpn ---------- */

static int namecmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void iface_snapshot(char *out, size_t n) {
    out[0] = 0;
    static const char *fams[] = {"wlan", "rmnet", "ccmni"};
    for (size_t fi = 0; fi < sizeof fams / sizeof fams[0]; fi++) {
        const char *fam = fams[fi];
        size_t fl = strlen(fam);
        char names[64][64];
        char *np[64];
        int cnt = 0;
        DIR *d = opendir("/sys/class/net");
        if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d)) && cnt < 64) {
            if (strncmp(e->d_name, fam, fl) == 0 && strlen(e->d_name) < 64)
                snprintf(names[cnt++], 64, "%s", e->d_name);
        }
        closedir(d);
        for (int i = 0; i < cnt; i++) np[i] = names[i];
        qsort(np, (size_t)cnt, sizeof np[0], namecmp);
        for (int i = 0; i < cnt; i++) {
            char dirp[PATH_MAX], ststr[64], s[64];
            snprintf(dirp, sizeof dirp, "/sys/class/net/%s", np[i]);
            struct stat st;
            if (stat(dirp, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(dirp, sizeof dirp, "/sys/class/net/%s/operstate", np[i]);
            read_first_line(dirp, s, sizeof s);
            const char *state = (strcmp(s, "up") == 0) ? "UP" : "DOWN";
            snprintf(ststr, sizeof ststr, " %s:%s", np[i], state);
            strncat(out, ststr, n - strlen(out) - 1);
        }
    }
}

/* hanya iface yang benar-benar berubah sejak snapshot lama yang ditembak. */
static void fire_iface_events(const char *old, const char *cur) {
    char dup[1024];
    snprintf(dup, sizeof dup, "%s", cur);
    char *save = NULL;
    for (char *tok = strtok_r(dup, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
        char *colon = strchr(tok, ':');
        if (!colon) continue;
        *colon = 0;
        const char *ifc = tok, *st = colon + 1;
        char needle[160];
        snprintf(needle, sizeof needle, "%s:%s", ifc, st);
        if (strstr(old, needle)) continue;
        if (strcmp(st, "UP") == 0) {
            if (debounce_ok("iface_up", 3)) {
                spawn_bg("--engine", "INTERFACE_UP");
                spawn_bg("--dns-flip", "INTERFACE_UP");
            }
        } else {
            if (debounce_ok("iface_down", 3)) {
                spawn_bg("--engine", "INTERFACE_DOWN");
                spawn_bg("--dns-flip", "INTERFACE_DOWN");
            }
        }
    }
}

static void route_snapshot(char *out, size_t n) {
    popen_first_line("ip route show default 2>/dev/null", out, n);
}

static void vpn_snapshot(char *out, size_t n) {
    out[0] = 0;
    static const char *vpns[] = {"tun0", "wg0", "tailscale0"};
    for (size_t i = 0; i < sizeof vpns / sizeof vpns[0]; i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof p, "/sys/class/net/%s", vpns[i]);
        struct stat st;
        if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncat(out, " ", n - strlen(out) - 1);
            strncat(out, vpns[i], n - strlen(out) - 1);
        }
    }
}

/* ---------- kesehatan jaringan ---------- */

static void get_network_health(char *out, size_t n) {
    int loss = 100;
    long times[32];
    int nt = 0;
    FILE *f = popen("ping -c 4 -W 2 1.1.1.1 2>/dev/null", "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof line, f) && nt < 32) {
            const char *pl = strstr(line, "% packet loss");
            if (pl) {
                const char *d = pl;
                while (d > line && isdigit((unsigned char)d[-1])) d--;
                loss = (int)strtol(d, NULL, 10);
            }
            for (const char *q = strstr(line, "time="); q; q = strstr(q + 1, "time=")) {
                q += 5;
                char num[32];
                int i = 0;
                while ((isdigit((unsigned char)q[i]) || q[i] == '.') && i < 30) {
                    num[i] = q[i];
                    i++;
                }
                num[i] = 0;
                if (!i) continue;
                char *dot = strchr(num, '.');
                if (dot) *dot = 0;
                if (num[0]) times[nt++] = strtol(num, NULL, 10);
            }
        }
        pclose(f);
    }
    if (nt == 0) {
        snprintf(out, n, "999:100:0");
        return;
    }
    long sum = 0;
    for (int i = 0; i < nt; i++) sum += times[i];
    long jsum = 0;
    int jc = 0;
    for (int i = 1; i < nt; i++) {
        long diff = times[i] - times[i - 1];
        if (diff < 0) diff = -diff;
        jsum += diff;
        jc++;
    }
    long avg = sum / nt;
    long jitter = jc > 0 ? jsum / jc : 0;
    snprintf(out, n, "%ld:%d:%ld", avg, loss, jitter);
}

/* /proc/net/snmp: delta OutSegs/RetransSegs sejak panggilan terakhir -> "x.y" persen. */
static void get_tcp_retransmit_rate(char *out, size_t n) {
    out[0] = 0;
    char p[PATH_MAX];
    pp_path(p, sizeof p, "/proc/net/snmp");
    FILE *f = fopen(p, "r");
    if (!f) {
        snprintf(out, n, "0.0");
        return;
    }
    char header[1024] = "", values[2048] = "", line[2048];
    while (fgets(line, sizeof line, f)) {
        trim_nl(line);
        if (strncmp(line, "Tcp:", 4) != 0) continue;
        if (!header[0]) snprintf(header, sizeof header, "%s", line);
        else { snprintf(values, sizeof values, "%s", line); break; }
    }
    fclose(f);
    if (!header[0] || !values[0]) {
        snprintf(out, n, "0.0");
        return;
    }
    char *save = NULL;
    char *tok = strtok_r(header, " ", &save);
    long out_idx = -1, re_idx = -1;
    long idx = 0;
    while (tok) {
        if (strcmp(tok, "OutSegs") == 0) out_idx = idx;
        else if (strcmp(tok, "RetransSegs") == 0) re_idx = idx;
        tok = strtok_r(NULL, " ", &save);
        idx++;
    }
    if (out_idx < 0 || re_idx < 0) {
        snprintf(out, n, "0.0");
        return;
    }
    const char *vtok_out = NULL, *vtok_re = NULL;
    save = NULL;
    tok = strtok_r(values, " ", &save);
    idx = 0;
    while (tok) {
        if (idx == out_idx) vtok_out = tok;
        if (idx == re_idx) vtok_re = tok;
        tok = strtok_r(NULL, " ", &save);
        idx++;
    }
    if (!vtok_out || !vtok_re) {
        snprintf(out, n, "0.0");
        return;
    }
    long long out_now = atoll(vtok_out), re_now = atoll(vtok_re);
    char po[64], pr[64];
    pp_get("tcp_prev_out", po, sizeof po);
    pp_get("tcp_prev_re", pr, sizeof pr);
    long long out_prev = po[0] ? atoll(po) : 0;
    long long re_prev = pr[0] ? atoll(pr) : 0;
    char b[32];
    snprintf(b, sizeof b, "%lld", out_now);
    pp_set("tcp_prev_out", b);
    snprintf(b, sizeof b, "%lld", re_now);
    pp_set("tcp_prev_re", b);
    long long d_out = out_now - out_prev;
    long long d_re = re_now - re_prev;
    if (d_out <= 0 || d_re < 0) {
        snprintf(out, n, "0.0");
        return;
    }
    long long pct_x10 = (d_re * 1000) / d_out;
    snprintf(out, n, "%lld.%lld", pct_x10 / 10, pct_x10 % 10);
}

static long compute_network_score(long rtt, long loss, long jitter, const char *retrans) {
    long ri = 0;
    if (retrans && retrans[0]) {
        char buf[32];
        snprintf(buf, sizeof buf, "%s", retrans);
        char *dot = strchr(buf, '.');
        if (dot) *dot = 0;
        ri = strtol(buf, NULL, 10);
    }
    long p_rtt = rtt / 6; if (p_rtt > 30) p_rtt = 30;
    long p_loss = loss * 3; if (p_loss > 35) p_loss = 35;
    long p_jitter = jitter; if (p_jitter > 15) p_jitter = 15;
    long p_re = ri * 3; if (p_re > 20) p_re = 20;
    long score = 100 - p_rtt - p_loss - p_jitter - p_re;
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return score;
}

/* ---------- wifi info / band / network key / MTU ---------- */

static void get_wifi_info(const char *iface, char *out, size_t n) {
    char rssi[64] = "", freq[64] = "";
    if (command_exists("iw")) {
        char cmd[256], line[512];
        snprintf(cmd, sizeof cmd, "iw dev %s link 2>/dev/null", iface);
        FILE *f = popen(cmd, "r");
        if (f) {
            while (fgets(line, sizeof line, f)) {
                trim_nl(line);
                if (!rssi[0] && strstr(line, "signal:"))
                    ws_token(line, 2, rssi, sizeof rssi);
                if (!freq[0] && strstr(line, "freq:"))
                    ws_token(line, 2, freq, sizeof freq);
            }
            pclose(f);
        }
    }
    if (!rssi[0] || !freq[0]) {
        char line[4096];
        FILE *f = popen("dumpsys wifi 2>/dev/null", "r");
        if (f) {
            while (fgets(line, sizeof line, f)) {
                trim_nl(line);
                if (!pp_strcasestr(line, "mWifiInfo")) continue;
                const char *r = strstr(line, "RSSI: ");
                if (r && !rssi[0]) {
                    r += 6;
                    size_t i = 0;
                    if (r[i] == '-') i++;
                    while (isdigit((unsigned char)r[i]) && i < sizeof rssi - 1) i++;
                    snprintf(rssi, sizeof rssi, "%.*s", (int)i, r);
                }
                const char *fr = strstr(line, "Frequency: ");
                if (fr && !freq[0]) {
                    fr += 11;
                    size_t i = 0;
                    while (isdigit((unsigned char)fr[i]) && i < sizeof freq - 1) i++;
                    snprintf(freq, sizeof freq, "%.*s", (int)i, fr);
                }
                break;
            }
            pclose(f);
        }
    }
    snprintf(out, n, "%s:%s", rssi[0] ? rssi : "0", freq[0] ? freq : "0");
}

static const char *classify_band(const char *freq) {
    if (!freq || !freq[0]) return "UNKNOWN";
    for (const char *c = freq; *c; c++)
        if (!isdigit((unsigned char)*c)) return "UNKNOWN";
    long f = strtol(freq, NULL, 10);
    if (f >= 5925) return "6GHZ";
    if (f >= 4900) return "5GHZ";
    if (f >= 2400) return "24GHZ";
    return "UNKNOWN";
}

static void route_iface(char *out, size_t n) {
    char line[512];
    popen_first_line("ip route get 1.1.1.1 2>/dev/null", line, sizeof line);
    if (!ws_token(line, 5, out, n)) out[0] = 0;
}

static void network_key(const char *iface, char *out, size_t n) {
    char gw[128] = "";
    FILE *f = popen("ip route get 1.1.1.1 2>/dev/null", "r");
    if (f) {
        char line[512], tok[128], next[128];
        while (fgets(line, sizeof line, f)) {
            trim_nl(line);
            for (int i = 1; ws_token(line, i, tok, sizeof tok); i++) {
                if (strcmp(tok, "via") != 0) continue;
                if (ws_token(line, i + 1, next, sizeof next)) {
                    snprintf(gw, sizeof gw, "%s", next);
                    break;
                }
            }
            if (gw[0]) break;
        }
        pclose(f);
    }
    if (!gw[0]) snprintf(gw, sizeof gw, "direct");
    char clean[128];
    size_t o = 0;
    for (const char *c = gw; *c && o < sizeof clean - 1; c++)
        clean[o++] = (*c == '.' || *c == ':') ? '_' : *c;
    clean[o] = 0;
    snprintf(out, n, "%s_%s", iface, clean);
}

static int route_has_vpn(void) {
    FILE *f = popen("ip link show 2>/dev/null", "r");
    if (!f) return 0;
    char line[512];
    int vpn = 0;
    while (fgets(line, sizeof line, f)) {
        const char *c = line;
        while (isdigit((unsigned char)*c)) c++;
        if (c == line) continue;
        if (*c == ':' && c[1] == ' ') {
            const char *name = c + 2;
            if (strncmp(name, "tun0", 4) == 0 || strncmp(name, "wg0", 3) == 0 ||
                strncmp(name, "tailscale", 9) == 0) {
                vpn = 1;
                break;
            }
        }
    }
    pclose(f);
    return vpn;
}

static long probe_mtu(const char *iface) {
    char key[256], ck[300], cached[32];
    network_key(iface, key, sizeof key);
    snprintf(ck, sizeof ck, "mtu_cache_%s", key);
    if (pp_get(ck, cached, sizeof cached) && cached[0])
        return strtol(cached, NULL, 10);
    if (route_has_vpn()) {
        pp_set(ck, "1420");
        return 1420;
    }
    if (sh("ping -M do -c 1 -W 1 -s 1400 1.1.1.1 >/dev/null 2>&1") != 0) {
        long mtu = (strncmp(iface, "wlan", 4) == 0) ? 1492 : 1440;
        char b[32];
        snprintf(b, sizeof b, "%ld", mtu);
        pp_set(ck, b);
        return mtu;
    }
    long lo = 1200, hi = 1472, best = 1400;
    while (lo <= hi) {
        long mid = (lo + hi) / 2;
        char cmd[128];
        snprintf(cmd, sizeof cmd,
                 "ping -M do -c 1 -W 1 -s %ld 1.1.1.1 >/dev/null 2>&1", mid);
        if (sh(cmd) == 0) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    long mtu = best + 28;
    char b[32];
    snprintf(b, sizeof b, "%ld", mtu);
    pp_set(ck, b);
    write_logf("Adaptive MTU probe on %s -> %ld (key: %s)", "SUCCESS", iface, mtu, key);
    return mtu;
}

static void set_default_qdisc(void) {
    char p[PATH_MAX];
    pp_path(p, sizeof p, "/proc/sys/net/core/default_qdisc");
    if (access(p, F_OK) != 0) return;
    static const char *qs[] = {"fq", "fq_codel", "pfifo_fast"};
    for (size_t i = 0; i < sizeof qs / sizeof qs[0]; i++) {
        if (write_sysfile("/proc/sys/net/core/default_qdisc", qs[i]) == 0) {
            write_logf("Default qdisc set to %s", "SUCCESS", qs[i]);
            return;
        }
    }
    write_log("Default qdisc locked by kernel, keeping current", "SKIP");
}

/* ---------- MTU / txqueuelen native (SIOCSIFMTU / SIOCSIFTXQLEN) ---------- */

static int set_link_val(const char *ifname, unsigned long req, int val) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof ifr);
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    ifr.ifr_ifru.ifru_ivalue = val;
    int rc = ioctl(s, (unsigned long)req, &ifr);
    close(s);
    return rc == 0 ? 0 : -1;
}

static void set_mtu(const char *ifname, int mtu) {
    set_link_val(ifname, SIOCSIFMTU, mtu);
}

static void set_txqueuelen(const char *ifname, int len) {
    set_link_val(ifname, SIOCSIFTXQLEN, len);
}

static void clear_wlan_promisc_allmulti(const char *ifname) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof ifr);
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0) {
        ifr.ifr_flags &= (short)~(IFF_PROMISC | IFF_ALLMULTI);
        ioctl(s, SIOCSIFFLAGS, &ifr);
    }
    close(s);
}


/* ---------- Smart Auto DNS: Private DNS per tipe jaringan ----------
 * Prioritas: SSID override -> DNS WiFi/Data (WebUI) -> dns_manual
 * (opsi Network Tweak) -> DNS sistem. TANPA fallback Google DNS.
 * Hanya aktif saat Smart Auto Mode menyala.
 */

static void get_current_dns_mode(char *out, size_t n) {
    popen_first_line("settings get global private_dns_mode 2>/dev/null", out, n);
    if (!out[0] || strcmp(out, "null") == 0) snprintf(out, n, "off");
}

static void get_current_dns_spec(char *out, size_t n) {
    popen_first_line("settings get global private_dns_specifier 2>/dev/null", out, n);
    if (!out[0] || strcmp(out, "null") == 0) snprintf(out, n, "none");
}

static int get_wifi_ssid(char *out, size_t n) {
    out[0] = 0;
    FILE *f = popen("dumpsys wifi 2>/dev/null", "r");
    if (!f) return 0;
    char line[4096], mline[4096] = "", sline[4096] = "";
    while (fgets(line, sizeof line, f)) {
        trim_nl(line);
        if (!mline[0] && pp_strcasestr(line, "mWifiInfo"))
            snprintf(mline, sizeof mline, "%s", line);
        if (!sline[0] && strstr(line, "SSID: \""))
            snprintf(sline, sizeof sline, "%s", line);
    }
    pclose(f);
    const char *src = NULL;
    if (mline[0]) src = strstr(mline, "SSID: \"");
    if (!src && sline[0]) src = strstr(sline, "SSID: \"");
    char raw[512] = "";
    if (src) {
        src += 7;
        const char *e = strchr(src, '"');
        if (e && (size_t)(e - src) < sizeof raw) {
            memcpy(raw, src, (size_t)(e - src));
            raw[e - src] = 0;
        }
    }
    if (!raw[0]) return 0;
    if (strcmp(raw, "<unknown ssid>") == 0 || strcmp(raw, "<unknown>") == 0 ||
        strcmp(raw, "<hidden>") == 0 || strcmp(raw, "0x") == 0 ||
        strcmp(raw, "0x0") == 0)
        return 0;
    snprintf(out, n, "%s", raw);
    return 1;
}

static void detect_network_type(char *out, size_t n) {
    char ifc[64];
    route_iface(ifc, sizeof ifc);
    if (strncmp(ifc, "wlan", 4) == 0 || strncmp(ifc, "eth", 3) == 0) {
        snprintf(out, n, "wifi");
        return;
    }
    if (strncmp(ifc, "rmnet", 5) == 0 || strncmp(ifc, "ccmni", 5) == 0 ||
        strncmp(ifc, "radio", 5) == 0 || strncmp(ifc, "pdp", 3) == 0) {
        snprintf(out, n, "mobile");
        return;
    }
    /* fallback (mis. trafik lewat VPN/tun) */
    char ssid[256];
    if (get_wifi_ssid(ssid, sizeof ssid)) {
        snprintf(out, n, "wifi");
        return;
    }
    FILE *f = popen("dumpsys connectivity 2>/dev/null", "r");
    if (f) {
        char line[4096];
        while (fgets(line, sizeof line, f)) {
            const char *m = strstr(line, "MOBILE");
            if (m && strstr(m, "CONNECTED")) {
                snprintf(out, n, "mobile");
                pclose(f);
                return;
            }
        }
        pclose(f);
    }
    snprintf(out, n, "unknown");
}

static void get_dns_cfg(const char *key, char *out, size_t n) {
    pp_get(key, out, n);
    if (!out[0] || strcmp(out, "null") == 0 || strcmp(out, "none") == 0 ||
        strcmp(out, "default") == 0)
        out[0] = 0;
}

static int get_ssid_dns(const char *ssid, char *out, size_t n) {
    out[0] = 0;
    if (!ssid || !ssid[0]) return 0;
    char f[PATH_MAX + 128];
    snprintf(f, sizeof f, "%s/dns_ssid_map.txt", g_moddir);
    FILE *fp = fopen(f, "r");
    if (!fp) return 0;
    char line[1024];
    while (fgets(line, sizeof line, fp)) {
        trim_nl(line);
        if (!line[0] || line[0] == '#') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *val = eq + 1;
        if (strcmp(line, ssid) == 0 && val[0]) {
            snprintf(out, n, "%s", val);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static void apply_private_dns(const char *target, const char *net_type, const char *event) {
    char mode[64], spec[256];
    get_current_dns_mode(mode, sizeof mode);
    get_current_dns_spec(spec, sizeof spec);

    if (strcmp(target, "default") == 0) {
        if (strcmp(mode, "off") == 0 || strcmp(mode, "opportunistic") == 0) return;
        sh("settings delete global private_dns_mode 2>/dev/null");
        sh("settings delete global private_dns_specifier 2>/dev/null");
        write_logf("Auto DNS [%s/%s]: %s/%s -> System Default", "SUCCESS",
                   event, net_type, mode, spec);
    } else {
        /* idempoten: skip jika sudah sama (anti spam settings put tiap 10 detik) */
        if (strcmp(mode, "hostname") == 0 && strcmp(spec, target) == 0) return;
        sh("settings put global private_dns_mode hostname 2>/dev/null");
        char q[600], cmd[900];
        shq(q, sizeof q, target);
        snprintf(cmd, sizeof cmd, "settings put global private_dns_specifier %s 2>/dev/null", q);
        sh(cmd);
        write_logf("Auto DNS [%s/%s]: %s/%s -> %s", "SUCCESS",
                   event, net_type, mode, spec, target);
    }
}

static void run_dns_flip(const char *event) {
    char am[8];
    pp_get("auto_mode", am, sizeof am);
    if (strcmp(am, "1") != 0) return;

    /* jeda agar status link/route settle setelah event */
    if (strncmp(event, "INTERFACE_", 10) == 0 || strncmp(event, "ROUTE_", 6) == 0 ||
        strncmp(event, "VPN_", 4) == 0 || strcmp(event, "AUTO_ON") == 0 ||
        strcmp(event, "BOOT") == 0)
        sleep(3);

    char net_type[16], target[256] = "";
    detect_network_type(net_type, sizeof net_type);
    if (strcmp(net_type, "unknown") == 0) return;

    if (strcmp(net_type, "wifi") == 0) {
        char ssid[256];
        if (get_wifi_ssid(ssid, sizeof ssid))
            get_ssid_dns(ssid, target, sizeof target);          /* 1. override per-SSID */
        if (!target[0])
            get_dns_cfg("dns_wifi", target, sizeof target);     /* 2. DNS WiFi WebUI */
    } else if (strcmp(net_type, "mobile") == 0) {
        get_dns_cfg("dns_mobile", target, sizeof target);       /* 3. DNS data seluler */
    } else {
        return;
    }

    if (!target[0] || strcmp(target, "default") == 0)
        get_dns_cfg("dns_manual", target, sizeof target);       /* 4. ops Network Tweak */

    if (!target[0]) snprintf(target, sizeof target, "default"); /* 5. DNS sistem */

    apply_private_dns(target, net_type, event);
}

/* ---------- module.prop status + TCP + game scanner ---------- */

static int any_proc_cmdline_has(const char *needle) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    char path[320], buf[4096];
    while ((e = readdir(d))) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        snprintf(path, sizeof path, "/proc/%s/cmdline", e->d_name);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        ssize_t r = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (r <= 0) continue;
        buf[r] = 0;
        for (ssize_t i = 0; i < r; i++)
            if (buf[i] == 0) buf[i] = ' ';
        if (strstr(buf, needle)) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

static void module_description(void) {
    const char *repl = any_proc_cmdline_has("PingPimp")
                           ? "description=[ 😋 WORKING ] "
                           : "description=[  ⚠️ NOT WORKING ] ";
    FILE *f = fopen(g_modprop, "r");
    if (!f) return;
    char tmp[PATH_MAX + 128];
    snprintf(tmp, sizeof tmp, "%s.tmp.%d", g_modprop, (int)getpid());
    FILE *o = fopen(tmp, "w");
    if (!o) {
        fclose(f);
        return;
    }
    char line[2048];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "description=", 12) == 0) {
            /* padanan sed -Ei: ganti "description=" + opsional "[...]<spasi>",
             * sisa deskripsi setelahnya dipertahankan (.* greedy sampai ']' terakhir) */
            char rest[2048], tail[2048] = "";
            snprintf(rest, sizeof rest, "%s", line + 12);
            trim_nl(rest);
            if (rest[0] == '[') {
                char *last = strrchr(rest, ']');
                if (last) {
                    char *after = last + 1;
                    while (*after == ' ' || *after == '\t') after++;
                    snprintf(tail, sizeof tail, "%s", after);
                } else {
                    snprintf(tail, sizeof tail, "%s", rest);
                }
            } else {
                snprintf(tail, sizeof tail, "%s", rest);
            }
            fprintf(o, "%s%s\n", repl, tail);
        } else {
            fprintf(o, "%s", line);
        }
    }
    fclose(f);
    fclose(o);
    rename(tmp, g_modprop);
}

static void set_best_tcp(const char *const *algos, int count) {
    char avail[512] = "";
    read_first_line("/proc/sys/net/ipv4/tcp_available_congestion_control",
                    avail, sizeof avail);
    char tried[256];
    tried[0] = 0;
    for (int i = 0; i < count; i++) {
        if (i) strncat(tried, " ", sizeof tried - strlen(tried) - 1);
        strncat(tried, algos[i], sizeof tried - strlen(tried) - 1);
        char tok[64];
        for (int t = 1; ws_token(avail, t, tok, sizeof tok); t++) {
            if (strcmp(tok, algos[i]) != 0) continue;
            if (write_sysfile("/proc/sys/net/ipv4/tcp_congestion_control", algos[i]) == 0) {
                write_logf("Auto-Detect: TCP optimized to %s", "SUCCESS", algos[i]);
                return;
            }
            break;
        }
    }
    write_logf("Auto-Detect: Failed to set TCP (tried: %s)", "FAILED", tried);
}

static void detect_games(void) {
    write_log("Game Scanner: Detecting installed games...", "STARTED");
    FILE *out = fopen(g_gamelist, "w");
    if (out) fclose(out);

    FILE *f = popen("cmd package query-activities -a android.intent.action.MAIN "
                    "-c android.intent.category.LAUNCHER 2>/dev/null", "r");
    static char *list[2048];
    int n = 0;
    char p[512] = "", line[4096];
    while (f && fgets(line, sizeof line, f)) {
        trim_nl(line);
        if (strstr(line, "geNa")) {
            /* awk -F=: $2 = antara '=' pertama dan '=' kedua */
            char *e1 = strchr(line, '=');
            if (e1) {
                char *e2 = strchr(e1 + 1, '=');
                size_t len = e2 ? (size_t)(e2 - e1 - 1) : strlen(e1 + 1);
                if (len >= sizeof p) len = sizeof p - 1;
                memcpy(p, e1 + 1, len);
                p[len] = 0;
            } else {
                p[0] = 0;
            }
        }
        if (strstr(line, "ry=0") && p[0] && n < 2048)
            list[n++] = strdup(p);
    }
    if (f) pclose(f);

    qsort(list, (size_t)n, sizeof list[0], namecmp);
    out = fopen(g_gamelist, "a");
    int gc = 0;
    char found[8192];
    found[0] = 0;
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        if (i && strcmp(list[i], list[i - 1]) == 0) continue;
        if (out) fprintf(out, "%s\n", list[i]);
        gc++;
        int l = snprintf(found + used, sizeof found - used, "%s,", list[i]);
        if (l > 0) used += (size_t)l;
        if (used >= sizeof found - 1) break;
    }
    if (out) fclose(out);
    write_logf("Game Scanner: Found %d games [%s]", "SUCCESS", gc, found);
    for (int i = 0; i < n; i++) free(list[i]);
}

/* ---------- pgrep + renice (native) ---------- */

static int scan_comm_pids(const char *prefix, long *pids, int max) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    char path[320], comm[64];
    size_t pl = strlen(prefix);
    while ((e = readdir(d)) && n < max) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        snprintf(path, sizeof path, "/proc/%s/comm", e->d_name);
        if (read_first_line(path, comm, sizeof comm) != 0) continue;
        if (strncmp(comm, prefix, pl) == 0)
            pids[n++] = strtol(e->d_name, NULL, 10);
    }
    closedir(d);
    return n;
}

static int ksoft_renice(int prio) {
    long pids[256];
    int n = scan_comm_pids("ksoftirqd", pids, 256);
    int failed = 0;
    for (int i = 0; i < n; i++)
        if (setpriority(PRIO_PROCESS, (id_t)pids[i], prio) != 0) failed = 1;
    return failed;
}

static int pid_in(long *pids, int n, long pid) {
    for (int i = 0; i < n; i++)
        if (pids[i] == pid) return 1;
    return 0;
}

/* padanan pgrep -f "rild|netd" + renice per pid */
static int cmdlines_renice(const char *const *pats, int npats, int prio) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    long pids[512];
    int n = 0;
    struct dirent *e;
    char path[320], buf[4096];
    while ((e = readdir(d)) && n < 512) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        long pid = strtol(e->d_name, NULL, 10);
        snprintf(path, sizeof path, "/proc/%ld/cmdline", pid);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        ssize_t r = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (r <= 0) continue;
        buf[r] = 0;
        for (ssize_t i = 0; i < r; i++)
            if (buf[i] == 0) buf[i] = ' ';
        for (int k = 0; k < npats; k++) {
            if (strstr(buf, pats[k]) && !pid_in(pids, n, pid)) {
                pids[n++] = pid;
                break;
            }
        }
    }
    closedir(d);
    int failed = 0;
    for (int i = 0; i < n; i++)
        if (setpriority(PRIO_PROCESS, (id_t)pids[i], prio) != 0) failed = 1;
    return failed;
}

static long pm_uid(const char *pkg) {
    FILE *f = popen("pm list packages -U", "r");
    if (!f) return -1;
    char line[1024];
    long uid = -1;
    size_t pl = strlen(pkg);
    while (fgets(line, sizeof line, f)) {
        trim_nl(line);
        if (strncmp(line, "package:", 8) != 0) continue;
        if (strncmp(line + 8, pkg, pl) != 0 || line[8 + pl] != ' ') continue;
        const char *u = strstr(line, "uid:");
        if (u) {
            uid = strtol(u + 4, NULL, 10);
            break;
        }
    }
    pclose(f);
    return uid;
}

/* ---------- apply engine + tabel preset ----------
 * Setiap op membawa string shell asli (disp) supaya baris log SKIP tetap
 * identik dengan versi shell; eksekusinya native (path) atau via sh (shcmd).
 */

typedef struct {
    const char *disp;
    const char *path;
    const char *val;
    const char *shcmd;
} op_t;

#define SYS(d, p, v) { d, p, v, NULL }
#define SH(d, c)     { d, NULL, NULL, c }

static void apply_ops(const char *name, const op_t *ops, int n) {
    int failed = 0, ok = 0;
    for (int i = 0; i < n; i++) {
        int rc;
        if (ops[i].path) rc = write_sysfile(ops[i].path, ops[i].val);
        else if (ops[i].shcmd) rc = sh(ops[i].shcmd);
        else rc = 0;
        if (rc == 0) ok++;
        else {
            failed++;
            write_logf("  -> not applied: %s", "SKIP", ops[i].disp);
        }
    }
    if (failed == 0) write_log(name, "SUCCESS");
    else if (ok > 0) write_logf("%s (%d/%d applied)", "", name, ok, n);
    else write_log(name, "FAILED");
}

/* TABLES:GENERATED — preset & op-list dihasilkan dari upload/PingPimp
 * oleh scripts/gen_pp_tables.py; jangan sunting tangan. */
/* Generated by scripts/gen_pp_tables.py from upload/PingPimp — do not hand-edit. */

static const op_t OP_DEFAULT[] = {
    SYS("echo '4096' > /proc/sys/net/core/netdev_max_backlog", "/proc/sys/net/core/netdev_max_backlog", "4096"),
    SYS("echo '4096' > /proc/sys/net/core/somaxconn", "/proc/sys/net/core/somaxconn", "4096"),
    SYS("echo '4096' > /proc/sys/net/core/netdev_budget", "/proc/sys/net/core/netdev_budget", "4096"),
    SYS("echo '8000' > /proc/sys/net/core/netdev_budget_usecs", "/proc/sys/net/core/netdev_budget_usecs", "8000"),
    SYS("echo '204800' > /proc/sys/net/core/optmem_max", "/proc/sys/net/core/optmem_max", "204800"),
    SYS("echo '262144' > /proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_default", "262144"),
    SYS("echo '1048576' > /proc/sys/net/core/rmem_max", "/proc/sys/net/core/rmem_max", "1048576"),
    SYS("echo '262144' > /proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_default", "262144"),
    SYS("echo '1048576' > /proc/sys/net/core/wmem_max", "/proc/sys/net/core/wmem_max", "1048576"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_slow_start_after_idle", "/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_sack", "/proc/sys/net/ipv4/tcp_sack", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_window_scaling", "/proc/sys/net/ipv4/tcp_window_scaling", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_moderate_rcvbuf", "/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_timestamps", "/proc/sys/net/ipv4/tcp_timestamps", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_no_metrics_save", "/proc/sys/net/ipv4/tcp_no_metrics_save", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_mtu_probing", "/proc/sys/net/ipv4/tcp_mtu_probing", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_rfc1337", "/proc/sys/net/ipv4/tcp_rfc1337", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_fastopen", "/proc/sys/net/ipv4/tcp_fastopen", "3"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_frto", "/proc/sys/net/ipv4/tcp_frto", "2"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_recovery", "/proc/sys/net/ipv4/tcp_recovery", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_early_retrans", "/proc/sys/net/ipv4/tcp_early_retrans", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_reordering", "/proc/sys/net/ipv4/tcp_reordering", "3"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_ecn", "/proc/sys/net/ipv4/tcp_ecn", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_tw_reuse", "/proc/sys/net/ipv4/tcp_tw_reuse", "1"),
    SYS("echo '4' > /proc/sys/net/ipv4/tcp_syn_retries", "/proc/sys/net/ipv4/tcp_syn_retries", "4"),
    SYS("echo '4' > /proc/sys/net/ipv4/tcp_synack_retries", "/proc/sys/net/ipv4/tcp_synack_retries", "4"),
    SYS("echo '4096 87380 1048576' > /proc/sys/net/ipv4/tcp_rmem", "/proc/sys/net/ipv4/tcp_rmem", "4096 87380 1048576"),
    SYS("echo '4096 65536 524288' > /proc/sys/net/ipv4/tcp_wmem", "/proc/sys/net/ipv4/tcp_wmem", "4096 65536 524288"),
    SYS("echo '16384 32768 49152' > /proc/sys/net/ipv4/tcp_mem", "/proc/sys/net/ipv4/tcp_mem", "16384 32768 49152"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_rmem_min", "/proc/sys/net/ipv4/udp_rmem_min", "16384"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_wmem_min", "/proc/sys/net/ipv4/udp_wmem_min", "16384"),
    SYS("echo '16384' > /proc/sys/net/ipv4/tcp_notsent_lowat", "/proc/sys/net/ipv4/tcp_notsent_lowat", "16384"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_abort_on_overflow", "/proc/sys/net/ipv4/tcp_abort_on_overflow", "0"),
    SYS("echo '300' > /proc/sys/net/ipv4/tcp_max_reordering", "/proc/sys/net/ipv4/tcp_max_reordering", "300"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_adv_win_scale", "/proc/sys/net/ipv4/tcp_adv_win_scale", "1"),
    SYS("echo '2048' > /proc/sys/net/ipv4/tcp_max_syn_backlog", "/proc/sys/net/ipv4/tcp_max_syn_backlog", "2048"),
    SYS("echo '8192' > /proc/sys/net/ipv4/tcp_max_tw_buckets", "/proc/sys/net/ipv4/tcp_max_tw_buckets", "8192"),
    SYS("echo '1024 65535' > /proc/sys/net/ipv4/ip_local_port_range", "/proc/sys/net/ipv4/ip_local_port_range", "1024 65535"),
    SYS("echo '30' > /proc/sys/net/ipv4/tcp_fin_timeout", "/proc/sys/net/ipv4/tcp_fin_timeout", "30"),
    SYS("echo '600' > /proc/sys/net/ipv4/tcp_keepalive_time", "/proc/sys/net/ipv4/tcp_keepalive_time", "600"),
    SYS("echo '30' > /proc/sys/net/ipv4/tcp_keepalive_intvl", "/proc/sys/net/ipv4/tcp_keepalive_intvl", "30"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_keepalive_probes", "/proc/sys/net/ipv4/tcp_keepalive_probes", "5"),
    SYS("echo '1' > /proc/sys/net/ipv4/route/flush", "/proc/sys/net/ipv4/route/flush", "1"),
    SH("cmd wifi force-low-latency-mode disabled 2>/dev/null || true", "cmd wifi force-low-latency-mode disabled 2>/dev/null || true"),
};
#define OP_DEFAULT_LEN ((int)(sizeof OP_DEFAULT / sizeof OP_DEFAULT[0]))

static const op_t OP_GAME[] = {
    SYS("echo '1000' > /proc/sys/net/core/netdev_max_backlog", "/proc/sys/net/core/netdev_max_backlog", "1000"),
    SYS("echo '1024' > /proc/sys/net/core/somaxconn", "/proc/sys/net/core/somaxconn", "1024"),
    SYS("echo '6000' > /proc/sys/net/core/netdev_budget", "/proc/sys/net/core/netdev_budget", "6000"),
    SYS("echo '6000' > /proc/sys/net/core/netdev_budget_usecs", "/proc/sys/net/core/netdev_budget_usecs", "6000"),
    SYS("echo '102400' > /proc/sys/net/core/optmem_max", "/proc/sys/net/core/optmem_max", "102400"),
    SYS("echo '131072' > /proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_default", "131072"),
    SYS("echo '524288' > /proc/sys/net/core/rmem_max", "/proc/sys/net/core/rmem_max", "524288"),
    SYS("echo '131072' > /proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_default", "131072"),
    SYS("echo '524288' > /proc/sys/net/core/wmem_max", "/proc/sys/net/core/wmem_max", "524288"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_slow_start_after_idle", "/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_sack", "/proc/sys/net/ipv4/tcp_sack", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_window_scaling", "/proc/sys/net/ipv4/tcp_window_scaling", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_moderate_rcvbuf", "/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_timestamps", "/proc/sys/net/ipv4/tcp_timestamps", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_no_metrics_save", "/proc/sys/net/ipv4/tcp_no_metrics_save", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_mtu_probing", "/proc/sys/net/ipv4/tcp_mtu_probing", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_rfc1337", "/proc/sys/net/ipv4/tcp_rfc1337", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_fastopen", "/proc/sys/net/ipv4/tcp_fastopen", "3"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_frto", "/proc/sys/net/ipv4/tcp_frto", "2"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_recovery", "/proc/sys/net/ipv4/tcp_recovery", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_early_retrans", "/proc/sys/net/ipv4/tcp_early_retrans", "1"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_reordering", "/proc/sys/net/ipv4/tcp_reordering", "5"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_ecn", "/proc/sys/net/ipv4/tcp_ecn", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_tw_reuse", "/proc/sys/net/ipv4/tcp_tw_reuse", "1"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_syn_retries", "/proc/sys/net/ipv4/tcp_syn_retries", "2"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_synack_retries", "/proc/sys/net/ipv4/tcp_synack_retries", "2"),
    SYS("echo '4096 32768 131072' > /proc/sys/net/ipv4/tcp_rmem", "/proc/sys/net/ipv4/tcp_rmem", "4096 32768 131072"),
    SYS("echo '4096 32768 131072' > /proc/sys/net/ipv4/tcp_wmem", "/proc/sys/net/ipv4/tcp_wmem", "4096 32768 131072"),
    SYS("echo '8192 16384 24576' > /proc/sys/net/ipv4/tcp_mem", "/proc/sys/net/ipv4/tcp_mem", "8192 16384 24576"),
    SYS("echo '32768' > /proc/sys/net/ipv4/udp_rmem_min", "/proc/sys/net/ipv4/udp_rmem_min", "32768"),
    SYS("echo '32768' > /proc/sys/net/ipv4/udp_wmem_min", "/proc/sys/net/ipv4/udp_wmem_min", "32768"),
    SYS("echo '2048' > /proc/sys/net/ipv4/tcp_notsent_lowat", "/proc/sys/net/ipv4/tcp_notsent_lowat", "2048"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_abort_on_overflow", "/proc/sys/net/ipv4/tcp_abort_on_overflow", "1"),
    SYS("echo '100' > /proc/sys/net/ipv4/tcp_max_reordering", "/proc/sys/net/ipv4/tcp_max_reordering", "100"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_adv_win_scale", "/proc/sys/net/ipv4/tcp_adv_win_scale", "0"),
    SYS("echo '1024' > /proc/sys/net/ipv4/tcp_max_syn_backlog", "/proc/sys/net/ipv4/tcp_max_syn_backlog", "1024"),
    SYS("echo '4096' > /proc/sys/net/ipv4/tcp_max_tw_buckets", "/proc/sys/net/ipv4/tcp_max_tw_buckets", "4096"),
    SYS("echo '1024 65535' > /proc/sys/net/ipv4/ip_local_port_range", "/proc/sys/net/ipv4/ip_local_port_range", "1024 65535"),
    SYS("echo '10' > /proc/sys/net/ipv4/tcp_fin_timeout", "/proc/sys/net/ipv4/tcp_fin_timeout", "10"),
    SYS("echo '300' > /proc/sys/net/ipv4/tcp_keepalive_time", "/proc/sys/net/ipv4/tcp_keepalive_time", "300"),
    SYS("echo '10' > /proc/sys/net/ipv4/tcp_keepalive_intvl", "/proc/sys/net/ipv4/tcp_keepalive_intvl", "10"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_keepalive_probes", "/proc/sys/net/ipv4/tcp_keepalive_probes", "3"),
    SYS("echo '1' > /proc/sys/net/ipv4/route/flush", "/proc/sys/net/ipv4/route/flush", "1"),
    SH("cmd wifi force-low-latency-mode enabled 2>/dev/null || true", "cmd wifi force-low-latency-mode enabled 2>/dev/null || true"),
};
#define OP_GAME_LEN ((int)(sizeof OP_GAME / sizeof OP_GAME[0]))

static const op_t OP_DOWNLOAD[] = {
    SYS("echo '16384' > /proc/sys/net/core/netdev_max_backlog", "/proc/sys/net/core/netdev_max_backlog", "16384"),
    SYS("echo '8192' > /proc/sys/net/core/somaxconn", "/proc/sys/net/core/somaxconn", "8192"),
    SYS("echo '60000' > /proc/sys/net/core/netdev_budget", "/proc/sys/net/core/netdev_budget", "60000"),
    SYS("echo '8000' > /proc/sys/net/core/netdev_budget_usecs", "/proc/sys/net/core/netdev_budget_usecs", "8000"),
    SYS("echo '409600' > /proc/sys/net/core/optmem_max", "/proc/sys/net/core/optmem_max", "409600"),
    SYS("echo '524288' > /proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_default", "524288"),
    SYS("echo '16777216' > /proc/sys/net/core/rmem_max", "/proc/sys/net/core/rmem_max", "16777216"),
    SYS("echo '524288' > /proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_default", "524288"),
    SYS("echo '16777216' > /proc/sys/net/core/wmem_max", "/proc/sys/net/core/wmem_max", "16777216"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_slow_start_after_idle", "/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_sack", "/proc/sys/net/ipv4/tcp_sack", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_window_scaling", "/proc/sys/net/ipv4/tcp_window_scaling", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_moderate_rcvbuf", "/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_timestamps", "/proc/sys/net/ipv4/tcp_timestamps", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_no_metrics_save", "/proc/sys/net/ipv4/tcp_no_metrics_save", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_mtu_probing", "/proc/sys/net/ipv4/tcp_mtu_probing", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_rfc1337", "/proc/sys/net/ipv4/tcp_rfc1337", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_fastopen", "/proc/sys/net/ipv4/tcp_fastopen", "3"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_frto", "/proc/sys/net/ipv4/tcp_frto", "2"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_recovery", "/proc/sys/net/ipv4/tcp_recovery", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_early_retrans", "/proc/sys/net/ipv4/tcp_early_retrans", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_reordering", "/proc/sys/net/ipv4/tcp_reordering", "3"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_ecn", "/proc/sys/net/ipv4/tcp_ecn", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_tw_reuse", "/proc/sys/net/ipv4/tcp_tw_reuse", "1"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_syn_retries", "/proc/sys/net/ipv4/tcp_syn_retries", "5"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_synack_retries", "/proc/sys/net/ipv4/tcp_synack_retries", "5"),
    SYS("echo '4096 87380 16777216' > /proc/sys/net/ipv4/tcp_rmem", "/proc/sys/net/ipv4/tcp_rmem", "4096 87380 16777216"),
    SYS("echo '4096 65536 16777216' > /proc/sys/net/ipv4/tcp_wmem", "/proc/sys/net/ipv4/tcp_wmem", "4096 65536 16777216"),
    SYS("echo '131072 262144 393216' > /proc/sys/net/ipv4/tcp_mem", "/proc/sys/net/ipv4/tcp_mem", "131072 262144 393216"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_rmem_min", "/proc/sys/net/ipv4/udp_rmem_min", "16384"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_wmem_min", "/proc/sys/net/ipv4/udp_wmem_min", "16384"),
    SYS("echo '4294967295' > /proc/sys/net/ipv4/tcp_notsent_lowat", "/proc/sys/net/ipv4/tcp_notsent_lowat", "4294967295"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_abort_on_overflow", "/proc/sys/net/ipv4/tcp_abort_on_overflow", "0"),
    SYS("echo '300' > /proc/sys/net/ipv4/tcp_max_reordering", "/proc/sys/net/ipv4/tcp_max_reordering", "300"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_adv_win_scale", "/proc/sys/net/ipv4/tcp_adv_win_scale", "2"),
    SYS("echo '4096' > /proc/sys/net/ipv4/tcp_max_syn_backlog", "/proc/sys/net/ipv4/tcp_max_syn_backlog", "4096"),
    SYS("echo '16384' > /proc/sys/net/ipv4/tcp_max_tw_buckets", "/proc/sys/net/ipv4/tcp_max_tw_buckets", "16384"),
    SYS("echo '1024 65535' > /proc/sys/net/ipv4/ip_local_port_range", "/proc/sys/net/ipv4/ip_local_port_range", "1024 65535"),
    SYS("echo '30' > /proc/sys/net/ipv4/tcp_fin_timeout", "/proc/sys/net/ipv4/tcp_fin_timeout", "30"),
    SYS("echo '1800' > /proc/sys/net/ipv4/tcp_keepalive_time", "/proc/sys/net/ipv4/tcp_keepalive_time", "1800"),
    SYS("echo '60' > /proc/sys/net/ipv4/tcp_keepalive_intvl", "/proc/sys/net/ipv4/tcp_keepalive_intvl", "60"),
    SYS("echo '9' > /proc/sys/net/ipv4/tcp_keepalive_probes", "/proc/sys/net/ipv4/tcp_keepalive_probes", "9"),
    SYS("echo '1' > /proc/sys/net/ipv4/route/flush", "/proc/sys/net/ipv4/route/flush", "1"),
    SH("cmd wifi force-low-latency-mode disabled 2>/dev/null || true", "cmd wifi force-low-latency-mode disabled 2>/dev/null || true"),
};
#define OP_DOWNLOAD_LEN ((int)(sizeof OP_DOWNLOAD / sizeof OP_DOWNLOAD[0]))

static const op_t OP_STREAMING[] = {
    SYS("echo '8192' > /proc/sys/net/core/netdev_max_backlog", "/proc/sys/net/core/netdev_max_backlog", "8192"),
    SYS("echo '4096' > /proc/sys/net/core/somaxconn", "/proc/sys/net/core/somaxconn", "4096"),
    SYS("echo '30000' > /proc/sys/net/core/netdev_budget", "/proc/sys/net/core/netdev_budget", "30000"),
    SYS("echo '8000' > /proc/sys/net/core/netdev_budget_usecs", "/proc/sys/net/core/netdev_budget_usecs", "8000"),
    SYS("echo '204800' > /proc/sys/net/core/optmem_max", "/proc/sys/net/core/optmem_max", "204800"),
    SYS("echo '262144' > /proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_default", "262144"),
    SYS("echo '8388608' > /proc/sys/net/core/rmem_max", "/proc/sys/net/core/rmem_max", "8388608"),
    SYS("echo '262144' > /proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_default", "262144"),
    SYS("echo '8388608' > /proc/sys/net/core/wmem_max", "/proc/sys/net/core/wmem_max", "8388608"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_slow_start_after_idle", "/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_sack", "/proc/sys/net/ipv4/tcp_sack", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_window_scaling", "/proc/sys/net/ipv4/tcp_window_scaling", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_moderate_rcvbuf", "/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_timestamps", "/proc/sys/net/ipv4/tcp_timestamps", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_no_metrics_save", "/proc/sys/net/ipv4/tcp_no_metrics_save", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_mtu_probing", "/proc/sys/net/ipv4/tcp_mtu_probing", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_rfc1337", "/proc/sys/net/ipv4/tcp_rfc1337", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_fastopen", "/proc/sys/net/ipv4/tcp_fastopen", "3"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_frto", "/proc/sys/net/ipv4/tcp_frto", "2"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_recovery", "/proc/sys/net/ipv4/tcp_recovery", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_early_retrans", "/proc/sys/net/ipv4/tcp_early_retrans", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_reordering", "/proc/sys/net/ipv4/tcp_reordering", "3"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_ecn", "/proc/sys/net/ipv4/tcp_ecn", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_tw_reuse", "/proc/sys/net/ipv4/tcp_tw_reuse", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_syn_retries", "/proc/sys/net/ipv4/tcp_syn_retries", "3"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_synack_retries", "/proc/sys/net/ipv4/tcp_synack_retries", "3"),
    SYS("echo '4096 87380 8388608' > /proc/sys/net/ipv4/tcp_rmem", "/proc/sys/net/ipv4/tcp_rmem", "4096 87380 8388608"),
    SYS("echo '4096 65536 4194304' > /proc/sys/net/ipv4/tcp_wmem", "/proc/sys/net/ipv4/tcp_wmem", "4096 65536 4194304"),
    SYS("echo '65536 131072 196608' > /proc/sys/net/ipv4/tcp_mem", "/proc/sys/net/ipv4/tcp_mem", "65536 131072 196608"),
    SYS("echo '32768' > /proc/sys/net/ipv4/udp_rmem_min", "/proc/sys/net/ipv4/udp_rmem_min", "32768"),
    SYS("echo '32768' > /proc/sys/net/ipv4/udp_wmem_min", "/proc/sys/net/ipv4/udp_wmem_min", "32768"),
    SYS("echo '131072' > /proc/sys/net/ipv4/tcp_notsent_lowat", "/proc/sys/net/ipv4/tcp_notsent_lowat", "131072"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_abort_on_overflow", "/proc/sys/net/ipv4/tcp_abort_on_overflow", "0"),
    SYS("echo '200' > /proc/sys/net/ipv4/tcp_max_reordering", "/proc/sys/net/ipv4/tcp_max_reordering", "200"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_adv_win_scale", "/proc/sys/net/ipv4/tcp_adv_win_scale", "1"),
    SYS("echo '2048' > /proc/sys/net/ipv4/tcp_max_syn_backlog", "/proc/sys/net/ipv4/tcp_max_syn_backlog", "2048"),
    SYS("echo '8192' > /proc/sys/net/ipv4/tcp_max_tw_buckets", "/proc/sys/net/ipv4/tcp_max_tw_buckets", "8192"),
    SYS("echo '1024 65535' > /proc/sys/net/ipv4/ip_local_port_range", "/proc/sys/net/ipv4/ip_local_port_range", "1024 65535"),
    SYS("echo '20' > /proc/sys/net/ipv4/tcp_fin_timeout", "/proc/sys/net/ipv4/tcp_fin_timeout", "20"),
    SYS("echo '1200' > /proc/sys/net/ipv4/tcp_keepalive_time", "/proc/sys/net/ipv4/tcp_keepalive_time", "1200"),
    SYS("echo '30' > /proc/sys/net/ipv4/tcp_keepalive_intvl", "/proc/sys/net/ipv4/tcp_keepalive_intvl", "30"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_keepalive_probes", "/proc/sys/net/ipv4/tcp_keepalive_probes", "5"),
    SYS("echo '1' > /proc/sys/net/ipv4/route/flush", "/proc/sys/net/ipv4/route/flush", "1"),
    SH("cmd wifi force-low-latency-mode disabled 2>/dev/null || true", "cmd wifi force-low-latency-mode disabled 2>/dev/null || true"),
};
#define OP_STREAMING_LEN ((int)(sizeof OP_STREAMING / sizeof OP_STREAMING[0]))

static const op_t OP_SOCIAL[] = {
    SYS("echo '2048' > /proc/sys/net/core/netdev_max_backlog", "/proc/sys/net/core/netdev_max_backlog", "2048"),
    SYS("echo '2048' > /proc/sys/net/core/somaxconn", "/proc/sys/net/core/somaxconn", "2048"),
    SYS("echo '4096' > /proc/sys/net/core/netdev_budget", "/proc/sys/net/core/netdev_budget", "4096"),
    SYS("echo '4000' > /proc/sys/net/core/netdev_budget_usecs", "/proc/sys/net/core/netdev_budget_usecs", "4000"),
    SYS("echo '204800' > /proc/sys/net/core/optmem_max", "/proc/sys/net/core/optmem_max", "204800"),
    SYS("echo '131072' > /proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_default", "131072"),
    SYS("echo '524288' > /proc/sys/net/core/rmem_max", "/proc/sys/net/core/rmem_max", "524288"),
    SYS("echo '131072' > /proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_default", "131072"),
    SYS("echo '524288' > /proc/sys/net/core/wmem_max", "/proc/sys/net/core/wmem_max", "524288"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_slow_start_after_idle", "/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_sack", "/proc/sys/net/ipv4/tcp_sack", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_window_scaling", "/proc/sys/net/ipv4/tcp_window_scaling", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_moderate_rcvbuf", "/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_timestamps", "/proc/sys/net/ipv4/tcp_timestamps", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_no_metrics_save", "/proc/sys/net/ipv4/tcp_no_metrics_save", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_mtu_probing", "/proc/sys/net/ipv4/tcp_mtu_probing", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_rfc1337", "/proc/sys/net/ipv4/tcp_rfc1337", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_fastopen", "/proc/sys/net/ipv4/tcp_fastopen", "3"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_frto", "/proc/sys/net/ipv4/tcp_frto", "2"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_recovery", "/proc/sys/net/ipv4/tcp_recovery", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_early_retrans", "/proc/sys/net/ipv4/tcp_early_retrans", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_reordering", "/proc/sys/net/ipv4/tcp_reordering", "3"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_ecn", "/proc/sys/net/ipv4/tcp_ecn", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_tw_reuse", "/proc/sys/net/ipv4/tcp_tw_reuse", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_syn_retries", "/proc/sys/net/ipv4/tcp_syn_retries", "3"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_synack_retries", "/proc/sys/net/ipv4/tcp_synack_retries", "3"),
    SYS("echo '4096 65536 524288' > /proc/sys/net/ipv4/tcp_rmem", "/proc/sys/net/ipv4/tcp_rmem", "4096 65536 524288"),
    SYS("echo '4096 65536 262144' > /proc/sys/net/ipv4/tcp_wmem", "/proc/sys/net/ipv4/tcp_wmem", "4096 65536 262144"),
    SYS("echo '16384 32768 49152' > /proc/sys/net/ipv4/tcp_mem", "/proc/sys/net/ipv4/tcp_mem", "16384 32768 49152"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_rmem_min", "/proc/sys/net/ipv4/udp_rmem_min", "16384"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_wmem_min", "/proc/sys/net/ipv4/udp_wmem_min", "16384"),
    SYS("echo '32768' > /proc/sys/net/ipv4/tcp_notsent_lowat", "/proc/sys/net/ipv4/tcp_notsent_lowat", "32768"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_abort_on_overflow", "/proc/sys/net/ipv4/tcp_abort_on_overflow", "0"),
    SYS("echo '300' > /proc/sys/net/ipv4/tcp_max_reordering", "/proc/sys/net/ipv4/tcp_max_reordering", "300"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_adv_win_scale", "/proc/sys/net/ipv4/tcp_adv_win_scale", "1"),
    SYS("echo '1024' > /proc/sys/net/ipv4/tcp_max_syn_backlog", "/proc/sys/net/ipv4/tcp_max_syn_backlog", "1024"),
    SYS("echo '4096' > /proc/sys/net/ipv4/tcp_max_tw_buckets", "/proc/sys/net/ipv4/tcp_max_tw_buckets", "4096"),
    SYS("echo '1024 65535' > /proc/sys/net/ipv4/ip_local_port_range", "/proc/sys/net/ipv4/ip_local_port_range", "1024 65535"),
    SYS("echo '15' > /proc/sys/net/ipv4/tcp_fin_timeout", "/proc/sys/net/ipv4/tcp_fin_timeout", "15"),
    SYS("echo '600' > /proc/sys/net/ipv4/tcp_keepalive_time", "/proc/sys/net/ipv4/tcp_keepalive_time", "600"),
    SYS("echo '15' > /proc/sys/net/ipv4/tcp_keepalive_intvl", "/proc/sys/net/ipv4/tcp_keepalive_intvl", "15"),
    SYS("echo '4' > /proc/sys/net/ipv4/tcp_keepalive_probes", "/proc/sys/net/ipv4/tcp_keepalive_probes", "4"),
    SYS("echo '1' > /proc/sys/net/ipv4/route/flush", "/proc/sys/net/ipv4/route/flush", "1"),
    SH("cmd wifi force-low-latency-mode enabled 2>/dev/null || true", "cmd wifi force-low-latency-mode enabled 2>/dev/null || true"),
};
#define OP_SOCIAL_LEN ((int)(sizeof OP_SOCIAL / sizeof OP_SOCIAL[0]))

static const op_t OP_OUTDOOR[] = {
    SYS("echo '4096' > /proc/sys/net/core/netdev_max_backlog", "/proc/sys/net/core/netdev_max_backlog", "4096"),
    SYS("echo '2048' > /proc/sys/net/core/somaxconn", "/proc/sys/net/core/somaxconn", "2048"),
    SYS("echo '8000' > /proc/sys/net/core/netdev_budget", "/proc/sys/net/core/netdev_budget", "8000"),
    SYS("echo '8000' > /proc/sys/net/core/netdev_budget_usecs", "/proc/sys/net/core/netdev_budget_usecs", "8000"),
    SYS("echo '102400' > /proc/sys/net/core/optmem_max", "/proc/sys/net/core/optmem_max", "102400"),
    SYS("echo '262144' > /proc/sys/net/core/rmem_default", "/proc/sys/net/core/rmem_default", "262144"),
    SYS("echo '2097152' > /proc/sys/net/core/rmem_max", "/proc/sys/net/core/rmem_max", "2097152"),
    SYS("echo '262144' > /proc/sys/net/core/wmem_default", "/proc/sys/net/core/wmem_default", "262144"),
    SYS("echo '2097152' > /proc/sys/net/core/wmem_max", "/proc/sys/net/core/wmem_max", "2097152"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_slow_start_after_idle", "/proc/sys/net/ipv4/tcp_slow_start_after_idle", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_sack", "/proc/sys/net/ipv4/tcp_sack", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_window_scaling", "/proc/sys/net/ipv4/tcp_window_scaling", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_moderate_rcvbuf", "/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_timestamps", "/proc/sys/net/ipv4/tcp_timestamps", "1"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_no_metrics_save", "/proc/sys/net/ipv4/tcp_no_metrics_save", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_mtu_probing", "/proc/sys/net/ipv4/tcp_mtu_probing", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_rfc1337", "/proc/sys/net/ipv4/tcp_rfc1337", "1"),
    SYS("echo '3' > /proc/sys/net/ipv4/tcp_fastopen", "/proc/sys/net/ipv4/tcp_fastopen", "3"),
    SYS("echo '2' > /proc/sys/net/ipv4/tcp_frto", "/proc/sys/net/ipv4/tcp_frto", "2"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_recovery", "/proc/sys/net/ipv4/tcp_recovery", "1"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_early_retrans", "/proc/sys/net/ipv4/tcp_early_retrans", "1"),
    SYS("echo '10' > /proc/sys/net/ipv4/tcp_reordering", "/proc/sys/net/ipv4/tcp_reordering", "10"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_ecn", "/proc/sys/net/ipv4/tcp_ecn", "0"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_tw_reuse", "/proc/sys/net/ipv4/tcp_tw_reuse", "1"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_syn_retries", "/proc/sys/net/ipv4/tcp_syn_retries", "5"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_synack_retries", "/proc/sys/net/ipv4/tcp_synack_retries", "5"),
    SYS("echo '4096 87380 2097152' > /proc/sys/net/ipv4/tcp_rmem", "/proc/sys/net/ipv4/tcp_rmem", "4096 87380 2097152"),
    SYS("echo '4096 65536 1048576' > /proc/sys/net/ipv4/tcp_wmem", "/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576"),
    SYS("echo '32768 65536 98304' > /proc/sys/net/ipv4/tcp_mem", "/proc/sys/net/ipv4/tcp_mem", "32768 65536 98304"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_rmem_min", "/proc/sys/net/ipv4/udp_rmem_min", "16384"),
    SYS("echo '16384' > /proc/sys/net/ipv4/udp_wmem_min", "/proc/sys/net/ipv4/udp_wmem_min", "16384"),
    SYS("echo '65536' > /proc/sys/net/ipv4/tcp_notsent_lowat", "/proc/sys/net/ipv4/tcp_notsent_lowat", "65536"),
    SYS("echo '0' > /proc/sys/net/ipv4/tcp_abort_on_overflow", "/proc/sys/net/ipv4/tcp_abort_on_overflow", "0"),
    SYS("echo '600' > /proc/sys/net/ipv4/tcp_max_reordering", "/proc/sys/net/ipv4/tcp_max_reordering", "600"),
    SYS("echo '1' > /proc/sys/net/ipv4/tcp_adv_win_scale", "/proc/sys/net/ipv4/tcp_adv_win_scale", "1"),
    SYS("echo '2048' > /proc/sys/net/ipv4/tcp_max_syn_backlog", "/proc/sys/net/ipv4/tcp_max_syn_backlog", "2048"),
    SYS("echo '8192' > /proc/sys/net/ipv4/tcp_max_tw_buckets", "/proc/sys/net/ipv4/tcp_max_tw_buckets", "8192"),
    SYS("echo '1024 65535' > /proc/sys/net/ipv4/ip_local_port_range", "/proc/sys/net/ipv4/ip_local_port_range", "1024 65535"),
    SYS("echo '30' > /proc/sys/net/ipv4/tcp_fin_timeout", "/proc/sys/net/ipv4/tcp_fin_timeout", "30"),
    SYS("echo '1200' > /proc/sys/net/ipv4/tcp_keepalive_time", "/proc/sys/net/ipv4/tcp_keepalive_time", "1200"),
    SYS("echo '30' > /proc/sys/net/ipv4/tcp_keepalive_intvl", "/proc/sys/net/ipv4/tcp_keepalive_intvl", "30"),
    SYS("echo '5' > /proc/sys/net/ipv4/tcp_keepalive_probes", "/proc/sys/net/ipv4/tcp_keepalive_probes", "5"),
    SYS("echo '1' > /proc/sys/net/ipv4/route/flush", "/proc/sys/net/ipv4/route/flush", "1"),
    SH("cmd wifi force-low-latency-mode disabled 2>/dev/null || true", "cmd wifi force-low-latency-mode disabled 2>/dev/null || true"),
};
#define OP_OUTDOOR_LEN ((int)(sizeof OP_OUTDOOR / sizeof OP_OUTDOOR[0]))

static const op_t OP_STATE[] = {
    SH("cmd settings put global netstats_enabled 0", "cmd settings put global netstats_enabled 0"),
    SH("cmd wifi set-scan-always-available disabled", "cmd wifi set-scan-always-available disabled"),
    SH("cmd wifi set-verbose-logging disabled -l 0", "cmd wifi set-verbose-logging disabled -l 0"),
};
#define OP_STATE_LEN ((int)(sizeof OP_STATE / sizeof OP_STATE[0]))

static const op_t OP_UNSTATE[] = {
    SH("cmd settings delete global netstats_enabled", "cmd settings delete global netstats_enabled"),
    SH("cmd wifi set-scan-always-available enabled", "cmd wifi set-scan-always-available enabled"),
    SH("cmd wifi set-verbose-logging enabled -l 1", "cmd wifi set-verbose-logging enabled -l 1"),
};
#define OP_UNSTATE_LEN ((int)(sizeof OP_UNSTATE / sizeof OP_UNSTATE[0]))

static const op_t OP_SAVER[] = {
    SH("cmd netpolicy set restrict-background true", "cmd netpolicy set restrict-background true"),
    SH("cmd settings put global ingress_rate_limit_bytes_per_second 1048576", "cmd settings put global ingress_rate_limit_bytes_per_second 1048576"),
};
#define OP_SAVER_LEN ((int)(sizeof OP_SAVER / sizeof OP_SAVER[0]))

static const op_t OP_UNSAVER[] = {
    SH("cmd netpolicy set restrict-background false", "cmd netpolicy set restrict-background false"),
    SH("cmd settings delete global ingress_rate_limit_bytes_per_second", "cmd settings delete global ingress_rate_limit_bytes_per_second"),
};
#define OP_UNSAVER_LEN ((int)(sizeof OP_UNSAVER / sizeof OP_UNSAVER[0]))

static const op_t OP_DISABLE[] = {
    SYS("sysctl -w net.ipv6.conf.all.disable_ipv6=1", "/proc/sys/net/ipv6/conf/all/disable_ipv6", "1"),
    SYS("sysctl -w net.ipv6.conf.default.disable_ipv6=1", "/proc/sys/net/ipv6/conf/default/disable_ipv6", "1"),
    SYS("sysctl -w net.ipv6.conf.lo.disable_ipv6=1", "/proc/sys/net/ipv6/conf/lo/disable_ipv6", "1"),
    SH("content update --uri content://telephony/carriers --bind protocol:s:IP --bind roaming_protocol:s:IP", "content update --uri content://telephony/carriers --bind protocol:s:IP --bind roaming_protocol:s:IP"),
    SH("cmd connectivity airplane-mode enable", "cmd connectivity airplane-mode enable"),
    SH("sleep 1", "sleep 1"),
    SH("cmd connectivity airplane-mode disable", "cmd connectivity airplane-mode disable"),
};
#define OP_DISABLE_LEN ((int)(sizeof OP_DISABLE / sizeof OP_DISABLE[0]))

static const op_t OP_ENABLE[] = {
    SYS("sysctl -w net.ipv6.conf.all.disable_ipv6=0", "/proc/sys/net/ipv6/conf/all/disable_ipv6", "0"),
    SYS("sysctl -w net.ipv6.conf.default.disable_ipv6=0", "/proc/sys/net/ipv6/conf/default/disable_ipv6", "0"),
    SYS("sysctl -w net.ipv6.conf.lo.disable_ipv6=0", "/proc/sys/net/ipv6/conf/lo/disable_ipv6", "0"),
    SH("content update --uri content://telephony/carriers --bind protocol:s:IPV4V6 --bind roaming_protocol:s:IPV4V6", "content update --uri content://telephony/carriers --bind protocol:s:IPV4V6 --bind roaming_protocol:s:IPV4V6"),
    SH("cmd connectivity airplane-mode enable", "cmd connectivity airplane-mode enable"),
    SH("sleep 1", "sleep 1"),
    SH("cmd connectivity airplane-mode disable", "cmd connectivity airplane-mode disable"),
};
#define OP_ENABLE_LEN ((int)(sizeof OP_ENABLE / sizeof OP_ENABLE[0]))

static const op_t OP_CONNTRACK_ON[] = {
    SYS("echo '32768' > $CT/nf_conntrack_max", "/proc/sys/net/netfilter/nf_conntrack_max", "32768"),
    SYS("echo '600' > $CT/nf_conntrack_tcp_timeout_established", "/proc/sys/net/netfilter/nf_conntrack_tcp_timeout_established", "600"),
    SYS("echo '30' > $CT/nf_conntrack_tcp_timeout_time_wait", "/proc/sys/net/netfilter/nf_conntrack_tcp_timeout_time_wait", "30"),
    SYS("echo '15' > $CT/nf_conntrack_udp_timeout", "/proc/sys/net/netfilter/nf_conntrack_udp_timeout", "15"),
    SYS("echo '60' > $CT/nf_conntrack_udp_timeout_stream", "/proc/sys/net/netfilter/nf_conntrack_udp_timeout_stream", "60"),
};
#define OP_CONNTRACK_ON_LEN ((int)(sizeof OP_CONNTRACK_ON / sizeof OP_CONNTRACK_ON[0]))

/* ---------- mode functions ---------- */

static int mode_default(void);
static int mode_game(void);
static int mode_download(void);
static int mode_streaming(void);
static int mode_social(void);
static int mode_browsing(void);
static int mode_outdoor(void);
static int mode_state(void);
static int mode_unstate(void);
static int mode_saver(void);
static int mode_unsaver(void);
static int mode_disable(void);
static int mode_enable(void);
static int mode_nic_offload(void);
static int mode_nic_on(void);
static int mode_wifi_ps_off(void);
static int mode_wifi_ps_on(void);
static int mode_conntrack_on(void);
static int mode_conntrack_off(void);
static int mode_irq_affinity(void);
static int mode_unirq_affinity(void);
static int mode_boost_ksoft(void);
static int mode_unboost_ksoft(void);
static int mode_hw_tweak(void);
static int mode_init_tc(void);
static int mode_engine(const char *event);
static int mode_auto_detect(void);
static int mode_auto_on(void);
static int mode_auto_off(void);
static int mode_daemon(void);
static int mode_boot(void);

static void call_mode(const char *name, int (*fn)(void)) {
    const char *saved = g_mode;
    g_mode = name;
    fn();
    g_mode = saved;
}

static void call_preset(const char *act) {
    if (strcmp(act, "game") == 0) call_mode("--game", mode_game);
    else if (strcmp(act, "download") == 0) call_mode("--download", mode_download);
    else if (strcmp(act, "streaming") == 0) call_mode("--streaming", mode_streaming);
    else if (strcmp(act, "social") == 0) call_mode("--social", mode_social);
    else if (strcmp(act, "browsing") == 0) call_mode("--browsing", mode_browsing);
    else call_mode("--default", mode_default);
}

static int mode_default(void) {
    apply_ops("Network Preset: Default", OP_DEFAULT, OP_DEFAULT_LEN);
    set_default_qdisc();
    return 0;
}

static int mode_game(void) {
    apply_ops("Network Preset: Game", OP_GAME, OP_GAME_LEN);
    set_default_qdisc();
    return 0;
}

static int mode_download(void) {
    apply_ops("Network Preset: Download", OP_DOWNLOAD, OP_DOWNLOAD_LEN);
    set_default_qdisc();
    return 0;
}

static int mode_streaming(void) {
    apply_ops("Network Preset: Streaming", OP_STREAMING, OP_STREAMING_LEN);
    set_default_qdisc();
    return 0;
}

static int mode_social(void) {
    apply_ops("Network Preset: Social / Browsing", OP_SOCIAL, OP_SOCIAL_LEN);
    set_default_qdisc();
    return 0;
}

static int mode_browsing(void) {
    return mode_social();
}

static int mode_outdoor(void) {
    apply_ops("Network Preset: Outdoor", OP_OUTDOOR, OP_OUTDOOR_LEN);
    set_default_qdisc();
    return 0;
}

static int mode_state(void) {
    apply_ops("Disable Network State", OP_STATE, OP_STATE_LEN);
    return 0;
}

static int mode_unstate(void) {
    apply_ops("Enable Network State", OP_UNSTATE, OP_UNSTATE_LEN);
    return 0;
}

static int mode_saver(void) {
    apply_ops("Enable Data Saver", OP_SAVER, OP_SAVER_LEN);
    return 0;
}

static int mode_unsaver(void) {
    apply_ops("Disable Data Saver", OP_UNSAVER, OP_UNSAVER_LEN);
    return 0;
}

static int mode_disable(void) {
    apply_ops("Disable IPv6 Connection", OP_DISABLE, OP_DISABLE_LEN);
    return 0;
}

static int mode_enable(void) {
    apply_ops("Enable IPv6 Connection", OP_ENABLE, OP_ENABLE_LEN);
    return 0;
}

static void nic_offload_common(int disable) {
    if (!command_exists("ethtool")) {
        write_log("NIC Offloading: ethtool not available on this ROM, skipping", "SKIP");
        return;
    }
    int ok = 0, total = 0;
    DIR *d = opendir("/sys/class/net");
    if (!d) return;
    struct dirent *e;
    char cmd[512];
    const char *sw = disable ? "off" : "on";
    while ((e = readdir(d))) {
        const char *n = e->d_name;
        if (strncmp(n, "wlan", 4) && strncmp(n, "eth", 3) &&
            strncmp(n, "rmnet", 5) && strncmp(n, "ccmni", 5) &&
            strncmp(n, "radio", 5))
            continue;
        total++;
        snprintf(cmd, sizeof cmd, "ethtool -K %s gro %s gso %s 2>/dev/null", n, sw, sw);
        if (sh(cmd) == 0) ok++;
    }
    closedir(d);
    if (ok > 0)
        write_logf("NIC Offloading: GRO/GSO %s on %d/%d interfaces", "SUCCESS",
                   disable ? "disabled" : "restored", ok, total);
    else
        write_log("NIC Offloading: driver rejected changes (not supported)", "SKIP");
}

static int mode_nic_offload(void) {
    nic_offload_common(1);
    return 0;
}

static int mode_nic_on(void) {
    nic_offload_common(0);
    return 0;
}

static void active_wlan_if(char *out, size_t n) {
    route_iface(out, n);
    if (strncmp(out, "wlan", 4) != 0) snprintf(out, n, "wlan0");
}

static int mode_wifi_ps_off(void) {
    /* UI ON = power save MATI (anti-jitter). Penamaan mengikuti aksi ke driver. */
    if (!command_exists("iw")) {
        write_log("Wi-Fi Low-Jitter: iw not available", "SKIP");
        return 0;
    }
    char ifc[64];
    active_wlan_if(ifc, sizeof ifc);
    char cmd[256];
    snprintf(cmd, sizeof cmd, "iw dev %s set power_save off >/dev/null 2>&1", ifc);
    if (sh(cmd) == 0)
        write_logf("Wi-Fi Low-Jitter: power_save disabled on %s", "SUCCESS", ifc);
    else
        write_log("Wi-Fi Low-Jitter: driver rejected", "SKIP");
    return 0;
}

static int mode_wifi_ps_on(void) {
    char ifc[64];
    active_wlan_if(ifc, sizeof ifc);
    char cmd[256];
    snprintf(cmd, sizeof cmd, "iw dev %s set power_save on >/dev/null 2>&1", ifc);
    if (sh(cmd) == 0)
        write_log("Wi-Fi Low-Jitter: power_save restored", "SUCCESS");
    else
        write_log("Wi-Fi Low-Jitter: restore failed", "SKIP");
    return 0;
}

static int mode_conntrack_on(void) {
    /* Simpan nilai asli SEKALI (ke state.json) agar restore akurat ke default kernel. */
    char b[64], v[128];
    if (!pp_get("conntrack_backup_max", b, sizeof b) || !b[0]) {
        static const char *bk[][2] = {
            {"conntrack_backup_max", "/proc/sys/net/netfilter/nf_conntrack_max"},
            {"conntrack_backup_tcp_est", "/proc/sys/net/netfilter/nf_conntrack_tcp_timeout_established"},
            {"conntrack_backup_tcp_tw", "/proc/sys/net/netfilter/nf_conntrack_tcp_timeout_time_wait"},
            {"conntrack_backup_udp", "/proc/sys/net/netfilter/nf_conntrack_udp_timeout"},
            {"conntrack_backup_udp_stream", "/proc/sys/net/netfilter/nf_conntrack_udp_timeout_stream"},
        };
        for (size_t i = 0; i < sizeof bk / sizeof bk[0]; i++) {
            char p[PATH_MAX + 128];
            pp_path(p, sizeof p, bk[i][1]);
            if (access(p, F_OK) != 0) continue;
            if (read_first_line(bk[i][1], v, sizeof v) == 0)
                pp_set(bk[i][0], v);
        }
    }
    /* est 600s selaras keepalive game — koneksi aktif tak akan ter-GC */
    apply_ops("ConnTrack Optimization", OP_CONNTRACK_ON, OP_CONNTRACK_ON_LEN);
    return 0;
}

static int mode_conntrack_off(void) {
    char b1[64], b2[64], b3[64], b4[64], b5[64];
    pp_get("conntrack_backup_max", b1, sizeof b1);
    pp_get("conntrack_backup_tcp_est", b2, sizeof b2);
    pp_get("conntrack_backup_tcp_tw", b3, sizeof b3);
    pp_get("conntrack_backup_udp", b4, sizeof b4);
    pp_get("conntrack_backup_udp_stream", b5, sizeof b5);
    if (!b1[0] && !b2[0] && !b3[0] && !b4[0] && !b5[0]) {
        write_log("ConnTrack: no backup", "SKIP");
        return 0;
    }
    static const char *pairs[][2] = {
        {"max", "nf_conntrack_max"},
        {"tcp_est", "nf_conntrack_tcp_timeout_established"},
        {"tcp_tw", "nf_conntrack_tcp_timeout_time_wait"},
        {"udp", "nf_conntrack_udp_timeout"},
        {"udp_stream", "nf_conntrack_udp_timeout_stream"},
    };
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; i++) {
        char k[96], v[64], path[160];
        snprintf(k, sizeof k, "conntrack_backup_%s", pairs[i][0]);
        if (pp_get(k, v, sizeof v) && v[0]) {
            snprintf(path, sizeof path, "/proc/sys/net/netfilter/%s", pairs[i][1]);
            write_sysfile(path, v);
        }
    }
    write_log("ConnTrack: restored kernel defaults", "SUCCESS");
    return 0;
}

static int irq_write_mask(const char *mask) {
    char p[PATH_MAX];
    pp_path(p, sizeof p, "/proc/interrupts");
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    char line[1024];
    int failed = 0;
    while (fgets(line, sizeof line, f)) {
        if (!pp_strcasestr(line, "wlan") && !pp_strcasestr(line, "rmnet") &&
            !pp_strcasestr(line, "ccmni"))
            continue;
        const char *c = line;
        while (isspace((unsigned char)*c)) c++;
        char num[32];
        int i = 0;
        while (c[i] && c[i] != ':' && i < 30) {
            num[i] = c[i];
            i++;
        }
        num[i] = 0;
        if (!i || c[i] != ':') continue;
        char path[128];
        snprintf(path, sizeof path, "/proc/irq/%s/smp_affinity", num);
        if (write_sysfile(path, mask) != 0) failed = 1;
    }
    fclose(f);
    return failed;
}

static long cpu_present_max(void) {
    char present[64];
    if (read_first_line("/sys/devices/system/cpu/present", present, sizeof present) != 0)
        return 0;
    char *dash = strchr(present, '-');
    return strtol(dash ? dash + 1 : present, NULL, 10);
}

static int mode_irq_affinity(void) {
    long cpu = cpu_present_max();
    const char *mask = (cpu >= 7) ? "c0" : (cpu >= 3) ? "c" : "3";
    int failed = irq_write_mask(mask);
    write_logf("Set IRQ affinity to mask %s", failed ? "FAILED" : "SUCCESS", mask);
    return 0;
}

static int mode_unirq_affinity(void) {
    int failed = irq_write_mask("ff");
    write_log("Restored IRQ affinity to all cores", failed ? "FAILED" : "SUCCESS");
    return 0;
}

static int mode_boost_ksoft(void) {
    int failed = ksoft_renice(-20);
    write_log("Boosted ksoftirqd priority", failed ? "FAILED" : "SUCCESS");
    return 0;
}

static int mode_unboost_ksoft(void) {
    int failed = ksoft_renice(0);
    write_log("Restored ksoftirqd priority to default", failed ? "FAILED" : "SUCCESS");
    return 0;
}

static int mode_hw_tweak(void) {
    long cpu = cpu_present_max();
    const char *mask = (cpu >= 7) ? "ff" : (cpu >= 3) ? "f" : "3";

    if (write_sysfile("/proc/sys/net/core/rps_sock_flow_entries", "32768") == 0)
        write_log("Set rps_sock_flow_entries", "SUCCESS");
    else
        write_log("Set rps_sock_flow_entries", "FAILED");

    DIR *d = opendir("/sys/class/net");
    if (d) {
        struct dirent *e;
        char qdir[PATH_MAX + 64], fpath[PATH_MAX + 512];
        while ((e = readdir(d))) {
            const char *n = e->d_name;
            if (!strncmp(n, "lo", 2) || !strncmp(n, "dummy", 5) || !strncmp(n, "ifb", 3))
                continue;
            snprintf(qdir, sizeof qdir, "/sys/class/net/%s/queues", n);
            DIR *q = opendir(qdir);
            if (!q) continue;
            struct dirent *qe;
            while ((qe = readdir(q))) {
                if (!strncmp(qe->d_name, "rx-", 3)) {
                    snprintf(fpath, sizeof fpath, "%s/%s/rps_cpus", qdir, qe->d_name);
                    write_sysfile(fpath, mask);
                    snprintf(fpath, sizeof fpath, "%s/%s/rps_flow_cnt", qdir, qe->d_name);
                    write_sysfile(fpath, "4096");
                } else if (!strncmp(qe->d_name, "tx-", 3)) {
                    snprintf(fpath, sizeof fpath, "%s/%s/xps_cpus", qdir, qe->d_name);
                    write_sysfile(fpath, mask);
                }
            }
            closedir(q);
        }
        closedir(d);
    }
    write_logf("Applied CPU mask %s to RPS/XPS queues", "SUCCESS", mask);

    char bpath[PATH_MAX + 64];
    snprintf(bpath, sizeof bpath, "%s/boost_apps.txt", g_moddir);
    if (access(bpath, F_OK) == 0) {
        int use_dscp =
            (sh("iptables -t mangle -A OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null") == 0);
        if (use_dscp)
            sh("iptables -t mangle -D OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null");

        FILE *bf = fopen(bpath, "r");
        if (bf) {
            char buf[8192];
            size_t r = fread(buf, 1, sizeof buf - 1, bf);
            buf[r] = 0;
            fclose(bf);
            int boosted = 0;
            char *save = NULL;
            for (char *tok = strtok_r(buf, ", \t\r\n", &save); tok;
                 tok = strtok_r(NULL, ", \t\r\n", &save)) {
                long uid = pm_uid(tok);
                if (uid < 0) continue;
                if (use_dscp) {
                    shf("iptables -t mangle -I OUTPUT -m owner --uid-owner %ld -j DSCP --set-dscp 46 2>/dev/null", uid);
                    shf("ip6tables -t mangle -I OUTPUT -m owner --uid-owner %ld -j DSCP --set-dscp 46 2>/dev/null", uid);
                } else {
                    shf("iptables -t mangle -I OUTPUT -m owner --uid-owner %ld -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null", uid);
                    shf("ip6tables -t mangle -I OUTPUT -m owner --uid-owner %ld -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null", uid);
                }
                boosted++;
            }
            if (boosted > 0)
                write_logf("Prioritized traffic for %d boosted apps", "SUCCESS", boosted);
        }
    }
    return 0;
}

static int mode_init_tc(void) {
    char ifc[64];
    route_iface(ifc, sizeof ifc);
    if (!ifc[0]) {
        write_log("Failed to find active interface for TC", "FAILED");
        return 0;
    }

    char act[32];
    if (!pp_get("auto_act", act, sizeof act) || !act[0])
        snprintf(act, sizeof act, "default");
    int isgame = strcmp(act, "game") == 0;

    int limit, flows, txq;
    const char *interval, *target;
    if (isgame) {
        limit = 64; flows = 128; interval = "10ms"; target = "1ms"; txq = 500;
    } else {
        const char *e = getenv("PP_FQ_LIMIT");
        limit = e ? atoi(e) : 1000;
        e = getenv("PP_FQ_FLOWS");
        flows = e ? atoi(e) : 1024;
        interval = "100ms"; target = "5ms"; txq = 1000;
    }

    shf("tc qdisc del dev %s root 2>/dev/null", ifc);
    int has_cake = (sh("tc qdisc add dev lo root cake >/dev/null 2>&1") == 0);
    if (has_cake) {
        sh("tc qdisc del dev lo root >/dev/null 2>&1");
        write_log("Detected CAKE qdisc support", "SUCCESS");
    } else {
        write_log("CAKE not found. Falling back to fq_codel", "SUCCESS");
    }

    if (has_cake) {
        if (isgame)
            shf("tc qdisc add dev %s root handle 1: cake besteffort nat diffserv4 bandwidth 50mbit raw 2>/dev/null || "
                "tc qdisc add dev %s root handle 1: cake besteffort nat 2>/dev/null", ifc, ifc);
        else
            shf("tc qdisc add dev %s root handle 1: cake besteffort nat 2>/dev/null", ifc);
        shf("tc filter add dev %s parent 1: protocol ip prio 1 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null", ifc);
        shf("tc filter add dev %s parent 1: protocol ipv6 prio 1 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null", ifc);
        shf("tc filter add dev %s parent 1: protocol ip prio 2 u32 match ip tos 0xb8 0xfc flowid 1:1 2>/dev/null", ifc);
        write_logf("Applied CAKE qdisc on %s (mode: %s)", "SUCCESS", ifc, act);
    } else {
        shf("tc qdisc add dev %s root handle 1: prio bands 3 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1 2>/dev/null", ifc);
        shf("tc qdisc add dev %s parent 1:1 handle 10: fq_codel quantum 1514 limit %d flows %d interval %s target %s noecn 2>/dev/null",
            ifc, limit, flows, interval, target);
        shf("tc qdisc add dev %s parent 1:2 handle 20: fq_codel quantum 1514 limit %d flows %d interval 100ms target 5ms noecn 2>/dev/null",
            ifc, limit, flows);
        shf("tc qdisc add dev %s parent 1:3 handle 30: fq_codel quantum 1514 limit %d flows %d interval 100ms target 10ms noecn 2>/dev/null",
            ifc, limit * 4, flows);
        shf("tc filter add dev %s parent 1: protocol ip prio 1 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null", ifc);
        shf("tc filter add dev %s parent 1: protocol ipv6 prio 1 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null", ifc);
        shf("tc filter add dev %s parent 1: protocol ip prio 2 u32 match ip tos 0xb8 0xfc flowid 1:1 2>/dev/null", ifc);
        shf("tc filter add dev %s parent 1: protocol ip prio 3 u32 match ip protocol 6 0xff match u8 0x10 0xff at 33 flowid 1:1 2>/dev/null", ifc);
        write_logf("Applied fq_codel on %s (mode: %s, limit: %d, target: %s)", "SUCCESS",
                   ifc, act, limit, target);
    }
    set_txqueuelen(ifc, txq);
    return 0;
}

static int mode_engine(const char *event) {
    if (!acquire_lock()) return 0;
    atexit(release_lock);

    char ifc[64];
    route_iface(ifc, sizeof ifc);

    char prev_state[128];
    if (!pp_get("engine_state", prev_state, sizeof prev_state)) prev_state[0] = 0;
    char prev_profile[64] = "", prev_iface[64] = "";
    {
        char *c = strchr(prev_state, ':');
        if (c) {
            *c = 0;
            snprintf(prev_iface, sizeof prev_iface, "%s", c + 1);
        }
        snprintf(prev_profile, sizeof prev_profile, "%s", prev_state);
    }

    if (!ifc[0] || strcmp(ifc, "lo") == 0) {
        if (strcmp(prev_profile, "OFFLINE") != 0) {
            pp_set("engine_state", "OFFLINE:none");
            DIR *d = opendir("/sys/class/net");
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (strcmp(e->d_name, "lo") == 0) continue;
                    shf("tc qdisc del dev %s root 2>/dev/null", e->d_name);
                }
                closedir(d);
            }
            write_logf("Network OFFLINE detected (Event: %s), cleared TC rules", "SUCCESS", event);
        }
        return 0;
    }

    char health[32], rtt_s[16] = "", loss_s[16] = "", jitter_s[16] = "";
    get_network_health(health, sizeof health);
    {
        char *save = NULL;
        char *t = strtok_r(health, ":", &save);
        if (t) snprintf(rtt_s, sizeof rtt_s, "%s", t);
        t = strtok_r(NULL, ":", &save);
        if (t) snprintf(loss_s, sizeof loss_s, "%s", t);
        t = strtok_r(NULL, ":", &save);
        if (t) snprintf(jitter_s, sizeof jitter_s, "%s", t);
    }
    long rtt = strtol(rtt_s, NULL, 10);
    long loss = strtol(loss_s, NULL, 10);
    long jitter = strtol(jitter_s, NULL, 10);
    char retrans[24];
    get_tcp_retransmit_rate(retrans, sizeof retrans);
    long score = compute_network_score(rtt, loss, jitter, retrans);

    long rssi = 0;
    const char *band = "NONE";
    char wifi_info[128];
    if (strncmp(ifc, "wlan", 4) == 0) {
        get_wifi_info(ifc, wifi_info, sizeof wifi_info);
        char freq[64] = "0";
        char *c = strchr(wifi_info, ':');
        if (c) {
            *c = 0;
            snprintf(freq, sizeof freq, "%s", c + 1);
        }
        rssi = strtol(wifi_info, NULL, 10);
        band = classify_band(freq);
    }

    const char *new_profile =
        (score < 65 || (rssi != 0 && rssi < -80)) ? "WEAK_NETWORK" : "LOW_LATENCY";

    int iface_changed = strcmp(ifc, prev_iface) != 0;

    char pending[128];
    if (!pp_get("pending_profile", pending, sizeof pending)) pending[0] = 0;
    char pending_profile[64] = "";
    long pending_count = 0;
    {
        char *c = strchr(pending, ':');
        if (c) {
            *c = 0;
            pending_count = strtol(c + 1, NULL, 10);
        }
        snprintf(pending_profile, sizeof pending_profile, "%s", pending);
    }

    int should_apply = 0;
    if (iface_changed || !prev_profile[0] || strcmp(prev_profile, "NONE") == 0 ||
        strcmp(prev_profile, "OFFLINE") == 0) {
        should_apply = 1;
        pp_del("pending_profile");
    } else if (strcmp(new_profile, prev_profile) == 0) {
        pp_del("pending_profile");
    } else if (strcmp(new_profile, pending_profile) == 0) {
        pending_count++;
        if (pending_count >= 2) {
            should_apply = 1;
            pp_del("pending_profile");
        } else {
            char b[96];
            snprintf(b, sizeof b, "%s:%ld", new_profile, pending_count);
            pp_set("pending_profile", b);
        }
    } else {
        char b[96];
        snprintf(b, sizeof b, "%s:1", new_profile);
        pp_set("pending_profile", b);
    }

    if (!should_apply) return 0;

    write_logf("Engine Triggered by Event: %s", "SUCCESS", event);
    write_logf("Metrics - IFACE: %s | RTT: %ldms | LOSS: %ld%% | JITTER: %ldms | RETRANS: %s%% | SCORE: %ld | RSSI: %ld | BAND: %s",
               "SUCCESS", ifc, rtt, loss, jitter, retrans, score, rssi, band);
    write_logf("Switching Network Profile to: %s (was: %s)", "SUCCESS",
               new_profile, prev_profile[0] ? prev_profile : "NONE");

    {
        char b[128];
        snprintf(b, sizeof b, "%s:%s", new_profile, ifc);
        pp_set("engine_state", b);
    }

    long txq;
    if (strcmp(new_profile, "WEAK_NETWORK") == 0) {
        setenv("PP_FQ_LIMIT", "256", 1);
        setenv("PP_FQ_FLOWS", "32", 1);
        txq = 1000;
    } else {
        setenv("PP_FQ_LIMIT", "1024", 1);
        setenv("PP_FQ_FLOWS", "1024", 1);
        txq = 3000;
    }
    if (strcmp(band, "24GHZ") == 0) txq += 1000;
    else if (strcmp(band, "6GHZ") == 0 && txq > 1500) txq = 1500;

    long mtu;
    char mk[192], mlast[32];
    snprintf(mk, sizeof mk, "mtu_last_%s", ifc);
    if (iface_changed) {
        mtu = probe_mtu(ifc);
    } else if (pp_get(mk, mlast, sizeof mlast) && mlast[0]) {
        mtu = strtol(mlast, NULL, 10);
    } else {
        mtu = probe_mtu(ifc);
    }
    {
        char b[32];
        snprintf(b, sizeof b, "%ld", mtu);
        pp_set(mk, b);
    }

    set_mtu(ifc, (int)mtu);
    write_logf("Set MTU for %s to %ld", "SUCCESS", ifc, mtu);

    set_txqueuelen(ifc, (int)txq);
    write_logf("Set TxQueueLen for %s to %ld (band: %s)", "SUCCESS", ifc, txq, band);

    call_mode("--hw-tweak", mode_hw_tweak);

    {
        char tcv[8];
        if (pp_get("tc", tcv, sizeof tcv) && strcmp(tcv, "1") == 0)
            call_mode("--init-tc", mode_init_tc);
    }

    /* Jaga Wi-Fi power save tetap mati melewati reconnect/roaming
     * (driver hampir selalu menyalakannya kembali saat link naik). */
    {
        char wps[8];
        if (pp_get("wifips", wps, sizeof wps) && strcmp(wps, "1") == 0 &&
            strncmp(ifc, "wlan", 4) == 0)
            call_mode("--wifi-ps-off", mode_wifi_ps_off);
    }
    return 0;
}

static int game_list_has(const char *pkg) {
    FILE *f = fopen(g_gamelist, "r");
    if (!f) return 0;
    char line[512];
    size_t pl = strlen(pkg);
    while (fgets(line, sizeof line, f)) {
        trim_nl(line);
        if (strncmp(line, pkg, pl) == 0 && line[pl] == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int mode_auto_detect(void) {
    char cur[32];
    if (!pp_get("auto_act", cur, sizeof cur) || !cur[0])
        snprintf(cur, sizeof cur, "default");

    char ifc[64];
    route_iface(ifc, sizeof ifc);
    if (!ifc[0]) snprintf(ifc, sizeof ifc, "wlan0");

    long now = (long)time(NULL);
    char rxp[PATH_MAX];
    snprintf(rxp, sizeof rxp, "/sys/class/net/%s/statistics/rx_bytes", ifc);
    char rxs[64];
    long rx_now = 0;
    if (read_first_line(rxp, rxs, sizeof rxs) == 0 && rxs[0])
        rx_now = strtol(rxs, NULL, 10);

    long speed = 0;
    char c[160];
    if (pp_get("auto_rx", c, sizeof c) && c[0]) {
        char c_rx[64] = "", c_t[64] = "", c_if[64] = "";
        char *save = NULL;
        char *t = strtok_r(c, "|", &save);
        if (t) {
            snprintf(c_rx, sizeof c_rx, "%s", t);
            t = strtok_r(NULL, "|", &save);
            if (t) {
                snprintf(c_t, sizeof c_t, "%s", t);
                t = strtok_r(NULL, "|", &save);
                if (t) snprintf(c_if, sizeof c_if, "%s", t);
            }
        }
        if (strcmp(c_if, ifc) == 0 && c_rx[0] && c_t[0]) {
            long dt = now - strtol(c_t, NULL, 10);
            if (dt >= 2 && dt <= 20) {
                long diff = rx_now - strtol(c_rx, NULL, 10);
                if (diff < 0) diff = 0;
                speed = diff / dt;
            }
        }
    }
    {
        char b[160];
        snprintf(b, sizeof b, "%ld|%ld|%s", rx_now, now, ifc);
        pp_set("auto_rx", b);
    }

    /* Sampling rx sudah jendela 2-20 s: trafik deras adalah bukti
     * pemakaian nyata, tidak perlu konfirmasi tambahan. */
    if (speed > 512000) {
        if (strcmp(cur, "download") != 0) {
            pp_set("auto_act", "download");
            write_logf("Auto-Detect: Activity changed to download (Speed: %ldKB/s)",
                       "SUCCESS", speed / 1024);
            call_mode("--download", mode_download);
            {
                const char *a[] = {"bbr", "cubic", "reno"};
                set_best_tcp(a, 3);
            }
            call_mode("--nic-on", mode_nic_on);
            call_mode("--unirq-affinity", mode_unirq_affinity);
            call_mode("--unboost-ksoft", mode_unboost_ksoft);
        }
        return 0;
    }

    char pkg[512];
    current_fg_pkg(pkg, sizeof pkg);
    if (!pkg[0]) {
        /* Layar utama / tanpa app — tweak terakhir dibiarkan; app baru
         * akan terdeteksi sebagai transisi pada tick berikutnya. */
        pp_set("auto_pkg", "");
        return 0;
    }

    char new_act[32];
    if (game_list_has(pkg)) {
        snprintf(new_act, sizeof new_act, "game");
    } else if (!classify_pkg(pkg, new_act, sizeof new_act)) {
        snprintf(new_act, sizeof new_act, "default");
    }

    /* App sama dengan tick sebelumnya — tidak ada pekerjaan ulang. */
    char last_pkg[512];
    pp_get("auto_pkg", last_pkg, sizeof last_pkg);
    if (strcmp(pkg, last_pkg) == 0) return 0;

    /* Konfirmasi pemakaian nyata: app harus masih di foreground 5 detik
     * kemudian — buka-lalu-tutup tidak pernah sampai ke apply. */
    sleep(5);
    char pkg2[512];
    current_fg_pkg(pkg2, sizeof pkg2);
    if (strcmp(pkg2, pkg) != 0) return 0;
    pp_set("auto_pkg", pkg);

    /* Kategori sama (mis. WA -> TikTok, dua-duanya social): tweak yang
     * berjalan sudah pas — cukup satu baris log, tanpa re-apply. */
    if (strcmp(new_act, cur) == 0) {
        write_logf("Auto-Detect: App %s still in category '%s'", "SKIP", pkg, new_act);
        return 0;
    }

    pp_set("auto_act", new_act);
    write_logf("Auto-Detect: Activity changed to %s (App: %s, Speed: %ldKB/s)",
               "SUCCESS", new_act, pkg, speed / 1024);

    call_preset(new_act);

    if (strcmp(new_act, "game") == 0) {
        {
            const char *a[] = {"westwood", "bbr", "cubic"};
            set_best_tcp(a, 3);
        }
        call_mode("--nic-offload", mode_nic_offload);
        call_mode("--irq-affinity", mode_irq_affinity);
        call_mode("--boost-ksoft", mode_boost_ksoft);
        {
            const char *pats[] = {"rild", "netd"};
            cmdlines_renice(pats, 2, -10);
        }
        call_mode("--init-tc", mode_init_tc);
    } else if (strcmp(new_act, "download") == 0 || strcmp(new_act, "streaming") == 0) {
        {
            const char *a[] = {"bbr", "cubic", "reno"};
            set_best_tcp(a, 3);
        }
        call_mode("--nic-on", mode_nic_on);
        call_mode("--unirq-affinity", mode_unirq_affinity);
        call_mode("--unboost-ksoft", mode_unboost_ksoft);
    } else {
        {
            const char *a[] = {"cubic", "bbr", "reno"};
            set_best_tcp(a, 3);
        }
        call_mode("--nic-on", mode_nic_on);
        call_mode("--unirq-affinity", mode_unirq_affinity);
        call_mode("--unboost-ksoft", mode_unboost_ksoft);
    }
    return 0;
}

static int mode_auto_on(void) {
    write_log("Smart Auto Mode Enabled", "SUCCESS");
    /* State auto lama tidak berlaku — deteksi mulai bersih. */
    pp_del("auto_act");
    pp_del("auto_pkg");
    spawn_bg("--dns-flip", "AUTO_ON");
    return 0;
}

static void str_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static int mode_auto_off(void) {
    write_log("Smart Auto Mode Disabled. Restoring manual configuration...", "SUCCESS");
    /* State auto tidak berlaku saat mode manual (mencegah deteksi
     * pertama setelah re-enable meloncat karena state basi). */
    pp_del("auto_act");
    pp_del("auto_pkg");

    char preset[64];
    pp_get("preset", preset, sizeof preset);
    str_lower(preset);
    char *sp = strchr(preset, ' ');
    if (sp) *sp = 0;
    if (preset[0]) call_preset(preset);
    else call_mode("--default", mode_default);

    char v[8];
    if (pp_get("nic", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--nic-offload", mode_nic_offload);
        else call_mode("--nic-on", mode_nic_on);
    }
    if (pp_get("irq", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--irq-affinity", mode_irq_affinity);
        else call_mode("--unirq-affinity", mode_unirq_affinity);
    }
    if (pp_get("ksoft", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--boost-ksoft", mode_boost_ksoft);
        else call_mode("--unboost-ksoft", mode_unboost_ksoft);
    }
    char algo[64];
    if (pp_get("tcp_algo", algo, sizeof algo) && algo[0])
        write_sysfile("/proc/sys/net/ipv4/tcp_congestion_control", algo);
    if (pp_get("wifips", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--wifi-ps-off", mode_wifi_ps_off);
        else call_mode("--wifi-ps-on", mode_wifi_ps_on);
    }
    if (pp_get("conntrack", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--conntrack-on", mode_conntrack_on);
        else call_mode("--conntrack-off", mode_conntrack_off);
    }

    char mdns[256];
    if (!pp_get("dns_manual", mdns, sizeof mdns) || !mdns[0])
        snprintf(mdns, sizeof mdns, "default");
    if (strcmp(mdns, "default") == 0 || !mdns[0]) {
        sh("settings delete global private_dns_mode 2>/dev/null");
        sh("settings delete global private_dns_specifier 2>/dev/null");
        write_log("Auto DNS: Restored to System Default", "SUCCESS");
    } else {
        sh("settings put global private_dns_mode hostname 2>/dev/null");
        char q[600], cmd[900];
        shq(q, sizeof q, mdns);
        snprintf(cmd, sizeof cmd, "settings put global private_dns_specifier %s 2>/dev/null", q);
        sh(cmd);
        write_logf("Auto DNS: Restored manual DNS -> %s", "SUCCESS", mdns);
    }
    return 0;
}

static volatile sig_atomic_t g_stop = 0;

static void on_stop(int sig) {
    (void)sig;
    g_stop = 1;
}

static void rm_pidfile(void) {
    unlink(g_pidfile);
}

static int display_on(void) {
    FILE *f = popen("dumpsys power 2>/dev/null", "r");
    if (!f) return 0;
    char line[4096];
    int on = 0;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, "mHoldingDisplaySuspendBlocker=true")) {
            on = 1;
            break;
        }
    }
    pclose(f);
    return on;
}

static int mode_daemon(void) {
    /* Single-instance via pidfile — pid mati dianggap crash, pidfile
     * ditimpa; boot membersihkan pidfile lama sebelum meluncurkan. */
    if (access(g_pidfile, F_OK) == 0) {
        char oldpid[32] = "";
        FILE *pf = fopen(g_pidfile, "r");
        if (pf) {
            if (fgets(oldpid, sizeof oldpid, pf)) trim_nl(oldpid);
            else oldpid[0] = 0;
            fclose(pf);
        }
        if (oldpid[0]) {
            char proc[96];
            snprintf(proc, sizeof proc, "/proc/%s", oldpid);
            struct stat st;
            if (stat(proc, &st) == 0) {
                write_logf("Daemon: already running (pid %s) — exit", "SKIP", oldpid);
                return 0;
            }
        }
    }
    FILE *pf = fopen(g_pidfile, "w");
    if (pf) {
        fprintf(pf, "%d\n", (int)getpid());
        fclose(pf);
    }
    atexit(rm_pidfile);
    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);
    signal(SIGCHLD, SIG_IGN);

    write_log("Event Dispatcher Daemon Started", "SUCCESS");

    char last_if[1024] = "", last_route[512] = "", last_vpn[256] = "";
    iface_snapshot(last_if, sizeof last_if);
    route_snapshot(last_route, sizeof last_route);
    vpn_snapshot(last_vpn, sizeof last_vpn);
    long tick = 0;

    /* Satu-satunya loop: event jaringan dihitung dari diff snapshot tiap
     * 10 s. TERM/INT memutus sleep() langsung, jadi keluar seketika. */
    while (!g_stop) {
        sleep(10);
        if (g_stop) break;

        char cur_if[1024];
        iface_snapshot(cur_if, sizeof cur_if);
        if (strcmp(cur_if, last_if) != 0) {
            fire_iface_events(last_if, cur_if);
            snprintf(last_if, sizeof last_if, "%s", cur_if);
        }

        char cur_route[512];
        route_snapshot(cur_route, sizeof cur_route);
        if (strcmp(cur_route, last_route) != 0 && (cur_route[0] || last_route[0])) {
            if (debounce_ok("route", 4)) {
                spawn_bg("--engine", "ROUTE_CHANGED");
                spawn_bg("--dns-flip", "ROUTE_CHANGED");
            }
        }
        snprintf(last_route, sizeof last_route, "%s", cur_route);

        char cur_vpn[256];
        vpn_snapshot(cur_vpn, sizeof cur_vpn);
        if (strcmp(cur_vpn, last_vpn) != 0 && cur_vpn[0]) {
            if (debounce_ok("vpn", 3)) {
                spawn_bg("--engine", "VPN_CONNECTED");
                spawn_bg("--dns-flip", "VPN_CONNECTED");
            }
        }
        snprintf(last_vpn, sizeof last_vpn, "%s", cur_vpn);

        if (display_on()) {
            tick++;
            if (tick % 3 == 1)
                spawn_bg("--engine", "PERIODIC_HEALTH_CHECK");
            char am[8];
            if (pp_get("auto_mode", am, sizeof am) && strcmp(am, "1") == 0) {
                spawn_bg("--auto-detect", NULL);
                spawn_bg("--dns-flip", "PERIODIC");
            }
        }
    }
    return 0; /* exit → atexit rm pidfile */
}

static int mode_boot(void) {
    FILE *lf = fopen(g_logfile, "w");
    if (lf) fclose(lf);
    write_log("PingPimp Boot Initialization Started", "SUCCESS");
    rmdir(g_lockdir);
    unlink(g_pidfile);
    /* Reset state runtime */
    pp_del("tcp_prev");
    pp_del("pending_profile");
    pp_del("debounce");
    pp_del("mtu_last");
    pp_del("auto_act");
    pp_del("auto_pkg");
    pp_del("auto_act_pending");

    pid_t pid = fork();
    if (pid == 0) {
        detect_games();
        _exit(0);
    }
    module_description();
    clear_wlan_promisc_allmulti("wlan0");
    write_sysfile("/proc/sys/net/ipv6/conf/wlan0/accept_ra", "0");

    char preset[64];
    pp_get("preset", preset, sizeof preset);
    str_lower(preset);
    if (preset[0]) {
        write_logf("Applying Preset: %s", "SUCCESS", preset);
        if (strcmp(preset, "game") == 0) call_mode("--game", mode_game);
        else if (strcmp(preset, "download") == 0) call_mode("--download", mode_download);
        else if (strcmp(preset, "streaming") == 0) call_mode("--streaming", mode_streaming);
        else if (strcmp(preset, "social media") == 0 || strcmp(preset, "social") == 0)
            call_mode("--social", mode_social);
        else if (strcmp(preset, "browsing") == 0) call_mode("--browsing", mode_browsing);
        else if (strcmp(preset, "outdoor") == 0) call_mode("--outdoor", mode_outdoor);
        else call_mode("--default", mode_default);
    } else {
        write_log("Applying Default Preset", "SUCCESS");
        call_mode("--default", mode_default);
    }

    char v[8];
    if (pp_get("ipv6_state", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--disable", mode_disable);
        else call_mode("--enable", mode_enable);
    }
    if (pp_get("net_state", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--state", mode_state);
        else call_mode("--unstate", mode_unstate);
    }
    if (pp_get("saver", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--saver", mode_saver);
        else call_mode("--unsaver", mode_unsaver);
    }
    if (pp_get("nic", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--nic-offload", mode_nic_offload);
        else call_mode("--nic-on", mode_nic_on);
    }
    if (pp_get("irq", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--irq-affinity", mode_irq_affinity);
        else call_mode("--unirq-affinity", mode_unirq_affinity);
    }
    if (pp_get("ksoft", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--boost-ksoft", mode_boost_ksoft);
        else call_mode("--unboost-ksoft", mode_unboost_ksoft);
    }
    if (pp_get("wifips", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--wifi-ps-off", mode_wifi_ps_off);
        else call_mode("--wifi-ps-on", mode_wifi_ps_on);
    }
    if (pp_get("conntrack", v, sizeof v) && v[0]) {
        if (strcmp(v, "1") == 0) call_mode("--conntrack-on", mode_conntrack_on);
        else call_mode("--conntrack-off", mode_conntrack_off);
    }
    char algo[64];
    if (pp_get("tcp_algo", algo, sizeof algo) && algo[0]) {
        if (write_sysfile("/proc/sys/net/ipv4/tcp_congestion_control", algo) == 0)
            write_logf("Set TCP Congestion Control to %s", "SUCCESS", algo);
        else
            write_logf("Set TCP Congestion Control to %s", "FAILED", algo);
    }

    char ipath[PATH_MAX + 64];
    snprintf(ipath, sizeof ipath, "%s/isolate_apps.txt", g_moddir);
    if (access(ipath, F_OK) == 0) {
        FILE *fp = fopen(ipath, "r");
        if (fp) {
            char buf[8192];
            size_t r = fread(buf, 1, sizeof buf - 1, fp);
            buf[r] = 0;
            fclose(fp);
            char *save = NULL;
            for (char *tok = strtok_r(buf, ", \t\r\n", &save); tok;
                 tok = strtok_r(NULL, ", \t\r\n", &save)) {
                long uid = pm_uid(tok);
                if (uid < 0) continue;
                shf("iptables -I OUTPUT -m owner --uid-owner %ld -j REJECT 2>/dev/null", uid);
                shf("ip6tables -I OUTPUT -m owner --uid-owner %ld -j REJECT 2>/dev/null", uid);
                write_logf("Isolated app UID: %ld", "SUCCESS", uid);
            }
        }
    }

    char conf[PATH_MAX] = "";
    if (access("/data/misc/wifi/wpa_supplicant.conf", F_OK) == 0) {
        snprintf(conf, sizeof conf, "%s", "/data/misc/wifi/wpa_supplicant.conf");
    } else {
        glob_t g;
        if (glob("/data/misc/wifi/*/wpa_supplicant.conf", 0, NULL, &g) == 0) {
            if (g.gl_pathc > 0)
                snprintf(conf, sizeof conf, "%s", g.gl_pathv[0]);
            globfree(&g);
        }
    }
    if (conf[0]) {
        int has_p2p = 0, has_scan = 0;
        FILE *wf = fopen(conf, "r");
        if (wf) {
            char wline[1024];
            while (fgets(wline, sizeof wline, wf)) {
                if (strstr(wline, "p2p_disabled=1")) has_p2p = 1;
                if (strstr(wline, "ap_scan=1")) has_scan = 1;
            }
            fclose(wf);
        }
        FILE *wa = fopen(conf, "a");
        if (wa) {
            if (!has_p2p) fprintf(wa, "p2p_disabled=1\n");
            if (!has_scan) fprintf(wa, "ap_scan=1\n");
            fclose(wa);
        }
        write_logf("Patched wpa_supplicant.conf (%s)", "SUCCESS", conf);
    } else {
        write_log("wpa_supplicant.conf not present on this Android version (10+), skipping", "SKIP");
    }

    sh("stop tcpdump 2>/dev/null");
    sh("stop vendor.tcpdump 2>/dev/null");
    sh("stop cnss_diag 2>/dev/null");
    sh("stop vendor.cnss_diag 2>/dev/null");

    struct stat wst;
    if (stat("/data/vendor/wlan_logs", &wst) == 0 && S_ISDIR(wst.st_mode)) {
        DIR *wd = opendir("/data/vendor/wlan_logs");
        if (wd) {
            struct dirent *we;
            char wpath[PATH_MAX];
            while ((we = readdir(wd))) {
                if (!strcmp(we->d_name, ".") || !strcmp(we->d_name, "..")) continue;
                snprintf(wpath, sizeof wpath, "/data/vendor/wlan_logs/%s", we->d_name);
                if (unlink(wpath) != 0) rmdir(wpath);
            }
            closedir(wd);
        }
        chmod("/data/vendor/wlan_logs", 0);
        write_log("Stopped WLAN diag services & cleared logs", "SUCCESS");
    }

    pp_set("engine_state", "NONE");

    {
        char am[8];
        if (pp_get("auto_mode", am, sizeof am) && strcmp(am, "1") == 0)
            spawn_bg("--dns-flip", "BOOT");
    }

    write_log("PingPimp Boot Initialization Completed. Launching Daemon...", "SUCCESS");
    /* exec: boot berubah menjadi daemon itu sendiri — tidak ada proses
     * launcher yang bertahan di ps. */
    return mode_daemon();
}

int main(int argc, char **argv) {
    init_paths();
    const char *m = (argc > 1) ? argv[1] : NULL;
    const char *ev = (argc > 2) ? argv[2] : NULL;
    g_mode = (m && m[0]) ? m : "system";

    if (!m || !m[0]) return 0;
    if (strcmp(m, "--default") == 0) return mode_default();
    if (strcmp(m, "--game") == 0) return mode_game();
    if (strcmp(m, "--download") == 0) return mode_download();
    if (strcmp(m, "--streaming") == 0) return mode_streaming();
    if (strcmp(m, "--social") == 0) return mode_social();
    if (strcmp(m, "--browsing") == 0) return mode_browsing();
    if (strcmp(m, "--outdoor") == 0) return mode_outdoor();
    if (strcmp(m, "--state") == 0) return mode_state();
    if (strcmp(m, "--unstate") == 0) return mode_unstate();
    if (strcmp(m, "--saver") == 0) return mode_saver();
    if (strcmp(m, "--unsaver") == 0) return mode_unsaver();
    if (strcmp(m, "--disable") == 0) return mode_disable();
    if (strcmp(m, "--enable") == 0) return mode_enable();
    if (strcmp(m, "--nic-offload") == 0) return mode_nic_offload();
    if (strcmp(m, "--nic-on") == 0) return mode_nic_on();
    if (strcmp(m, "--wifi-ps-off") == 0) return mode_wifi_ps_off();
    if (strcmp(m, "--wifi-ps-on") == 0) return mode_wifi_ps_on();
    if (strcmp(m, "--conntrack-on") == 0) return mode_conntrack_on();
    if (strcmp(m, "--conntrack-off") == 0) return mode_conntrack_off();
    if (strcmp(m, "--irq-affinity") == 0) return mode_irq_affinity();
    if (strcmp(m, "--unirq-affinity") == 0) return mode_unirq_affinity();
    if (strcmp(m, "--boost-ksoft") == 0) return mode_boost_ksoft();
    if (strcmp(m, "--unboost-ksoft") == 0) return mode_unboost_ksoft();
    if (strcmp(m, "--hw-tweak") == 0) return mode_hw_tweak();
    if (strcmp(m, "--init-tc") == 0) return mode_init_tc();
    if (strcmp(m, "--engine") == 0) return mode_engine(ev ? ev : "");
    if (strcmp(m, "--dns-flip") == 0) {
        run_dns_flip(ev ? ev : "MANUAL");
        return 0;
    }
    if (strcmp(m, "--auto-detect") == 0) return mode_auto_detect();
    if (strcmp(m, "--auto-on") == 0) return mode_auto_on();
    if (strcmp(m, "--auto-off") == 0) return mode_auto_off();
    if (strcmp(m, "--daemon") == 0) return mode_daemon();
    if (strcmp(m, "--boot") == 0) return mode_boot();
    return 0;
}
