import { exec as ksuExec, toast, listPackages, getPackagesInfo } from 'kernelsu';

// === FEATURE: i18n (INTERNATIONALIZATION) ===
let currentTranslations = {};

const defaultMessages = [
  "Blaming lag? Sounds like a massive skill issue tbh.",
  "Ping so low I'm literally predicting the future.",
  "My connection is officially more stable than my mental health.",
  "They think I'm scripting. Nah, just PingPimp doing its thing.",
  "Imagine lagging in 2026. Couldn't be me.",
  "I'd tell you to touch grass, but your ping is too high to render it.",
  "PingPimp on, brain off, still carrying this team.",
  "If only PingPimp could optimize my life choices...",
  "My ping is lower than my GPA right now. We take those.",
  "Powered by PingPimp. Your excuses are officially invalid.",
  "Lag? Sorry, I don't speak 'McDonalds Wi-Fi'.",
  "Running on pure caffeine and 0% packet loss.",
  "Enemies crying in all-chat is my favorite background music.",
  "Smooth connection, chaotic gameplay. Perfectly balanced.",
  "PingPimp: Turning script kiddies into absolute gods since day one."
];

async function loadLanguage(lang) {
  try {
    const response = await fetch(`assets/lang/${lang}.json`);
    if (!response.ok) throw new Error("Language file not found");
    currentTranslations = await response.json();
    
    localStorage.setItem('pingpimp_lang', lang);
    
    if (lang === 'ar') {
      document.documentElement.setAttribute('dir', 'rtl');
    } else {
      document.documentElement.setAttribute('dir', 'ltr');
    }

    applyTranslations();
  } catch (error) {
    console.error("Failed to load language:", lang, error);
    if (lang !== 'en') loadLanguage('en');
  }
}

function applyTranslations() {
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n');
    if (currentTranslations[key]) {
      el.innerHTML = currentTranslations[key]; 
    }
  });

  document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    const key = el.getAttribute('data-i18n-placeholder');
    if (currentTranslations[key]) {
      el.placeholder = currentTranslations[key];
    }
  });

  const currentLang = localStorage.getItem('pingpimp_lang') || 'en';
  document.querySelectorAll('.language-option').forEach(opt => {
    opt.classList.toggle('selected', opt.dataset.lang === currentLang);
  });

  rotateBannerMessage();
  loadPresetTweakOptions();
  loadPrivateDnsOptions();
  renderAppList('isolate');
  renderAppList('prioritize');
}

function initLanguageModal() {
  const btnLang = document.getElementById('btn-language');
  const closeLang = document.getElementById('closeLangDialog');
  const langDialog = document.getElementById('languageDialog');

  if (btnLang && langDialog) {
    btnLang.addEventListener('click', () => langDialog.classList.add('active'));
    closeLang.addEventListener('click', () => langDialog.classList.remove('active'));
    langDialog.addEventListener('click', (e) => {
      if (e.target === langDialog) langDialog.classList.remove('active');
    });

    document.querySelectorAll('.language-option').forEach(opt => {
      opt.addEventListener('click', () => {
        loadLanguage(opt.dataset.lang);
        langDialog.classList.remove('active');
      });
    });
  }
}

// === CONFIGURATION & HELPERS ===
function getRandomMessage() {
  const messagesArray = currentTranslations['bannerMessages'] || defaultMessages;
  return messagesArray[Math.floor(Math.random() * messagesArray.length)];
}

function rotateBannerMessage() {
  const el = document.getElementById("banner-message");
  if (el) el.textContent = getRandomMessage();
}

async function exec(command) {
  try {
    const { errno, stdout, stderr } = await ksuExec(command);
    if (errno !== 0) throw new Error(stderr || `Exit ${errno}`);
    return stdout.trim();
  } catch (err) {
    throw err;
  }
}

// === NATIVE SELECT INITIALIZATION ===
function initNativeSelect(selectId, options, onSelect) {
  const selectEl = document.getElementById(selectId);
  if (!selectEl) return;
  
  const newSelectEl = selectEl.cloneNode(false);
  let selectedValue = null;
  
  options.forEach(opt => {
    const optionEl = document.createElement("option");
    optionEl.value = opt.value;
    optionEl.textContent = opt.text;
    if (opt.selected) {
        selectedValue = opt.value;
    }
    newSelectEl.appendChild(optionEl);
  });

  selectEl.parentNode.replaceChild(newSelectEl, selectEl);
  
  if (selectedValue !== null) {
      newSelectEl.value = selectedValue;
  }
  
  newSelectEl.addEventListener("change", (e) => {
    const val = e.target.value;
    const text = e.target.options[e.target.selectedIndex].text;
    if (onSelect) onSelect(val, text);
  });
}

