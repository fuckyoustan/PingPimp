#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define MODDIR        "/data/adb/modules/PingPimp"
#define LOG_FILE      MODDIR "/log.txt"
#define STATE_FILE    MODDIR "/NetworkState"
#define PENDING_FILE  MODDIR "/.pending_profile"
#define TCP_CACHE     MODDIR "/.tcp_stats_prev"
#define LOCKDIR       MODDIR "/.engine.lock"
#define MTU_CACHE_DIR MODDIR "/mtu_cache"
#define GAME_LIST     MODDIR "/game_list.txt"
#define MODULE_PROP   MODDIR "/module.prop"

static char g_label[64] = "system";
static int  g_fq_limit = 1000;   /* pengganti env PP_FQ_LIMIT  */
static int  g_fq_flows = 1024;   /* pengganti env PP_FQ_FLOWS  */

static int run_mode(const char *mode, const char *arg);

/* ===================== UTILITAS DASAR ===================== */

static void write_log(const char *tweak, const char *status) {
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    fprintf(f, "[%s] [%s] Tweak: %s | Status: %s\n", ts, g_label, tweak, status);
    fclose(f);
}

static int write_proc(const char *path, const char *val) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int ok = (write(fd, val, strlen(val)) >= 0) ? 0 : -1;
    close(fd);
    return ok;
}

/* "net.ipv4.tcp_rmem" -> /proc/sys/net/ipv4/tcp_rmem */
static int write_sysctl(const char *key, const char *val) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/sys/%s", key);
    for (char *p = path + 11; *p; p++)
        if (*p == '.') *p = '/';
    return write_proc(path, val);
}

static int write_file(const char *path, const char *val) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s\n", val);
    fclose(f);
    return 0;
}

static int run_cmd(const char *fmt, ...) {
    char cmd[2048], full[2120];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    snprintf(full, sizeof(full), "%s >/dev/null 2>&1", cmd);
    int rc = system(full);
    return (rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0) ? 0 : -1;
}

