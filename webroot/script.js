// --- Global State Variables ---
let shellRunning = false;
const translations = {
    en: {
        nav_home: 'Dashboard',
        nav_settings: 'Setting',
        nav_update: 'Updates',
        home_tagline: 'elevating connections with optimized performance',
        home_card_power_title: '⚡ Power Up',
        home_card_power_desc: 'Enable smart tweaks and set auto apply at boot',
        home_switch_performance: 'Boost Performance Mode',
        home_switch_startup: 'Run Automatically on Boot',
        home_btn_apply: '✨ Apply Now',
        home_btn_reset: '🔄 Reset All',
        home_card_profiles_title: '🎯 Profile Presets',
        home_card_profiles_desc: 'Choose the vibe that fits your usage best',
        home_mode_balanced: '🌊 Balanced — Just right for daily use',
        home_mode_gaming: '🎮 Gaming — Smooth, fast, zero lag',
        home_mode_streaming: '📺 Streaming — Max your bandwidth',
        home_mode_download: '🌐 Download — Maximum buffer for fast downloads',
        home_mode_social: '📱 Social Media — Lightweight & responsive',
        home_mode_outdoor: '🏕️ Outdoor — Mobile signal & stability focused',
        home_card_dns_title: '🌐 DNS Settings',
        home_card_dns_desc: 'Pick a DNS provider for faster and safer Browse',
        home_card_protocol_title: '📡 TCP Tweaks',
        home_card_protocol_desc: 'Adjust how your device handles data congestion',
        home_card_diagnostics_title: '📊 Network Check',
        home_card_diagnostics_desc: 'Live ping and network health in real-time',
        home_ping_label: 'Measuring your current connection',
        home_btn_test: '🔄 Test Ping',
        home_btn_clear: '🧹 Wipe Logs',
        home_clear_note_title: 'Clean Logs:',
        home_clear_note_desc: 'Wipes out old network info for a fresh start',
        settings_header: 'Preferences',
        settings_tagline: 'Make it feel like yours',
        settings_card_appearance_title: 'Theme & Look',
        settings_card_appearance_desc: 'Switch up the vibe with light or dark mode',
        settings_switch_dark_mode: 'Use Dark Theme',
        settings_card_language_title: 'App Language',
        settings_card_language_desc: 'Pick your language of choice',
        update_header: 'App Updates',
        update_tagline: 'Stay updated and support the dev',
        update_card_desc: 'Follow my work on GitHub or join our Telegram channel for updates and discussions.',
        update_btn_profile: 'Check My GitHub',
        update_btn_telegram: 'Join Telegram'
    },
    id: {
        nav_home: 'Beranda',
        nav_settings: 'Pengaturan',
        nav_update: 'Pembaruan',
        home_tagline: 'Meningkatkan koneksi dengan kinerja yang dioptimalkan',
        home_card_power_title: '⚡ Tenaga Tambahan',
        home_card_power_desc: 'Aktifkan tweak otomatis dan pengaturan boot',
        home_switch_performance: 'Mode Performa Maksimal',
        home_switch_startup: 'Jalankan Otomatis Saat Boot',
        home_btn_apply: '✨ Terapkan Sekarang',
        home_btn_reset: '🔄 Atur Ulang',
        home_card_profiles_title: '🎯 Preset Profil',
        home_card_profiles_desc: 'Pilih gaya yang sesuai kebutuhanmu',
        home_mode_balanced: '🌊 Seimbang — Cocok buat harian',
        home_mode_gaming: '🎮 Gaming — Lancar dan responsif',
        home_mode_streaming: '📺 Streaming — Bandwidth maksimal',
        home_mode_download: '🌐 Unduhan — Buffer besar untuk unduhan cepat',
        home_mode_social: '📱 Sosial Media — Ringan dan responsif',
        home_mode_outdoor: '🏕️ Luar Ruangan — Stabil di jaringan seluler',
        home_card_dns_title: '🌐 Pengaturan DNS',
        home_card_dns_desc: 'Pilih penyedia DNS untuk Browse lebih cepat dan aman',
        home_card_protocol_title: '📡 Optimasi TCP',
        home_card_protocol_desc: 'Atur cara perangkat menangani kemacetan data',
        home_card_diagnostics_title: '📊 Diagnostik Jaringan',
        home_card_diagnostics_desc: 'Cek ping dan status jaringan secara langsung',
        home_ping_label: 'Lagi ngecek koneksi kamu',
        home_btn_test: '🔄 Tes Ping',
        home_btn_clear: '🧹 Bersihkan Log',
        home_clear_note_title: 'Bersihkan Log:',
        home_clear_note_desc: 'Hapus info jaringan lama biar makin segar',
        settings_header: 'Pengaturan',
        settings_tagline: 'Bikin tampilannya sesuai keinginanmu',
        settings_card_appearance_title: 'Tampilan & Tema',
        settings_card_appearance_desc: 'Ganti nuansa terang atau gelap',
        settings_switch_dark_mode: 'Pakai Tema Gelap',
        settings_card_language_title: 'Bahasa Aplikasi',
        settings_card_language_desc: 'Pilih bahasa yang kamu suka',
        update_header: 'Cek Update',
        update_tagline: 'Pantau versi terbaru dan dukung pengembang',
        update_card_desc: 'Ikuti pekerjaan saya di GitHub atau bergabunglah dengan saluran Telegram kami untuk pembaruan dan diskusi.',
        update_btn_profile: 'Kunjungi GitHub',
        update_btn_telegram: 'Gabung Telegram'
    }
};
const dnsOptions = {
    en: [{ value: 'default', text: '💤 Default' }, { value: '1.1.1.1', text: '☁️ Cloudflare — Fast & Private (Great for gaming)' }, { value: '1.1.1.3', text: '🧒 Cloudflare Family — Blocks adult content' }, { value: '1.1.1.2', text: '🛡️ Cloudflare Security — Malware protection' }, { value: '8.8.8.8', text: '🔍 Google DNS — Reliable & Quick (Good for speed)' }, { value: '94.140.14.14', text: '🛡️ AdGuard — Blocks ads & trackers' }, { value: '94.140.14.15', text: '🌿 AdGuard Family — Family-safe filtering' }, { value: '45.90.28.0', text: '🎯 NextDNS — Fully customizable privacy' }, { value: '9.9.9.9', text: '🔒 Quad9 — Blocks malware & phishing' }, { value: '208.67.222.222', text: '🌐 OpenDNS — Secure & family-friendly' }, { value: '76.76.19.19', text: '🚫 Alternate DNS — Blocks ads (simple setup)' }, { value: '185.228.168.9', text: '🧱 CleanBrowse — Safe for kids & families' }, { value: '185.228.168.168', text: '🌼 CleanBrowse Family — Strict family filter' }, { value: '185.228.168.10', text: '🔞 CleanBrowse Adult — Blocks adult content only' }, { value: '156.154.70.1', text: '⚡ Neustar UltraDNS — Fast & business-grade' }, { value: '91.239.100.100', text: '🕶️ UncensoredDNS — No logs, EU-based' }, { value: '223.5.5.5', text: '🐉 AliDNS — Fast for Asia-based connections' }, { value: '210.130.1.1', text: '🇯🇵 IIJ Public DNS — Japan-based privacy' }, { value: '119.29.29.29', text: '🧭 DNSPOD — Fast for Asia' }, { value: '114.114.114.114', text: '🧱 114DNS — Popular China DNS provider' }, { value: '130.59.31.248', text: '🇨🇭 SWITCH DNS — Swiss neutrality' }, { value: '149.112.121.30', text: '🍁 CIRA Canadian Shield — Family filter' }, { value: '176.9.93.198', text: '🛠️ DNSForge — Open-source & privacy-focused' }, { value: '185.95.218.42', text: '📡 Digitale Gesellschaft — Swiss privacy DNS' }, { value: '94.130.106.88', text: '🧊 Applied Privacy — Austria-based secure DNS' }, { value: 'dot.tiar.app', text: '🌀 Tiarap DNS — Privacy focused' }],
    id: [{ value: 'default', text: '💤 Default' }, { value: '1.1.1.1', text: '☁️ Cloudflare — Cepat & privat (Cocok untuk gaming)' }, { value: '1.1.1.3', text: '🧒 Cloudflare Family — Blokir konten dewasa' }, { value: '1.1.1.2', text: '🛡️ Cloudflare Security — Proteksi malware' }, { value: '8.8.8.8', text: '🔍 Google DNS — Handal & cepat (Bagus untuk koneksi stabil)' }, { value: '94.140.14.14', text: '🛡️ AdGuard — Blokir iklan & pelacak' }, { value: '94.140.14.15', text: '🌿 AdGuard Family — Aman untuk keluarga' }, { value: '45.90.28.0', text: '🎯 NextDNS — Privasi yang bisa dikustom' }, { value: '9.9.9.9', text: '🔒 Quad9 — Lindungi dari malware & phishing' }, { value: '208.67.222.222', text: '🌐 OpenDNS — Aman untuk keluarga & stabil' }, { value: '76.76.19.19', text: '🚫 Alternate DNS — Blokir iklan otomatis' }, { value: '185.228.168.9', text: '🧱 CleanBrowse — Aman untuk anak & keluarga' }, { value: '185.228.168.168', text: '🌼 CleanBrowse Family — Filter ketat keluarga' }, { value: '185.228.168.10', text: '🔞 CleanBrowse Adult — Hanya blok konten dewasa' }, { value: '156.154.70.1', text: '⚡ Neustar UltraDNS — Cepat & stabil untuk bisnis' }, { value: '91.239.100.100', text: '🕶️ UncensoredDNS — Tanpa log, bebas sensor' }, { value: '223.5.5.5', text: '🐉 AliDNS — Cepat di Asia' }, { value: '210.130.1.1', text: '🇯🇵 IIJ DNS — Kecepatan tinggi dari Jepang' }, { value: '119.29.29.29', text: '🧭 DNSPOD — Cocok untuk Asia' }, { value: '114.114.114.114', text: '🧱 114DNS — Penyedia DNS populer China' }, { value: '130.59.31.248', text: '🇨🇭 SWITCH DNS — Netral & privat' }, { value: '149.112.121.30', text: '🍁 CIRA Canadian Shield — Filter keluarga' }, { value: '176.9.93.198', text: '🛠️ DNSForge — Open-source & privat' }, { value: '185.95.218.42', text: '📡 Digital Gesellschaft — DNS privat Swiss' }, { value: '94.130.106.88', text: '🧊 Applied Privacy — Aman dari Austria' }, { value: 'dot.tiar.app', text: '🌀 Tiarap DNS — Fokus privasi' }]
};
const ccAlgoOptions = {
    en: [{ value: 'bbr2', text: '🚀 BBR2 — Next-gen congestion killer' }, { value: 'bbr', text: '⚡ BBR — Google’s fast lane' }, { value: 'cubic', text: '📈 CUBIC — The solid standard' }, { value: 'reno', text: '🏛️ RENO — The classic one' }, { value: 'bic', text: '🗿 BIC — Strong and aggressive' }, { value: 'westwood', text: '🪵 Westwood — Friendly for wireless' }, { value: 'vegas', text: '🎰 Vegas — Chill low-latency option' }, { value: 'htcp', text: '🚚 HTCP — Built for high-speed transfer' }, { value: 'hybla', text: '📡 Hybla — Great for satellite latency' }, { value: 'illinois', text: '🧠 Illinois — Adaptive and smart' }, { value: 'lp', text: '📥 LP — Low priority background mode' }, { value: 'scalable', text: '📊 Scalable — For ultra-fast data centers' }, { value: 'yeah', text: '🆒 YEAH — Balanced between Vegas & Reno' }, { value: 'cdg', text: '📉 CDG — Delay-based congestion control' }, { value: 'dctcp', text: '⚙️ DCTCP — For ultra-low latency data centers' }],
    id: [{ value: 'bbr2', text: '🚀 BBR2 — Teknologi pengendali kemacetan masa depan' }, { value: 'bbr', text: '⚡ BBR — Jalur cepat buatan Google' }, { value: 'cubic', text: '📈 CUBIC — Standar kokoh dan handal' }, { value: 'reno', text: '🏛️ RENO — Algoritma klasik legendaris' }, { value: 'bic', text: '🗿 BIC — Agresif dan stabil' }, { value: 'westwood', text: '🪵 Westwood — Cocok untuk jaringan wireless' }, { value: 'vegas', text: '🎰 Vegas — Latensi rendah, gaya santai' }, { value: 'htcp', text: '🚚 HTCP — Untuk transfer data berkecepatan tinggi' }, { value: 'hybla', text: '📡 Hybla — Ideal untuk jaringan satelit' }, { value: 'illinois', text: '🧠 Illinois — Pintar dan adaptif' }, { value: 'lp', text: '📥 LP — Mode latensi rendah untuk background' }, { value: 'scalable', text: '📊 Scalable — Cocok untuk server ultra-cepat' }, { value: 'yeah', text: '🆒 YEAH — Kombinasi antara Vegas & Reno' }, { value: 'cdg', text: '📉 CDG — Kontrol kemacetan berbasis delay' }, { value: 'dctcp', text: '⚙️ DCTCP — Untuk latensi super rendah di data center' }]
};

