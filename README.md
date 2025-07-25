![banner](https://raw.githubusercontent.com/fuckyoustan/PingPimp/refs/heads/main/Banner.jpg)

If your network is trippin' with lag and high ping, you need **PingPimp**. This beast gets all up in your Android's TCP/IP guts and slaps your connection into shape aggressively, smartly, and smooth AF.

# ✨ Features

- 🔧 Aggressive Network Tweaks
Cranks up TCP/IP settings, queueing strategies, and system buffers tuned for lower latency and smoother packet handling.
- 🌐 Public DNS Selector (25+ Options)
Easily switch between top-tier DNS providers like Cloudflare, Google, NextDNS, AdGuard, and more straight from the WebUI.
- 📡 Congestion Control Mode Selector (15 Modes)
Pick your preferred TCP congestion control algorithm (BBR, BBR2, Cubic, Reno, Hybla, etc.) for better throughput and stability based on your device.
- 🚀 IPv6 / ARP / Neigh Tweaks
Optimizes neighbor table aging, delays, and filters reducing noisy background traffic and improving connection handling.
- 🧼 WLAN Log Cleaner
Automatically clears cluttered WiFi logs to keep your system clean, lightweight, and snappy.
- 🖥 WebUI Integration
Manage all tweaks on the fly without terminal hassle. Real time switching for DNS, congestion control, and more.

## 📦 Installation

1. Flash via Root Manager
2. Reboot
3. Tweak it your way!

# 📝 Note for users
Some congestion control options may not work on all devices.
To check which ones your device supports, run these commands in a terminal or shell:

```bash
sysctl net.ipv4.tcp_available_congestion_control
```

This helps you pick the best setting for your device

## 💬 Support / Chat

Drop your bugs, feature requests, or anything else here 👉 [Telegram](https://t.me/fuckyoustan)
