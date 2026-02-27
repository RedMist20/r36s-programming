#!/bin/sh
# R36S Tools Menu for EmulationStation (dialog-based)
# Safe network utilities + driver info (no deauth / disruption actions)

OUT="/tmp/tools_menu_output.txt"
TITLE="R36S Tools Menu"
export TERM=${TERM:-linux}

run_dialog_menu() {
  if command -v openvt >/dev/null 2>&1; then
    openvt -c 1 -f -- sh -c 'tools_menu_internal'
    return $?
  fi
  if [ -e /dev/tty ]; then
    tools_menu_internal < /dev/tty > /dev/tty 2>&1
    return $?
  fi
  echo "No interactive TTY available for dialog."
  echo "Tip: install openvt (util-linux) or run this from SSH terminal."
  sleep 5
  return 1
}

tools_menu_internal() {
  while :; do
    CHOICE="$(dialog --clear --title "$TITLE" \
      --menu "Select an action:" 0 0 0 \
      1 "WiFi Scan (SSID list)" \
      2 "Toggle Monitor Mode (wlan0)" \
      3 "Set wlan0 MANAGED (safe sequence)" \
      4 "Set wlan0 MONITOR (safe sequence)" \
      5 "wlan0 DOWN" \
      6 "wlan0 UP" \
      7 "Show: iw dev" \
      8 "Set wlan0 channel (asks number)" \
      9 "Show wlan0 txpower (iwlist)" \
      10 "Set wlan0 txpower fixed (asks mBm)" \
      11 "Show Adapter + Driver Info" \
      12 "Restart Network" \
      13 "Log dmesg last 50 lines" \
      14 "Check rtl8812au module present" \
      15 "Exit" \
      3>&1 1>&2 2>&3)"

    [ $? -ne 0 ] && CHOICE="15"

    case "$CHOICE" in
      1) wifi_scan ;;
      2) toggle_monitor ;;
      3) set_managed ;;
      4) set_monitor ;;
      5) wlan_down ;;
      6) wlan_up ;;
      7) show_iw_dev ;;
      8) set_channel ;;
      9) show_txpower ;;
      10) set_txpower ;;
      11) adapter_driver_info ;;
      12) restart_network ;;
      13) dmesg_last50 ;;
      14) check_rtl8812au ;;
      15) clear; exit 0 ;;
    esac
  done
}

need_wlan0() {
  if ! ip link show wlan0 >/dev/null 2>&1; then
    echo "No wlan0 found."
    echo
    ip -br link 2>/dev/null || true
    return 1
  fi
  return 0
}

wifi_scan() {
  {
    echo "=== WiFi Scan (wlan0) ==="
    date
    echo

    if ! need_wlan0; then exit 0; fi
    if ! command -v iw >/dev/null 2>&1; then
      echo "iw not installed."
      exit 0
    fi

    echo "Bringing wlan0 up..."
    ip link set wlan0 up 2>/dev/null || true
    echo

    echo "Scan results (SSID / freq / signal):"
    echo
    iw dev wlan0 scan 2>/dev/null \
      | awk '
          /SSID: /{ssid=substr($0,7)}
          /freq: /{freq=$2}
          /signal: /{sig=$2" "$3}
          /last seen: /{
            if (ssid=="") ssid="(hidden)"
            printf "SSID: %-30s  Freq: %-6s  Signal: %s\n", ssid, freq, sig
            ssid=""; freq=""; sig=""
          }'
  } > "$OUT"

  dialog --title "WiFi Scan" --textbox "$OUT" 0 0
}

toggle_monitor() {
  {
    echo "=== Toggle Monitor Mode (wlan0) ==="
    date
    echo

    if ! need_wlan0; then exit 0; fi
    if ! command -v iw >/dev/null 2>&1; then
      echo "iw not installed."
      exit 0
    fi

    CUR="$(iw dev wlan0 info 2>/dev/null | awk '/type/ {print $2; exit}')"
    echo "Current type: ${CUR:-unknown}"
    echo

    ip link set wlan0 down 2>/dev/null || true

    if [ "$CUR" = "monitor" ]; then
      echo "Switching to managed..."
      iw dev wlan0 set type managed 2>/dev/null || iw dev wlan0 set type station 2>/dev/null || true
    else
      echo "Switching to monitor..."
      iw dev wlan0 set monitor none 2>/dev/null || iw dev wlan0 set type monitor 2>/dev/null || true
    fi

    ip link set wlan0 up 2>/dev/null || true
    echo

    NEW="$(iw dev wlan0 info 2>/dev/null | awk '/type/ {print $2; exit}')"
    echo "New type: ${NEW:-unknown}"
  } > "$OUT"

  dialog --title "Toggle Monitor Mode" --textbox "$OUT" 0 0
}