// === FEATURE: TCP ALGORITHM ===
async function loadTcpAlgorithms() {
  try {
    const available = (await exec("cat /proc/sys/net/ipv4/tcp_available_congestion_control 2>/dev/null")).trim();
    const current = (await exec("cat /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null")).trim();
    if (!available) {
      initNativeSelect("select-tcp", [{ text: "Not available", value: "" }], null);
      return;
    }
    const algos = available.split(/\s+/).filter(x => x);
    const options = algos.map(algo => ({ value: algo, text: algo, selected: algo === current }));
    
    initNativeSelect("select-tcp", options, async (value, text) => {
      if (value) {
        try {
            await exec(`echo "${value}" > /data/adb/modules/PingPimp/tcp.txt`);
            await exec(`echo "${value}" > /proc/sys/net/ipv4/tcp_congestion_control`);
            toast(`TCP Algorithm set to ${text}`);
        } catch (err) { toast("Failed to set TCP algorithm"); }
      }
    });
  } catch (err) { initNativeSelect("select-tcp", [{ text: "Error loading", value: "" }], null); }
}

// === FEATURE: PRIVATE DNS ===
const getDnsOptionsList = () => {
  const t = (key, fallback) => currentTranslations[key] || fallback;
  return [
    { value: 'default', text: t('dnsDefault', 'Default') },
    { value: 'custom', text: t('dnsCustom', 'Custom DNS') }, 
    { value: '360', text: '360 Secure DNS' },
    { value: 'adguard', text: 'AdGuard' },
    { value: 'adguard-family', text: 'AdGuard Family' },
    { value: 'adguard-unfiltered', text: 'AdGuard Unfiltered' },
    { value: 'aha-chicago', text: 'AhaDNS Chicago' },
    { value: 'aha-india', text: 'AhaDNS India' },
    { value: 'aha-italy', text: 'AhaDNS Italy' },
    { value: 'aha-losangeles', text: 'AhaDNS Los Angeles' },
    { value: 'aha-netherlands', text: 'AhaDNS Netherlands' },
    { value: 'aha-newyork', text: 'AhaDNS New York' },
    { value: 'aha-norway', text: 'AhaDNS Norway' },
    { value: 'aha-poland', text: 'AhaDNS Poland' },
    { value: 'aha-spain', text: 'AhaDNS Spain' },
    { value: 'ali', text: 'AliDNS' },
    { value: 'applied-privacy', text: 'Applied Privacy DNS' },
    { value: 'arapurayil', text: 'Arapurayil DNS' },
    { value: 'bitgeek', text: 'BitGeek DNS' },
    { value: 'bitwiseshift', text: 'BitwiseShift DNS' },
    { value: 'blah-finland', text: 'Blah DNS Finland' },
    { value: 'blah-germany', text: 'Blah DNS Germany' },
    { value: 'blah-japan', text: 'Blah DNS Japan' },
    { value: 'censurfridns-anycast', text: 'UncensoredDNS Anycast' },
    { value: 'censurfridns-unicast', text: 'UncensoredDNS Unicast' },
    { value: 'cfiec', text: 'CFIEC Public DNS' },
    { value: 'cira-private', text: 'CIRA Private' },
    { value: 'cira-protected', text: 'CIRA Protected' },
    { value: 'cleanbrowsing', text: 'CleanBrowsing' },
    { value: 'cleanbrowsing-adult', text: 'CleanBrowsing Adult' },
    { value: 'cleanbrowsing-family', text: 'CleanBrowsing Family' },
    { value: 'cleanbrowsing-security', text: 'CleanBrowsing Security' },
    { value: 'cloudflare-family', text: 'Cloudflare Family' },
    { value: 'cloudflare-security', text: 'Cloudflare Security' },
    { value: 'cloudflare-standard', text: 'Cloudflare Standard' },
    { value: 'cmrg', text: 'CMRG DNS' },
    { value: 'comss-east', text: 'Comss East DNS' },
    { value: 'comss-west', text: 'Comss West DNS' },
    { value: 'controld-ads', text: 'ControlD Block Ads' },
    { value: 'controld-malware', text: 'ControlD Block Malware' },
    { value: 'controld-nonfiltering', text: 'ControlD Non-filtering' },
    { value: 'controld-social', text: 'ControlD Block Social' },
    { value: 'cznic', text: 'CZ.NIC ODVR' },
    { value: 'dandelionsprout', text: 'Dandelion Sprout DNS' },
    { value: 'decloudus', text: 'DeCloudUs DNS' },
    { value: 'digitale-gesellschaft', text: 'Digitale Gesellschaft DNS' },
    { value: 'dnsforfamily', text: 'DNS For Family' },
    { value: 'dnsforge', text: 'DNS Forge' },
    { value: 'dnslify', text: 'DNSlify DNS' },
    { value: 'dnspod', text: 'DNSPod Public DNS+' },
    { value: 'dnsprivacy1', text: 'DNS Privacy AT 1' },
    { value: 'dnsprivacy2', text: 'DNS Privacy AT 2' },
    { value: 'ffmuc', text: 'FFMUC DNS' },
    { value: 'future', text: 'Future DNS' },
    { value: 'getdnsapi', text: 'Stubby DNS (getdnsapi.net)' },
    { value: 'go6lab', text: 'Go6Lab DNS' },
    { value: 'google', text: 'Google DNS' },
    { value: 'ibksturm', text: 'ibksturm DNS' },
    { value: 'iij', text: 'IIJ DNS' },
    { value: 'larsdebruin', text: 'Lars de Bruin DNS' },
    { value: 'lelux', text: 'Lelux DNS' },
    { value: 'libredns', text: 'LibreDNS' },
    { value: 'mullvad-adblock', text: 'Mullvad Ad Blocking' },
    { value: 'mullvad-nonfiltering', text: 'Mullvad Non-filtering' },
    { value: 'nextdns', text: 'NextDNS' },
    { value: 'nextdns-anycast', text: 'NextDNS Anycast' },
    { value: 'neutopia', text: 'Neutopia DNS' },
    { value: 'niccl', text: 'NIC CL DNS' },
    { value: 'oarc', text: 'OARC DNS' },
    { value: 'OpenBLD', text: 'OpenBLD DNS' },
    { value: 'opendns', text: 'OpenDNS' },
    { value: 'oszx', text: 'OSZX DNS' },
    { value: 'privacy-japan', text: 'TiarapDNS Japan' },
    { value: 'privacy-singapore', text: 'TiarapDNS Singapore' },
    { value: 'pumplex', text: 'PumpleX DNS' },
    { value: 'quad9-ecs', text: 'Quad9 ECS' },
    { value: 'quad9-standard', text: 'Quad9 Standard' },
    { value: 'quad9-unsecured', text: 'Quad9 Unsecured' },
    { value: 'redfish', text: 'Redfish DNS' },
    { value: 'restena', text: 'Fondation Restena DNS' },
    { value: 'rethink-nonfiltering', text: 'RethinkDNS Non-filtering' },
    { value: 'seby', text: 'SebyDNS' },
    { value: 'securedns', text: 'SecureDNS EU' },
    { value: 'sinodun', text: 'dnsovertls.sinodun.com' },
    { value: 'sinodun1', text: 'dnsovertls1.sinodun.com' },
    { value: 'snopyta', text: 'Snopyta DNS' },
    { value: 'switch', text: 'Switch DNS' }
  ];
};

