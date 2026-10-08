#!/system/bin/sh

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

for dir in /data/adb/ap/bin /data/adb/ksu/bin; do
    [ -L "$dir/PingPimp" ] && rm -f "$dir/PingPimp"
done