set_managed() {
  {
    echo "=== Set wlan0 to MANAGED (safe order) ==="
    date
    echo

    if ! need_wlan0; then exit 0; fi
    if ! command -v iw >/dev/null 2>&1; then echo "iw not installed."; exit 0; fi

    echo "ip link set wlan0 down"
    ip link set wlan0 down 2>/dev/null || true
    echo "iw dev wlan0 set type managed"
    iw dev wlan0 set type managed 2>/dev/null || iw dev wlan0 set type station 2>/dev/null || true
    echo "ip link set wlan0 up"
    ip link set wlan0 up 2>/dev/null || true
    echo
    iw dev wlan0 info 2>/dev/null || true
  } > "$OUT"

  dialog --title "Set Managed" --textbox "$OUT" 0 0
}

set_monitor() {
  {
    echo "=== Set wlan0 to MONITOR (safe order) ==="
    date
    echo

    if ! need_wlan0; then exit 0; fi
    if ! command -v iw >/dev/null 2>&1; then echo "iw not installed."; exit 0; fi

    echo "ip link set wlan0 down"
    ip link set wlan0 down 2>/dev/null || true
    echo "iw dev wlan0 set monitor none"
    iw dev wlan0 set monitor none 2>/dev/null || iw dev wlan0 set type monitor 2>/dev/null || true
    echo "ip link set wlan0 up"
    ip link set wlan0 up 2>/dev/null || true
    echo
    iw dev wlan0 info 2>/dev/null || true
  } > "$OUT"

  dialog --title "Set Monitor" --textbox "$OUT" 0 0
}

wlan_down() {
  {
    echo "=== wlan0 DOWN ==="
    date
    echo
    if ! need_wlan0; then exit 0; fi
    ip link set wlan0 down 2>/dev/null || true
    ip -br link wlan0 2>/dev/null || true
  } > "$OUT"
  dialog --title "wlan0 down" --textbox "$OUT" 0 0
}

wlan_up() {
  {
    echo "=== wlan0 UP ==="
    date
    echo
    if ! need_wlan0; then exit 0; fi
    ip link set wlan0 up 2>/dev/null || true
    ip -br link wlan0 2>/dev/null || true
  } > "$OUT"
  dialog --title "wlan0 up" --textbox "$OUT" 0 0
}

show_iw_dev() {
  {
    echo "=== iw dev ==="
    date
    echo
    if command -v iw >/dev/null 2>&1; then
      iw dev 2>/dev/null || true
    else
      echo "iw not installed."
    fi
  } > "$OUT"
  dialog --title "iw dev" --textbox "$OUT" 0 0
}

set_channel() {
  if ! need_wlan0; then
    echo "No wlan0 found." > "$OUT"
    dialog --title "Set channel" --textbox "$OUT" 0 0
    return 0
  fi
  if ! command -v iw >/dev/null 2>&1; then
    echo "iw not installed." > "$OUT"
    dialog --title "Set channel" --textbox "$OUT" 0 0
    return 0
  fi

  CH="$(dialog --title "Set wlan0 channel" --inputbox \
"Enter channel number (ex: 1-14 for 2.4GHz, common 5GHz like 36/40/44/48/149/153/157/161):" \
10 72 6 3>&1 1>&2 2>&3)"
  [ $? -ne 0 ] && return 0

  {
    echo "=== Set wlan0 channel ==="
    date
    echo
    echo "Requested channel: $CH"
    echo
    echo "Note: Some drivers require monitor mode or interface down/up."
    echo

    # Try while up first
    echo "iw dev wlan0 set channel $CH"
    iw dev wlan0 set channel "$CH" 2>/dev/null || true

    echo
    echo "If that failed, trying down -> set -> up..."
    ip link set wlan0 down 2>/dev/null || true
    iw dev wlan0 set channel "$CH" 2>/dev/null || true
    ip link set wlan0 up 2>/dev/null || true

    echo
    iw dev wlan0 info 2>/dev/null || true
  } > "$OUT"

  dialog --title "Set channel" --textbox "$OUT" 0 0
}

show_txpower() {
  {
    echo "=== wlan0 txpower (read) ==="
    date
    echo
    if ! need_wlan0; then exit 0; fi
    if command -v iwlist >/dev/null 2>&1; then
      iwlist wlan0 txpower 2>/dev/null || true
    else
      echo "iwlist not installed (wireless-tools)."
    fi
  } > "$OUT"
  dialog --title "txpower read" --textbox "$OUT" 0 0
}