const dnsMap = {
  'default': '', 'custom': '', 
  'cloudflare-standard': 'one.one.one.one',
  'cloudflare-family': 'family.cloudflare-dns.com',
  'cloudflare-security': 'security.cloudflare-dns.com',
  'google': 'dns.google',
  'quad9-standard': 'dns.quad9.net',
  'quad9-unsecured': 'dns10.quad9.net',
  'quad9-ecs': 'dns11.quad9.net',
  'cleanbrowsing': 'doh.cleanbrowsing.org',
  'cleanbrowsing-family': 'family-filter-dns.cleanbrowsing.org',
  'cleanbrowsing-adult': 'adult-filter-dns.cleanbrowsing.org',
  'cleanbrowsing-security': 'security-filter-dns.cleanbrowsing.org',
  'nextdns': 'dns.nextdns.io',
  'nextdns-anycast': 'anycast.dns.nextdns.io',
  'adguard': 'dns.adguard-dns.com',
  'adguard-family': 'family.adguard-dns.com',
  'adguard-unfiltered': 'unfiltered.adguard-dns.com',
  'opendns': 'dns.opendns.com',
  'redfish': 'dns.rubyfish.cn',
  'switch': 'dns.switch.ch',
  'future': 'dns.futuredns.me',
  'comss-west': 'dns.comss.one',
  'comss-east': 'dns.east.comss.one',
  'cira-private': 'family.canadianshield.cira.ca',
  'cira-protected': 'protected.canadianshield.cira.ca',
  'blah-finland': 'dot-fi.blahdns.com',
  'blah-japan': 'dot-jp.blahdns.com',
  'blah-germany': 'dot-de.blahdns.com',
  'snopyta': 'fi.dot.dns.snopyta.org',
  'dnsforfamily': 'dns-dot.dnsforfamily.com',
  'cznic': 'odvr.nic.cz',
  'ali': 'dns.alidns.com',
  'cfiec': 'dns.cfiec.net',
  '360': 'dot.360.cn',
  'iij': 'public.dns.iij.jp',
  'dnspod': 'dot.pub',
  'privacy-singapore': 'dot.tiarap.org',
  'privacy-japan': 'jp.tiar.app',
  'oszx': 'dns.oszx.co',
  'pumplex': 'dns.pumplex.com',
  'applied-privacy': 'dot1.applied-privacy.net',
  'decloudus': 'dns.decloudus.com',
  'lelux': 'resolver-eu.lelux.fi',
  'dnsforge': 'dnsforge.de',
  'restena': 'kaitain.restena.lu',
  'ffmuc': 'dot.ffmuc.net',
  'digitale-gesellschaft': 'dns.digitale-gesellschaft.ch',
  'libredns': 'dot.libredns.gr',
  'ibksturm': 'ibksturm.synology.me',
  'getdnsapi': 'getdnsapi.net',
  'sinodun': 'dnsovertls.sinodun.com',
  'sinodun1': 'dnsovertls1.sinodun.com',
  'censurfridns-unicast': 'unicast.censurfridns.dk',
  'censurfridns-anycast': 'anycast.censurfridns.dk',
  'cmrg': 'dns.cmrg.net',
  'larsdebruin': 'dns.larsdebruin.net',
  'bitwiseshift': 'dns-tls.bitwiseshift.net',
  'dnsprivacy1': 'ns1.dnsprivacy.at',
  'dnsprivacy2': 'ns2.dnsprivacy.at',
  'bitgeek': 'dns.bitgeek.in',
  'neutopia': 'dns.neutopia.org',
  'go6lab': 'privacydns.go6lab.si',
  'securedns': 'dot.securedns.eu',
  'niccl': 'dnsotls.lab.nic.cl',
  'oarc': 'tls-dns-u.odvr.dns-oarc.net',
  'aha-netherlands': 'dot.nl.ahadns.net',
  'aha-india': 'dot.in.ahadns.net',
  'aha-losangeles': 'dot.la.ahadns.net',
  'aha-newyork': 'dot.ny.ahadns.net',
  'aha-poland': 'dot.pl.ahadns.net',
  'aha-italy': 'dot.it.ahadns.net',
  'aha-spain': 'dot.es.ahadns.net',
  'aha-norway': 'dot.no.ahadns.net',
  'aha-chicago': 'dot.chi.ahadns.net',
  'seby': 'dot.seby.io',
  'dnslify': 'doh.dnslify.com',
  'rethink-nonfiltering': 'max.rethinkdns.com',
  'controld-nonfiltering': 'p0.freedns.controld.com',
  'controld-malware': 'p1.freedns.controld.com',
  'controld-ads': 'p2.freedns.controld.com',
  'controld-social': 'p3.freedns.controld.com',
  'mullvad-nonfiltering': 'doh.mullvad.net',
  'mullvad-adblock': 'adblock.doh.mullvad.net',
  'arapurayil': 'dns.arapurayil.com',
  'OpenBLD': 'ric.openbld.net',
  'dandelionsprout': 'dandelionsprout.asuscomm.com'
};

