#!/system/bin/sh

while [ -z "$(getprop sys.boot_completed)" ]; do
	sleep 1
done

pimp() {
    local file="$1"
    local value="$2"
    [ -f "$file" ] && ( [ -w "$file" ] || chmod +w "$file" 2>/dev/null ) && echo "$value" > "$file"
}

# Disable HTTP Proxy
settings put global http_proxy_port 0

# Disable WLAN log
rm -rf /data/vendor/wlan_logs
touch /data/vendor/wlan_logs
chmod 000 /data/vendor/wlan_logs

# Set txqueuelen
ifconfig wlan0 txqueuelen 4000 2>/dev/null

# DNS Override
for iface in rmnet0 rmnet1 net wcdma hspa lte ltea ppp0 pdpbr1 wlan0; do
    resetprop net.${iface}.dns1 1.1.1.1
    resetprop net.${iface}.dns2 1.0.0.1
done

resetprop net.dns1 1.1.1.1
resetprop net.dns2 1.0.0.1

# DNS redirect via iptables
iptables -t nat -A OUTPUT -p tcp --dport 53 -j DNAT --to-destination 1.1.1.1:53
iptables -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 1.0.0.1:53

# ==== Core Network Buffers ====
pimp /proc/sys/net/core/netdev_max_backlog 8192
pimp /proc/sys/net/core/rmem_default 262144
pimp /proc/sys/net/core/rmem_max 2097152
pimp /proc/sys/net/core/wmem_default 262144
pimp /proc/sys/net/core/wmem_max 2097152
pimp /proc/sys/net/core/optmem_max 512000
pimp /proc/sys/net/core/somaxconn 4096

# ==== TCP Performance ====
pimp /proc/sys/net/ipv4/tcp_low_latency 1
pimp /proc/sys/net/ipv4/tcp_sack 1
pimp /proc/sys/net/ipv4/tcp_fack 1
pimp /proc/sys/net/ipv4/tcp_timestamps 0
pimp /proc/sys/net/ipv4/tcp_window_scaling 1
pimp /proc/sys/net/ipv4/tcp_moderate_rcvbuf 1
pimp /proc/sys/net/ipv4/tcp_no_metrics_save 1
pimp /proc/sys/net/ipv4/tcp_tw_reuse 1
pimp /proc/sys/net/ipv4/tcp_tw_recycle 1
pimp /proc/sys/net/ipv4/tcp_fin_timeout 15
pimp /proc/sys/net/ipv4/tcp_keepalive_probes 5
pimp /proc/sys/net/ipv4/tcp_keepalive_intvl 30
pimp /proc/sys/net/ipv4/tcp_syncookies 1
pimp /proc/sys/net/ipv4/tcp_ecn 0
pimp /proc/sys/net/ipv4/tcp_rfc1337 1
pimp /proc/sys/net/ipv4/ip_no_pmtu_disc 0
pimp /proc/sys/net/ipv4/tcp_delack_min 5
pimp /proc/sys/net/ipv4/tcp_retries2 5
pimp /proc/sys/net/ipv4/tcp_syn_retries 5
pimp /proc/sys/net/ipv4/tcp_synack_retries 2
pimp /proc/sys/net/ipv4/tcp_max_syn_backlog 8192
pimp /proc/sys/net/ipv4/tcp_max_tw_buckets 262144
pimp /proc/sys/net/ipv4/tcp_fastopen 3

# ==== TCP Socket Buffers ====
pimp /proc/sys/net/ipv4/tcp_rmem "4096 39000 187000"
pimp /proc/sys/net/ipv4/tcp_wmem "4096 39000 187000"
pimp /proc/sys/net/ipv4/tcp_mem "187000 187000 187000"
pimp /proc/sys/net/ipv4/route/flush 1

# ==== Congestion Control ====
CC_FILE="/proc/sys/net/ipv4/tcp_congestion_control"
AVAILABLE_CC=$(cat /proc/sys/net/ipv4/tcp_available_congestion_control 2>/dev/null)
for algo in bbr2 bbr cubic; do
    echo "$AVAILABLE_CC" | grep -qw "$algo" && echo "$algo" > "$CC_FILE" && break
done

# ==== ARP & Neigh Optimization ====
pimp /proc/sys/net/ipv4/neigh/default/gc_thresh1 1024
pimp /proc/sys/net/ipv4/neigh/default/gc_thresh2 2048
pimp /proc/sys/net/ipv4/neigh/default/gc_thresh3 4096

for iface in wlan0 rmnet0 rmnet_data0 rmnet_data1 lo; do
    for p in proxy_delay delay_first_probe_time anycast_delay; do
        pimp /proc/sys/net/ipv4/neigh/$iface/$p 0
        pimp /proc/sys/net/ipv6/neigh/$iface/$p 0
    done
done

# ==== IPv4 conf ====
for param in accept_redirects secure_redirects accept_source_route; do
    pimp /proc/sys/net/ipv4/conf/all/$param 0
    pimp /proc/sys/net/ipv4/conf/default/$param 0
done

# ==== IPv6 Tweak ====
pimp /proc/sys/net/ipv6/route/gc_interval 0
pimp /proc/sys/net/ipv6/route/gc_min_interval 0
pimp /proc/sys/net/ipv6/neigh/default/gc_interval 0
pimp /proc/sys/net/ipv6/ip6frag_secret_interval 0

# ==== Optional cleanup ====
resetprop net.tcp.default_init_rwnd 60

exit 0
