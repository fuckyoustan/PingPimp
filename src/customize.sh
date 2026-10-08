#!/system/bin/sh

ui_print "======================================"
ui_print "           MODULE INFORMATION          "
ui_print "======================================"
ui_print " Name       : $(grep_prop name "${TMPDIR}/module.prop")"
sleep 0.1
ui_print " Version    : $(grep_prop version "${TMPDIR}/module.prop")"
sleep 0.1
ui_print " Author     : $(grep_prop author "${TMPDIR}/module.prop")"
sleep 0.2
ui_print ""
ui_print "======================================"
ui_print "           DEVICE INFORMATION          "
ui_print "======================================"
ui_print " Model      : $(getprop ro.product.model)"
sleep 0.1
ui_print " Brand      : $(getprop ro.product.manufacturer)"
sleep 0.1
ui_print " Board      : $(getprop ro.product.board)"
sleep 0.1
ui_print " Android    : $(getprop ro.build.version.release)"
sleep 0.1
ui_print " Kernel     : $(uname -r)"
sleep 0.1
ui_print " CPU        : $(getprop ro.hardware)"
sleep 0.1
ui_print " RAM        : $(free | awk '/Mem:/ {print $2}') kB"
sleep 0.2
ui_print ""
ui_print "======================================"
ui_print "           ROOT ENVIRONMENT            "
ui_print "======================================"
if [ "$APATCH" ]; then
    ROOT_METHOD="APatch"
    ROOT_VERSION="$APATCH_VER ($APATCH_VER_CODE)"
elif [ "$KSU" ]; then
    if [ "$KSU_NEXT" ]; then
        ROOT_METHOD="KernelSU Next"
        ROOT_VERSION="$KSU_KERNEL_VER_CODE ($KSU_VER_CODE)"
    else
        ROOT_METHOD="KernelSU"
        ROOT_VERSION="$KSU_KERNEL_VER_CODE ($KSU_VER_CODE)"
    fi
elif [ "$MAGISK_VER_CODE" ]; then
    ROOT_METHOD="Magisk"
    ROOT_VERSION="$MAGISK_VER ($MAGISK_VER_CODE)"
else
    ROOT_METHOD="Unknown"
    ROOT_VERSION="N/A"
fi
ui_print " Method     : ${ROOT_METHOD}"
ui_print " Version    : ${ROOT_VERSION}"
sleep 0.3
ui_print ""
ui_print "======================================"
ui_print "             FINALIZATION              "
ui_print "======================================"
ui_print " Configuring systemless mount"
touch "$MODPATH/skip_mountify"
if [ "$KSU" = "true" ] || [ "$APATCH" = "true" ]; then
    touch "$MODPATH/skip_mount"
    BIN_PATH="/data/adb/modules/PingPimp/system/bin"
    manager_paths="/data/adb/ap/bin /data/adb/ksu/bin"
    for dir in $manager_paths; do
        [ -d "$dir" ] && {
            ln -sf "$BIN_PATH/PingPimp" "$dir/PingPimp"
            ui_print "  + $dir/PingPimp -> $BIN_PATH/PingPimp"
        }
    done
fi
sleep 2
ui_print " Installation successful"
ui_print " Completed at : $(date '+%d %b %Y - %H:%M %Z')"
ui_print " Dont forget to join my telegram channel"
ui_print "======================================"