function loadPrivateDnsOptions() {
  const savedValue = localStorage.getItem('pingpimp_dns') || 'default';
  const customContainer = document.getElementById('custom-dns-container');
  const customInput = document.getElementById('custom-dns-input');
  
  if (savedValue === 'custom') {
    customContainer.style.display = 'block';
    customInput.value = localStorage.getItem('pingpimp_custom_dns') || '';
  }

  const options = getDnsOptionsList().map(opt => ({
    ...opt,
    selected: opt.value === savedValue
  }));
  
  initNativeSelect("select-dns", options, async (value, text) => {
    localStorage.setItem('pingpimp_dns', value);
    
    if (value === 'custom') {
      customContainer.style.display = 'block';
      const savedCustom = localStorage.getItem('pingpimp_custom_dns') || '';
      customInput.value = savedCustom;
      if (savedCustom) {
        try {
          await exec(`settings put global private_dns_mode hostname`);
          await exec(`settings put global private_dns_specifier ${savedCustom}`);
          toast(`DNS set to Custom: ${savedCustom}`);
        } catch (e) { toast("Failed to set DNS"); }
      } else {
        toast("Please enter your custom DNS provider");
      }
    } else {
      customContainer.style.display = 'none';
      const dotName = dnsMap[value] || '';
      try {
        if (value === 'default') {
          await exec(`settings delete global private_dns_mode`);
          await exec(`settings delete global private_dns_specifier`);
        } else {
          await exec(`settings put global private_dns_mode hostname`);
          await exec(`settings put global private_dns_specifier ${dotName}`);
        }
        toast(`Private DNS set to ${text}`);
      } catch (e) {
        toast("Failed to set DNS");
      }
    }
  });

  document.getElementById('btn-save-custom-dns').addEventListener('click', async () => {
    const customDot = customInput.value.trim();
    if (!customDot) return toast("DNS address cannot be empty!");
    
    localStorage.setItem('pingpimp_custom_dns', customDot);
    try {
      await exec(`settings put global private_dns_mode hostname`);
      await exec(`settings put global private_dns_specifier ${customDot}`);
      toast(`Custom DNS Applied!`);
    } catch (e) { toast("Failed to apply custom DNS"); }
  });
}