static int cmd_out(char *buf, size_t len, const char *fmt, ...) {
    char cmd[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    buf[0] = 0;
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    size_t n = fread(buf, 1, len - 1, p);
    buf[n] = 0;
    pclose(p);
    return 0;
}

static int read_first_line(const char *path, char *buf, size_t len) {
    buf[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fgets(buf, len, f)) {
        char *nl = strpbrk(buf, "\r\n");
        if (nl) *nl = 0;
    } else buf[0] = 0;
    fclose(f);
    return 0;
}

/* Baca file config kecil; strip \r \n spasi (setara: tr -d '\r\n ') */
static int read_file_trim(const char *path, char *buf, size_t len) {
    buf[0] = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char raw[4096];
    ssize_t n = read(fd, raw, sizeof(raw) - 1);
    close(fd);
    if (n <= 0) return 0;               /* file ada tapi kosong */
    raw[n] = 0;
    char *w = buf;
    for (ssize_t i = 0; i < n && (size_t)(w - buf) < len - 1; i++)
        if (raw[i] != '\r' && raw[i] != '\n' && raw[i] != ' ') *w++ = raw[i];
    *w = 0;
    return 0;
}

/* Baca list (isolate/boost): strip \r\n, siap split ',' */
static int read_list(const char *path, char *buf, size_t len) {
    buf[0] = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, len - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = 0;
    char *w = buf;
    for (ssize_t i = 0; i < n; i++)
        if (buf[i] != '\r' && buf[i] != '\n') *w++ = buf[i];
    *w = 0;
    return 0;
}

/* cfg_is("tc.txt","1") — setara [ "$(tr -d '\r\n ' < f)" = "1" ] */
static int cfg_is(const char *name, const char *val) {
    char path[256], v[256];
    snprintf(path, sizeof(path), MODDIR "/%s", name);
    if (read_file_trim(path, v, sizeof(v)) != 0) return 0;
    return !strcmp(v, val);
}

static int auto_mode_on(void) { return cfg_is("auto_mode.txt", "1"); }

static void log_summary(const char *name, int ok, int total) {
    char msg[256];
    if (total > 0 && ok == total) {
        write_log(name, "SUCCESS");
    } else if (ok > 0) {
        snprintf(msg, sizeof(msg), "%s (%d/%d applied)", name, ok, total);
        write_log(msg, "PARTIAL");            /* shell lama: status kosong; kini eksplisit */
    } else {
        write_log(name, "FAILED");
    }
}

static void log_skip(const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    write_log(msg, "SKIP");
}

/* ===================== LOCK & DEBOUNCE ===================== */

static int acquire_lock(void) {
    for (int i = 0; i < 10; i++) {
        if (mkdir(LOCKDIR, 0700) == 0) return 0;
        sleep(1);
    }
    return -1;
}

static void release_lock(void) { rmdir(LOCKDIR); }

static int debounce_ok(const char *key, int min_gap) {
    char path[256], buf[32], v[32];
    snprintf(path, sizeof(path), MODDIR "/.debounce_%s", key);
    long now = (long)time(NULL);
    if (read_first_line(path, buf, sizeof(buf)) == 0 && buf[0]) {
        long last = atol(buf);
        if (now - last < min_gap) return 0;
    }
    snprintf(v, sizeof(v), "%ld", now);
    write_file(path, v);
    return 1;
}

/* ===================== DATA PRESET ===================== */

static const char PRESET_DEFAULT[] =
    "net.core.netdev_max_backlog 4096\n"
    "net.core.somaxconn 4096\n"
    "net.core.netdev_budget 4096\n"
    "net.core.netdev_budget_usecs 8000\n"
    "net.core.optmem_max 204800\n"
    "net.core.rmem_default 262144\n"
    "net.core.rmem_max 1048576\n"
    "net.core.wmem_default 262144\n"
    "net.core.wmem_max 1048576\n"
    "net.ipv4.tcp_slow_start_after_idle 0\n"
    "net.ipv4.tcp_sack 1\n"
    "net.ipv4.tcp_window_scaling 1\n"
    "net.ipv4.tcp_moderate_rcvbuf 1\n"
    "net.ipv4.tcp_timestamps 1\n"
    "net.ipv4.tcp_no_metrics_save 0\n"
    "net.ipv4.tcp_mtu_probing 1\n"
    "net.ipv4.tcp_rfc1337 1\n"
    "net.ipv4.tcp_fastopen 3\n"
    "net.ipv4.tcp_frto 2\n"
    "net.ipv4.tcp_recovery 1\n"
    "net.ipv4.tcp_early_retrans 1\n"
    "net.ipv4.tcp_reordering 3\n"
    "net.ipv4.tcp_ecn 0\n"
    "net.ipv4.tcp_tw_reuse 1\n"
    "net.ipv4.tcp_syn_retries 4\n"
    "net.ipv4.tcp_synack_retries 4\n"
    "net.ipv4.tcp_rmem 4096 87380 1048576\n"
    "net.ipv4.tcp_wmem 4096 65536 524288\n"
    "net.ipv4.tcp_mem 16384 32768 49152\n"
    "net.ipv4.udp_rmem_min 16384\n"
    "net.ipv4.udp_wmem_min 16384\n"
    "net.ipv4.tcp_notsent_lowat 16384\n"
    "net.ipv4.tcp_abort_on_overflow 0\n"
    "net.ipv4.tcp_max_reordering 300\n"
    "net.ipv4.tcp_adv_win_scale 1\n"
    "net.ipv4.tcp_max_syn_backlog 2048\n"
    "net.ipv4.tcp_max_tw_buckets 8192\n"
    "net.ipv4.ip_local_port_range 1024 65535\n"
    "net.ipv4.tcp_fin_timeout 30\n"
    "net.ipv4.tcp_keepalive_time 600\n"
    "net.ipv4.tcp_keepalive_intvl 30\n"
    "net.ipv4.tcp_keepalive_probes 5\n"
    "net.ipv4.route.flush 1\n";

static const char PRESET_GAME[] =
    "net.core.netdev_max_backlog 1000\n"
    "net.core.somaxconn 1024\n"
    "net.core.netdev_budget 6000\n"
    "net.core.netdev_budget_usecs 6000\n"
    "net.core.optmem_max 102400\n"
    "net.core.rmem_default 131072\n"
    "net.core.rmem_max 524288\n"
    "net.core.wmem_default 131072\n"
    "net.core.wmem_max 524288\n"
    "net.ipv4.tcp_slow_start_after_idle 0\n"
    "net.ipv4.tcp_sack 1\n"
    "net.ipv4.tcp_window_scaling 1\n"
    "net.ipv4.tcp_moderate_rcvbuf 0\n"
    "net.ipv4.tcp_timestamps 1\n"
    "net.ipv4.tcp_no_metrics_save 0\n"
    "net.ipv4.tcp_mtu_probing 1\n"
    "net.ipv4.tcp_rfc1337 1\n"
    "net.ipv4.tcp_fastopen 3\n"
    "net.ipv4.tcp_frto 2\n"
    "net.ipv4.tcp_recovery 1\n"
    "net.ipv4.tcp_early_retrans 1\n"
    "net.ipv4.tcp_reordering 5\n"
    "net.ipv4.tcp_ecn 0\n"
    "net.ipv4.tcp_tw_reuse 1\n"
    "net.ipv4.tcp_syn_retries 2\n"
    "net.ipv4.tcp_synack_retries 2\n"
    "net.ipv4.tcp_rmem 4096 32768 131072\n"
    "net.ipv4.tcp_wmem 4096 32768 131072\n"
    "net.ipv4.tcp_mem 8192 16384 24576\n"
    "net.ipv4.udp_rmem_min 32768\n"
    "net.ipv4.udp_wmem_min 32768\n"
    "net.ipv4.tcp_notsent_lowat 2048\n"
    "net.ipv4.tcp_abort_on_overflow 1\n"
    "net.ipv4.tcp_max_reordering 100\n"
    "net.ipv4.tcp_adv_win_scale 0\n"
    "net.ipv4.tcp_max_syn_backlog 1024\n"
    "net.ipv4.tcp_max_tw_buckets 4096\n"
    "net.ipv4.ip_local_port_range 1024 65535\n"
    "net.ipv4.tcp_fin_timeout 10\n"
    "net.ipv4.tcp_keepalive_time 300\n"
    "net.ipv4.tcp_keepalive_intvl 10\n"
    "net.ipv4.tcp_keepalive_probes 3\n"
    "net.ipv4.route.flush 1\n";

static const char PRESET_DOWNLOAD[] =
    "net.core.netdev_max_backlog 16384\n"
    "net.core.somaxconn 8192\n"
    "net.core.netdev_budget 60000\n"
    "net.core.netdev_budget_usecs 8000\n"
    "net.core.optmem_max 409600\n"
    "net.core.rmem_default 524288\n"
    "net.core.rmem_max 16777216\n"
    "net.core.wmem_default 524288\n"
    "net.core.wmem_max 16777216\n"
    "net.ipv4.tcp_slow_start_after_idle 0\n"
    "net.ipv4.tcp_sack 1\n"
    "net.ipv4.tcp_window_scaling 1\n"
    "net.ipv4.tcp_moderate_rcvbuf 1\n"
    "net.ipv4.tcp_timestamps 1\n"
    "net.ipv4.tcp_no_metrics_save 0\n"
    "net.ipv4.tcp_mtu_probing 1\n"
    "net.ipv4.tcp_rfc1337 1\n"
    "net.ipv4.tcp_fastopen 3\n"
    "net.ipv4.tcp_frto 2\n"
    "net.ipv4.tcp_recovery 1\n"
    "net.ipv4.tcp_early_retrans 1\n"
    "net.ipv4.tcp_reordering 3\n"
    "net.ipv4.tcp_ecn 1\n"
    "net.ipv4.tcp_tw_reuse 1\n"
    "net.ipv4.tcp_syn_retries 5\n"
    "net.ipv4.tcp_synack_retries 5\n"
    "net.ipv4.tcp_rmem 4096 87380 16777216\n"
    "net.ipv4.tcp_wmem 4096 65536 16777216\n"
    "net.ipv4.tcp_mem 131072 262144 393216\n"
    "net.ipv4.udp_rmem_min 16384\n"
    "net.ipv4.udp_wmem_min 16384\n"
    "net.ipv4.tcp_notsent_lowat 4294967295\n"
    "net.ipv4.tcp_abort_on_overflow 0\n"
    "net.ipv4.tcp_max_reordering 300\n"
    "net.ipv4.tcp_adv_win_scale 2\n"
    "net.ipv4.tcp_max_syn_backlog 4096\n"
    "net.ipv4.tcp_max_tw_buckets 16384\n"
    "net.ipv4.ip_local_port_range 1024 65535\n"
    "net.ipv4.tcp_fin_timeout 30\n"
    "net.ipv4.tcp_keepalive_time 1800\n"
    "net.ipv4.tcp_keepalive_intvl 60\n"
    "net.ipv4.tcp_keepalive_probes 9\n"
    "net.ipv4.route.flush 1\n";

static const char PRESET_STREAMING[] =
    "net.core.netdev_max_backlog 8192\n"
    "net.core.somaxconn 4096\n"
    "net.core.netdev_budget 30000\n"
    "net.core.netdev_budget_usecs 8000\n"
    "net.core.optmem_max 204800\n"
    "net.core.rmem_default 262144\n"
    "net.core.rmem_max 8388608\n"
    "net.core.wmem_default 262144\n"
    "net.core.wmem_max 8388608\n"
    "net.ipv4.tcp_slow_start_after_idle 0\n"
    "net.ipv4.tcp_sack 1\n"
    "net.ipv4.tcp_window_scaling 1\n"
    "net.ipv4.tcp_moderate_rcvbuf 1\n"
    "net.ipv4.tcp_timestamps 1\n"
    "net.ipv4.tcp_no_metrics_save 0\n"
    "net.ipv4.tcp_mtu_probing 1\n"
    "net.ipv4.tcp_rfc1337 1\n"
    "net.ipv4.tcp_fastopen 3\n"
    "net.ipv4.tcp_frto 2\n"
    "net.ipv4.tcp_recovery 1\n"
    "net.ipv4.tcp_early_retrans 1\n"
    "net.ipv4.tcp_reordering 3\n"
    "net.ipv4.tcp_ecn 0\n"
    "net.ipv4.tcp_tw_reuse 1\n"
    "net.ipv4.tcp_syn_retries 3\n"
    "net.ipv4.tcp_synack_retries 3\n"
    "net.ipv4.tcp_rmem 4096 87380 8388608\n"
    "net.ipv4.tcp_wmem 4096 65536 4194304\n"
    "net.ipv4.tcp_mem 65536 131072 196608\n"
    "net.ipv4.udp_rmem_min 32768\n"
    "net.ipv4.udp_wmem_min 32768\n"
    "net.ipv4.tcp_notsent_lowat 131072\n"
    "net.ipv4.tcp_abort_on_overflow 0\n"
    "net.ipv4.tcp_max_reordering 200\n"
    "net.ipv4.tcp_adv_win_scale 1\n"
    "net.ipv4.tcp_max_syn_backlog 2048\n"
    "net.ipv4.tcp_max_tw_buckets 8192\n"
    "net.ipv4.ip_local_port_range 1024 65535\n"
    "net.ipv4.tcp_fin_timeout 20\n"
    "net.ipv4.tcp_keepalive_time 1200\n"
    "net.ipv4.tcp_keepalive_intvl 30\n"
    "net.ipv4.tcp_keepalive_probes 5\n"
    "net.ipv4.route.flush 1\n";

static const char PRESET_SOCIAL[] =
    "net.core.netdev_max_backlog 2048\n"
    "net.core.somaxconn 2048\n"
    "net.core.netdev_budget 4096\n"
    "net.core.netdev_budget_usecs 4000\n"
    "net.core.optmem_max 204800\n"
    "net.core.rmem_default 131072\n"
    "net.core.rmem_max 524288\n"
    "net.core.wmem_default 131072\n"
    "net.core.wmem_max 524288\n"
    "net.ipv4.tcp_slow_start_after_idle 0\n"
    "net.ipv4.tcp_sack 1\n"
    "net.ipv4.tcp_window_scaling 1\n"
    "net.ipv4.tcp_moderate_rcvbuf 1\n"
    "net.ipv4.tcp_timestamps 1\n"
    "net.ipv4.tcp_no_metrics_save 0\n"
    "net.ipv4.tcp_mtu_probing 1\n"
    "net.ipv4.tcp_rfc1337 1\n"
    "net.ipv4.tcp_fastopen 3\n"
    "net.ipv4.tcp_frto 2\n"
    "net.ipv4.tcp_recovery 1\n"
    "net.ipv4.tcp_early_retrans 1\n"
    "net.ipv4.tcp_reordering 3\n"
    "net.ipv4.tcp_ecn 0\n"
    "net.ipv4.tcp_tw_reuse 1\n"
    "net.ipv4.tcp_syn_retries 3\n"
    "net.ipv4.tcp_synack_retries 3\n"
    "net.ipv4.tcp_rmem 4096 65536 524288\n"
    "net.ipv4.tcp_wmem 4096 65536 262144\n"
    "net.ipv4.tcp_mem 16384 32768 49152\n"
    "net.ipv4.udp_rmem_min 16384\n"
    "net.ipv4.udp_wmem_min 16384\n"
    "net.ipv4.tcp_notsent_lowat 32768\n"
    "net.ipv4.tcp_abort_on_overflow 0\n"
    "net.ipv4.tcp_max_reordering 300\n"
    "net.ipv4.tcp_adv_win_scale 1\n"
    "net.ipv4.tcp_max_syn_backlog 1024\n"
    "net.ipv4.tcp_max_tw_buckets 4096\n"
    "net.ipv4.ip_local_port_range 1024 65535\n"
    "net.ipv4.tcp_fin_timeout 15\n"
    "net.ipv4.tcp_keepalive_time 600\n"
    "net.ipv4.tcp_keepalive_intvl 15\n"
    "net.ipv4.tcp_keepalive_probes 4\n"
    "net.ipv4.route.flush 1\n";

static const char PRESET_OUTDOOR[] =
    "net.core.netdev_max_backlog 4096\n"
    "net.core.somaxconn 2048\n"
    "net.core.netdev_budget 8000\n"
    "net.core.netdev_budget_usecs 8000\n"
    "net.core.optmem_max 102400\n"
    "net.core.rmem_default 262144\n"
    "net.core.rmem_max 2097152\n"
    "net.core.wmem_default 262144\n"
    "net.core.wmem_max 2097152\n"
    "net.ipv4.tcp_slow_start_after_idle 1\n"
    "net.ipv4.tcp_sack 1\n"
    "net.ipv4.tcp_window_scaling 1\n"
    "net.ipv4.tcp_moderate_rcvbuf 1\n"
    "net.ipv4.tcp_timestamps 1\n"
    "net.ipv4.tcp_no_metrics_save 0\n"
    "net.ipv4.tcp_mtu_probing 1\n"
    "net.ipv4.tcp_rfc1337 1\n"
    "net.ipv4.tcp_fastopen 3\n"
    "net.ipv4.tcp_frto 2\n"
    "net.ipv4.tcp_recovery 1\n"
    "net.ipv4.tcp_early_retrans 1\n"
    "net.ipv4.tcp_reordering 10\n"
    "net.ipv4.tcp_ecn 0\n"
    "net.ipv4.tcp_tw_reuse 1\n"
    "net.ipv4.tcp_syn_retries 5\n"
    "net.ipv4.tcp_synack_retries 5\n"
    "net.ipv4.tcp_rmem 4096 87380 2097152\n"
    "net.ipv4.tcp_wmem 4096 65536 1048576\n"
    "net.ipv4.tcp_mem 32768 65536 98304\n"
    "net.ipv4.udp_rmem_min 16384\n"
    "net.ipv4.udp_wmem_min 16384\n"
    "net.ipv4.tcp_notsent_lowat 65536\n"
    "net.ipv4.tcp_abort_on_overflow 0\n"
    "net.ipv4.tcp_max_reordering 600\n"
    "net.ipv4.tcp_adv_win_scale 1\n"
    "net.ipv4.tcp_max_syn_backlog 2048\n"
    "net.ipv4.tcp_max_tw_buckets 8192\n"
    "net.ipv4.ip_local_port_range 1024 65535\n"
    "net.ipv4.tcp_fin_timeout 30\n"
    "net.ipv4.tcp_keepalive_time 1200\n"
    "net.ipv4.tcp_keepalive_intvl 30\n"
    "net.ipv4.tcp_keepalive_probes 5\n"
    "net.ipv4.route.flush 1\n";

static void set_default_qdisc(void) {
    if (access("/proc/sys/net/core/default_qdisc", F_OK) != 0) return;
    const char *qs[] = {"fq", "fq_codel", "pfifo_fast"};
    char msg[96];
    for (int i = 0; i < 3; i++) {
        if (write_proc("/proc/sys/net/core/default_qdisc", qs[i]) == 0) {
            snprintf(msg, sizeof(msg), "Default qdisc set to %s", qs[i]);
            write_log(msg, "SUCCESS");
            return;
        }
    }
    write_log("Default qdisc locked by kernel, keeping current", "SKIP");
}

static void apply_preset(const char *name, const char *data, int wifi_low_latency) {
    char buf[16384];
    snprintf(buf, sizeof(buf), "%s", data);
    int total = 0, ok = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = 0;
        total++;
        if (write_sysctl(line, sp + 1) == 0) ok++;
        else log_skip("  -> not applied: %s = %s", line, sp + 1);
    }
    /* cmd wifi ... || true : selalu dianggap sukses */
    run_cmd("cmd wifi force-low-latency-mode %s", wifi_low_latency ? "enabled" : "disabled");
    set_default_qdisc();
    log_summary(name, ok, total);
}

