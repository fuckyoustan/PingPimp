#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MODDIR "/data/adb/modules/PingPimp"
#define PKG_LIST_FILE "/data/system/packages.list"

void trim(char *str) {
    char *p = str;
    int l = strlen(p);
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(str, p, l + 1);
}

void write_file(const char *path, const char *val) {
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(val, f);
        fclose(f);
    }
}

void exec_cmd(const char *cmd) {
    system(cmd);
}

int read_config(const char *filename, char *buffer, size_t size) {
    FILE *f = fopen(filename, "r");
    if (!f) return 0;
    if (fgets(buffer, size, f)) {
        trim(buffer);
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

void get_uid(const char *pkg, char *uid_out) {
    FILE *f = fopen(PKG_LIST_FILE, "r");
    uid_out[0] = '\0';
    if (!f) return;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char curr_pkg[256];
        char curr_uid[64];
        if (sscanf(line, "%255s %63s", curr_pkg, curr_uid) == 2) {
            if (strcmp(curr_pkg, pkg) == 0) {
                strcpy(uid_out, curr_uid);
                break;
            }
        }
    }
    fclose(f);
}

void apply_default() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "4096");
    write_file("/proc/sys/net/core/rmem_default", "262144");
    write_file("/proc/sys/net/core/rmem_max", "524288");
    write_file("/proc/sys/net/core/wmem_default", "262144");
    write_file("/proc/sys/net/core/wmem_max", "524288");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "1");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "4");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 1048576");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "8192");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "8192");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "20");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_game() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "1000");
    write_file("/proc/sys/net/core/rmem_default", "131072");
    write_file("/proc/sys/net/core/rmem_max", "262144");
    write_file("/proc/sys/net/core/wmem_default", "131072");
    write_file("/proc/sys/net/core/wmem_max", "262144");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "1");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "2");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 65536 262144");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 262144");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "32768");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "32768");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "10");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "0");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "1");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode enabled 2>/dev/null");
}

void apply_download() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "16384");
    write_file("/proc/sys/net/core/rmem_default", "524288");
    write_file("/proc/sys/net/core/rmem_max", "4194304");
    write_file("/proc/sys/net/core/wmem_default", "524288");
    write_file("/proc/sys/net/core/wmem_max", "4194304");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "3");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 4194304");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 4194304");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "8192");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "8192");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "30");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_streaming() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "8192");
    write_file("/proc/sys/net/core/rmem_default", "262144");
    write_file("/proc/sys/net/core/rmem_max", "2097152");
    write_file("/proc/sys/net/core/wmem_default", "262144");
    write_file("/proc/sys/net/core/wmem_max", "2097152");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "3");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 2097152");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 2097152");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "16384");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "16384");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "20");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_social() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "4096");
    write_file("/proc/sys/net/core/rmem_default", "131072");
    write_file("/proc/sys/net/core/rmem_max", "524288");
    write_file("/proc/sys/net/core/wmem_default", "131072");
    write_file("/proc/sys/net/core/wmem_max", "524288");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "1");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "3");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "8192");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "8192");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "15");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode enabled 2>/dev/null");
}

void apply_outdoor() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "8192");
    write_file("/proc/sys/net/core/rmem_default", "262144");
    write_file("/proc/sys/net/core/rmem_max", "1048576");
    write_file("/proc/sys/net/core/wmem_default", "262144");
    write_file("/proc/sys/net/core/wmem_max", "1048576");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "1");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "6");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 1048576");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "16384");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "16384");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "30");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_hw_tweaks() {
    exec_cmd("HEX_MASK=$(printf '%x' $(( (1 << 4) - 1 ))); "
             "for IFACE in /sys/class/net/wlan* /sys/class/net/rmnet*; do "
             "if [ -d \"$IFACE\" ]; then "
             "for Q in \"$IFACE\"/queues/*/rps_cpus; do "
             "[ -f \"$Q\" ] && echo \"$HEX_MASK\" > \"$Q\" 2>/dev/null; "
             "done; fi; done");
             
    exec_cmd("CONF=\"/data/misc/wifi/wpa_supplicant.conf\"; "
             "if [ -f \"$CONF\" ]; then "
             "grep -q \"p2p_disabled=1\" \"$CONF\" || echo \"p2p_disabled=1\" >> \"$CONF\"; "
             "grep -q \"ap_scan=1\" \"$CONF\" || echo \"ap_scan=1\" >> \"$CONF\"; "
             "fi; "
             "iw wlan0 set power_save off 2>/dev/null");
             
    exec_cmd("for iface in lo wlan0 rmnet_data0 rmnet_ipa0 dummy0 rmnet0 rndis0; do "
             "if [ -d \"/sys/class/net/$iface\" ]; then "
             "case \"$iface\" in "
             "lo|dummy0) Q=100 ;; "
             "wlan0) Q=3000 ;; "
             "rmnet*) Q=2500 ;; "
             "*) Q=1000 ;; "
             "esac; "
             "ifconfig \"$iface\" txqueuelen $Q 2>/dev/null; "
             "ip link set \"$iface\" txqueuelen $Q 2>/dev/null; "
             "fi; done");

    exec_cmd("stop tcpdump 2>/dev/null; stop vendor.tcpdump 2>/dev/null; "
             "stop cnss_diag 2>/dev/null; stop vendor.cnss_diag 2>/dev/null; "
             "if [ -d /data/vendor/wlan_logs ]; then "
             "rm -rf /data/vendor/wlan_logs/*; chmod 000 /data/vendor/wlan_logs; fi");
}