// === FEATURE: PRESETS ===
const getPresetOptions = () => {
  const t = (key, fallback) => currentTranslations[key] || fallback;
  return [
    { value: 'default', text: t('presetDefault', 'Default') }, 
    { value: 'game', text: t('presetGame', 'Game') },
    { value: 'download', text: t('presetDownload', 'Download') }, 
    { value: 'streaming', text: t('presetStreaming', 'Streaming') },
    { value: 'social media', text: t('presetSocialMedia', 'Social Media') }, 
    { value: 'browsing', text: t('presetBrowsing', 'Browsing') },
    { value: 'outdoor', text: t('presetOutdoor', 'Outdoor') }
  ];
};

async function loadPresetTweakOptions() {
  let currentPreset = 'default';
  try {
    const content = await exec("cat /data/adb/modules/PingPimp/preset.txt 2>/dev/null");
    if(content && content.trim() !== '') {
        currentPreset = content.trim().toLowerCase();
    }
  } catch (e) {}
  
  const options = getPresetOptions().map(opt => ({ ...opt, selected: opt.value.toLowerCase() === currentPreset }));
  
  initNativeSelect("select-preset", options, async (value, text) => {
    try {
      await exec(`echo "${value}" > /data/adb/modules/PingPimp/preset.txt`);
      let cmdFlag = value.toLowerCase().replace(/[^a-z0-9]/g, '');
      if (value === 'social media') cmdFlag = 'social';
      await exec(`PingPimp --${cmdFlag}`);
      toast(`Preset tweaked for ${text}`);
    } catch (err) { toast("Failed to apply preset tweak"); }
  });
}

// === FEATURE: SWITCHES ===
async function initSwitch(id, file, flagOn, flagOff, name) {
  const switchEl = document.getElementById(id);
  if (!switchEl) return;
  try {
    const content = await exec(`cat /data/adb/modules/PingPimp/${file} 2>/dev/null`);
    switchEl.checked = content.trim() === "1";
  } catch (e) { switchEl.checked = false; }
  
  switchEl.onchange = async () => {
    const val = switchEl.checked ? "1" : "0";
    const cmd = switchEl.checked ? `PingPimp --${flagOn}` : `PingPimp --${flagOff}`;
    try {
      await exec(`echo "${val}" > /data/adb/modules/PingPimp/${file}`);
      await exec(cmd);
      toast(`${name} ${switchEl.checked ? "Enabled" : "Disabled"}`);
    } catch (err) {
      toast(`Failed to toggle ${name}`);
      switchEl.checked = !switchEl.checked;
    }
  };
}