static void apply_cmds(const char *name, const char *const *cmds, int n) {
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (run_cmd("%s", cmds[i]) == 0) ok++;
        else log_skip("  -> not applied: %s", cmds[i]);
    }
    log_summary(name, ok, n);
}

/* ===================== NETWORK HEALTH ===================== */

static void get_active_iface(char *iface, size_t len) {
    iface[0] = 0;
    char out[1024];
    cmd_out(out, sizeof(out), "ip route get 1.1.1.1 2>/dev/null");
    char *save = NULL;
    char *tok = strtok_r(out, " \t\r\n", &save);
    for (int i = 1; tok && i < 5; i++)
        tok = strtok_r(NULL, " \t\r\n", &save);
    if (tok) snprintf(iface, len, "%s", tok);
}

static void get_network_health(int *rtt, int *loss, int *jit) {
    *rtt = 999; *loss = 100; *jit = 0;
    char out[4096];
    if (cmd_out(out, sizeof(out), "ping -c 4 -W 2 1.1.1.1") != 0 || !out[0]) return;

    char *pl = strstr(out, "% packet loss");
    if (pl) {
        char *d = pl;
        while (d > out && isdigit((unsigned char)*(d - 1))) d--;
        *loss = atoi(d);
    }
    int sum = 0, cnt = 0, prev = -1, jsum = 0, jcnt = 0;
    for (char *p = strstr(out, "time="); p; p = strstr(p + 5, "time=")) {
        int t = atoi(p + 5);
        sum += t; cnt++;
        if (prev >= 0) { int d = t - prev; if (d < 0) d = -d; jsum += d; jcnt++; }
        prev = t;
    }
    if (cnt == 0) { *rtt = 999; *loss = 100; *jit = 0; return; }
    *rtt = sum / cnt;
    if (jcnt > 0) *jit = jsum / jcnt;
}

static double get_tcp_retransmit_rate(void) {
    FILE *f = fopen("/proc/net/snmp", "r");
    if (!f) return 0.0;
    char line[8192], hdr[8192] = "", val[8192] = "";
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Tcp:", 4) != 0) continue;
        if (!found) { snprintf(hdr, sizeof(hdr), "%s", line); found = 1; }
        else { snprintf(val, sizeof(val), "%s", line); break; }
    }
    fclose(f);
    if (!hdr[0] || !val[0]) return 0.0;

    int out_idx = -1, re_idx = -1, idx = 0;
    char *save = NULL;
    strtok_r(hdr, " \t\r\n", &save);                     /* "Tcp:" */
    for (char *tok = strtok_r(NULL, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save)) {
        if (!strcmp(tok, "OutSegs")) out_idx = idx;
        else if (!strcmp(tok, "RetransSegs")) re_idx = idx;
        idx++;
    }
    if (out_idx < 0 || re_idx < 0) return 0.0;

    long out_now = 0, re_now = 0;
    int i2 = 0;
    save = NULL;
    strtok_r(val, " \t\r\n", &save);
    for (char *tok = strtok_r(NULL, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save)) {
        if (i2 == out_idx) out_now = atol(tok);
        else if (i2 == re_idx) re_now = atol(tok);
        i2++;
    }

    long out_prev = 0, re_prev = 0;
    char c[64];
    FILE *cf = fopen(TCP_CACHE, "r");
    if (cf) {
        if (fgets(c, sizeof(c), cf)) out_prev = atol(c);
        if (fgets(c, sizeof(c), cf)) re_prev = atol(c);
        fclose(cf);
    }
    cf = fopen(TCP_CACHE, "w");
    if (cf) { fprintf(cf, "%ld\n%ld\n", out_now, re_now); fclose(cf); }

    long d_out = out_now - out_prev, d_re = re_now - re_prev;
    if (d_out <= 0 || d_re < 0) return 0.0;
    long pct_x10 = d_re * 1000 / d_out;
    return (pct_x10 / 10) + (pct_x10 % 10) / 10.0;
}

static int compute_network_score(int rtt, int loss, int jitter, double retrans) {
    int ri = (int)retrans;
    int p_rtt = rtt / 6;    if (p_rtt > 30) p_rtt = 30;
    int p_loss = loss * 3;  if (p_loss > 35) p_loss = 35;
    int p_jit = jitter;     if (p_jit > 15) p_jit = 15;
    int p_re = ri * 3;      if (p_re > 20) p_re = 20;
    int score = 100 - p_rtt - p_loss - p_jit - p_re;
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return score;
}

/* ===================== WIFI / MTU ===================== */

static void get_wifi_info(const char *iface, int *rssi, int *freq) {
    *rssi = 0; *freq = 0;
    char out[4096];
    if (run_cmd("command -v iw") == 0) {
        cmd_out(out, sizeof(out), "iw dev %s link", iface);
        char *s = strstr(out, "signal:");
        if (s) *rssi = atoi(s + 7);
        char *fr = strstr(out, "freq:");
        if (fr) *freq = atoi(fr + 5);
    }
    if (*rssi == 0 || *freq == 0) {
        cmd_out(out, sizeof(out), "dumpsys wifi 2>/dev/null | grep -im1 mWifiInfo");
        char *s = strstr(out, "RSSI: ");
        if (s && *rssi == 0) *rssi = atoi(s + 6);
        char *fr = strstr(out, "Frequency: ");
        if (fr && *freq == 0) *freq = atoi(fr + 11);
    }
}

static const char *classify_band(int freq) {
    if (freq >= 5925) return "6GHZ";
    if (freq >= 4900) return "5GHZ";
    if (freq >= 2400) return "24GHZ";
    return "UNKNOWN";
}

static void network_key(const char *iface, char *out, size_t len) {
    char rt[1024], gw[80] = "direct";
    cmd_out(rt, sizeof(rt), "ip route get 1.1.1.1 2>/dev/null");
    char *via = strstr(rt, " via ");
    if (via) sscanf(via + 5, "%79s", gw);
    for (char *p = gw; *p; p++)
        if (*p == '.' || *p == ':') *p = '_';
    snprintf(out, len, "%s_%s", iface, gw);
}

static int probe_mtu(const char *iface) {
    char key[160], cache_path[300], buf[32];
    network_key(iface, key, sizeof(key));
    snprintf(cache_path, sizeof(cache_path), "%s/%s", MTU_CACHE_DIR, key);
    if (read_first_line(cache_path, buf, sizeof(buf)) == 0 && buf[0])
        return atoi(buf);

    char link[8192];
    cmd_out(link, sizeof(link), "ip link show");
    if (strstr(link, ": tun0") || strstr(link, ": wg0") || strstr(link, ": tailscale")) {
        write_file(cache_path, "1420");
        return 1420;
    }
    if (run_cmd("ping -M do -c 1 -W 1 -s 1400 1.1.1.1") != 0) {
        int m = strncmp(iface, "wlan", 4) == 0 ? 1492 : 1440;
        char v[16]; snprintf(v, sizeof(v), "%d", m);
        write_file(cache_path, v);
        return m;
    }
    int lo = 1200, hi = 1472, best = 1400;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (run_cmd("ping -M do -c 1 -W 1 -s %d 1.1.1.1", mid) == 0) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    int mtu = best + 28;
    char v[16]; snprintf(v, sizeof(v), "%d", mtu);
    write_file(cache_path, v);
    { char msg[300]; snprintf(msg, sizeof(msg), "Adaptive MTU probe on %s -> %d (key: %s)", iface, mtu, key); write_log(msg, "SUCCESS"); }
    return mtu;
}

/* ===================== SMART AUTO DNS ===================== */

static void get_settings(const char *key, char *out, size_t len, const char *dflt) {
    cmd_out(out, len, "settings get global %s", key);
    for (char *p = out; *p; p++)
        if (*p == '\r' || *p == '\n') { *p = 0; break; }
    if (!out[0] || !strcmp(out, "null")) snprintf(out, len, "%s", dflt);
}