set_txpower() {
  if ! need_wlan0; then
    echo "No wlan0 found." > "$OUT"
    dialog --title "Set txpower" --textbox "$OUT" 0 0
    return 0
  fi
  if ! command -v iw >/dev/null 2>&1; then
    echo "iw not installed." > "$OUT"
    dialog --title "Set txpower" --textbox "$OUT" 0 0
    return 0
  fi

  VAL="$(dialog --title "Set txpower fixed" --inputbox \
"Enter value in mBm (example: 1000 = 10 dBm). Some drivers reject this." \
10 72 1000 3>&1 1>&2 2>&3)"
  [ $? -ne 0 ] && return 0

  {
    echo "=== Set wlan0 txpower fixed ==="
    date
    echo
    echo "Requested: ${VAL} mBm"
    echo

    echo "Bringing wlan0 down..."
    ip link set wlan0 down 2>/dev/null || true

    echo "iw dev wlan0 set txpower fixed ${VAL}mBm"
    iw dev wlan0 set txpower fixed "${VAL}mBm" 2>/dev/null || true

    echo "Bringing wlan0 up..."
    ip link set wlan0 up 2>/dev/null || true
    echo

    echo "Read-back (may not reflect actual):"
    if command -v iwlist >/dev/null 2>&1; then
      iwlist wlan0 txpower 2>/dev/null || true
    else
      echo "iwlist not installed."
    fi
  } > "$OUT"

  dialog --title "Set txpower" --textbox "$OUT" 0 0
}

adapter_driver_info() {
  {
    echo "=== Adapter + Driver Info ==="
    date
    echo
    echo "Kernel: $(uname -r)"
    echo

    echo "USB devices (lsusb):"
    if command -v lsusb >/dev/null 2>&1; then lsusb; else echo "lsusb not installed."; fi
    echo

    echo "Interfaces:"
    ip -br link 2>/dev/null || true
    echo
    ip -br addr 2>/dev/null || true
    echo

    echo "Loaded modules (rtl/8812/cfg80211/mac80211):"
    lsmod 2>/dev/null | egrep -i '8812|88xx|rtl|cfg80211|mac80211' || echo "(none found)"
    echo

    echo "Recent dmesg matches (rtl/8812):"
    dmesg 2>/dev/null | egrep -i '8812|rtl|cfg80211|mac80211' | tail -n 50 || true
  } > "$OUT"
  dialog --title "Adapter + Driver Info" --textbox "$OUT" 0 0
}

restart_network() {
  {
    echo "=== Restart Network ==="
    date
    echo

    DID=0
    if command -v systemctl >/dev/null 2>&1; then
      for svc in NetworkManager networking network; do
        systemctl restart "$svc" 2>/dev/null && echo "Restarted: $svc" && DID=1
      done
    fi

    if [ "$DID" -eq 0 ] && command -v service >/dev/null 2>&1; then
      for svc in network-manager networking network; do
        service "$svc" restart 2>/dev/null && echo "Restarted: $svc" && DID=1
      done
    fi

    echo
    echo "Fallback: bounce wlan0"
    if ip link show wlan0 >/dev/null 2>&1; then
      ip link set wlan0 down 2>/dev/null || true
      sleep 1
      ip link set wlan0 up 2>/dev/null || true
    fi

    echo
    ip -br link 2>/dev/null || true
    echo
    ip -br addr 2>/dev/null || true
  } > "$OUT"
  dialog --title "Restart Network" --textbox "$OUT" 0 0
}

dmesg_last50() {
  {
    echo "=== dmesg (last 50 lines) ==="
    date
    echo
    dmesg 2>/dev/null | tail -n 50
  } > "$OUT"
  dialog --title "dmesg last 50" --textbox "$OUT" 0 0
}

check_rtl8812au() {
  {
    echo "=== rtl8812au module check ==="
    date
    echo

    echo "lsmod | grep -i rtl8812au"
    lsmod 2>/dev/null | grep -i rtl8812au || echo "(not loaded)"
    echo

    echo "modinfo rtl8812au"
    modinfo rtl8812au 2>/dev/null || echo "modinfo: ERROR: module not found"
    echo

    uname_r="$(uname -r)"
    echo "Searching for 8812au*.ko under /lib/modules/$uname_r (first 50):"
    find "/lib/modules/$uname_r" -type f \( -name "*8812au*.ko" -o -name "*8812*.ko" \) 2>/dev/null | head -n 50
  } > "$OUT"
  dialog --title "Check rtl8812au" --textbox "$OUT" 0 0
}

# Kick it off
run_dialog_menu
exit $?