function startPingMonitor() {
  const pingEl = document.getElementById("live-ping");
  if (!pingEl) return;
  async function checkPing() {
    try {
      const out = await exec("ping -c 1 -W 1 1.1.1.1");
      const match = out.match(/time=([\d.]+)\s*ms/);
      if (match && match[1]) {
        const pingValue = parseFloat(match[1]);
        pingEl.textContent = `${pingValue} ms`;
        if (pingValue < 60) pingEl.style.color = "#69f0ae";
        else if (pingValue < 120) pingEl.style.color = "#ffd740";
        else pingEl.style.color = "#ff5252";
      } else { pingEl.textContent = "Timeout"; pingEl.style.color = "#ff5252"; }
    } catch (e) { pingEl.textContent = "Offline"; pingEl.style.color = "#ff5252"; }
    setTimeout(checkPing, 2000);
  }
  checkPing();
}

async function updateDeviceInfo() {
  const setTxt = (id, val) => document.getElementById(id).textContent = val || "-";
  try {
    const ver = await exec("grep '^version=' /data/adb/modules/PingPimp/module.prop 2>/dev/null || echo 'version=Unknown'");
    setTxt("PingPimpVer", ver.replace("version=", "").trim());
    setTxt("device-kernel", (await exec("uname -r")).trim());
    setTxt("device-model", (await exec("getprop ro.product.model")).trim());
    setTxt("device-android", (await exec("getprop ro.build.version.release")).trim());
    setTxt("device-chipset", (await exec("getprop ro.board.platform")).trim());
    setTxt("device-abis", (await exec("getprop ro.product.cpu.abilist")).trim());
  } catch (e) { console.warn("Info fetch error", e); }
}

let isolatedApps = new Set();
let prioritizedApps = new Set();
let cachedUserApps = [];
let cachedSystemApps = []; 
let currentIsolateView = 'user'; 
let currentPrioritizeView = 'user'; 

// === APP DETECTION ===
async function fetchUserPackagesInfo() {
  try {
    const allPkgsList = await listPackages(); 
    let fullInfo = [];
    
    if (allPkgsList && allPkgsList.length > 0) {
        fullInfo = await getPackagesInfo(allPkgsList);
    }

    const t = await exec("pm list packages -3");
    let thirdPartyPkgs = new Set();
    if (t) {
        t.split("\n").forEach(e => {
            const pkg = e.replace("package:", "").trim();
            if (pkg) thirdPartyPkgs.add(pkg);
        });
    }

    cachedUserApps = [];
    cachedSystemApps = [];

    fullInfo.forEach(pkg => {
        const formattedPkg = {
            packageName: pkg.packageName,
            appLabel: pkg.appLabel || pkg.label || pkg.appName || pkg.packageName,
            uid: pkg.uid
        };
        
        if (thirdPartyPkgs.has(pkg.packageName)) {
            cachedUserApps.push(formattedPkg);
        } else {
            cachedSystemApps.push(formattedPkg);
        }
    });

  } catch (err) {
    console.error("Failed to fetch packages:", err);
    toast("Failed to load application list");
  }
}

function loadAppConfigs() {
    const savedIso = localStorage.getItem('pingpimp_isolated_apps');
    if (savedIso) savedIso.split(',').filter(x=>x).forEach(pkg => isolatedApps.add(pkg));

    const savedPrio = localStorage.getItem('pingpimp_prioritized_apps');
    if (savedPrio) savedPrio.split(',').filter(x=>x).forEach(pkg => prioritizedApps.add(pkg));
}