static int get_wifi_ssid(char *ssid, size_t len) {
    ssid[0] = 0;
    char out[8192];
    cmd_out(out, sizeof(out), "dumpsys wifi 2>/dev/null | grep -im1 mWifiInfo | grep -o 'SSID: \"[^\"]*\"'");
    if (!out[0])
        cmd_out(out, sizeof(out), "dumpsys wifi 2>/dev/null | grep -o 'SSID: \"[^\"]*\"' | head -n 1");
    if (!out[0]) return -1;
    char *q1 = strchr(out, '"');
    if (!q1) return -1;
    char *q2 = strchr(q1 + 1, '"');
    if (!q2) return -1;
    *q2 = 0;
    snprintf(ssid, len, "%s", q1 + 1);
    if (!ssid[0] || !strcmp(ssid, "<unknown ssid>") || !strcmp(ssid, "<unknown>") ||
        !strcmp(ssid, "<hidden>") || !strcmp(ssid, "0x") || !strcmp(ssid, "0x0"))
        return -1;
    return 0;
}

static void detect_network_type(char *out, size_t len) {
    char iface[64], buf[8192];
    get_active_iface(iface, sizeof(iface));
    if (!strncmp(iface, "wlan", 4) || !strncmp(iface, "eth", 3)) { snprintf(out, len, "wifi"); return; }
    if (!strncmp(iface, "rmnet", 5) || !strncmp(iface, "ccmni", 5) ||
        !strncmp(iface, "radio", 5) || !strncmp(iface, "pdp", 3)) { snprintf(out, len, "mobile"); return; }
    char ssid[128];
    if (get_wifi_ssid(ssid, sizeof(ssid)) == 0) { snprintf(out, len, "wifi"); return; }
    if (cmd_out(buf, sizeof(buf), "dumpsys connectivity 2>/dev/null | grep 'MOBILE.*CONNECTED'") == 0 && buf[0]) {
        snprintf(out, len, "mobile"); return;
    }
    snprintf(out, len, "unknown");
}

static void read_dns_config(const char *file, char *out, size_t len) {
    char path[256], v[256];
    snprintf(path, sizeof(path), MODDIR "/%s", file);
    v[0] = 0;
    read_file_trim(path, v, sizeof(v));
    if (!v[0] || !strcmp(v, "null") || !strcmp(v, "none"))
        snprintf(out, len, "dns.google");
    else
        snprintf(out, len, "%s", v);
}

static int get_ssid_dns(const char *ssid, char *out, size_t len) {
    out[0] = 0;
    if (!ssid || !ssid[0]) return -1;
    FILE *f = fopen(MODDIR "/dns_ssid_map.txt", "r");
    if (!f) return -1;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (!line[0] || line[0] == '#') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        if (!strcmp(line, ssid) && eq[1]) {
            snprintf(out, len, "%s", eq + 1);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

static void apply_private_dns(const char *target, const char *net_type, const char *event) {
    char mode[64], spec[300];
    get_settings("private_dns_mode", mode, sizeof(mode), "off");
    get_settings("private_dns_specifier", spec, sizeof(spec), "none");

    if (!strcmp(target, "default")) {
        if (!strcmp(mode, "off") || !strcmp(mode, "opportunistic")) return;
        run_cmd("settings delete global private_dns_mode");
        run_cmd("settings delete global private_dns_specifier");
        char msg[512];
        snprintf(msg, sizeof(msg), "Auto DNS [%s/%s]: %s/%s -> System Default", event, net_type, mode, spec);
        write_log(msg, "SUCCESS");
    } else {
        if (!strcmp(mode, "hostname") && !strcmp(spec, target)) return;
        run_cmd("settings put global private_dns_mode hostname");
        run_cmd("settings put global private_dns_specifier %s", target);
        char msg[512];
        snprintf(msg, sizeof(msg), "Auto DNS [%s/%s]: %s/%s -> %s", event, net_type, mode, spec, target);
        write_log(msg, "SUCCESS");
    }
}

static void run_dns_flip(const char *event) {
    if (!auto_mode_on()) return;
    if (!strncmp(event, "INTERFACE_", 10) || !strncmp(event, "ROUTE_", 6) ||
        !strncmp(event, "VPN_", 4) || !strcmp(event, "AUTO_ON") || !strcmp(event, "BOOT"))
        sleep(3);

    char net_type[16];
    detect_network_type(net_type, sizeof(net_type));
    if (!strcmp(net_type, "unknown")) return;

    char target[256] = "";
    if (!strcmp(net_type, "wifi")) {
        char ssid[128];
        if (get_wifi_ssid(ssid, sizeof(ssid)) == 0)
            get_ssid_dns(ssid, target, sizeof(target));
        if (!target[0]) read_dns_config("dns_wifi.txt", target, sizeof(target));
    } else if (!strcmp(net_type, "mobile")) {
        read_dns_config("dns_mobile.txt", target, sizeof(target));
    } else return;

    if (!target[0]) snprintf(target, sizeof(target), "dns.google");
    apply_private_dns(target, net_type, event);
}

/* ===================== TOGGLE & TWEAK ===================== */

static int if_interesting(const char *n) {
    return !strncmp(n, "wlan", 4) || !strncmp(n, "eth", 3) || !strncmp(n, "rmnet", 5) ||
           !strncmp(n, "ccmni", 5) || !strncmp(n, "radio", 5);
}

static void cmd_nic(int offload_off) {
    if (run_cmd("command -v ethtool") != 0) {
        write_log("NIC Offloading: ethtool not available on this ROM, skipping", "SKIP");
        return;
    }
    int ok = 0, total = 0;
    DIR *d = opendir("/sys/class/net");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!if_interesting(e->d_name)) continue;
        total++;
        if (run_cmd("ethtool -K %s gro %s gso %s", e->d_name,
                    offload_off ? "off" : "on", offload_off ? "off" : "on") == 0) ok++;
    }
    closedir(d);
    char msg[128];
    if (ok > 0) {
        snprintf(msg, sizeof(msg), "NIC Offloading: GRO/GSO %s on %d/%d interfaces",
                 offload_off ? "disabled" : "restored", ok, total);
        write_log(msg, "SUCCESS");
    } else {
        write_log("NIC Offloading: driver rejected changes (not supported)", "SKIP");
    }
}

static void cmd_wifi_ps(int power_save_off) {
    if (power_save_off && run_cmd("command -v iw") != 0) {
        write_log("Wi-Fi Low-Jitter: iw not available", "SKIP");
        return;
    }
    char iface[64];
    get_active_iface(iface, sizeof(iface));
    if (strncmp(iface, "wlan", 4) != 0) snprintf(iface, sizeof(iface), "wlan0");
    if (run_cmd("iw dev %s set power_save %s", iface, power_save_off ? "off" : "on") == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Wi-Fi Low-Jitter: power_save %s on %s",
                 power_save_off ? "disabled" : "restored", iface);
        write_log(msg, "SUCCESS");
    } else {
        write_log(power_save_off ? "Wi-Fi Low-Jitter: driver rejected" : "Wi-Fi Low-Jitter: restore failed", "SKIP");
    }
}

static const char *const CT_KEYS[]  = {"max", "tcp_est", "tcp_tw", "udp", "udp_stream", NULL};
static const char *const CT_PATHS[] = {
    "nf_conntrack_max",
    "nf_conntrack_tcp_timeout_established",
    "nf_conntrack_tcp_timeout_time_wait",
    "nf_conntrack_udp_timeout",
    "nf_conntrack_udp_timeout_stream"
};
#define CT_BASE "/proc/sys/net/netfilter/"
#define CT_BACKUP MODDIR "/.conntrack_backup"

static void cmd_conntrack_on(void) {
    char path[256], cur[64], line[160];
    FILE *bf;
    if (access(CT_BACKUP, F_OK) != 0) {
        bf = fopen(CT_BACKUP, "w");
        if (bf) {
            for (int i = 0; CT_KEYS[i]; i++) {
                snprintf(path, sizeof(path), CT_BASE "%s", CT_PATHS[i]);
                cur[0] = 0;
                read_first_line(path, cur, sizeof(cur));
                if (cur[0]) fprintf(bf, "%s:%s\n", CT_KEYS[i], cur);
            }
            fclose(bf);
        }
    }
    const char *vals[] = {"32768", "600", "30", "15", "60"};
    int ok = 0;
    for (int i = 0; CT_KEYS[i]; i++) {
        snprintf(path, sizeof(path), CT_BASE "%s", CT_PATHS[i]);
        if (write_proc(path, vals[i]) == 0) ok++;
        else log_skip("  -> not applied: %s = %s", CT_PATHS[i], vals[i]);
    }
    log_summary("ConnTrack Optimization", ok, 5);
}

static void cmd_conntrack_off(void) {
    FILE *f = fopen(CT_BACKUP, "r");
    if (!f) { write_log("ConnTrack: no backup", "SKIP"); return; }
    char line[160], path[256];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        char *c = strchr(line, ':');
        if (!c) continue;
        *c = 0;
        for (int i = 0; CT_KEYS[i]; i++) {
            if (!strcmp(line, CT_KEYS[i])) {
                snprintf(path, sizeof(path), CT_BASE "%s", CT_PATHS[i]);
                write_proc(path, c + 1);
                break;
            }
        }
    }
    fclose(f);
    write_log("ConnTrack: restored kernel defaults", "SUCCESS");
}

static int cpu_last(void) {
    char buf[64];
    if (read_first_line("/sys/devices/system/cpu/present", buf, sizeof(buf)) != 0) return 0;
    char *dash = strchr(buf, '-');
    return dash ? atoi(dash + 1) : atoi(buf);
}

static int set_irq_affinity(const char *mask, const char *logname) {
    int failed = 0;
    FILE *f = fopen("/proc/interrupts", "r");
    if (!f) { write_log(logname, "FAILED"); return -1; }
    char line[1024], path[128];
    while (fgets(line, sizeof(line), f)) {
        if (!strcasestr(line, "wlan") && !strcasestr(line, "rmnet") && !strcasestr(line, "ccmni"))
            continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        int irq = atoi(line);
        snprintf(path, sizeof(path), "/proc/irq/%d/smp_affinity", irq);
        if (write_proc(path, mask) != 0) failed = 1;
    }
    fclose(f);
    write_log(logname, failed ? "FAILED" : "SUCCESS");
    return failed;
}