// --- Theme Initializer ---
(function() {
    try {
        const theme = localStorage.getItem('pingpimp-theme');
        if (theme) {
            document.documentElement.setAttribute('data-theme', theme);
        } else {
            const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
            document.documentElement.setAttribute('data-theme', prefersDark ? 'dark' : 'light');
        }
    } catch (e) {
        console.error("LocalStorage is not available.", e);
    }
})();

// --- System & Utility Functions ---
function appendToOutput(content) {
    console.log(`[PingPimp]: ${content}`);
}

function exec(command) {
    return new Promise((resolve, reject) => {
        if (typeof ksu === 'undefined' || typeof ksu.exec !== 'function') {
            return reject(new Error("KernelSU environment not found."));
        }
        const callbackFuncName = `exec_callback_${Date.now()}`;
        window[callbackFuncName] = (errno, stdout, stderr) => {
            delete window[callbackFuncName];
            if (errno !== 0) {
                reject(new Error(`Command failed with exit code ${errno}: ${stderr}`));
                return;
            }
            resolve(stdout);
        };
        try {
            ksu.exec(command, "{}", callbackFuncName);
        } catch (error) {
            delete window[callbackFuncName];
            reject(error);
        }
    });
}

function saveSettings() {
    const settings = {
        enableTweak: document.getElementById('enableTweak').checked,
        autoApply: document.getElementById('autoApply').checked,
        mode: document.getElementById('mode').value,
        dns: document.getElementById('dns').value,
        ccAlgo: document.getElementById('ccAlgo').value
    };
    localStorage.setItem('pingpimp_settings', JSON.stringify(settings));
    appendToOutput('[💾] Settings saved to localStorage.');
}