function renderAppList(type) {
  const containerId = type === 'isolate' ? 'isolate-list-container' : 'prioritize-list-container';
  const searchId = type === 'isolate' ? 'isolate-search' : 'prioritize-search';
  const activeSet = type === 'isolate' ? isolatedApps : prioritizedApps;
  
  const container = document.getElementById(containerId);
  const searchInput = document.getElementById(searchId);
  if (!container || !searchInput) return;

  const query = searchInput.value.toLowerCase().trim();
  
  let sourceArray = cachedUserApps;
  if (type === 'isolate' && currentIsolateView === 'system') {
      sourceArray = cachedSystemApps;
  } else if (type === 'prioritize' && currentPrioritizeView === 'system') {
      sourceArray = cachedSystemApps; 
  }
  
  let filtered = sourceArray.filter(pkg => {
     const name = (pkg.appLabel || "").toLowerCase();
     const pkgId = pkg.packageName.toLowerCase();
     return name.includes(query) || pkgId.includes(query);
  });

  filtered.sort((a, b) => {
      const aActive = activeSet.has(a.packageName);
      const bActive = activeSet.has(b.packageName);
      
      if (aActive && !bActive) return -1;
      if (!aActive && bActive) return 1;
      return (a.appLabel || a.packageName).localeCompare(b.appLabel || b.packageName);
  });

  container.innerHTML = "";
  
  if (filtered.length === 0) {
      const noAppsText = currentTranslations['noAppsFound'] || 'No apps found';
      container.innerHTML = `<div style="text-align:center;padding:20px;color:var(--app-on-surface);opacity:0.4;">${noAppsText}</div>`;
      return;
  }

  const fragment = document.createDocumentFragment();

  filtered.forEach(pkg => {
    const isSelected = activeSet.has(pkg.packageName);
    
    const item = document.createElement('div');
    item.className = 'isolate-app-item';

    const wrapper = document.createElement('div');
    wrapper.className = 'isolate-content-wrapper';

    const img = document.createElement('img');
    img.className = 'isolate-app-icon';
    img.src = `ksu://icon/${pkg.packageName}`; 
    img.onerror = () => {
        img.style.display = 'none';
        const fallback = document.createElement('div');
        fallback.className = 'isolate-app-icon-placeholder';
        // Menggunakan font offline untuk icon aplikasi cadangan
        fallback.innerHTML = '<span class="material-symbols-outlined">android</span>';
        wrapper.prepend(fallback);
    };

    const textGroup = document.createElement('div');
    textGroup.className = 'isolate-text-group';

    const label = document.createElement('div');
    label.className = 'isolate-app-label';
    label.textContent = pkg.appLabel || pkg.packageName; 
    if (isSelected) label.style.color = "var(--app-primary)";

    const subLabel = document.createElement('div');
    subLabel.className = 'isolate-app-pkg';
    subLabel.textContent = pkg.packageName;

    textGroup.appendChild(label);
    textGroup.appendChild(subLabel);

    wrapper.appendChild(img);
    wrapper.appendChild(textGroup);

    const switchLabel = document.createElement('label');
    switchLabel.className = 'custom-switch';
    
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = isSelected;
    
    const slider = document.createElement('span');
    slider.className = 'switch-slider';

    switchLabel.appendChild(input);
    switchLabel.appendChild(slider);

    input.addEventListener('change', async () => {
        const uid = pkg.uid; 
        const isChecked = input.checked;
        
        if (type === 'isolate') {
            if (isChecked) {
                if (uid) {
                    try {
                        await exec(`iptables -I OUTPUT -m owner --uid-owner ${uid} -j REJECT`);
                        await exec(`ip6tables -I OUTPUT -m owner --uid-owner ${uid} -j REJECT`);
                    } catch(e) {}
                }
                isolatedApps.add(pkg.packageName);
                toast(`Isolated: ${pkg.appLabel}`);
            } else {
                if (uid) {
                    try {
                        await exec(`iptables -D OUTPUT -m owner --uid-owner ${uid} -j REJECT 2>/dev/null || true`);
                        await exec(`ip6tables -D OUTPUT -m owner --uid-owner ${uid} -j REJECT 2>/dev/null || true`);
                    } catch(e) {}
                }
                isolatedApps.delete(pkg.packageName);
                toast(`Restored: ${pkg.appLabel}`);
            }
            localStorage.setItem('pingpimp_isolated_apps', Array.from(isolatedApps).join(','));
        } 
        else if (type === 'prioritize') {
            if (isChecked) {
                if (uid) {
                    try {
                        await exec(`iptables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46`);
                        await exec(`ip6tables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46`);
                    } catch(e) { 
                        try {
                            await exec(`iptables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j TOS --set-tos 0x10`);
                            await exec(`ip6tables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j TOS --set-tos 0x10`);
                        } catch(fallbackErr){}
                    }
                }
                prioritizedApps.add(pkg.packageName);
                toast(`Boosted: ${pkg.appLabel}`);
            } else {
                if (uid) {
                    try {
                        await exec(`iptables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46 2>/dev/null || true`);
                        await exec(`ip6tables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46 2>/dev/null || true`);
                        await exec(`iptables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j TOS --set-tos 0x10 2>/dev/null || true`);
                        await exec(`ip6tables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j TOS --set-tos 0x10 2>/dev/null || true`);
                    } catch(e) {}
                }
                prioritizedApps.delete(pkg.packageName);
                toast(`Normal: ${pkg.appLabel}`);
            }
            localStorage.setItem('pingpimp_prioritized_apps', Array.from(prioritizedApps).join(','));
        }
        renderAppList(type);
    });

    item.appendChild(wrapper);
    item.appendChild(switchLabel);
    fragment.appendChild(item);
  });

  container.appendChild(fragment);
}