/* Scan /proc: setpriority utk proses dgn comm cocok */
static int renice_procs(const char *const *comms, int nice_val) {
    int failed = 0;
    DIR *d = opendir("/proc");
    if (!d) return -1;
    struct dirent *e;
    char path[128], comm[128];
    while ((e = readdir(d))) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(comm, sizeof(comm), f)) {
            char *nl = strpbrk(comm, "\r\n"); if (nl) *nl = 0;
            for (int i = 0; comms[i]; i++) {
                if (!strcmp(comm, comms[i])) {
                    if (setpriority(PRIO_PROCESS, atoi(e->d_name), nice_val) != 0) failed = 1;
                    break;
                }
            }
        }
        fclose(f);
    }
    closedir(d);
    return failed;
}

static void cmd_ksoft(int boost) {
    const char *comms[] = {"ksoftirqd/0", "ksoftirqd/1", "ksoftirqd/2", "ksoftirqd/3",
                           "ksoftirqd/4", "ksoftirqd/5", "ksoftirqd/6", "ksoftirqd/7", NULL};
    int failed = renice_procs(comms, boost ? -20 : 0);
    write_log(boost ? "Boosted ksoftirqd priority" : "Restored ksoftirqd priority to default",
              failed ? "FAILED" : "SUCCESS");
}

static void boost_rild_netd(void) {
    const char *comms[] = {"rild", "netd", NULL};
    renice_procs(comms, -10);
}

static int lookup_uid(const char *pkg, char *uid, size_t len) {
    char out[512];
    cmd_out(out, sizeof(out), "pm list packages -U 2>/dev/null | grep -E '^package:%s '", pkg);
    char *u = strstr(out, "uid:");
    if (!u) { uid[0] = 0; return -1; }
    sscanf(u + 4, "%63s", uid);
    return uid[0] ? 0 : -1;
}

static void apply_boost_apps(void) {
    char buf[8192];
    if (read_list(MODDIR "/boost_apps.txt", buf, sizeof(buf)) != 0) return;

    int use_dscp = (run_cmd("iptables -t mangle -A OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46") == 0);
    if (use_dscp)
        run_cmd("iptables -t mangle -D OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46");

    int count = 0;
    char uid[64];
    char *save = NULL;
    for (char *pkg = strtok_r(buf, ",", &save); pkg; pkg = strtok_r(NULL, ",", &save)) {
        if (!pkg[0]) continue;
        if (lookup_uid(pkg, uid, sizeof(uid)) != 0) continue;
        if (use_dscp) {
            run_cmd("iptables  -t mangle -I OUTPUT -m owner --uid-owner %s -j DSCP --set-dscp 46", uid);
            run_cmd("ip6tables -t mangle -I OUTPUT -m owner --uid-owner %s -j DSCP --set-dscp 46", uid);
        } else {
            run_cmd("iptables  -t mangle -I OUTPUT -m owner --uid-owner %s -j MARK --set-mark 0x40000000/0x40000000", uid);
            run_cmd("ip6tables -t mangle -I OUTPUT -m owner --uid-owner %s -j MARK --set-mark 0x40000000/0x40000000", uid);
        }
        count++;
    }
    if (count > 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Prioritized traffic for %d boosted apps", count);
        write_log(msg, "SUCCESS");
    }
}

static void cmd_hw_tweak(void) {
    int cpus = cpu_last();
    const char *mask = cpus >= 7 ? "ff" : (cpus >= 3 ? "f" : "3");

    write_log("Set rps_sock_flow_entries",
        write_proc("/proc/sys/net/core/rps_sock_flow_entries", "32768") == 0 ? "SUCCESS" : "FAILED");

    DIR *d = opendir("/sys/class/net");
    if (d) {
        struct dirent *e;
        char base[256], q[340];
        while ((e = readdir(d))) {
            const char *n = e->d_name;
            if (!strncmp(n, "lo", 2) || !strncmp(n, "dummy", 5) || !strncmp(n, "ifb", 3)) continue;
            snprintf(base, sizeof(base), "/sys/class/net/%s/queues", n);
            DIR *qd = opendir(base);
            if (!qd) continue;
            struct dirent *qe;
            while ((qe = readdir(qd))) {
                if (!strncmp(qe->d_name, "rx-", 3)) {
                    snprintf(q, sizeof(q), "%s/%s/rps_cpus", base, qe->d_name);
                    write_proc(q, mask);
                    snprintf(q, sizeof(q), "%s/%s/rps_flow_cnt", base, qe->d_name);
                    write_proc(q, "4096");
                } else if (!strncmp(qe->d_name, "tx-", 3)) {
                    snprintf(q, sizeof(q), "%s/%s/xps_cpus", base, qe->d_name);
                    write_proc(q, mask);
                }
            }
            closedir(qd);
        }
        closedir(d);
    }
    { char msg[96]; snprintf(msg, sizeof(msg), "Applied CPU mask %s to RPS/XPS queues", mask); write_log(msg, "SUCCESS"); }

    apply_boost_apps();
}

static void cmd_init_tc(void) {
    char iface[64];
    get_active_iface(iface, sizeof(iface));
    if (!iface[0]) { write_log("Failed to find active interface for TC", "FAILED"); return; }

    char act[32] = "";
    read_first_line(MODDIR "/.current_auto_act", act, sizeof(act));
    if (!act[0]) snprintf(act, sizeof(act), "default");
    int is_game = !strcmp(act, "game");

    int limit  = is_game ? 64   : g_fq_limit;
    int flows  = is_game ? 128  : g_fq_flows;
    const char *interval = is_game ? "10ms" : "100ms";
    const char *target   = is_game ? "1ms"  : "5ms";
    int txq   = is_game ? 500 : 1000;

    run_cmd("tc qdisc del dev %s root", iface);
    int has_cake = (run_cmd("tc qdisc add dev lo root cake") == 0);
    if (has_cake) {
        run_cmd("tc qdisc del dev lo root");
        write_log("Detected CAKE qdisc support", "SUCCESS");
    } else {
        write_log("CAKE not found. Falling back to fq_codel", "SUCCESS");
    }

    if (has_cake) {
        if (is_game) {
            if (run_cmd("tc qdisc add dev %s root handle 1: cake besteffort nat diffserv4 bandwidth 50mbit raw", iface) != 0)
                run_cmd("tc qdisc add dev %s root handle 1: cake besteffort nat", iface);
        } else {
            run_cmd("tc qdisc add dev %s root handle 1: cake besteffort nat", iface);
        }
        run_cmd("tc filter add dev %s parent 1: protocol ip   prio 1 handle 0x40000000/0x40000000 fw flowid 1:1", iface);
        run_cmd("tc filter add dev %s parent 1: protocol ipv6 prio 1 handle 0x40000000/0x40000000 fw flowid 1:1", iface);
        run_cmd("tc filter add dev %s parent 1: protocol ip   prio 2 u32 match ip tos 0xb8 0xfc flowid 1:1", iface);
        char msg[128];
        snprintf(msg, sizeof(msg), "Applied CAKE qdisc on %s (mode: %s)", iface, act);
        write_log(msg, "SUCCESS");
    } else {
        run_cmd("tc qdisc add dev %s root handle 1: prio bands 3 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1", iface);
        run_cmd("tc qdisc add dev %s parent 1:1 handle 10: fq_codel quantum 1514 limit %d flows %d interval %s target %s noecn",
                iface, limit, flows, interval, target);
        run_cmd("tc qdisc add dev %s parent 1:2 handle 20: fq_codel quantum 1514 limit %d flows %d interval 100ms target 5ms noecn",
                iface, limit, flows);
        run_cmd("tc qdisc add dev %s parent 1:3 handle 30: fq_codel quantum 1514 limit %d flows %d interval 100ms target 10ms noecn",
                iface, limit * 4, flows);
        run_cmd("tc filter add dev %s parent 1: protocol ip   prio 1 handle 0x40000000/0x40000000 fw flowid 1:1", iface);
        run_cmd("tc filter add dev %s parent 1: protocol ipv6 prio 1 handle 0x40000000/0x40000000 fw flowid 1:1", iface);
        run_cmd("tc filter add dev %s parent 1: protocol ip   prio 2 u32 match ip tos 0xb8 0xfc flowid 1:1", iface);
        run_cmd("tc filter add dev %s parent 1: protocol ip   prio 3 u32 match ip protocol 6 0xff match u8 0x10 0xff at 33 flowid 1:1", iface);
        char msg[192];
        snprintf(msg, sizeof(msg), "Applied fq_codel on %s (mode: %s, limit: %d, target: %s)", iface, act, limit, target);
        write_log(msg, "SUCCESS");
    }
    run_cmd("ifconfig %s txqueuelen %d", iface, txq);
    run_cmd("ip link set %s txqueuelen %d", iface, txq);
}

/* ===================== ENGINE ===================== */

static void clear_all_tc(void) {
    DIR *d = opendir("/sys/class/net");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, "lo")) continue;
        run_cmd("tc qdisc del dev %s root", e->d_name);
    }
    closedir(d);
}

