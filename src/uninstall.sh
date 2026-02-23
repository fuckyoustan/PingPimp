#!/system/bin/sh

PROPS="
persist.sys.radio.force_lte_ca
persist.data.netmgrd.qos.enable
persist.vendor.data.mode
persist.vendor.data.iwlan.enable
persist.data.tcp_conns
persist.sys.fflag.override.settings_network_and_internet_v2
"

for prop in $PROPS; do
    if command -v resetprop >/dev/null 2>&1; then
        resetprop --delete "$prop" >/dev/null 2>&1
    fi
done

if [ -d /data/vendor/wlan_logs ]; then
    chmod 777 /data/vendor/wlan_logs >/dev/null 2>&1
fi

resetprop net.dns1 ""
resetprop net.dns2 ""

for iface in rmnet0 rmnet1 net wcdma hspa lte ltea ppp0 pdpbr1 wlan0; do
    resetprop net.$iface.dns1 ""
    resetprop net.$iface.dns2 ""
done

settings put global private_dns_mode off
settings delete global private_dns_specifier