function showTab(tabId) {
  document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
  document.querySelectorAll('.nav-button').forEach(el => el.classList.remove('active'));
  
  document.getElementById(`tab-${tabId}`).classList.add('active');
  document.querySelector(`.nav-button[data-target="${tabId}"]`).classList.add('active');
}

// === INIT ===
document.addEventListener("DOMContentLoaded", async () => {
  const loadingOverlay = document.getElementById('loading-overlay');
  
  initLanguageModal();
  const savedLang = localStorage.getItem('pingpimp_lang') || 'en';
  await loadLanguage(savedLang);

  await Promise.all([
    updateDeviceInfo(),
    loadPresetTweakOptions(),
    loadTcpAlgorithms(),
    loadPrivateDnsOptions(),
    initSwitch("switch-netstate", "state.txt", "state", "unstate", "Network State"),
    initSwitch("switch-saver", "saver.txt", "saver", "unsaver", "Data Saver"),
    initSwitch("switch-ipv6", "ipv6_state.txt", "disable", "enable", "Disable IPv6"), 
    loadAppConfigs(),
    fetchUserPackagesInfo()
  ]);
  
  startPingMonitor();
  rotateBannerMessage();
  setInterval(rotateBannerMessage, 7000);
  
  document.querySelectorAll('.nav-button').forEach(item => {
    item.addEventListener('click', () => showTab(item.dataset.target));
  });
  
  const btnUserIso = document.getElementById('btn-user-apps-isolate');
  const btnSysIso = document.getElementById('btn-system-apps-isolate');
  
  if (btnUserIso && btnSysIso) {
    btnUserIso.addEventListener('click', () => {
      currentIsolateView = 'user';
      btnUserIso.classList.add('active');
      btnSysIso.classList.remove('active');
      renderAppList('isolate');
    });
    
    btnSysIso.addEventListener('click', () => {
      currentIsolateView = 'system';
      btnSysIso.classList.add('active');
      btnUserIso.classList.remove('active');
      renderAppList('isolate');
    });
  }

  const btnUserPrio = document.getElementById('btn-user-apps-prioritize');
  const btnSysPrio = document.getElementById('btn-system-apps-prioritize');
  
  if (btnUserPrio && btnSysPrio) {
    btnUserPrio.addEventListener('click', () => {
      currentPrioritizeView = 'user';
      btnUserPrio.classList.add('active');
      btnSysPrio.classList.remove('active');
      renderAppList('prioritize');
    });
    
    btnSysPrio.addEventListener('click', () => {
      currentPrioritizeView = 'system';
      btnSysPrio.classList.add('active');
      btnUserPrio.classList.remove('active');
      renderAppList('prioritize');
    });
  }

  renderAppList('isolate');
  renderAppList('prioritize');

  let searchTimeoutIso;
  document.getElementById("isolate-search").addEventListener("input", () => {
    clearTimeout(searchTimeoutIso);
    searchTimeoutIso = setTimeout(() => { renderAppList('isolate'); }, 300);
  });

  let searchTimeoutPrio;
  document.getElementById("prioritize-search").addEventListener("input", () => {
    clearTimeout(searchTimeoutPrio);
    searchTimeoutPrio = setTimeout(() => { renderAppList('prioritize'); }, 300);
  });

  loadingOverlay.style.opacity = "0";
  setTimeout(() => { if (loadingOverlay.parentNode) loadingOverlay.remove(); }, 300);
});