static void cmd_engine(const char *event) {
    if (acquire_lock() != 0) return;
    atexit(release_lock);

    char iface[64];
    get_active_iface(iface, sizeof(iface));

    char prev_state[160] = "", prev_profile[80] = "", prev_iface[80] = "";
    read_first_line(STATE_FILE, prev_state, sizeof(prev_state));
    char *c = strchr(prev_state, ':');
    if (c) { *c = 0; snprintf(prev_profile, sizeof(prev_profile), "%s", prev_state);
             snprintf(prev_iface, sizeof(prev_iface), "%s", c + 1); }
    else snprintf(prev_profile, sizeof(prev_profile), "%s", prev_state);

    if (!iface[0] || !strcmp(iface, "lo")) {
        if (strcmp(prev_profile, "OFFLINE") != 0) {
            write_file(STATE_FILE, "OFFLINE:none");
            clear_all_tc();
            char msg[256];
            snprintf(msg, sizeof(msg), "Network OFFLINE detected (Event: %s), cleared TC rules", event);
            write_log(msg, "SUCCESS");
        }
        return;
    }

    int rtt, loss, jit;
    get_network_health(&rtt, &loss, &jit);
    double retrans = get_tcp_retransmit_rate();
    int score = compute_network_score(rtt, loss, jit, retrans);

    int rssi = 0, freq = 0;
    const char *band = "NONE";
    if (!strncmp(iface, "wlan", 4)) {
        get_wifi_info(iface, &rssi, &freq);
        band = classify_band(freq);
    }

    const char *new_profile =
        (score < 65 || (rssi != 0 && rssi < -80)) ? "WEAK_NETWORK" : "LOW_LATENCY";
    int iface_changed = strcmp(iface, prev_iface) != 0;

    char pend[128] = "", pend_profile[80] = "";
    int pend_count = 0;
    read_first_line(PENDING_FILE, pend, sizeof(pend));
    char *pc = strchr(pend, ':');
    if (pc) { *pc = 0; snprintf(pend_profile, sizeof(pend_profile), "%s", pend); pend_count = atoi(pc + 1); }

    int should_apply = 0;
    if (iface_changed || !prev_profile[0] ||
        !strcmp(prev_profile, "NONE") || !strcmp(prev_profile, "OFFLINE")) {
        should_apply = 1;
        unlink(PENDING_FILE);
    } else if (!strcmp(new_profile, prev_profile)) {
        unlink(PENDING_FILE);
    } else if (!strcmp(new_profile, pend_profile)) {
        pend_count++;
        if (pend_count >= 2) { should_apply = 1; unlink(PENDING_FILE); }
        else { char v[96]; snprintf(v, sizeof(v), "%s:%d", new_profile, pend_count); write_file(PENDING_FILE, v); }
    } else {
        char v[96]; snprintf(v, sizeof(v), "%s:1", new_profile); write_file(PENDING_FILE, v);
    }
    if (!should_apply) return;

    { char m[160]; snprintf(m, sizeof(m), "Engine Triggered by Event: %s", event); write_log(m, "SUCCESS"); }
    { char m[512];
      snprintf(m, sizeof(m), "Metrics - IFACE: %s | RTT: %dms | LOSS: %d%% | JITTER: %dms | RETRANS: %.1f%% | SCORE: %d | RSSI: %d | BAND: %s",
               iface, rtt, loss, jit, retrans, score, rssi, band);
      write_log(m, "SUCCESS"); }
    { char m[192];
      snprintf(m, sizeof(m), "Switching Network Profile to: %s (was: %s)", new_profile,
               prev_profile[0] ? prev_profile : "NONE");
      write_log(m, "SUCCESS"); }

    { char v[160]; snprintf(v, sizeof(v), "%s:%s", new_profile, iface); write_file(STATE_FILE, v); }

    int txq;
    if (!strcmp(new_profile, "WEAK_NETWORK")) { g_fq_limit = 256; g_fq_flows = 32; txq = 1000; }
    else { g_fq_limit = 1024; g_fq_flows = 1024; txq = 3000; }

    if (!strcmp(band, "24GHZ")) txq += 1000;
    else if (!strcmp(band, "6GHZ") && txq > 1500) txq = 1500;

    int mtu = 0;
    char last_mtu_path[256], mbuf[32];
    snprintf(last_mtu_path, sizeof(last_mtu_path), MODDIR "/.last_mtu_%s", iface);
    if (iface_changed) mtu = probe_mtu(iface);
    else if (read_first_line(last_mtu_path, mbuf, sizeof(mbuf)) == 0 && mbuf[0]) mtu = atoi(mbuf);
    if (mtu <= 0) mtu = probe_mtu(iface);
    { char v[16]; snprintf(v, sizeof(v), "%d", mtu); write_file(last_mtu_path, v); }

    run_cmd("ip link set dev %s mtu %d", iface, mtu);
    { char m[128]; snprintf(m, sizeof(m), "Set MTU for %s to %d", iface, mtu); write_log(m, "SUCCESS"); }

    run_cmd("ifconfig %s txqueuelen %d", iface, txq);
    run_cmd("ip link set %s txqueuelen %d", iface, txq);
    { char m[192]; snprintf(m, sizeof(m), "Set TxQueueLen for %s to %d (band: %s)", iface, txq, band); write_log(m, "SUCCESS"); }

    run_mode("--hw-tweak", NULL);

    if (cfg_is("tc.txt", "1"))
        run_mode("--init-tc", NULL);

    if (cfg_is("wifips.txt", "1") && !strncmp(iface, "wlan", 4))
        run_mode("--wifi-ps-off", NULL);
}

/* ===================== AUTO DETECT ===================== */

static void set_best_tcp(const char *a, const char *b, const char *cc) {
    const char *algos[4] = {a, b, cc, NULL};
    char avail[256], pad[300];
    cmd_out(avail, sizeof(avail), "cat /proc/sys/net/ipv4/tcp_available_congestion_control");
    snprintf(pad, sizeof(pad), " %s ", avail);
    char tried[128] = "";
    for (int i = 0; algos[i]; i++) {
        if (i) strncat(tried, " ", sizeof(tried) - strlen(tried) - 1);
        strncat(tried, algos[i], sizeof(tried) - strlen(tried) - 1);
        char pat[64];
        snprintf(pat, sizeof(pat), " %s ", algos[i]);
        if (strstr(pad, pat) && write_sysctl("net.ipv4.tcp_congestion_control", algos[i]) == 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Auto-Detect: TCP optimized to %s", algos[i]);
            write_log(msg, "SUCCESS");
            return;
        }
    }
    char msg[192];
    snprintf(msg, sizeof(msg), "Auto-Detect: Failed to set TCP (tried: %s)", tried);
    write_log(msg, "FAILED");
}