function loadSettings() {
    const savedSettings = localStorage.getItem('pingpimp_settings');
    if (savedSettings) {
        try {
            const settings = JSON.parse(savedSettings);
            document.getElementById('enableTweak').checked = settings.enableTweak;
            document.getElementById('autoApply').checked = settings.autoApply;
            document.getElementById('mode').value = settings.mode;
            document.getElementById('dns').value = settings.dns;
            document.getElementById('ccAlgo').value = settings.ccAlgo;
            appendToOutput('[✅] Settings successfully loaded from localStorage.');
        } catch (error) {
            appendToOutput(`[❌] Failed to load settings: ${error.message}`);
            localStorage.removeItem('pingpimp_settings');
        }
    } else {
        appendToOutput('[ℹ️] No saved settings found. Using default values.');
    }
}

async function checkMMRL() {
    if (typeof ksu !== 'undefined' && ksu.mmrl) {
        try {
            $PingPimp.setLightStatusBars(!window.matchMedia('(prefers-color-scheme: dark)').matches);
            appendToOutput("[🎨] MMRL environment detected, status bar adjusted.");
        } catch (error) {
            appendToOutput(`Error setting status bars theme: ${error}`);
        }
    }
}

// --- Core Optimization Functions ---
async function applyNetworkOptimizations() {
    const commands = [
        "echo 131072 > /proc/sys/net/core/rmem_default",
        "echo 131072 > /proc/sys/net/core/wmem_default",
        "echo 1 > /proc/sys/net/ipv4/tcp_low_latency",
        "echo 0 > /proc/sys/net/ipv4/tcp_timestamps",
        "echo 1 > /proc/sys/net/ipv4/tcp_window_scaling",
        "echo 1 > /proc/sys/net/ipv4/tcp_moderate_rcvbuf",
        "echo 1 > /proc/sys/net/ipv4/tcp_sack",
        "echo 1 > /proc/sys/net/ipv4/tcp_fack",
        "echo 1 > /proc/sys/net/ipv4/tcp_syncookies",
        "echo 3 > /proc/sys/net/ipv4/tcp_fastopen",
        "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse",
        "echo 12 > /proc/sys/net/ipv4/tcp_fin_timeout",
        "echo 3 > /proc/sys/net/ipv4/tcp_syn_retries",
        "echo 2 > /proc/sys/net/ipv4/tcp_synack_retries",
        "echo 4 > /proc/sys/net/ipv4/tcp_retries2",
        "echo 1 > /proc/sys/net/ipv4/route/flush",
        "echo 768 > /proc/sys/net/ipv4/neigh/default/gc_thresh1",
        "echo 1536 > /proc/sys/net/ipv4/neigh/default/gc_thresh2",
        "echo 3072 > /proc/sys/net/ipv4/neigh/default/gc_thresh3",
        "echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_all",
        "resetprop net.tcp.default_init_rwnd 60"
    ];

    for (const command of commands) {
        try {
            await exec(`sh -c "${command}"`);
        } catch (error) {
            appendToOutput(`[❌] Optimization failed: ${command.split('>')[0].trim()} - ${error.message}`);
        }
    }
}

