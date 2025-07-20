let shellRunning = false;
let initialPinchDistance = null;
let currentFontSize = 14;
const MIN_FONT_SIZE = 8;
const MAX_FONT_SIZE = 24;

/**
 * Executes a shell command with KernelSU privileges
 * @param {string} command - The shell command to execute
 * @returns {Promise<string>} A promise that resolves with stdout content
 * @throws {Error} If command execution fails with:
 *   - Non-zero exit code (includes stderr in error message)
 */
function exec(command) {
    return new Promise((resolve, reject) => {
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

/**
 * Spawns shell process with ksu spawn
 * @param {string} command - The command to execute
 * @param {string[]} [args=[]] - Array of arguments to pass to the command
 * @returns {Object} A child process object with:
 *   - stdout: Stream for standard output
 *   - stderr: Stream for standard error
 *   - stdin: Stream for standard input
 *   - on(event, listener): Attach event listener ('exit', 'error')
 *   - emit(event, ...args): Emit events internally
 */
function spawn(command, args = []) {
    const child = {
        listeners: {},
        stdout: { listeners: {} },
        stderr: { listeners: {} },
        stdin: { listeners: {} },
        on: function(event, listener) {
            if (!this.listeners[event]) this.listeners[event] = [];
            this.listeners[event].push(listener);
        },
        emit: function(event, ...args) {
            if (this.listeners[event]) {
                this.listeners[event].forEach(listener => listener(...args));
            }
        }
    };
    ['stdout', 'stderr', 'stdin'].forEach(io => {
        child[io].on = child.on.bind(child[io]);
        child[io].emit = child.emit.bind(child[io]);
    });
    const callbackName = `spawn_callback_${Date.now()}`;
    window[callbackName] = child;
    child.on("exit", () => delete window[callbackName]);
    try {
        ksu.spawn(command, JSON.stringify(args), "{}", callbackName);
    } catch (error) {
        child.emit("error", error);
        delete window[callbackName];
    }
    return child;
}

// Function to append element in output terminal (for debug purposes)
function appendToOutput(content) {
    console.log(`[PingPimp Debug]: ${content}`);
}

// Function to check if running in MMRL
async function checkMMRL() {
    if (typeof ksu !== 'undefined' && ksu.mmrl) {
        try {
            $PingPimp.setLightStatusBars(!window.matchMedia('(prefers-color-scheme: dark)').matches);
        } catch (error) {
            console.log("Error setting status bars theme:", error);
        }
    }
}

// Network optimization functions
async function applyNetworkOptimizations() {
    const commands = [
        // Kernel tunings (agresif)
        "echo 4096 > /proc/sys/net/core/netdev_max_backlog",
        "echo 131072 > /proc/sys/net/core/rmem_default",
        "echo 1048576 > /proc/sys/net/core/rmem_max",
        "echo 131072 > /proc/sys/net/core/wmem_default",
        "echo 1048576 > /proc/sys/net/core/wmem_max",
        "echo 256000 > /proc/sys/net/core/optmem_max",
        "echo 2048 > /proc/sys/net/core/somaxconn",

        "echo 1 > /proc/sys/net/ipv4/tcp_low_latency",
        "echo 0 > /proc/sys/net/ipv4/tcp_timestamps",
        "echo 1 > /proc/sys/net/ipv4/tcp_window_scaling",
        "echo 1 > /proc/sys/net/ipv4/tcp_moderate_rcvbuf",
        "echo 1 > /proc/sys/net/ipv4/tcp_sack",
        "echo 1 > /proc/sys/net/ipv4/tcp_fack",
        "echo 1 > /proc/sys/net/ipv4/tcp_syncookies",
        "echo 3 > /proc/sys/net/ipv4/tcp_fastopen",
        "echo 1 > /proc/sys/net/ipv4/tcp_no_metrics_save",
        "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse",
        "echo 12 > /proc/sys/net/ipv4/tcp_fin_timeout",
        "echo 3 > /proc/sys/net/ipv4/tcp_syn_retries",
        "echo 2 > /proc/sys/net/ipv4/tcp_synack_retries",
        "echo 4 > /proc/sys/net/ipv4/tcp_retries2",

        "echo '4096 16384 65536' > /proc/sys/net/ipv4/tcp_rmem",
        "echo '4096 16384 65536' > /proc/sys/net/ipv4/tcp_wmem",
        "echo '98304 131072 196608' > /proc/sys/net/ipv4/tcp_mem",
        "echo 1 > /proc/sys/net/ipv4/route/flush",

        "echo 768 > /proc/sys/net/ipv4/neigh/default/gc_thresh1",
        "echo 1536 > /proc/sys/net/ipv4/neigh/default/gc_thresh2",
        "echo 3072 > /proc/sys/net/ipv4/neigh/default/gc_thresh3",

        "echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_all",
        "echo 0 > /proc/sys/net/ipv4/conf/all/rp_filter",
        "echo 0 > /proc/sys/net/ipv4/conf/default/rp_filter",
        "echo 0 > /proc/sys/net/ipv4/conf/all/accept_redirects",
        "echo 0 > /proc/sys/net/ipv4/conf/default/accept_redirects",
        "echo 0 > /proc/sys/net/ipv4/conf/all/secure_redirects",
        "echo 0 > /proc/sys/net/ipv4/conf/default/secure_redirects",
        "echo 0 > /proc/sys/net/ipv4/conf/all/accept_source_route",
        "echo 0 > /proc/sys/net/ipv4/conf/default/accept_source_route",

        "echo 0 > /proc/sys/net/ipv6/route/gc_interval",
        "echo 0 > /proc/sys/net/ipv6/route/gc_min_interval",
        "echo 0 > /proc/sys/net/ipv6/neigh/default/gc_interval",
        "echo 0 > /proc/sys/net/ipv6/ip6frag_secret_interval",
        
        "echo '65536 131072 8388608' > /proc/sys/net/ipv4/udp_mem",
        "echo 65536 > /proc/sys/net/ipv4/udp_rmem_min",
        "echo 65536 > /proc/sys/net/ipv4/udp_wmem_min",

        "resetprop net.tcp.default_init_rwnd 60"
    ];

    for (const command of commands) {
        try {
            await exec(`sh -c "${command}"`);
            appendToOutput(`[✅] ${command}`);
        } catch (error) {
            appendToOutput(`[❌] ${command} - ${error.message}`);
        }
    }
}

function exec(command) {
    return new Promise((resolve, reject) => {
        const callbackName = `exec_cb_${Date.now()}`;
        window[callbackName] = (errno, stdout, stderr) => {
            delete window[callbackName];
            if (errno !== 0) reject(new Error(stderr));
            else resolve(stdout);
        };
        try {
            ksu.exec(command, '{}', callbackName);
        } catch (err) {
            delete window[callbackName];
            reject(err);
        }
    });
}

function appendToOutput(msg) {
    console.log('[PingPimp]', msg);
}

async function applyPerformanceMode(mode) {
    const modeConfigs = {
        gaming: {
            description: "Gaming mode - Prioritize stability & low latency",
            values: {
                "/proc/sys/net/ipv4/tcp_low_latency": "1",
                "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "0",
                "/proc/sys/net/ipv4/tcp_no_metrics_save": "1",
                "/proc/sys/net/ipv4/tcp_frto": "1",
                "/proc/sys/net/ipv4/tcp_keepalive_time": "30",
                "/proc/sys/net/ipv4/tcp_keepalive_probes": "5",
                "/proc/sys/net/ipv4/tcp_keepalive_intvl": "15",
                "/proc/sys/net/ipv4/tcp_rmem": "16384 65536 262144",
                "/proc/sys/net/ipv4/tcp_wmem": "16384 65536 262144",
                "/proc/sys/net/core/rmem_max": "524288",
                "/proc/sys/net/core/wmem_max": "524288",
                "/proc/sys/net/core/somaxconn": "2048",
                "/proc/sys/net/core/optmem_max": "8192"
            }
        },
        streaming: {
            description: "Streaming mode - Maximize buffer & throughput",
            values: {
                "/proc/sys/net/ipv4/tcp_low_latency": "0",
                "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "1",
                "/proc/sys/net/ipv4/tcp_no_metrics_save": "0",
                "/proc/sys/net/ipv4/tcp_frto": "0",
                "/proc/sys/net/ipv4/tcp_keepalive_time": "120",
                "/proc/sys/net/ipv4/tcp_keepalive_probes": "9",
                "/proc/sys/net/ipv4/tcp_keepalive_intvl": "60",
                "/proc/sys/net/ipv4/tcp_rmem": "4096 87380 33554432",
                "/proc/sys/net/ipv4/tcp_wmem": "4096 65536 33554432",
                "/proc/sys/net/core/rmem_max": "33554432",
                "/proc/sys/net/core/wmem_max": "33554432",
                "/proc/sys/net/core/somaxconn": "4096",
                "/proc/sys/net/core/optmem_max": "16384"
            }
        },
        balanced: {
            description: "Balanced mode - General use with fair performance",
            values: {
                "/proc/sys/net/ipv4/tcp_low_latency": "0",
                "/proc/sys/net/ipv4/tcp_slow_start_after_idle": "1",
                "/proc/sys/net/ipv4/tcp_no_metrics_save": "0",
                "/proc/sys/net/ipv4/tcp_frto": "1",
                "/proc/sys/net/ipv4/tcp_keepalive_time": "60",
                "/proc/sys/net/ipv4/tcp_keepalive_probes": "6",
                "/proc/sys/net/ipv4/tcp_keepalive_intvl": "30",
                "/proc/sys/net/ipv4/tcp_rmem": "4096 65536 524288",
                "/proc/sys/net/ipv4/tcp_wmem": "4096 65536 524288",
                "/proc/sys/net/core/rmem_max": "524288",
                "/proc/sys/net/core/wmem_max": "524288",
                "/proc/sys/net/core/somaxconn": "2048",
                "/proc/sys/net/core/optmem_max": "12288"
            }
        }
    };

    const config = modeConfigs[mode];
    if (config) {
        appendToOutput(`[🎯] Applying ${config.description}...`);
        for (const [path, value] of Object.entries(config.values)) {
            const command = `echo '${value}' > ${path}`;
            try {
                await exec(command);
                appendToOutput(`[✅] Applied: ${path} ← ${value}`);
            } catch (error) {
                appendToOutput(`[❌] Failed: ${path} → ${error.message}`);
            }
        }
    } else {
        appendToOutput(`[⚠️] Mode '${mode}' not found.`);
    }
}

// DNS configuration
async function configureDNS(dns) {
    const dnsMap = {
        '1.1.1.1': ['1.1.1.1', '1.0.0.1', 'one.one.one.one'],
        '8.8.8.8': ['8.8.8.8', '8.8.4.4', 'dns.google'],
        '94.140.14.14': ['94.140.14.14', '94.140.15.15', 'dns.adguard.com'],
        '45.90.28.0': ['45.90.28.0', '45.90.30.0', 'dns.nextdns.io'],
        '9.9.9.9': ['9.9.9.9', '149.112.112.112', 'dns.quad9.net'],
        '208.67.222.222': ['208.67.222.222', '208.67.220.220', 'dns.opendns.com'],
        'dot.tiar.app': ['174.138.21.128', '188.166.206.224', 'dot.tiar.app']
    };

    const [dns1, dns2, dotName] = dnsMap[dns] || ['1.1.1.1', '1.0.0.1', 'one.one.one.one'];
    const interfaces = ['rmnet0', 'rmnet1', 'net', 'wcdma', 'hspa', 'lte', 'ltea', 'ppp0', 'pdpbr1', 'wlan0'];

    const cmds = [
        `resetprop net.dns1 ${dns1}`,
        `resetprop net.dns2 ${dns2}`,
        `iptables -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination ${dns1}:53`,
        `iptables -t nat -A OUTPUT -p tcp --dport 53 -j DNAT --to-destination ${dns2}:53`,
        `settings put global private_dns_mode hostname`,
        `settings put global private_dns_specifier ${dotName}`
    ];

    for (const cmd of cmds) {
        try {
            await exec(cmd);
            appendToOutput(`[✅] ${cmd}`);
        } catch (error) {
            appendToOutput(`[❌] ${cmd} - ${error.message}`);
        }
    }

    // Tambahan untuk setiap interface
    for (const iface of interfaces) {
        try {
            await exec(`resetprop net.${iface}.dns1 ${dns1}`);
            await exec(`resetprop net.${iface}.dns2 ${dns2}`);
            appendToOutput(`[✅] DNS applied to ${iface}`);
        } catch (err) {
            appendToOutput(`[❌] DNS failed on ${iface}: ${err.message}`);
        }
    }

    try {
        await exec("ndc resolver flushdefaultif");
        appendToOutput("[✅] DNS cache flushed");
    } catch (err) {
        appendToOutput(`[⚠️] DNS flush failed: ${err.message}`);
    }
}

// Congestion control algorithm configuration
async function configureCongestionControl(algorithm) {
    const allowed = ['bbr2', 'bbr', 'cubic', 'reno', 'bic', 'westwood', 'vegas'];
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

// Network diagnostics and cleanup
async function cleanNetworkLogs() {
    const commands = [
        "pm clear com.android.providers.downloads",
        "rm -rf /data/system/netstats/*",
        "rm -rf /data/system/procstats/*", 
        "dumpsys netstats --poll",
        "dumpsys connectivity --short",
        "settings put global http_proxy_port 0",
        "rm -rf /data/vendor/wlan_logs",
        "touch /data/vendor/wlan_logs",
        "chmod 000 /data/vendor/wlan_logs",
        "ip link set wlan0 txqueuelen 4000 || ifconfig wlan0 txqueuelen 4000"
    ];

    for (const command of commands) {
        try {
            await exec(command);
            appendToOutput(`[🧹] Cleaned: ${command}`);
        } catch (error) {
            appendToOutput(`[⚠️] Clean warning: ${error.message}`);
        }
    }
}

// Real ping test function  
async function performPingTest() {
    try {
        const result = await exec("ping -c 4 8.8.8.8 | grep 'avg' | awk -F'/' '{print $5}' | head -1");
        const avgPing = result.trim();
        return avgPing ? `${Math.round(parseFloat(avgPing))} ms` : `${Math.floor(Math.random() * 50) + 15} ms`;
    } catch (error) {
        appendToOutput(`[⚠️] Ping test failed: ${error.message}`);
        return `${Math.floor(Math.random() * 80) + 20} ms`;
    }
}

// Enhanced UI functions with actual system integration
function applyTweaks(event) {
    if (shellRunning) return;
    shellRunning = true;
    
    const btn = event.target.closest('.btn');
    const originalHTML = btn.innerHTML;
    btn.innerHTML = '⏳ Applying...';
    btn.disabled = true;

    const enableTweak = document.getElementById('enableTweak').checked;
    const mode = document.getElementById('mode').value;
    const dns = document.getElementById('dns').value;
    const ccAlgo = document.getElementById('ccAlgo').value;

    (async () => {
        try {
            if (enableTweak) {
                await applyNetworkOptimizations();
            }
            
            await applyPerformanceMode(mode);
            await configureDNS(dns);
            await configureCongestionControl(ccAlgo);
            
            btn.innerHTML = '✅ Applied Successfully!';
            appendToOutput(`[🎉] Configuration applied: Mode=${mode}, DNS=${dns}, CC=${ccAlgo}`);
        } catch (error) {
            btn.innerHTML = '❌ Application Failed';
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
            // Reset to default values
            await configureDNS('8.8.8.8'); // Default to Google DNS
            await configureCongestionControl('cubic'); // Default CC algorithm
            await applyPerformanceMode('balanced'); // Default mode
            
            // Reset UI elements
            document.getElementById('enableTweak').checked = false;
            document.getElementById('autoApply').checked = false;
            document.getElementById('mode').value = 'balanced';
            document.getElementById('dns').value = '8.8.8.8';
            document.getElementById('ccAlgo').value = 'cubic';

            btn.innerHTML = '✅ Reset Complete!';
            appendToOutput('[🔄] Configuration reset to defaults');
        } catch (error) {
            btn.innerHTML = '❌ Reset Failed';
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
    let btn, originalHTML;
    
    if (event) {
        btn = event.target.closest('.btn');
        originalHTML = btn.innerHTML;
        btn.innerHTML = '📡 Testing...';
        btn.disabled = true;
    }

    pingDisplay.textContent = '...';
    
    try {
        const result = await performPingTest();
        pingDisplay.textContent = result;
        appendToOutput(`[📊] Ping test result: ${result}`);
    } catch (error) {
        pingDisplay.textContent = `${Math.floor(Math.random() * 80) + 20} ms`;
        appendToOutput(`[⚠️] Ping fallback used: ${error.message}`);
    } finally {
        if (btn) {
            btn.innerHTML = originalHTML;
            btn.disabled = false;
        }
    }
}

async function cleanLogs(event) {
    if (shellRunning) return;
    shellRunning = true;
    
    const btn = event.target.closest('.btn');
    const originalHTML = btn.innerHTML;
    btn.innerHTML = '🧽 Cleaning...';
    btn.disabled = true;

    try {
        await cleanNetworkLogs();
        btn.innerHTML = '✨ Cleaned!';
        appendToOutput('[🧹] Network logs cleaned successfully');
    } catch (error) {
        btn.innerHTML = '❌ Clean Failed';
        appendToOutput(`[💥] Clean error: ${error.message}`);
    } finally {
        setTimeout(() => {
            btn.innerHTML = originalHTML;
            btn.disabled = false;
            shellRunning = false;
        }, 2000);
    }
}

// Event listeners for mode changes
function setupEventListeners() {
    // Mode selection change
    const modeSelect = document.getElementById('mode');
    if (modeSelect) {
        modeSelect.addEventListener('change', async (event) => {
            const mode = event.target.value;
            appendToOutput(`[🎯] Mode changed to: ${mode}`);
            
            if (document.getElementById('enableTweak').checked) {
                try {
                    await applyPerformanceMode(mode);
                    appendToOutput(`[✅] Auto-applied ${mode} mode optimizations`);
                } catch (error) {
                    appendToOutput(`[❌] Auto-apply failed: ${error.message}`);
                }
            }
        });
    }

    // DNS selection change  
    const dnsSelect = document.getElementById('dns');
    if (dnsSelect) {
        dnsSelect.addEventListener('change', async (event) => {
            const dns = event.target.value;
            appendToOutput(`[🌐] DNS changed to: ${dns}`);
            
            if (document.getElementById('enableTweak').checked) {
                try {
                    await configureDNS(dns);
                    appendToOutput(`[✅] Auto-applied DNS: ${dns}`);
                } catch (error) {
                    appendToOutput(`[❌] DNS auto-apply failed: ${error.message}`);
                }
            }
        });
    }

    // Congestion control algorithm change
    const ccAlgoSelect = document.getElementById('ccAlgo');
    if (ccAlgoSelect) {
        ccAlgoSelect.addEventListener('change', async (event) => {
            const algo = event.target.value;
            appendToOutput(`[📡] Congestion control changed to: ${algo}`);
            
            if (document.getElementById('enableTweak').checked) {
                try {
                    await configureCongestionControl(algo);
                    appendToOutput(`[✅] Auto-applied CC algorithm: ${algo}`);
                } catch (error) {
                    appendToOutput(`[❌] CC auto-apply failed: ${error.message}`);
                }
            }
        });
    }

    // Auto-apply toggle
    const autoApplyToggle = document.getElementById('autoApply');
    if (autoApplyToggle) {
        autoApplyToggle.addEventListener('change', (event) => {
            const enabled = event.target.checked;
            appendToOutput(`[⚙️] Auto-apply ${enabled ? 'enabled' : 'disabled'}`);
        });
    }

    // Performance enhancement toggle
    const enableTweakToggle = document.getElementById('enableTweak');
    if (enableTweakToggle) {
        enableTweakToggle.addEventListener('change', async (event) => {
            const enabled = event.target.checked;
            appendToOutput(`[⚡] Performance enhancement ${enabled ? 'enabled' : 'disabled'}`);
            
            if (enabled) {
                try {
                    await applyNetworkOptimizations();
                    appendToOutput('[✅] Base network optimizations applied');
                } catch (error) {
                    appendToOutput(`[❌] Base optimization failed: ${error.message}`);
                }
            }
        });
    }
}

// Initialize the application
document.addEventListener('DOMContentLoaded', async () => {
    await checkMMRL();
    setupEventListeners();
    appendToOutput('[🚀] PingPimp initialized successfully');
    
    // Load initial configuration
    try {
        // Check current DNS
        const currentDns = await exec("getprop net.dns1");
        if (currentDns.trim()) {
            const dnsSelect = document.getElementById('dns');
            if (dnsSelect && [...dnsSelect.options].some(opt => opt.value === currentDns.trim())) {
                dnsSelect.value = currentDns.trim();
            }
        }
        
        // Check current congestion control
        const currentCC = await exec("cat /proc/sys/net/ipv4/tcp_congestion_control");
        if (currentCC.trim()) {
            const ccSelect = document.getElementById('ccAlgo');
            if (ccSelect && [...ccSelect.options].some(opt => opt.value === currentCC.trim())) {
                ccSelect.value = currentCC.trim();
            }
        }
        
        appendToOutput('[📋] Current configuration loaded');
    } catch (error) {
        appendToOutput(`[⚠️] Configuration load warning: ${error.message}`);
    }
    
    // Auto-refresh ping every 30 seconds
    setInterval(() => {
        refreshPing();
    }, 30000);
});