static int in_game_list(const char *pkg) {
    FILE *f = fopen(GAME_LIST, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (!strcmp(line, pkg)) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

static int matches_any(const char *pkg, const char *const *list) {
    for (int i = 0; list[i]; i++)
        if (strcasestr(pkg, list[i])) return 1;
    return 0;
}

static void get_foreground_app(char *pkg, size_t len) {
    pkg[0] = 0;
    char out[8192];
    cmd_out(out, sizeof(out), "cmd activity stack info 1 0");
    char *line = out;
    while (line && *line) {
        char *eol = strchr(line, '\n');
        if (eol) *eol = 0;
        if (strstr(line, ",0") && strstr(line, "=t")) {
            /* awk -F'[ /]' '{print $4}' */
            char *save = NULL;
            char *tok = strtok_r(line, " /", &save);
            for (int i = 1; tok && i < 4; i++) tok = strtok_r(NULL, " /", &save);
            if (tok) snprintf(pkg, len, "%s", tok);
            break;
        }
        line = eol ? eol + 1 : NULL;
    }
}

static void cmd_auto_detect(void) {
    char current_act[32] = "";
    read_first_line(MODDIR "/.current_auto_act", current_act, sizeof(current_act));
    if (!current_act[0]) snprintf(current_act, sizeof(current_act), "default");

    char iface[64];
    get_active_iface(iface, sizeof(iface));
    if (!iface[0]) snprintf(iface, sizeof(iface), "wlan0");

    long now = (long)time(NULL);
    char rxpath[256], rxb[64];
    long rx_now = 0;
    snprintf(rxpath, sizeof(rxpath), "/sys/class/net/%s/statistics/rx_bytes", iface);
    if (read_first_line(rxpath, rxb, sizeof(rxb)) == 0) rx_now = atol(rxb);

    long speed = 0;
    char c_rx[64] = "", c_t[64] = "", c_if[64] = "";
    FILE *f = fopen(MODDIR "/.auto_rx_cache", "r");
    if (f) {
        if (fgets(c_rx, sizeof(c_rx), f)) {}
        if (fgets(c_t, sizeof(c_t), f)) {}
        if (fgets(c_if, sizeof(c_if), f)) {}
        fclose(f);
    }
    if (c_rx[0] && c_t[0] && c_if[0] && !strcmp(c_if, iface)) {
        long dt = now - atol(c_t);
        if (dt >= 2 && dt <= 20) {
            long diff = rx_now - atol(c_rx);
            if (diff < 0) diff = 0;
            speed = diff / dt;
        }
    }
    f = fopen(MODDIR "/.auto_rx_cache", "w");
    if (f) { fprintf(f, "%ld\n%ld\n%s\n", rx_now, now, iface); fclose(f); }

    char new_act[32] = "", pkg[256] = "";
    if (speed > 512000) {
        snprintf(new_act, sizeof(new_act), "download");
    } else {
        get_foreground_app(pkg, sizeof(pkg));
        if (pkg[0]) {
            static const char *const CAT_STREAM[] = {"tube","netflix","twitch","disney","hulu","spotify","bilibili","viu","iqiyi","music",NULL};
            static const char *const CAT_BROWSE[] = {"chrome","firefox","opera","edge","brave","ucbrowser",NULL};
            static const char *const CAT_SOCIAL[] = {"whatsapp","insta","facebook","twitter","tiktok","gram","snap","reddit",NULL};
            if (in_game_list(pkg)) snprintf(new_act, sizeof(new_act), "game");
            else if (matches_any(pkg, CAT_STREAM)) snprintf(new_act, sizeof(new_act), "streaming");
            else if (matches_any(pkg, CAT_BROWSE)) snprintf(new_act, sizeof(new_act), "browsing");
            else if (matches_any(pkg, CAT_SOCIAL)) snprintf(new_act, sizeof(new_act), "social");
            else snprintf(new_act, sizeof(new_act), "default");
        }
    }

    if (!new_act[0] || !strcmp(new_act, current_act)) {
        unlink(MODDIR "/.pending_auto_act");
        return;
    }

    char pend[96] = "", pend_act[32] = "";
    int pend_count = 0;
    read_first_line(MODDIR "/.pending_auto_act", pend, sizeof(pend));
    char *c = strchr(pend, ':');
    if (c) { *c = 0; snprintf(pend_act, sizeof(pend_act), "%s", pend); pend_count = atoi(c + 1); }
    if (!strcmp(pend_act, new_act)) pend_count++;
    else pend_count = 1;

    if (pend_count < 2) {
        char v[64]; snprintf(v, sizeof(v), "%s:%d", new_act, pend_count);
        write_file(MODDIR "/.pending_auto_act", v);
        return;
    }

    unlink(MODDIR "/.pending_auto_act");
    write_file(MODDIR "/.current_auto_act", new_act);
    { char m[512];
      snprintf(m, sizeof(m), "Auto-Detect: Activity changed to %s (App: %s, Speed: %ldKB/s)",
               new_act, pkg[0] ? pkg : "none", speed / 1024);
      write_log(m, "SUCCESS"); }

    char mode[48];
    snprintf(mode, sizeof(mode), "--%s", new_act);
    run_mode(mode, NULL);

    if (!strcmp(new_act, "game")) {
        set_best_tcp("westwood", "bbr", "cubic");
        run_mode("--nic-offload", NULL);
        run_mode("--irq-affinity", NULL);
        run_mode("--boost-ksoft", NULL);
        boost_rild_netd();
        run_mode("--init-tc", NULL);
    } else if (!strcmp(new_act, "download") || !strcmp(new_act, "streaming")) {
        set_best_tcp("bbr", "cubic", "reno");
        run_mode("--nic-on", NULL);
        run_mode("--unirq-affinity", NULL);
        run_mode("--unboost-ksoft", NULL);
    } else {
        set_best_tcp("cubic", "bbr", "reno");
        run_mode("--nic-on", NULL);
        run_mode("--unirq-affinity", NULL);
        run_mode("--unboost-ksoft", NULL);
    }
}

/* ===================== AUTO ON/OFF ===================== */

static void spawn(const char *mode, const char *arg) {
    pid_t pid = fork();
    if (pid == 0) {
        run_mode(mode, arg);
        exit(0);            /* exit() menjalankan atexit (release lock engine) */
    }
}

static void cmd_auto_off(void) {
    write_log("Smart Auto Mode Disabled. Restoring manual configuration...", "SUCCESS");

    char preset[64] = "";
    read_file_trim(MODDIR "/preset.txt", preset, sizeof(preset));
    if (preset[0]) {
        for (char *p = preset; *p; p++) *p = tolower((unsigned char)*p);
        char *sp = strchr(preset, ' '); if (sp) *sp = 0;
        char mode[80];
        snprintf(mode, sizeof(mode), "--%s", preset);
        run_mode(mode, NULL);
    } else {
        run_mode("--default", NULL);
    }

    char v[128];
    if (read_file_trim(MODDIR "/nic.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--nic-offload" : "--nic-on", NULL);
    if (read_file_trim(MODDIR "/irq.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--irq-affinity" : "--unirq-affinity", NULL);
    if (read_file_trim(MODDIR "/ksoft.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--boost-ksoft" : "--unboost-ksoft", NULL);
    if (read_file_trim(MODDIR "/tcp.txt", v, sizeof(v)) == 0 && v[0]) {
        if (write_sysctl("net.ipv4.tcp_congestion_control", v) != 0) {
            char m[160];
            snprintf(m, sizeof(m), "Set TCP Congestion Control to %s", v);
            write_log(m, "FAILED");
        }
    }
    if (read_file_trim(MODDIR "/wifips.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--wifi-ps-off" : "--wifi-ps-on", NULL);
    if (read_file_trim(MODDIR "/conntrack.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--conntrack-on" : "--conntrack-off", NULL);

    char dns[256], tmp[256];
    snprintf(dns, sizeof(dns), "default");
    if (read_file_trim(MODDIR "/dns_manual.txt", tmp, sizeof(tmp)) == 0 && tmp[0])
        snprintf(dns, sizeof(dns), "%s", tmp);
    if (!strcmp(dns, "default") || !dns[0]) {
        run_cmd("settings delete global private_dns_mode");
        run_cmd("settings delete global private_dns_specifier");
        write_log("Auto DNS: Restored to System Default", "SUCCESS");
    } else {
        run_cmd("settings put global private_dns_mode hostname");
        run_cmd("settings put global private_dns_specifier %s", dns);
        char m[320];
        snprintf(m, sizeof(m), "Auto DNS: Restored manual DNS -> %s", dns);
        write_log(m, "SUCCESS");
    }
}

/* ===================== DAEMON ===================== */

static void cmd_daemon(void) {
    write_log("Event Dispatcher Daemon Started", "SUCCESS");

    /* Tick loop (background, setara "( ... ) &") */
    if (fork() == 0) {
        int tick = 0;
        for (;;) {
            if (run_cmd("dumpsys power 2>/dev/null | grep -q mHoldingDisplaySuspendBlocker=true") == 0) {
                tick++;
                if (tick % 3 == 1)
                    run_mode("--engine", "PERIODIC_HEALTH_CHECK");
                if (auto_mode_on()) {
                    run_mode("--auto-detect", NULL);
                    run_mode("--dns-flip", "PERIODIC");
                }
            }
            sleep(10);
        }
    }

    /* ip monitor loop (foreground daemon) */
    FILE *p = popen("ip monitor link route address 2>/dev/null", "r");
    if (!p) return;
    char last_if_state[128] = "";
    char last_route[256] = "";
    char line[1024];
    while (fgets(line, sizeof(line), p)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;

        if (strstr(line, ": wlan") || strstr(line, ": rmnet") || strstr(line, ": ccmni")) {
            char ifname[64] = "";
            char *c1 = strchr(line, ':');
            if (c1) {
                char *start = c1 + 1;
                while (*start == ' ') start++;
                char *c2 = strchr(start, ':');
                if (c2) {
                    size_t n = c2 - start;
                    if (n > 0 && n < sizeof(ifname)) { memcpy(ifname, start, n); ifname[n] = 0; }
                }
            }
            if (!ifname[0]) continue;

            char new_state[128] = "";
            if (strstr(line, "state UP")) snprintf(new_state, sizeof(new_state), "%s:UP", ifname);
            else if (strstr(line, "state DOWN")) snprintf(new_state, sizeof(new_state), "%s:DOWN", ifname);

            if (new_state[0] && strcmp(new_state, last_if_state)) {
                snprintf(last_if_state, sizeof(last_if_state), "%s", new_state);
                if (strstr(new_state, ":UP")) {
                    if (debounce_ok("iface_up", 3)) {
                        spawn("--engine", "INTERFACE_UP");
                        spawn("--dns-flip", "INTERFACE_UP");
                    }
                } else {
                    if (debounce_ok("iface_down", 3)) {
                        spawn("--engine", "INTERFACE_DOWN");
                        spawn("--dns-flip", "INTERFACE_DOWN");
                    }
                }
            }
        } else if (strstr(line, "tun0") || strstr(line, "wg0") || strstr(line, "tailscale")) {
            if (strstr(line, "inet ")) {
                if (debounce_ok("vpn", 3)) {
                    spawn("--engine", "VPN_CONNECTED");
                    spawn("--dns-flip", "VPN_CONNECTED");
                }
            }
        } else if (!strncmp(line, "default via", 11) || !strncmp(line, "Deleted default via", 19)) {
            char key[256] = "";
            char *save = NULL;
            char *tok = strtok_r(line, " ", &save);
            for (int i = 0; tok && i < 5; i++) {
                if (i) strncat(key, " ", sizeof(key) - strlen(key) - 1);
                strncat(key, tok, sizeof(key) - strlen(key) - 1);
                tok = strtok_r(NULL, " ", &save);
            }
            if (key[0] && strcmp(key, last_route)) {
                snprintf(last_route, sizeof(last_route), "%s", key);
                if (debounce_ok("route", 4)) {
                    spawn("--engine", "ROUTE_CHANGED");
                    spawn("--dns-flip", "ROUTE_CHANGED");
                }
            }
        }
    }
    pclose(p);
}

/* ===================== BOOT ===================== */

static void detect_games(void) {
    write_log("Game Scanner: Detecting installed games...", "STARTED");
    FILE *lf = fopen(GAME_LIST, "w");
    if (lf) fclose(lf);
    run_cmd("cmd package query-activities -a android.intent.action.MAIN -c android.intent.category.LAUNCHER | "
            "awk -F= '/geNa/{p=$2} /ry=0/{if(p!=\"\") print p}' | sort -u >> %s", GAME_LIST);

    int count = 0;
    char list[4096] = "";
    FILE *f = fopen(GAME_LIST, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
            if (!line[0]) continue;
            count++;
            if (list[0]) strncat(list, ",", sizeof(list) - strlen(list) - 1);
            strncat(list, line, sizeof(list) - strlen(list) - 1);
        }
        fclose(f);
    }
    char msg[4200];
    snprintf(msg, sizeof(msg), "Game Scanner: Found %d games [%s]", count, list);
    write_log(msg, "SUCCESS");
}

static void module_description(void) {
    if (run_cmd("pgrep -f PingPimp") == 0)
        run_cmd("sed -Ei 's/^description=(\\[.*][[:space:]]*)?/description=[ \xF0\x9F\x98\x8B WORKING ] /g' %s", MODULE_PROP);
    else
        run_cmd("sed -Ei 's/^description=(\\[.*][[:space:]]*)?/description=[  \xE2\x9A\xA0\xEF\xB8\x8F NOT WORKING ] /g' %s", MODULE_PROP);
}

static void patch_wpa(void) {
    const char *paths[] = {
        "/data/misc/wifi/wpa_supplicant.conf",
        "/data/misc/wifi/wpa_supplicant/wpa_supplicant.conf",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], F_OK) == 0) {
            run_cmd("grep -q p2p_disabled=1 %s || echo p2p_disabled=1 >> %s", paths[i], paths[i]);
            run_cmd("grep -q ap_scan=1 %s || echo ap_scan=1 >> %s", paths[i], paths[i]);
            write_log("Patched wpa_supplicant.conf", "SUCCESS");
            return;
        }
    }
    write_log("wpa_supplicant.conf not present on this Android version (10+), skipping", "SKIP");
}

static void restore_isolated(void) {
    char buf[8192];
    if (read_list(MODDIR "/isolate_apps.txt", buf, sizeof(buf)) != 0) return;
    char uid[64];
    char *save = NULL;
    for (char *pkg = strtok_r(buf, ",", &save); pkg; pkg = strtok_r(NULL, ",", &save)) {
        if (!pkg[0]) continue;
        if (lookup_uid(pkg, uid, sizeof(uid)) != 0) continue;
        run_cmd("iptables  -I OUTPUT -m owner --uid-owner %s -j REJECT", uid);
        run_cmd("ip6tables -I OUTPUT -m owner --uid-owner %s -j REJECT", uid);
        char m[160];
        snprintf(m, sizeof(m), "Isolated app UID: %s", uid);
        write_log(m, "SUCCESS");
    }
}

static void cmd_boot(void) {
    FILE *f = fopen(LOG_FILE, "w");
    if (f) fclose(f);
    write_log("PingPimp Boot Initialization Started", "SUCCESS");

    rmdir(LOCKDIR);
    unlink(TCP_CACHE);
    unlink(PENDING_FILE);
    unlink(MODDIR "/.current_auto_act");
    run_cmd("rm -f %s/.debounce_* %s/.last_mtu_*", MODDIR, MODDIR);
    mkdir(MTU_CACHE_DIR, 0755);

    if (fork() == 0) { detect_games(); _exit(0); }
    module_description();

    run_cmd("ifconfig wlan0 -allmulti");
    run_cmd("ifconfig wlan0 -promisc");
    write_proc("/proc/sys/net/ipv6/conf/wlan0/accept_ra", "0");

    char preset[64] = "";
    read_file_trim(MODDIR "/preset.txt", preset, sizeof(preset));
    if (preset[0]) {
        for (char *p = preset; *p; p++) *p = tolower((unsigned char)*p);
        char *sp = strchr(preset, ' '); if (sp) *sp = 0;
        char m[96];
        snprintf(m, sizeof(m), "Applying Preset: %s", preset);
        write_log(m, "SUCCESS");
        char mode[80];
        snprintf(mode, sizeof(mode), "--%s", preset);
        run_mode(mode, NULL);
    } else {
        write_log("Applying Default Preset", "SUCCESS");
        run_mode("--default", NULL);
    }

    char v[128];
    if (read_file_trim(MODDIR "/ipv6_state.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--disable" : "--enable", NULL);
    if (read_file_trim(MODDIR "/state.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--state" : "--unstate", NULL);
    if (read_file_trim(MODDIR "/saver.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--saver" : "--unsaver", NULL);
    if (read_file_trim(MODDIR "/nic.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--nic-offload" : "--nic-on", NULL);
    if (read_file_trim(MODDIR "/irq.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--irq-affinity" : "--unirq-affinity", NULL);
    if (read_file_trim(MODDIR "/ksoft.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--boost-ksoft" : "--unboost-ksoft", NULL);
    if (read_file_trim(MODDIR "/wifips.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--wifi-ps-off" : "--wifi-ps-on", NULL);
    if (read_file_trim(MODDIR "/conntrack.txt", v, sizeof(v)) == 0 && v[0])
        run_mode(!strcmp(v, "1") ? "--conntrack-on" : "--conntrack-off", NULL);

    if (read_file_trim(MODDIR "/tcp.txt", v, sizeof(v)) == 0 && v[0]) {
        char m[160];
        snprintf(m, sizeof(m), "Set TCP Congestion Control to %s", v);
        write_log(m, write_sysctl("net.ipv4.tcp_congestion_control", v) == 0 ? "SUCCESS" : "FAILED");
    }

    restore_isolated();
    patch_wpa();

    run_cmd("stop tcpdump");
    run_cmd("stop vendor.tcpdump");
    run_cmd("stop cnss_diag");
    run_cmd("stop vendor.cnss_diag");
    if (access("/data/vendor/wlan_logs", F_OK) == 0) {
        run_cmd("rm -rf /data/vendor/wlan_logs/*");
        run_cmd("chmod 000 /data/vendor/wlan_logs");
        write_log("Stopped WLAN diag services & cleared logs", "SUCCESS");
    }

    write_file(STATE_FILE, "NONE");

    if (auto_mode_on())
        spawn("--dns-flip", "BOOT");

    write_log("PingPimp Boot Initialization Completed. Launching Daemon...", "SUCCESS");
    spawn("--daemon", NULL);
}

/* ===================== DISPATCH ===================== */

static int run_mode(const char *mode, const char *arg) {
    snprintf(g_label, sizeof(g_label), "%s", mode);

    if      (!strcmp(mode, "--default"))  apply_preset("Network Preset: Default (Balanced)", PRESET_DEFAULT, 0);
    else if (!strcmp(mode, "--game"))     apply_preset("Network Preset: Game (Ultra Low Latency)", PRESET_GAME, 1);
    else if (!strcmp(mode, "--download")) apply_preset("Network Preset: Download (Maximum Throughput)", PRESET_DOWNLOAD, 0);
    else if (!strcmp(mode, "--streaming"))apply_preset("Network Preset: Streaming (Smooth Buffer)", PRESET_STREAMING, 0);
    else if (!strcmp(mode, "--social") || !strcmp(mode, "--browsing"))
                                         apply_preset("Network Preset: Social / Browsing (Snappy)", PRESET_SOCIAL, 1);
    else if (!strcmp(mode, "--outdoor"))  apply_preset("Network Preset: Outdoor (Weak Signal Resilient)", PRESET_OUTDOOR, 0);
    else if (!strcmp(mode, "--state")) {
        static const char *const C[] = {
            "cmd settings put global netstats_enabled 0",
            "cmd wifi set-scan-always-available disabled",
            "cmd wifi set-verbose-logging disabled -l 0" };
        apply_cmds("Disable Network State", C, 3);
    }
    else if (!strcmp(mode, "--unstate")) {
        static const char *const C[] = {
            "cmd settings delete global netstats_enabled",
            "cmd wifi set-scan-always-available enabled",
            "cmd wifi set-verbose-logging enabled -l 1" };
        apply_cmds("Enable Network State", C, 3);
    }
    else if (!strcmp(mode, "--saver")) {
        static const char *const C[] = {
            "cmd netpolicy set restrict-background true",
            "cmd settings put global ingress_rate_limit_bytes_per_second 1048576" };
        apply_cmds("Enable Data Saver", C, 2);
    }
    else if (!strcmp(mode, "--unsaver")) {
        static const char *const C[] = {
            "cmd netpolicy set restrict-background false",
            "cmd settings delete global ingress_rate_limit_bytes_per_second" };
        apply_cmds("Disable Data Saver", C, 2);
    }
    else if (!strcmp(mode, "--disable") || !strcmp(mode, "--enable")) {
        int dis = !strcmp(mode, "--disable");
        const char *val = dis ? "1" : "0";
        int ok = 0;
        ok += write_sysctl("net.ipv6.conf.all.disable_ipv6", val) == 0;
        ok += write_sysctl("net.ipv6.conf.default.disable_ipv6", val) == 0;
        ok += write_sysctl("net.ipv6.conf.lo.disable_ipv6", val) == 0;
        run_cmd("content update --uri content://telephony/carriers --bind protocol:s:%s --bind roaming_protocol:s:%s",
                dis ? "IP" : "IPV4V6", dis ? "IP" : "IPV4V6");
        run_cmd("cmd connectivity airplane-mode enable");
        sleep(1);
        run_cmd("cmd connectivity airplane-mode disable");
        log_summary(dis ? "Disable IPv6 Connection" : "Enable IPv6 Connection", ok, 3);
    }
    else if (!strcmp(mode, "--nic-offload"))  cmd_nic(1);
    else if (!strcmp(mode, "--nic-on"))       cmd_nic(0);
    else if (!strcmp(mode, "--wifi-ps-off"))  cmd_wifi_ps(1);
    else if (!strcmp(mode, "--wifi-ps-on"))   cmd_wifi_ps(0);
    else if (!strcmp(mode, "--conntrack-on")) cmd_conntrack_on();
    else if (!strcmp(mode, "--conntrack-off"))cmd_conntrack_off();
    else if (!strcmp(mode, "--irq-affinity")) {
        int cpus = cpu_last();
        const char *mask = cpus >= 7 ? "c0" : (cpus >= 3 ? "c" : "3");
        char logname[96];
        snprintf(logname, sizeof(logname), "Set IRQ affinity to mask %s", mask);
        set_irq_affinity(mask, logname);
    }
    else if (!strcmp(mode, "--unirq-affinity"))
        set_irq_affinity("ff", "Restored IRQ affinity to all cores");
    else if (!strcmp(mode, "--boost-ksoft"))   cmd_ksoft(1);
    else if (!strcmp(mode, "--unboost-ksoft")) cmd_ksoft(0);
    else if (!strcmp(mode, "--hw-tweak"))      cmd_hw_tweak();
    else if (!strcmp(mode, "--init-tc"))       cmd_init_tc();
    else if (!strcmp(mode, "--engine"))        cmd_engine(arg ? arg : "");
    else if (!strcmp(mode, "--dns-flip"))      run_dns_flip(arg ? arg : "MANUAL");
    else if (!strcmp(mode, "--auto-detect"))   cmd_auto_detect();
    else if (!strcmp(mode, "--auto-on")) {
        write_log("Smart Auto Mode Enabled", "SUCCESS");
        spawn("--dns-flip", "AUTO_ON");
    }
    else if (!strcmp(mode, "--auto-off"))      cmd_auto_off();
    else if (!strcmp(mode, "--daemon"))        cmd_daemon();
    else if (!strcmp(mode, "--boot"))          cmd_boot();

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return 0;
    return run_mode(argv[1], argc > 2 ? argv[2] : NULL);
}