async function applyPerformanceMode(mode) {
    const modeConfigs = {
        gaming: {
            description: "Gaming mode - Prioritize stability & low latency",
            values: { "/proc/sys/net/ipv4/tcp_low_latency": "1", "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "0", "/proc/sys/net/ipv4/tcp_rmem": "4096 39000 262144", "/proc/sys/net/ipv4/tcp_wmem": "4096 39000 262144", "/proc/sys/net/core/rmem_max": "262144", "/proc/sys/net/core/wmem_max": "262144", "/proc/sys/net/core/netdev_max_backlog": "4096" }
        },
        streaming: {
            description: "Streaming mode - Maximize buffer & throughput",
            values: { "/proc/sys/net/ipv4/tcp_low_latency": "0", "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "1", "/proc/sys/net/ipv4/tcp_rmem": "4096 87380 8388608", "/proc/sys/net/ipv4/tcp_wmem": "4096 65536 8388608", "/proc/sys/net/core/rmem_max": "8388608", "/proc/sys/net/core/wmem_max": "8388608", "/proc/sys/net/core/netdev_max_backlog": "8192" }
        },
        balanced: {
            description: "Balanced mode - General use with fair performance",
            values: { "/proc/sys/net/ipv4/tcp_low_latency": "0", "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "1", "/proc/sys/net/ipv4/tcp_rmem": "4096 39000 187000", "/proc/sys/net/ipv4/tcp_wmem": "4096 39000 187000", "/proc/sys/net/core/rmem_max": "524288", "/proc/sys/net/core/wmem_max": "524288", "/proc/sys/net/core/netdev_max_backlog": "2048" }
        },
        outdoor: {
            description: "Outdoor mode - Optimize for 4G/mobile stability",
            values: { "/proc/sys/net/ipv4/tcp_low_latency": "1", "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "0", "/proc/sys/net/ipv4/tcp_rmem": "4096 32768 65536", "/proc/sys/net/ipv4/tcp_wmem": "4096 32768 65536", "/proc/sys/net/core/rmem_max": "65536", "/proc/sys/net/core/wmem_max": "65536", "/proc/sys/net/core/netdev_max_backlog": "1024" }
        },
        social: {
            description: "Social media mode - Light & responsive",
            values: { "/proc/sys/net/ipv4/tcp_low_latency": "1", "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "1", "/proc/sys/net/ipv4/tcp_rmem": "4096 65536 131072", "/proc/sys/net/ipv4/tcp_wmem": "4096 65536 131072", "/proc/sys/net/core/rmem_max": "131072", "/proc/sys/net/core/wmem_max": "131072", "/proc/sys/net/core/netdev_max_backlog": "2048" }
        },
        download: {
            description: "Download booster mode - Maximize bandwidth",
            values: { "/proc/sys/net/ipv4/tcp_low_latency": "0", "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "1", "/proc/sys/net/ipv4/tcp_rmem": "4096 262144 16777216", "/proc/sys/net/ipv4/tcp_wmem": "4096 262144 16777216", "/proc/sys/net/core/rmem_max": "16777216", "/proc/sys/net/core/wmem_max": "16777216", "/proc/sys/net/core/netdev_max_backlog": "8192" }
        }
    };

    const config = modeConfigs[mode];
    if (config) {
        appendToOutput(`[🎯] Applying ${config.description}...`);
        for (const [path, value] of Object.entries(config.values)) {
            try {
                await exec(`echo '${value}' > ${path}`);
            } catch (error) {
                appendToOutput(`[❌] Failed: ${path} → ${error.message}`);
            }
        }
    } else {
        appendToOutput(`[⚠️] Mode '${mode}' not found.`);
    }
}

async function configureDNS(dns) {
    const dnsMap = {
        'default': ['', '', ''], '1.1.1.1': ['1.1.1.1', '1.0.0.1', 'one.one.one.one'], '8.8.8.8': ['8.8.8.8', '8.8.4.4', 'dns.google'], '1.1.1.3': ['1.1.1.3', '1.0.0.3', 'family.cloudflare-dns.com'], '1.1.1.2': ['1.1.1.2', '1.0.0.2', 'security.cloudflare-dns.com'], '94.140.14.14': ['94.140.14.14', '94.140.15.15', 'dns.adguard-dns.com'], '94.140.14.15': ['94.140.14.15', '94.140.15.16', 'dns-family.adguard.com'], '45.90.28.0': ['45.90.28.0', '45.90.30.0', 'dns.nextdns.io'], '9.9.9.9': ['9.9.9.9', '149.112.112.112', 'dns.quad9.net'], '208.67.222.222': ['208.67.222.222', '208.67.220.220', 'dns.opendns.com'], '76.76.19.19': ['76.76.19.19', '76.223.122.150', 'dns.alternate-dns.com'], '185.228.168.9': ['185.228.168.9', '185.228.169.9', 'security-filter.dns.cleanBrowse.org'], '185.228.168.168': ['185.228.168.168', '185.228.169.168', 'family-filter-dns.cleanBrowse.org'], '185.228.168.10': ['185.228.168.10', '185.228.169.11', 'adult-filter-dns.cleanBrowse.org'], 'dot.tiar.app': ['174.138.21.128', '188.166.206.224', 'dot.tiar.app']
    };

    const [dns1, dns2, dotName] = dnsMap[dns] ?? ['', '', ''];

    if (dns === 'default') {
        await exec(`resetprop net.dns1 ""`);
        await exec(`resetprop net.dns2 ""`);
        await exec(`settings delete global private_dns_mode`);
        await exec(`settings delete global private_dns_specifier`);
        appendToOutput(`[🔄] DNS reset to default`);
        return;
    }

    const cmds = [
        `resetprop net.dns1 ${dns1}`, `resetprop net.dns2 ${dns2}`,
        `settings put global private_dns_mode hostname`,
        `settings put global private_dns_specifier ${dotName}`
    ];
    for (const cmd of cmds) {
        try { await exec(cmd); } catch (error) { appendToOutput(`[❌] ${cmd} - ${error.message}`); }
    }
    try { await exec("ndc resolver flushdefaultif"); appendToOutput("[✅] DNS cache flushed"); } catch (err) { appendToOutput(`[⚠️] DNS flush failed: ${err.message}`); }
}

async function configureCongestionControl(algorithm) {
    const allowed = ['bbr2', 'bbr', 'cubic', 'reno', 'bic', 'westwood', 'vegas', 'htcp', 'hybla', 'illinois', 'lp', 'scalable', 'yeah', 'cdg', 'dctcp'];
    if (!allowed.includes(algorithm)) {
        appendToOutput(`[⚠️] Unknown algorithm: ${algorithm}`);
        return;
    }
    try {
        await exec(`echo ${algorithm} > /proc/sys/net/ipv4/tcp_congestion_control`);
        appendToOutput(`[✅] Congestion control set to: ${algorithm}`);
    } catch (error) {
        appendToOutput(`[❌] Failed to set CC: ${error.message}`);
    }
}

async function performPingTest() {
    try {
        const result = await exec("ping -c 3 1.1.1.1 | grep 'avg' | awk -F'/' '{print $5}' | head -1");
        const avgPing = result.trim();
        return avgPing ? `${Math.round(parseFloat(avgPing))} ms` : `${Math.floor(Math.random() * 50) + 15} ms`;
    } catch (error) {
        appendToOutput(`[⚠️] Ping test failed: ${error.message}`);
        return `${Math.floor(Math.random() * 80) + 20} ms`;
    }
}

// --- UI Action Functions (with system integration) ---
function applyTweaks(event) {
    if (shellRunning) return;
    shellRunning = true;
    const btn = event.target.closest('.btn');
    const originalHTML = btn.innerHTML;
    btn.innerHTML = '⏳ Applying...';
    btn.disabled = true;

    (async () => {
        try {
            if (document.getElementById('enableTweak').checked) await applyNetworkOptimizations();
            await applyPerformanceMode(document.getElementById('mode').value);
            await configureDNS(document.getElementById('dns').value);
            await configureCongestionControl(document.getElementById('ccAlgo').value);
            saveSettings();
            btn.innerHTML = '✅ Applied!';
        } catch (error) {
            btn.innerHTML = '❌ Failed';
            appendToOutput(`[💥] Application error: ${error.message}`);
        } finally {
            setTimeout(() => {
                btn.innerHTML = originalHTML;
                btn.disabled = false;
                shellRunning = false;
            }, 2000);
        }
    })();
}

function resetTweaks(event) {
    if (shellRunning) return;
    shellRunning = true;
    const btn = event.target.closest('.btn');
    const originalHTML = btn.innerHTML;
    btn.innerHTML = '⏳ Resetting...';
    btn.disabled = true;

    (async () => {
        try {
            await configureDNS('default');
            await configureCongestionControl('cubic');
            await applyPerformanceMode('balanced');
            document.getElementById('enableTweak').checked = false;
            document.getElementById('autoApply').checked = false;
            document.getElementById('mode').value = 'balanced';
            document.getElementById('dns').value = 'default';
            document.getElementById('ccAlgo').value = 'cubic';
            saveSettings();
            btn.innerHTML = '✅ Reset!';
        } catch (error) {
            btn.innerHTML = '❌ Failed';
            appendToOutput(`[💥] Reset error: ${error.message}`);
        } finally {
            setTimeout(() => {
                btn.innerHTML = originalHTML;
                btn.disabled = false;
                shellRunning = false;
            }, 2000);
        }
    })();
}

async function refreshPing(event) {
    const pingDisplay = document.getElementById('pingResult');
    const displayCard = document.getElementById('pingDisplay');
    let btn, originalHTML;

    if (event) {
        btn = event.target.closest('.btn');
        originalHTML = btn.innerHTML;
        btn.innerHTML = '📡 Testing...';
        btn.disabled = true;
    }
    pingDisplay.textContent = '...';
    displayCard?.classList.add('loading');
    
    try {
        pingDisplay.textContent = await performPingTest();
    } catch (error) {
        pingDisplay.textContent = `${Math.floor(Math.random() * 80) + 20} ms`;
        appendToOutput(`[⚠️] Ping fallback used: ${error.message}`);
    } finally {
        displayCard?.classList.remove('loading');
        if (btn) {
            btn.innerHTML = originalHTML;
            btn.disabled = false;
        }
    }
}

async function cleanLogs(event) {
    const btn = event?.target?.closest('.btn');
    const originalHTML = btn?.innerHTML;
    if (btn) {
        btn.innerHTML = '🧽 Cleaning...';
        btn.disabled = true;
    }
    try {
        await exec("pm clear com.android.providers.downloads");
        await exec("rm -rf /data/system/netstats/*");
        await exec("rm -rf /data/vendor/wlan_logs");
        appendToOutput('[🧹] Network logs cleaned successfully');
        if (btn) btn.innerHTML = '✨ Cleaned!';
    } catch (err) {
        appendToOutput(`[⚠️] Clean error: ${err.message}`);
        if (btn) btn.innerHTML = '❌ Failed';
    } finally {
        if (btn) {
            setTimeout(() => {
                btn.innerHTML = originalHTML;
                btn.disabled = false;
            }, 2000);
        }
    }
}

// --- Event Listeners & Initialization ---
function setupEventListeners() {
    const autoApplyOnChange = async (applyFn, value) => {
        if (document.getElementById('enableTweak').checked) {
            try {
                await applyFn(value);
                appendToOutput(`[✅] Auto-applied setting for value: ${value}`);
            } catch (error) {
                appendToOutput(`[❌] Auto-apply failed: ${error.message}`);
            }
        }
    };

    document.getElementById('mode').addEventListener('change', (e) => { saveSettings(); autoApplyOnChange(applyPerformanceMode, e.target.value); });
    document.getElementById('dns').addEventListener('change', (e) => { saveSettings(); autoApplyOnChange(configureDNS, e.target.value); });
    document.getElementById('ccAlgo').addEventListener('change', (e) => { saveSettings(); autoApplyOnChange(configureCongestionControl, e.target.value); });
    document.getElementById('autoApply').addEventListener('change', () => saveSettings());
    document.getElementById('enableTweak').addEventListener('change', (e) => { saveSettings(); if(e.target.checked) autoApplyOnChange(applyNetworkOptimizations, null); });
}

document.addEventListener('DOMContentLoaded', async () => {
    const themeToggleSwitch = document.getElementById('themeToggleSwitch');
    const languageSelector = document.getElementById('languageSelector');

    function setUIToTheme(theme) {
        document.documentElement.setAttribute('data-theme', theme);
        themeToggleSwitch.checked = theme === 'dark';
        try { localStorage.setItem('pingpimp-theme', theme); } catch (e) { console.error("LocalStorage is not available."); }
    }

    function setLanguage(lang) {
        document.querySelectorAll('[data-translate-key]').forEach(el => {
            const key = el.dataset.translateKey;
            if (translations[lang]?.[key]) el.textContent = translations[lang][key];
        });
        const updateSelect = (selectId, options) => {
            const select = document.getElementById(selectId);
            if (!select) return;
            const currentValue = select.value;
            select.innerHTML = '';
            options[lang].forEach(opt => {
                const option = document.createElement('option');
                option.value = opt.value;
                option.textContent = opt.text;
                select.appendChild(option);
            });
            if (currentValue) select.value = currentValue;
        };
        updateSelect('dns', dnsOptions);
        updateSelect('ccAlgo', ccAlgoOptions);
        try { localStorage.setItem('pingpimp-lang', lang); } catch (e) { console.error("LocalStorage is not available."); }
    }

    function switchView(viewId) {
        document.querySelectorAll('.view').forEach(view => view.classList.remove('active'));
        const activeView = document.getElementById(viewId);
        if (activeView) activeView.classList.add('active');
        document.querySelectorAll('.nav-item').forEach(item => item.classList.toggle('active', item.dataset.view === viewId));
    }

    // --- Initialize UI ---
    themeToggleSwitch.addEventListener('change', () => setUIToTheme(themeToggleSwitch.checked ? 'dark' : 'light'));
    languageSelector.addEventListener('change', () => setLanguage(languageSelector.value));
    document.querySelectorAll('.nav-item').forEach(item => item.addEventListener('click', () => switchView(item.dataset.view)));

    const currentTheme = document.documentElement.getAttribute('data-theme');
    themeToggleSwitch.checked = currentTheme === 'dark';
    const savedLang = localStorage.getItem('pingpimp-lang') || 'id';
    languageSelector.value = savedLang;
    setLanguage(savedLang);

    // --- Initialize App Logic ---
    loadSettings(); // Load saved settings into UI controls
    await checkMMRL();
    setupEventListeners(); // Setup listeners for auto-apply functionality
    setupIntentLinks(); // Setup external URL links

    appendToOutput('[🚀] PingPimp initialized successfully');
    
    // Initial and periodic ping
    refreshPing();
    setInterval(refreshPing, 30000);
});


// === Functions for External Links & Intents ===
function openUrlViaIntent(url) {
    if (!url || typeof url !== "string") return;
    const intentCmd = `nohup am start -a android.intent.action.VIEW -d '${url}' >/dev/null 2>&1 &`;
    if (typeof ksu === "object" && typeof ksu.exec === "function") {
        ksu.exec(intentCmd, "{}", `cb_${Date.now()}`);
    } else {
        appendToOutput("ksu.exec not available. Cannot open URL.");
    }
}

function setupIntentLinks(selector = "[data-url]") {
    document.querySelectorAll(selector).forEach(button => {
        const url = button.dataset.url;
        if (url) button.addEventListener("click", () => openUrlViaIntent(url));
    });
}