void init_tc() {
    const char *tc_script = 
        "IFACE=$(ip route get 1.1.1.1 2>/dev/null | awk '{print $5}' | head -n1); "
        "[ -z \"$IFACE\" ] && exit 0; "
        "tc qdisc del dev $IFACE root 2>/dev/null; "
        "tc qdisc add dev $IFACE root handle 1: prio bands 3 2>/dev/null; "
        "tc qdisc add dev $IFACE parent 1:1 handle 10: fq_codel 2>/dev/null || tc qdisc add dev $IFACE parent 1:1 handle 10: pfifo_fast 2>/dev/null; "
        "tc filter add dev $IFACE parent 1: protocol ip prio 1 u32 match ip tos 0xb8 0xfc flowid 1:1 2>/dev/null; "
        "tc filter add dev $IFACE parent 1: protocol ip prio 2 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null; "
        "tc filter add dev $IFACE parent 1: protocol ipv6 prio 2 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null;";
    exec_cmd(tc_script);
}

int check_dscp_support() {
    int status = system("iptables -t mangle -A OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null");
    if (status == 0) {
        system("iptables -t mangle -D OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null");
        return 1;
    }
    return 0;
}

void apply_iptables_list(const char *filename, int mode) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MODDIR, filename);
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    int use_dscp = 0;
    if (mode == 1) {
        use_dscp = check_dscp_support();
    }

    char line[4096];
    if (fgets(line, sizeof(line), f)) {
        char *token = strtok(line, ",");
        while (token != NULL) {
            char pkg[256];
            strcpy(pkg, token);
            trim(pkg);

            if (strlen(pkg) > 0) {
                char uid[64];
                get_uid(pkg, uid);

                if (strlen(uid) > 0) {
                    char cmd[512];
                    if (mode == 1) {
                        if (use_dscp) {
                            snprintf(cmd, sizeof(cmd), "iptables -t mangle -I OUTPUT -m owner --uid-owner %s -j DSCP --set-dscp 46 2>/dev/null", uid);
                            exec_cmd(cmd);
                            snprintf(cmd, sizeof(cmd), "ip6tables -t mangle -I OUTPUT -m owner --uid-owner %s -j DSCP --set-dscp 46 2>/dev/null", uid);
                            exec_cmd(cmd);
                        } else {
                            snprintf(cmd, sizeof(cmd), "iptables -t mangle -I OUTPUT -m owner --uid-owner %s -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null", uid);
                            exec_cmd(cmd);
                            snprintf(cmd, sizeof(cmd), "ip6tables -t mangle -I OUTPUT -m owner --uid-owner %s -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null", uid);
                            exec_cmd(cmd);
                        }
                    } else { // Isolate Mode
                        snprintf(cmd, sizeof(cmd), "iptables -I OUTPUT -m owner --uid-owner %s -j REJECT 2>/dev/null", uid);
                        exec_cmd(cmd);
                        snprintf(cmd, sizeof(cmd), "ip6tables -I OUTPUT -m owner --uid-owner %s -j REJECT 2>/dev/null", uid);
                        exec_cmd(cmd);
                    }
                }
            }
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    char *mode = argv[1];

    if (strcmp(mode, "--default") == 0) apply_default();
    else if (strcmp(mode, "--game") == 0) apply_game();
    else if (strcmp(mode, "--download") == 0) apply_download();
    else if (strcmp(mode, "--streaming") == 0) apply_streaming();
    else if (strcmp(mode, "--social") == 0 || strcmp(mode, "--browsing") == 0) apply_social();
    else if (strcmp(mode, "--outdoor") == 0) apply_outdoor();
    
    else if (strcmp(mode, "--state") == 0) {
        exec_cmd("cmd settings put global netstats_enabled 0");
        exec_cmd("cmd wifi set-scan-always-available disabled");
        exec_cmd("cmd wifi set-verbose-logging disabled -l 0");
    } 
    else if (strcmp(mode, "--unstate") == 0) {
        exec_cmd("cmd settings delete global netstats_enabled");
        exec_cmd("cmd wifi set-scan-always-available enabled");
        exec_cmd("cmd wifi set-verbose-logging enabled -l 1");
    }
    
    else if (strcmp(mode, "--saver") == 0) {
        exec_cmd("cmd netpolicy set restrict-background true");
        exec_cmd("cmd settings put global ingress_rate_limit_bytes_per_second 1048576");
    } 
    else if (strcmp(mode, "--unsaver") == 0) {
        exec_cmd("cmd netpolicy set restrict-background false");
        exec_cmd("cmd settings delete global ingress_rate_limit_bytes_per_second");
    }
    
    else if (strcmp(mode, "--disable") == 0) {
        exec_cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=1 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=1 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=1 >/dev/null 2>&1");
    } 
    else if (strcmp(mode, "--enable") == 0) {
        exec_cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=0 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=0 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=0 >/dev/null 2>&1");
    }
    
    else if (strcmp(mode, "--hw-tweak") == 0) apply_hw_tweaks();
    else if (strcmp(mode, "--init-tc") == 0) init_tc();
    else if (strcmp(mode, "--boot") == 0) {
        char buffer[128];
        
        if (read_config(MODDIR "/preset.txt", buffer, sizeof(buffer))) {
            for(int i = 0; buffer[i]; i++) buffer[i] = tolower(buffer[i]);
            if (strcmp(buffer, "game") == 0) apply_game();
            else if (strcmp(buffer, "download") == 0) apply_download();
            else if (strcmp(buffer, "streaming") == 0) apply_streaming();
            else if (strstr(buffer, "social") != NULL) apply_social();
            else if (strcmp(buffer, "browsing") == 0) apply_social();
            else if (strcmp(buffer, "outdoor") == 0) apply_outdoor();
            else apply_default();
        } else {
            apply_default();
        }

        // 2. Terapkan Switch Lainnya
        if (read_config(MODDIR "/ipv6_state.txt", buffer, sizeof(buffer))) {
            if (strcmp(buffer, "1") == 0) exec_cmd(argv[0]), exec_cmd(" --disable");
            else exec_cmd(argv[0]), exec_cmd(" --enable");
        }

        if (read_config(MODDIR "/state.txt", buffer, sizeof(buffer)) && strcmp(buffer, "1") == 0) {
            exec_cmd(argv[0]); exec_cmd(" --state");
        } else {
            exec_cmd(argv[0]); exec_cmd(" --unstate");
        }

        if (read_config(MODDIR "/saver.txt", buffer, sizeof(buffer)) && strcmp(buffer, "1") == 0) {
            exec_cmd(argv[0]); exec_cmd(" --saver");
        } else {
            exec_cmd(argv[0]); exec_cmd(" --unsaver");
        }

        if (read_config(MODDIR "/tcp.txt", buffer, sizeof(buffer)) && strlen(buffer) > 0) {
            write_file("/proc/sys/net/ipv4/tcp_congestion_control", buffer);
        }

        apply_iptables_list("isolate_apps.txt", 0);
        apply_iptables_list("boost_apps.txt", 1);
        apply_hw_tweaks();
        init_tc();
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE); 
        if (pid > 0) exit(EXIT_SUCCESS);
        
        setsid();
        char last_iface[64] = {0};
        char curr_iface[64] = {0};

        while (1) {
            FILE *fp = popen("ip route get 1.1.1.1 2>/dev/null | awk '{print $5}' | head -n1", "r");
            if (fp) {
                curr_iface[0] = '\0';
                if (fgets(curr_iface, sizeof(curr_iface), fp) != NULL) {
                    trim(curr_iface);
                    if (strlen(curr_iface) > 0 && strcmp(curr_iface, last_iface) != 0) {
                        strcpy(last_iface, curr_iface);
                        init_tc();
                    }
                }
                pclose(fp);
            }
            sleep(3); 
        }
    }

    return 0;
}
