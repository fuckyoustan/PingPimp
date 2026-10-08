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

// ===  ICON ENGINE ===
const MIUIX_ICONS = {
  'Stacked_Line_Chart': '<path d="M3 17.5 8.5 12l3.5 3.5L21 6.5"/><path d="M14.5 6.5H21V13"/>',
  'home': '<path d="M4 10.8 12 4l8 6.8"/><path d="M6 9.5V20h12V9.5"/><path d="M10 20v-5.5h4V20"/>',
  'tune': '<path d="M4 21v-7M4 10V3M12 21v-9M12 8V3M20 21v-5M20 12V3M1.5 14h5M9.5 8h5M17.5 16h5"/>',
  'shield': '<path d="M12 3.2 19.5 6v5.3c0 4.8-3.2 8.1-7.5 9.5-4.3-1.4-7.5-4.7-7.5-9.5V6z"/>',
  'rocket_launch': '<path d="M12 2.8c2.7 1.8 4.3 4.9 4.3 8.2l2.2 3.4-3.5-.8c-.8 1.2-1.8 2-3 2s-2.2-.8-3-2l-3.5.8 2.2-3.4c0-3.3 1.6-6.4 4.3-8.2z"/><circle cx="12" cy="9.3" r="1.5"/><path d="M8.7 14.9 7.3 20l4.7-1.9 4.7 1.9-1.4-5.1"/>',
  'settings': '<circle cx="12" cy="12" r="3.2"/><path d="M19.4 15a1.7 1.7 0 0 0 .34 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.7 1.7 0 0 0-1.82-.34 1.7 1.7 0 0 0-1.03 1.55V21a2 2 0 1 1-4 0v-.09a1.7 1.7 0 0 0-1.1-1.55 1.7 1.7 0 0 0-1.81.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.7 1.7 0 0 0 .34-1.82 1.7 1.7 0 0 0-1.55-1.03H3a2 2 0 1 1 0-4h.09a1.7 1.7 0 0 0 1.55-1.1 1.7 1.7 0 0 0-.34-1.81l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.7 1.7 0 0 0 1.82.34h.01a1.7 1.7 0 0 0 1.03-1.55V3a2 2 0 1 1 4 0v.09a1.7 1.7 0 0 0 1.03 1.55h.01a1.7 1.7 0 0 0 1.82-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.7 1.7 0 0 0-.34 1.82v.01a1.7 1.7 0 0 0 1.55 1.03H21a2 2 0 1 1 0 4h-.09a1.7 1.7 0 0 0-1.55 1.03z"/>',
  'stars': '<path d="m12 3 2.9 5.9 6.5.95-4.7 4.6 1.1 6.5L12 17.9l-5.8 3.05 1.1-6.5-4.7-4.6 6.5-.95z"/>',
  'smartphone': '<rect x="7" y="2.6" width="10" height="18.8" rx="2.6"/><path d="M10.8 18.6h2.4"/>',
  'android': '<path d="M5.2 11.2a6.8 6.8 0 0 1 13.6 0"/><path d="M8.2 8.4 6.6 5.7M15.8 8.4l1.6-2.7"/><path d="M5.2 11.2v6.3a1.8 1.8 0 0 0 1.8 1.8h9.9a1.8 1.8 0 0 0 1.8-1.8v-6.3"/><circle cx="9.7" cy="12.6" r=".55" fill="currentColor" stroke="none"/><circle cx="14.3" cy="12.6" r=".55" fill="currentColor" stroke="none"/><path d="M9.7 19.3v-1.8M14.3 19.3v-1.8"/>',
  'memory': '<rect x="6.2" y="6.2" width="11.6" height="11.6" rx="2"/><rect x="9.7" y="9.7" width="4.6" height="4.6" rx="1"/><path d="M9.2 3.2v3M14.8 3.2v3M9.2 17.8v3M14.8 17.8v3M3.2 9.2h3M3.2 14.8h3M17.8 9.2h3M17.8 14.8h3"/>',
  'laptop_mac': '<rect x="4.2" y="4.6" width="15.6" height="10.8" rx="1.8"/><path d="M2.4 19.4h19.2"/>',
  'code': '<path d="m8.6 7-4.6 5 4.6 5M15.4 7l4.6 5-4.6 5"/>',
  'send': '<path d="M21 3.4 10.8 13.6"/><path d="m21 3.4-6.4 17.2-3.8-7.2-7.2-3.8z"/>',
  'smart_toy': '<rect x="4.8" y="7.8" width="14.4" height="11.4" rx="3"/><circle cx="9.6" cy="13" r=".6" fill="currentColor" stroke="none"/><circle cx="14.4" cy="13" r=".6" fill="currentColor" stroke="none"/><path d="M12 7.8V4.4"/><circle cx="12" cy="3.4" r="1"/><path d="M9.8 16.4h4.4"/>',
  'signal_cellular_alt': '<path d="M4.6 19.4h14.8L4.6 4.6z"/>',
  'wifi': '<path d="M3 9.4a14.5 14.5 0 0 1 18 0M5.9 12.8a9.6 9.6 0 0 1 12.2 0M8.8 16.1a4.9 4.9 0 0 1 6.4 0"/><circle cx="12" cy="19.3" r="1.1" fill="currentColor" stroke="none"/>',
  'network_node': '<path d="m12 3.4 8.6 4.8L12 13 3.4 8.2z"/><path d="m3.4 12.6 8.6 4.8 8.6-4.8"/><path d="m3.4 16.6 8.6 4.8 8.6-4.8"/>',
  'dns': '<rect x="3.6" y="4.2" width="16.8" height="15.6" rx="2.6"/><circle cx="7.9" cy="8.6" r=".6" fill="currentColor" stroke="none"/><path d="M11.6 8.6h4.5"/><circle cx="7.9" cy="15.4" r=".6" fill="currentColor" stroke="none"/><path d="M11.6 15.4h4.5"/>',
  'network_manage': '<path d="M9 3.2v4.6M15 3.2v4.6"/><path d="M6.8 7.8h10.4v3.9a5.2 5.2 0 0 1-10.4 0z"/><path d="M12 16.9v3.9"/>',
  'memory_alt': '<path d="M3 12.2h3.8L9.6 5.2l4.8 14 2.8-7H21"/>',
  'speed': '<path d="M5 18.6a8.6 8.6 0 1 1 14 0"/><path d="m12 14.2 3.6-4.6"/><circle cx="12" cy="14.2" r="1.3"/>',
  'bolt': '<path d="M13.2 2.8 4.8 13.4h6l-1 7.8 8.4-10.6h-6z"/>',
  'vpn_lock': '<rect x="5.4" y="10.4" width="13.2" height="9.6" rx="2.4"/><path d="M8.4 10.4V7.6a3.6 3.6 0 0 1 7.2 0v2.8"/><circle cx="12" cy="15.2" r=".8"/>',
  'data_saver_on': '<circle cx="12" cy="12" r="8.4"/><path d="M12 7.8v8.4M7.8 12h8.4"/>',
  'style': '<path d="M6.2 17.8 17.8 6.2"/><path d="m14.5 4.5.8 1.9 1.9.8-1.9.8-.8 1.9-.8-1.9-1.9-.8 1.9-.8z"/><path d="m5.5 3 .6 1.4 1.4.6-1.4.6-.6 1.4-.6-1.4L3.5 5l1.4-.6z"/><path d="m18.5 15 .6 1.4 1.4.6-1.4.6-.6 1.4-.6-1.4-1.4-.6 1.4-.6z"/>',
  'bottom_navigation': '<path d="M4 11V6a2.4 2.4 0 0 1 2.4-2.4h11.2A2.4 2.4 0 0 1 20 6v5"/><rect x="3" y="11" width="18" height="9" rx="2.8"/><circle cx="7.6" cy="15.5" r=".7" fill="currentColor" stroke="none"/><circle cx="12" cy="15.5" r=".7" fill="currentColor" stroke="none"/><circle cx="16.4" cy="15.5" r=".7" fill="currentColor" stroke="none"/>',
  'palette': '<path d="M12 3.2a8.8 8.8 0 0 0 0 17.6c1.4 0 2-.8 2-1.8s-.7-1.6-.7-2.6c0-1 .8-1.8 2-1.8h1.9A3.6 3.6 0 0 0 20.8 11c0-4.3-3.9-7.8-8.8-7.8z"/><circle cx="7.6" cy="11.4" r=".7" fill="currentColor" stroke="none"/><circle cx="10.4" cy="7.6" r=".7" fill="currentColor" stroke="none"/><circle cx="14.8" cy="8.4" r=".7" fill="currentColor" stroke="none"/>',
  'translate': '<path d="M3.2 5.6h8.6M7.3 4v1.6"/><path d="M9.5 5.6c-.8 4-3.4 7.4-6.3 9.4"/><path d="M5.5 9.4c1.6 2.7 4 4.6 6.4 5.6"/><path d="m12.8 20.4 4.2-11 4.2 11"/><path d="M14.5 16.8h5"/>',
  'save': '<path d="M12 3.6v10.8"/><path d="m7.6 10.6 4.4 4.4 4.4-4.4"/><path d="M4.6 16.4v2a2.2 2.2 0 0 0 2.2 2.2h10.4a2.2 2.2 0 0 0 2.2-2.2v-2"/>',
  'chevron_right': '<path d="m9.6 5.4 6.6 6.6-6.6 6.6"/>',
  'expand_more': '<path d="m6 9.6 6 6 6-6"/>',
  'close': '<path d="m6.4 6.4 11.2 11.2M17.6 6.4 6.4 17.6"/>',
  'search': '<circle cx="10.8" cy="10.8" r="6.6"/><path d="m15.7 15.7 4.6 4.6"/>',
  'check': '<path d="m5.2 12.6 4.4 4.4 9.2-9.6"/>',
  'wallpaper': '<rect x="4.2" y="4.2" width="15.6" height="11.4" rx="2"/><path d="M7.6 4.2v4.4M16.4 4.2v4.4"/><rect x="4.2" y="8.6" width="15.6" height="7" rx="1.6"/><circle cx="9" cy="11.6" r="1" fill="currentColor" stroke="none"/><path d="m12.8 15.6 2.6-3 2.6 3z"/>',
  'upload': '<path d="M12 16V5.6"/><path d="m7.6 9 4.4-4.4L16.4 9"/><path d="M4.6 16.4v2a2.2 2.2 0 0 0 2.2 2.2h10.4a2.2 2.2 0 0 0 2.2-2.2v-2"/>',
  'delete': '<path d="M5.4 7.6h13.2M9.4 7.6V5.8a1.4 1.4 0 0 1 1.4-1.4h2.4a1.4 1.4 0 0 1 1.4 1.4v1.8M7 7.6l.8 11a1.8 1.8 0 0 0 1.8 1.6h4.8a1.8 1.8 0 0 0 1.8-1.6l.8-11"/><path d="M10.2 11.2v5.2M13.8 11.2v5.2"/>',
  'lan': '<rect x="9" y="3.4" width="6" height="5" rx="1.2"/><rect x="3.4" y="15.6" width="6" height="5" rx="1.2"/><rect x="14.6" y="15.6" width="6" height="5" rx="1.2"/><path d="M12 8.4v3.4M6.4 15.6v-1.6a1 1 0 0 1 1-1h9.2a1 1 0 0 1 1 1v1.6"/>',
  'wifi_tethering': '<circle cx="12" cy="16" r="1.6" fill="currentColor" stroke="none"/><path d="M9 13a4.2 4.2 0 0 1 6 0M6.6 10.6a7.6 7.6 0 0 1 10.8 0M4.2 8.2a11 11 0 0 1 15.6 0"/>',
  'check_circle': '<circle cx="12" cy="12" r="8.6"/><path d="m8.4 12.4 2.6 2.6 4.8-5.2"/>',
  'blur_on': '<circle cx="7" cy="7" r="1.3" fill="currentColor" stroke="none"/><circle cx="12" cy="4.8" r="1.3" fill="currentColor" stroke="none"/><circle cx="17" cy="7" r="1.3" fill="currentColor" stroke="none"/><circle cx="4.8" cy="12" r="1.3" fill="currentColor" stroke="none"/><circle cx="12" cy="12" r="1.3" fill="currentColor" stroke="none"/><circle cx="19.2" cy="12" r="1.3" fill="currentColor" stroke="none"/><circle cx="7" cy="17" r="1.3" fill="currentColor" stroke="none"/><circle cx="12" cy="19.2" r="1.3" fill="currentColor" stroke="none"/><circle cx="17" cy="17" r="1.3" fill="currentColor" stroke="none"/><circle cx="9" cy="9" r="1" fill="currentColor" stroke="none"/><circle cx="15" cy="9" r="1" fill="currentColor" stroke="none"/><circle cx="9" cy="15" r="1" fill="currentColor" stroke="none"/><circle cx="15" cy="15" r="1" fill="currentColor" stroke="none"/>',
  'info': '<circle cx="12" cy="12" r="8.6"/><path d="M12 11.2v5"/><circle cx="12" cy="7.9" r=".8" fill="currentColor" stroke="none"/>'
};

const MATERIAL_ICON_MAP = {
  'bottom_navigation': 'menu',
  'network_node': 'layers',
  'network_manage': 'settings_ethernet',
  'memory_alt': 'developer_board'
};

const IconManager = {
  isMaterial() {
    return document.documentElement.getAttribute('data-theme') === 'material';
  },
  render(el, name) {
    if (!el) return;
    el.dataset.icon = name;
    if (el.dataset.branding === 'true' || this.isMaterial()) {
      const ligature = MATERIAL_ICON_MAP[name] || name;
      el.innerHTML = `<span class="material-symbols-outlined">${ligature}</span>`;
    } else {
      const paths = MIUIX_ICONS[name] || MIUIX_ICONS['chevron_right'];
      el.innerHTML = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${paths}</svg>`;
    }
  },
  create(name, className, branding) {
    const el = document.createElement('span');
    el.className = 'pp-icon' + (className ? ' ' + className : '');
    if (branding) el.dataset.branding = 'true';
    this.render(el, name);
    return el;
  },
  renderAll() {
    document.querySelectorAll('.pp-icon[data-icon]').forEach(el => this.render(el, el.dataset.icon));
  }
};

async function loadLanguage(lang) {
  try {
    const response = await fetch(`assets/lang/${lang}.json`);
    if (!response.ok) throw new Error("Language file not found");
    currentTranslations = await response.json();
    lsSet('pingpimp_lang', lang);
    persistUiSettings();
    document.documentElement.setAttribute('dir', lang === 'ar' ? 'rtl' : 'ltr');
    applyTranslations();
  } catch (error) {
    console.error("Failed to load language:", lang, error);
    if (lang !== 'en') loadLanguage('en');
  }
}

function applyTranslations() {
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n');
    if (currentTranslations[key]) el.innerHTML = currentTranslations[key];
  });
  document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    const key = el.getAttribute('data-i18n-placeholder');
    if (currentTranslations[key]) el.placeholder = currentTranslations[key];
  });

  const currentLang = localStorage.getItem('pingpimp_lang') || 'en';
  document.querySelectorAll('.language-option').forEach(opt => {
    opt.classList.toggle('selected', opt.dataset.lang === currentLang);
  });

  rotateBannerMessage();
  loadPresetTweakOptions();
  loadPrivateDnsOptions();
  loadAutoDnsOptions();
  renderAppList('isolate');
  renderAppList('prioritize');

  const autoSwitch = document.getElementById('switch-auto-mode');
  if (autoSwitch) applyAutoModeUIState(autoSwitch.checked);
}

function initLanguageModal() {
  const btnLang = document.getElementById('opt-language');
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

const ThemeManager = {
  init(override) {
    const root = document.documentElement;
    const o = override || {};
    this.applyTheme(o.theme || (root.getAttribute('data-theme') === 'material' ? 'material' : 'miuix'), false);
    this.applyMonet(o.monet || (root.getAttribute('data-monet') === 'off' ? 'off' : 'on'), false);
    this.applyNav(o.nav || (root.getAttribute('data-nav') === 'bar' ? 'bar' : 'floating'), false);
    this.applyGlass(o.glass || (root.getAttribute('data-glass') === 'on' ? 'on' : 'off'), false);

    document.querySelectorAll('#themeSegment .pp-segment-btn').forEach(btn => {
      btn.addEventListener('click', () => this.applyTheme(btn.dataset.ui, true));
    });
    document.querySelectorAll('#navSegment .pp-segment-btn').forEach(btn => {
      btn.addEventListener('click', () => this.applyNav(btn.dataset.nav, true));
    });
    const monetSwitch = document.getElementById('switch-monet');
    if (monetSwitch) {
      monetSwitch.addEventListener('change', () => {
        this.applyMonet(monetSwitch.checked ? 'on' : 'off', true);
      });
    }
    const glassSwitch = document.getElementById('switch-glass');
    if (glassSwitch) {
      glassSwitch.addEventListener('change', () => {
        this.applyGlass(glassSwitch.checked ? 'on' : 'off', true);
      });
    }
  },
  applyTheme(theme, notify) {
    document.documentElement.setAttribute('data-theme', theme);
    lsSet('pingpimp_theme', theme);
    document.querySelectorAll('#themeSegment .pp-segment-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.ui === theme);
    });
    IconManager.renderAll();
    persistUiSettings();
    if (notify) {
      const t = (k, f) => currentTranslations[k] || f;
      toast(t('toastThemeApplied', `Theme applied: ${theme === 'miuix' ? 'Miuix' : 'Material You'}`));
    }
  },
  applyMonet(mode, notify) {
    document.documentElement.setAttribute('data-monet', mode);
    lsSet('pingpimp_monet', mode);
    const sw = document.getElementById('switch-monet');
    if (sw) sw.checked = (mode === 'on');
    persistUiSettings();
    if (notify) {
      const t = (k, f) => currentTranslations[k] || f;
      toast(mode === 'on' ? t('toastMonetOn', 'Monet dynamic colors enabled') : t('toastMonetOff', 'Monet dynamic colors disabled'));
    }
  },
  applyNav(nav, notify) {
    document.documentElement.setAttribute('data-nav', nav);
    lsSet('pingpimp_nav', nav);
    const navEl = document.getElementById('bottomNav');
    if (navEl) navEl.classList.remove('nav-hidden');
    document.querySelectorAll('#navSegment .pp-segment-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.nav === nav);
    });
    persistUiSettings();
    if (notify) {
      const t = (k, f) => currentTranslations[k] || f;
      toast(t('toastNavApplied', nav === 'floating' ? 'Floating navigation bar' : 'Docked bottom bar'));
    }
  },
  applyGlass(mode, notify) {
    document.documentElement.setAttribute('data-glass', mode);
    lsSet('pingpimp_glass', mode);
    const sw = document.getElementById('switch-glass');
    if (sw) sw.checked = (mode === 'on');
    persistUiSettings();
    if (notify) {
      const t = (k, f) => currentTranslations[k] || f;
      toast(mode === 'on' ? t('toastGlassOn', 'Liquid glass enabled') : t('toastGlassOff', 'Liquid glass disabled'));
    }
  }
};

// === BOTTOM SHEET PICKER ===
function initPickerSheet() {
  const scrim = document.getElementById('pickerScrim');
  if (!scrim) return;
  scrim.addEventListener('click', (e) => {
    if (e.target === scrim) scrim.classList.remove('active');
  });
}

function openPickerSheet(title, options, currentValue, onPick) {
  const scrim = document.getElementById('pickerScrim');
  const titleEl = document.getElementById('pickerTitle');
  const listEl = document.getElementById('pickerOptions');
  if (!scrim || !listEl) return;

  titleEl.textContent = title || '';
  listEl.innerHTML = '';
  let selectedRow = null;

  options.forEach(opt => {
    const row = document.createElement('div');
    row.className = 'picker-option' + (opt.value === currentValue ? ' selected' : '');

    const text = document.createElement('span');
    text.className = 'picker-option-text';
    text.textContent = opt.text;

    const check = IconManager.create('check', 'picker-check');

    row.appendChild(text);
    row.appendChild(check);

    row.addEventListener('click', () => {
      scrim.classList.remove('active');
      if (onPick) onPick(opt.value, opt.text);
    });

    listEl.appendChild(row);
    if (opt.value === currentValue) selectedRow = row;
  });

  scrim.classList.add('active');

  if (selectedRow) {
    setTimeout(() => {
      try {
        const rowRect = selectedRow.getBoundingClientRect();
        const listRect = listEl.getBoundingClientRect();
        listEl.scrollTop += (rowRect.top - listRect.top) - (listEl.clientHeight / 2) + (rowRect.height / 2);
      } catch (e) { }
    }, 80);
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

// === SAFE LOCALSTORAGE ===
function lsGet(key) { try { return localStorage.getItem(key); } catch (e) { return null; } }
function lsSet(key, val) { try { localStorage.setItem(key, val); } catch (e) { } }

const UI_SETTINGS_PATH = '/data/adb/modules/PingPimp/webui.conf';

async function loadUiSettings() {
  try {
    const out = await exec(`cat ${UI_SETTINGS_PATH} 2>/dev/null`);
    if (!out) return null;
    const map = {};
    out.split('\n').forEach(line => {
      const i = line.indexOf('=');
      if (i > 0) map[line.slice(0, i).trim()] = line.slice(i + 1).trim();
    });
    return map;
  } catch (e) { return null; }
}

let persistTimer = null;
function persistUiSettings() {
  clearTimeout(persistTimer);
  persistTimer = setTimeout(() => {
    const root = document.documentElement;
    const theme = root.getAttribute('data-theme') === 'material' ? 'material' : 'miuix';
    const monet = root.getAttribute('data-monet') === 'off' ? 'off' : 'on';
    const nav   = root.getAttribute('data-nav')   === 'bar'  ? 'bar'  : 'floating';
    const glass = root.getAttribute('data-glass') === 'on'   ? 'on'   : 'off';
    const lang  = lsGet('pingpimp_lang') || 'en';
    const lines = [`theme=${theme}`, `monet=${monet}`, `nav=${nav}`, `glass=${glass}`, `lang=${lang}`];
    const args = lines.map(l => `'${l}'`).join(' ');
    exec(`printf '%s\\n' ${args} > ${UI_SETTINGS_PATH}`).catch(() => { });
  }, 120);
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
    if (opt.selected) selectedValue = opt.value;
    newSelectEl.appendChild(optionEl);
  });

  selectEl.parentNode.replaceChild(newSelectEl, selectEl);
  if (selectedValue !== null) newSelectEl.value = selectedValue;

  newSelectEl.classList.add('pp-native-select');

  let chip = newSelectEl.nextElementSibling;
  if (!chip || !chip.classList.contains('pp-select-value')) {
    chip = document.createElement('button');
    chip.type = 'button';
    chip.className = 'pp-select-value';
    const txt = document.createElement('span');
    txt.className = 'pp-select-value-text';
    chip.appendChild(txt);
    chip.appendChild(IconManager.create('expand_more'));
    newSelectEl.insertAdjacentElement('afterend', chip);
  }

  const updateChip = () => {
    const sel = newSelectEl.options[newSelectEl.selectedIndex];
    chip.querySelector('.pp-select-value-text').textContent = sel ? sel.textContent : '';
  };
  updateChip();

  chip.onclick = (e) => {
    e.stopPropagation();
    if (newSelectEl.disabled) return;
    const row = newSelectEl.closest('.pp-option');
    const titleEl = row ? row.querySelector('.pp-option-title') : null;
    openPickerSheet(titleEl ? titleEl.textContent : '', options, newSelectEl.value, (val, text) => {
      newSelectEl.value = val;
      updateChip();
      if (onSelect) onSelect(val, text);
    });
  };
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
  const providers = [
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
  providers.sort((a, b) => a.text.localeCompare(b.text, undefined, { sensitivity: 'base' }));
  return [
    { value: 'default', text: t('dnsDefault', 'Default') },
    { value: 'custom', text: t('dnsCustom', 'Custom DNS') },
    ...providers
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

  const options = getDnsOptionsList().map(opt => ({ ...opt, selected: opt.value === savedValue }));

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
          await exec(`echo "${savedCustom}" > /data/adb/modules/PingPimp/dns_manual.txt`);
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
          await exec(`echo "default" > /data/adb/modules/PingPimp/dns_manual.txt`);
        } else {
          await exec(`settings put global private_dns_mode hostname`);
          await exec(`settings put global private_dns_specifier ${dotName}`);
          await exec(`echo "${dotName}" > /data/adb/modules/PingPimp/dns_manual.txt`);
        }
        toast(`Private DNS set to ${text}`);
      } catch (e) {
        toast("Failed to set DNS");
      }
    }
  });

  const btnSaveCustom = document.getElementById('btn-save-custom-dns');
  if (btnSaveCustom && !btnSaveCustom.dataset.bound) {
    btnSaveCustom.dataset.bound = '1';
    btnSaveCustom.addEventListener('click', async () => {
      const customDot = customInput.value.trim();
      if (!customDot) return toast("DNS address cannot be empty!");
      localStorage.setItem('pingpimp_custom_dns', customDot);
      try {
        await exec(`settings put global private_dns_mode hostname`);
        await exec(`settings put global private_dns_specifier ${customDot}`);
        await exec(`echo "${customDot}" > /data/adb/modules/PingPimp/dns_manual.txt`);
        toast(`Custom DNS Applied!`);
      } catch (e) { toast("Failed to apply custom DNS"); }
    });
  }
}

function reverseDnsLookup(hostname) {
  if (!hostname) return 'google';
  for (const key of Object.keys(dnsMap)) {
    if (dnsMap[key] === hostname && dnsMap[key] !== '') return key;
  }
  return 'custom';
}

function getAutoDnsOptionsList() {
  const t = (key, fallback) => currentTranslations[key] || fallback;
  const providers = getDnsOptionsList().filter(o =>
    !['default', 'custom', 'google'].includes(o.value)
  );
  return [
    { value: 'default', text: t('dnsDefault', 'Default') },
    { value: 'custom',  text: t('dnsCustom', 'Custom DNS') },
    ...providers
  ];
}

async function readModuleFile(file) {
  try {
    return (await exec(`cat /data/adb/modules/PingPimp/${file} 2>/dev/null`)).trim();
  } catch (e) { return ''; }
}

async function triggerAutoDnsFlip() {
  try { await exec(`PingPimp --dns-flip`); } catch (e) { /* abaikan */ }
}

async function loadAutoDnsOptions() {
  const configs = [
    { selectId: 'select-auto-dns-mobile', file: 'dns_mobile.txt', rowId: 'row-auto-dns-mobile',
      customContainer: 'auto-dns-custom-mobile', customInput: 'auto-dns-custom-input-mobile',
      saveBtn: 'btn-save-auto-dns-mobile', storeKey: 'pingpimp_auto_dns_custom_mobile', netLabel: 'Mobile Data' },
    { selectId: 'select-auto-dns-wifi', file: 'dns_wifi.txt', rowId: 'row-auto-dns-wifi',
      customContainer: 'auto-dns-custom-wifi', customInput: 'auto-dns-custom-input-wifi',
      saveBtn: 'btn-save-auto-dns-wifi', storeKey: 'pingpimp_auto_dns_custom_wifi', netLabel: 'WiFi' }
  ];

  for (const cfg of configs) {
    if (!document.getElementById(cfg.selectId)) continue;

    const savedHost = await readModuleFile(cfg.file);
    let selectedKey;
    if (!savedHost) selectedKey = 'default';
    else if (savedHost === 'default') selectedKey = 'default';
    else selectedKey = reverseDnsLookup(savedHost);
    if (selectedKey === 'google') selectedKey = 'default'; // file lama berisi fallback Google

    const customContainer = document.getElementById(cfg.customContainer);
    const customInput = document.getElementById(cfg.customInput);

    if (selectedKey === 'custom') {
      customContainer.style.display = 'block';
      customInput.value = savedHost;
      localStorage.setItem(cfg.storeKey, savedHost);
    }

    const options = getAutoDnsOptionsList().map(o => ({ ...o, selected: o.value === selectedKey }));

    initNativeSelect(cfg.selectId, options, async (value, text) => {
      let host = '';
      if (value === 'custom') {
        customContainer.style.display = 'block';
        const prev = localStorage.getItem(cfg.storeKey) || '';
        customInput.value = prev;
        if (prev) {
          await exec(`echo "${prev}" > /data/adb/modules/PingPimp/${cfg.file}`);
          triggerAutoDnsFlip();
          toast(`Auto DNS (${cfg.netLabel}): ${prev}`);
        } else {
          toast("Please enter your custom DNS provider");
        }
        return;
      }
      if (value === 'default') host = 'default';
      else host = dnsMap[value] || 'default';

      customContainer.style.display = 'none';
      try {
        await exec(`echo "${host}" > /data/adb/modules/PingPimp/${cfg.file}`);
        triggerAutoDnsFlip();
        toast(`Auto DNS (${cfg.netLabel}): ${text}`);
      } catch (e) { toast("Failed to save Auto DNS"); }
    });

    const saveBtn = document.getElementById(cfg.saveBtn);
    if (saveBtn && !saveBtn.dataset.bound) {
      saveBtn.dataset.bound = '1';
      saveBtn.addEventListener('click', async () => {
        const customDot = customInput.value.trim();
        if (!customDot) return toast("DNS address cannot be empty!");
        localStorage.setItem(cfg.storeKey, customDot);
        try {
          await exec(`echo "${customDot}" > /data/adb/modules/PingPimp/${cfg.file}`);
          triggerAutoDnsFlip();
          toast(`Auto DNS (${cfg.netLabel}) custom applied: ${customDot}`);
        } catch (e) { toast("Failed to apply custom DNS"); }
      });
    }
  }
}

async function syncManualDnsToModule() {
  try {
    const val = localStorage.getItem('pingpimp_dns') || 'default';
    let host = 'default';
    if (val === 'custom') host = localStorage.getItem('pingpimp_custom_dns') || 'default';
    else if (val !== 'default') host = dnsMap[val] || 'default';
    await exec(`echo "${host}" > /data/adb/modules/PingPimp/dns_manual.txt`);
  } catch (e) { /* abaikan */ }
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
    if (content && content.trim() !== '') {
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

async function updateDeviceInfo() {
  const setTxt = (id, val) => document.getElementById(id).textContent = val || "-";
  try {
    const ver = await exec("grep '^version=' /data/adb/modules/PingPimp/module.prop 2>/dev/null || echo 'version=Unknown'");
    setTxt("PingPimpVer", ver.replace("version=", "").trim());
    setTxt("device-kernel", (await exec("uname -r")).trim());
    setTxt("device-model", (await exec("getprop ro.product.model")).trim());
    setTxt("device-android", (await exec("getprop ro.build.version.release")).trim());
    setTxt("device-chipset", (await exec("getprop ro.board.platform")).trim());
  } catch (e) { console.warn("Info fetch error", e); }
}

// === FEATURE: CUSTOM BANNER ===
const BANNER_ASSETS = '/data/adb/modules/PingPimp/webroot/assets';
const BANNER_CONFIG = '/data/adb/modules/PingPimp/banner.txt';
const BANNER_DEFAULT = 'assets/a.gif';
const BANNER_EXTS = ['gif', 'jpg', 'jpeg', 'png', 'webp'];
const BANNER_MAX_BYTES = 5 * 1024 * 1024; // 5 MB

function bannerUrlFromName(name) {
  return `assets/${name}?t=${Date.now()}`;
}

async function getBannerUrl() {
  try {
    const config = await exec(`cat ${BANNER_CONFIG} 2>/dev/null`);
    const name = (config || '').trim();
    if (name && name !== 'default' && BANNER_EXTS.some(e => name.endsWith('.' + e))) {
      const ok = await exec(`test -f "${BANNER_ASSETS}/${name}" && echo ok`);
      if (ok === 'ok') return bannerUrlFromName(name);
    }
  } catch (e) { /* fallback */ }
  return BANNER_DEFAULT;
}

async function setBannerSrc() {
  const img = document.querySelector('.banner img');
  if (!img) return;
  img.src = await getBannerUrl();
}

async function uploadBannerFile(file) {
  if (!file) return;

  // Validasi ekstensi
  const ext = (file.name.split('.').pop() || '').toLowerCase();
  if (!BANNER_EXTS.includes(ext)) {
    toast('Unsupported format. Use GIF, JPG, PNG, or WebP.');
    return;
  }

  // Validasi ukuran
  if (file.size > BANNER_MAX_BYTES) {
    toast('File too large. Maximum 5MB.');
    return;
  }
  if (file.size === 0) {
    toast('File is empty.');
    return;
  }

  toast('Uploading banner...');

  try {
    // Baca sebagai base64
    const b64 = await new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => {
        // Format data URI: "data:image/gif;base64,AAAA..."
        const comma = reader.result.indexOf(',');
        if (comma < 0) return reject(new Error('Invalid data URI'));
        resolve(reader.result.slice(comma + 1));
      };
      reader.onerror = () => reject(new Error('Failed to read file'));
      reader.readAsDataURL(file);
    });

    // Hapus custom banner lama (semua ekstensi)
    for (const e of BANNER_EXTS) {
      try { await exec(`rm -f ${BANNER_ASSETS}/custom_banner.${e}`); } catch (err) {}
    }

    // Tulis base64 dalam chunk (hindari shell arg limit)
    const b64Tmp = '/data/adb/modules/PingPimp/.banner_b64';
    const CHUNK = 60000; // ~60KB base64 per exec
    await exec(`: > ${b64Tmp}`);
    for (let i = 0; i < b64.length; i += CHUNK) {
      await exec(`printf '%s' '${b64.slice(i, i + CHUNK)}' >> ${b64Tmp}`);
    }

    // Decode ke file final
    const target = `${BANNER_ASSETS}/custom_banner.${ext}`;
    await exec(`base64 -d ${b64Tmp} > "${target}"`);
    await exec(`rm -f ${b64Tmp}`);

    // Simpan konfigurasi
    await exec(`echo "custom_banner.${ext}" > ${BANNER_CONFIG}`);

    toast('Banner updated!');
    setBannerSrc();
  } catch (err) {
    console.error('Banner upload failed:', err);
    toast('Failed to upload banner. Check file size.');
  }
}

async function resetBanner() {
  try {
    for (const e of BANNER_EXTS) {
      try { await exec(`rm -f ${BANNER_ASSETS}/custom_banner.${e}`); } catch (err) {}
    }
    await exec(`echo "default" > ${BANNER_CONFIG}`);
    toast('Banner reset to default');
    setBannerSrc();
  } catch (err) {
    toast('Failed to reset banner');
  }
}

function initCustomBanner() {
  const uploadBtn = document.getElementById('btn-banner-upload');
  const resetBtn = document.getElementById('btn-banner-reset');
  const fileInput = document.getElementById('banner-file-input');

  if (uploadBtn && fileInput) {
    uploadBtn.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', (e) => {
      if (e.target.files && e.target.files[0]) {
        uploadBannerFile(e.target.files[0]);
        e.target.value = '';
      }
    });
  }

  if (resetBtn) {
    resetBtn.addEventListener('click', resetBanner);
  }
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
    cachedUserApps = [];
    cachedSystemApps = [];

    const t = await exec("pm list packages -3");
    let thirdPartyPkgs = new Set();
    if (t) {
      t.split("\n").forEach(e => {
        const pkg = e.replace("package:", "").trim();
        if (pkg) thirdPartyPkgs.add(pkg);
      });
    }

    const allPkgsOut = await exec("pm list packages -U");
    let rawPackages = [];

    if (allPkgsOut) {
      allPkgsOut.split("\n").forEach(line => {
        const match = line.match(/^package:(.+)\s+uid:(\d+)$/);
        if (match) {
          rawPackages.push({ packageName: match[1], uid: match[2], appLabel: match[1] });
        }
      });
    }

    await Promise.all(rawPackages.map(async (pkgObj) => {
      try {
        const query = JSON.stringify([pkgObj.packageName]);
        let info = await getPackagesInfo(query);
        if (typeof info === 'string') info = JSON.parse(info);
        if (Array.isArray(info) && info.length > 0) {
          const appData = info[0];
          const label = appData.appLabel || appData.label || appData.appName;
          if (label) pkgObj.appLabel = label;
        }
      } catch (err) {}
    }));

    rawPackages.forEach(pkg => {
      if (thirdPartyPkgs.has(pkg.packageName)) cachedUserApps.push(pkg);
      else cachedSystemApps.push(pkg);
    });
  } catch (err) {
    console.error("Failed to fetch packages:", err);
    toast("Failed to load application list");
  }
}

async function loadAppConfigs() {
  try {
    const savedIso = await exec("cat /data/adb/modules/PingPimp/isolate_apps.txt 2>/dev/null");
    if (savedIso) {
      savedIso.replace(/\n|\r/g, "").split(',').filter(x => x.trim()).forEach(pkg => isolatedApps.add(pkg.trim()));
    }
  } catch (e) {}

  try {
    const savedPrio = await exec("cat /data/adb/modules/PingPimp/boost_apps.txt 2>/dev/null");
    if (savedPrio) {
      savedPrio.replace(/\n|\r/g, "").split(',').filter(x => x.trim()).forEach(pkg => prioritizedApps.add(pkg.trim()));
    }
  } catch (e) {}
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
  if (type === 'isolate' && currentIsolateView === 'system') sourceArray = cachedSystemApps;
  else if (type === 'prioritize' && currentPrioritizeView === 'system') sourceArray = cachedSystemApps;

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
    container.innerHTML = `<div class="list-state">${noAppsText}</div>`;
    return;
  }

  // Satu card grouped berisi semua baris (gaya list HyperOS / M3)
  const card = document.createElement('div');
  card.className = 'pp-card';
  const fragment = document.createDocumentFragment();

  filtered.forEach(pkg => {
    const isSelected = activeSet.has(pkg.packageName);

    const item = document.createElement('div');
    item.className = 'app-item' + (isSelected ? ' app-item-active' : '');

    const wrap = document.createElement('div');
    wrap.className = 'app-item-leading';
    wrap.style.display = 'flex';
    wrap.style.alignItems = 'center';
    wrap.style.gap = '14px';
    wrap.style.flex = '1';
    wrap.style.minWidth = '0';

    const img = document.createElement('img');
    img.className = 'app-icon';
    img.src = `ksu://icon/${pkg.packageName}`;

    img.onerror = () => {
      if (typeof window.ksu !== 'undefined' && typeof window.ksu.getPackagesIcons === 'function') {
        try {
          const iconQuery = JSON.stringify([pkg.packageName]);
          const iconResultStr = window.ksu.getPackagesIcons(iconQuery, 100);
          const iconData = JSON.parse(iconResultStr);
          if (iconData && iconData.length > 0 && iconData[0].icon) {
            img.src = iconData[0].icon;
            img.onerror = null;
            return;
          }
        } catch (e) {}
      }
      img.style.display = 'none';
      const fallback = document.createElement('div');
      fallback.className = 'app-icon-placeholder';
      fallback.appendChild(IconManager.create('android'));
      wrap.prepend(fallback);
    };

    const textGroup = document.createElement('div');
    textGroup.className = 'app-item-text';

    const label = document.createElement('div');
    label.className = 'app-item-label';
    label.textContent = pkg.appLabel || pkg.packageName;

    const subLabel = document.createElement('div');
    subLabel.className = 'app-item-pkg';
    subLabel.textContent = pkg.packageName;

    textGroup.appendChild(label);
    textGroup.appendChild(subLabel);

    wrap.appendChild(img);
    wrap.appendChild(textGroup);

    const switchLabel = document.createElement('label');
    switchLabel.className = 'pp-switch';

    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = isSelected;

    const slider = document.createElement('span');
    slider.className = 'pp-switch-slider';

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
            } catch (e) {}
          }
          isolatedApps.add(pkg.packageName);
          toast(`Isolated: ${pkg.appLabel}`);
        } else {
          if (uid) {
            try {
              await exec(`iptables -D OUTPUT -m owner --uid-owner ${uid} -j REJECT 2>/dev/null || true`);
              await exec(`ip6tables -D OUTPUT -m owner --uid-owner ${uid} -j REJECT 2>/dev/null || true`);
            } catch (e) {}
          }
          isolatedApps.delete(pkg.packageName);
          toast(`Restored: ${pkg.appLabel}`);
        }
        const isoStr = Array.from(isolatedApps).join(',');
        await exec(`echo "${isoStr}" > /data/adb/modules/PingPimp/isolate_apps.txt`);
      } else if (type === 'prioritize') {
        if (isChecked) {
          if (uid) {
            try {
              let dscpSupported = true;
              try {
                await exec(`iptables -t mangle -A OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null`);
                await exec(`iptables -t mangle -D OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null`);
              } catch (e) { dscpSupported = false; }

              if (dscpSupported) {
                await exec(`iptables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46`);
                await exec(`ip6tables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46`);
              } else {
                await exec(`iptables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j MARK --set-mark 0x40000000/0x40000000`);
                await exec(`ip6tables -t mangle -I OUTPUT -m owner --uid-owner ${uid} -j MARK --set-mark 0x40000000/0x40000000`);
              }
              await exec(`PingPimp --init-tc`);
            } catch (e) {}
          }
          prioritizedApps.add(pkg.packageName);
          toast(`Boosted: ${pkg.appLabel}`);
        } else {
          if (uid) {
            try {
              await exec(`iptables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46 2>/dev/null || true`);
              await exec(`ip6tables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j DSCP --set-dscp 46 2>/dev/null || true`);
              await exec(`iptables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null || true`);
              await exec(`ip6tables -t mangle -D OUTPUT -m owner --uid-owner ${uid} -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null || true`);
            } catch (e) {}
          }
          prioritizedApps.delete(pkg.packageName);
          toast(`Normal: ${pkg.appLabel}`);
        }
        const prioStr = Array.from(prioritizedApps).join(',');
        await exec(`echo "${prioStr}" > /data/adb/modules/PingPimp/boost_apps.txt`);
        await exec(`PingPimp --hw-tweak`);
      }
      renderAppList(type);
    });

    item.appendChild(wrap);
    item.appendChild(switchLabel);
    fragment.appendChild(item);
  });

  card.appendChild(fragment);
  container.appendChild(card);
}

function setHeaderTitle(title, brandingIcon) {
  const headerTitle = document.getElementById('main-header-title');
  if (!headerTitle) return;
  headerTitle.innerHTML = '';
  // Ikon hanya untuk tab Home (branding PingPimp, selalu Material Symbols)
  if (brandingIcon) {
    headerTitle.appendChild(IconManager.create(brandingIcon, null, true));
  }
  const text = document.createElement('span');
  text.className = 'header-text';
  text.textContent = title;
  headerTitle.appendChild(text);
}

function showTab(tabId) {
  document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
  document.querySelectorAll('.nav-button').forEach(el => el.classList.remove('active'));
  document.getElementById(`tab-${tabId}`).classList.add('active');
  const activeNav = document.querySelector(`.nav-button[data-target="${tabId}"]`);
  activeNav.classList.add('active');

  if (tabId === 'home') {
    setHeaderTitle('PingPimp', 'Stacked_Line_Chart');   // dengan ikon branding
  } else {
    const navLabel = activeNav.querySelector('.nav-label').textContent;
    setHeaderTitle(navLabel, null);                      // tanpa ikon
  }

  const navEl = document.getElementById('bottomNav');
  if (navEl) navEl.classList.remove('nav-hidden');
  window.scrollTo(0, 0);
}

// === AUTO MODE UI LOCK ===
function applyAutoModeUIState(isAuto) {
  // Hanya tweak PER-AKTIVITAS yang dikunci saat Auto Mode aktif.
  // Opsi Optimize Internet (netstate, wifips, conntrack, ipv6, saver)
  // adalah preferensi global user — tidak dikelola auto mode, tetap bebas.
  const selectsToLock = ['select-preset', 'select-tcp', 'select-dns'];
  const switchesToLock = ['switch-nicoffload', 'switch-irq', 'switch-ksoft'];

  selectsToLock.forEach(id => {
    const el = document.getElementById(id);
    if (el) {
      el.disabled = isAuto;
      const parent = el.closest('.pp-option');
      if (parent) parent.classList.toggle('disabled-by-auto', isAuto);
    }
  });

  switchesToLock.forEach(id => {
    const el = document.getElementById(id);
    if (el) {
      el.disabled = isAuto;
      const parent = el.closest('.pp-option');
      if (parent) parent.classList.toggle('disabled-by-auto', isAuto);
    }
  });

  const manualCustom = document.getElementById('custom-dns-container');
  if (manualCustom) manualCustom.classList.toggle('disabled-by-auto', isAuto);

  const autoDnsControls = [
    'select-auto-dns-mobile', 'select-auto-dns-wifi',
    'auto-dns-custom-input-mobile', 'auto-dns-custom-input-wifi',
    'btn-save-auto-dns-mobile', 'btn-save-auto-dns-wifi'
  ];
  autoDnsControls.forEach(id => {
    const el = document.getElementById(id);
    if (el) el.disabled = !isAuto;
  });
  ['row-auto-dns-mobile', 'row-auto-dns-wifi', 'auto-dns-custom-mobile', 'auto-dns-custom-wifi'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.classList.toggle('disabled-by-auto', !isAuto);
  });
}

// ============================================================
// LIQUID NAV — port DampedDragAnimation + FloatingBottomBar (WeaveMask)
// ============================================================
class LgSpring {
  constructor(initial, dampingRatio, stiffness, threshold = 0.001) {
    this.value = initial; this.target = initial; this.velocity = 0;
    this.zeta = dampingRatio; this.k = stiffness; this.eps = threshold;
  }
  animateTo(t) { this.target = t; }
  snapTo(v) { this.value = this.target = v; this.velocity = 0; }
  get settled() {
    return Math.abs(this.value - this.target) < this.eps && Math.abs(this.velocity) < this.eps * 10;
  }
  step(dt) {
    if (this.settled) { this.value = this.target; this.velocity = 0; return false; }
    const omega = Math.sqrt(this.k);
    const c = 2 * this.zeta * omega;
    const a = -this.k * (this.value - this.target) - c * this.velocity;
    this.velocity += a * dt;
    this.value += this.velocity * dt;
    return true;
  }
}

function initLiquidNav() {
  const nav = document.getElementById('bottomNav');
  if (!nav) return;
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

  nav.querySelectorAll('.nav-glass-spec, .nav-glass-hl, .nav-slider-pill').forEach(n => n.remove());

  const spec = document.createElement('div'); spec.className = 'nav-glass-spec';
  const hl = document.createElement('div'); hl.className = 'nav-glass-hl';
  const pill = document.createElement('div'); pill.className = 'nav-slider-pill';
  spec.style.cssText = 'position:absolute;inset:0;pointer-events:none;z-index:3;';
  hl.style.cssText = 'position:absolute;inset:0;pointer-events:none;mix-blend-mode:screen;z-index:3;';
  pill.style.cssText = 'position:absolute;left:0;top:4px;height:calc(100% - 8px);border-radius:100px;pointer-events:none;will-change:transform;z-index:3;';
  nav.appendChild(spec);
  nav.appendChild(pill);
  nav.appendChild(hl);

  const isOn = () =>
    document.documentElement.getAttribute('data-theme') === 'miuix' &&
    document.documentElement.getAttribute('data-nav') === 'floating';
  const isGlass = () => document.documentElement.getAttribute('data-glass') === 'on';
  const isRtl = () => document.documentElement.getAttribute('dir') === 'rtl';

  const sValue  = new LgSpring(0, 1.0, 1000, 0.001);
  const sPress  = new LgSpring(0, 1.0, 1000, 0.001);
  const sScaleX = new LgSpring(1, 0.6, 250, 0.001);
  const sScaleY = new LgSpring(1, 0.7, 250, 0.001);
  const sVel    = new LgSpring(0, 0.5, 300, 0.05);
  const sOffset = new LgSpring(0, 1.0, 300, 0.5);
  const PRESSED_SCALE = 78 / 56;
  const RUBBER_PX = 4;

  let btns = [], step = 0, baseX = 0, totalW = 1, pillW = 0;
  let releasePending = false;

  function measure() {
    btns = Array.from(nav.querySelectorAll('.nav-button'));
    if (btns.length < 2) return false;
    totalW = nav.clientWidth || 1;
    step = btns[1].offsetLeft - btns[0].offsetLeft;
    baseX = btns[0].offsetLeft;
    pillW = btns[0].offsetWidth;
    pill.style.width = pillW + 'px';
    return step !== 0;
  }
  const count = () => btns.length;
  function currentIndex() {
    const a = nav.querySelector('.nav-button.active');
    return a ? Math.max(0, btns.indexOf(a)) : 0;
  }
  function syncVisibility() {
    const on = isOn(), glass = on && isGlass();
    pill.style.display = on ? 'block' : 'none';
    spec.style.display = glass ? 'block' : 'none';
    hl.style.display = glass ? 'block' : 'none';
    if (!on) ['--lg-bar-x', '--lg-press', '--lg-tabs', '--lg-hl-x'].forEach(p => nav.style.removeProperty(p));
  }
  function press() {
    sPress.animateTo(1);
    sScaleX.animateTo(PRESSED_SCALE);
    sScaleY.animateTo(PRESSED_SCALE);
  }
  function releaseVisuals() {
    sPress.animateTo(0); sScaleX.animateTo(1); sScaleY.animateTo(1);
  }
  function animateToValue(idx) {
    idx = clamp(idx, 0, count() - 1);
    press();
    sValue.animateTo(idx);
    if (Math.abs(sVel.value) > 0.01) sVel.animateTo(0);
    releasePending = true;
    kick();
  }

  let raf = null, last = 0;
  function frame(t) {
    const dt = clamp((t - last) / 1000 || 0.016, 0.001, 0.032);
    last = t;
    sVel.animateTo(sValue.velocity);
    let live = false;
    live = sValue.step(dt)  || live;
    live = sPress.step(dt)  || live;
    live = sScaleX.step(dt) || live;
    live = sScaleY.step(dt) || live;
    live = sVel.step(dt)    || live;
    live = sOffset.step(dt) || live;
    if (releasePending && Math.abs(sValue.value - sValue.target) < 0.025 * Math.max(1, count() - 1)) {
      releaseVisuals(); releasePending = false;
    }
    render();
    raf = live ? requestAnimationFrame(frame) : null;
  }
  function kick() { if (raf == null) { last = performance.now(); raf = requestAnimationFrame(frame); } }

  function render() {
    if (!isOn() || !step) return;
    const x = baseX + sValue.value * step;
    const glass = isGlass();
    const v = sVel.value / 10;
    const sx = glass ? sScaleX.value / (1 - clamp(v * 0.75, -0.2, 0.2)) : 1;
    const sy = glass ? sScaleY.value * (1 - clamp(v * 0.25, -0.2, 0.2)) : 1;
    pill.style.transform = `translate3d(${x.toFixed(2)}px,0,0) scale(${sx.toFixed(4)},${sy.toFixed(4)})`;
    const frac = clamp(sOffset.value / totalW, -1, 1);
    nav.style.setProperty('--lg-bar-x',
      (RUBBER_PX * Math.sign(frac) * (1 - Math.pow(1 - Math.abs(frac), 3))).toFixed(2) + 'px');
    nav.style.setProperty('--lg-press', sPress.value.toFixed(3));
    nav.style.setProperty('--lg-tabs', (1 + 0.2 * sPress.value).toFixed(3));
    nav.style.setProperty('--lg-hl-x', (x + pillW / 2).toFixed(1) + 'px');
  }

  let dragging = false, horizontal = null, didMove = false;
  let startX = 0, startY = 0, lastX = 0;

  nav.addEventListener('pointerdown', (e) => {
    if (!isOn()) return;
    if (!step && !measure()) return;
    dragging = true; horizontal = null; didMove = false;
    startX = lastX = e.clientX; startY = e.clientY;
    press();
    kick();
  });

  window.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    if (horizontal === null) {
      const adx = Math.abs(e.clientX - startX), ady = Math.abs(e.clientY - startY);
      if (adx > 6 || ady > 6) horizontal = adx > ady;
    }
    if (!horizontal) return;
    const dx = e.clientX - lastX; lastX = e.clientX;
    didMove = Math.abs(e.clientX - startX) > 8;
    if (!step) return;
    const sign = isRtl() ? -1 : 1;
    sValue.animateTo(clamp(sValue.target + (dx / Math.abs(step)) * sign, 0, count() - 1));
    sOffset.snapTo(clamp(sOffset.value + dx, -totalW, totalW));
    kick();
  });

  function endDrag() {
    if (!dragging) return;
    dragging = false;
    if (horizontal !== true) { releaseVisuals(); kick(); return; }
    const targetIdx = clamp(Math.round(sValue.target), 0, count() - 1);
    sOffset.animateTo(0);
    animateToValue(targetIdx);
    if (targetIdx !== currentIndex()) showTab(btns[targetIdx].dataset.target);
    setTimeout(() => { didMove = false; }, 120);
  }
  window.addEventListener('pointerup', endDrag);
  window.addEventListener('pointercancel', () => {
    if (!dragging) return;
    dragging = false;
    sOffset.animateTo(0);
    animateToValue(currentIndex());
  });

  nav.addEventListener('click', (e) => {
    if (didMove) { e.preventDefault(); e.stopPropagation(); didMove = false; }
  }, true);

  function onActiveChanged() {
    if (!isOn()) return;
    measure();
    const idx = currentIndex();
    if (Math.abs(sValue.target - idx) > 0.001) animateToValue(idx);
    else { render(); kick(); }
  }

  const mo = new MutationObserver(() => {
    syncVisibility();
    if (isOn()) onActiveChanged();
  });
  mo.observe(nav, { attributes: true, attributeFilter: ['class'], subtree: true });
  mo.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme', 'data-nav', 'data-glass', 'dir'] });

  window.addEventListener('resize', () => { if (measure()) { sValue.snapTo(currentIndex()); render(); } });
  if (document.fonts && document.fonts.ready) {
    document.fonts.ready.then(() => { if (measure()) { sValue.snapTo(currentIndex()); render(); } });
  }
  syncVisibility();
  measure();
  sValue.snapTo(currentIndex());
  render();
}

// === INIT ===
document.addEventListener("DOMContentLoaded", async () => {
  const loadingOverlay = document.getElementById('loading-overlay');

  function dismissLoading() {
    if (!loadingOverlay) return;
    loadingOverlay.style.opacity = "0";
    setTimeout(() => { if (loadingOverlay.parentNode) loadingOverlay.remove(); }, 300);
  }

  // --- Fase 1: init wajib + pulihkan pengaturan persisten ---
  try {
    const oneOf = (v, a, b) => (v === a || v === b) ? v : undefined;
    const persisted = await loadUiSettings();
    ThemeManager.init({
      theme: oneOf(persisted?.theme, 'miuix', 'material'),
      monet: oneOf(persisted?.monet, 'on', 'off'),
      nav:   oneOf(persisted?.nav, 'floating', 'bar'),
      glass: oneOf(persisted?.glass, 'on', 'off')
    });
    initLiquidNav();
    initPickerSheet();
    initLanguageModal();

    const savedLang = (persisted && persisted.lang) || lsGet('pingpimp_lang') || 'en';
    await loadLanguage(savedLang);
  } catch (err) {
    console.error('Init error:', err);
  } finally {
    dismissLoading();
  }

  // --- Fase 2: fitur ---
  try {
    updateDeviceInfo();
    loadPresetTweakOptions();
    loadTcpAlgorithms();
    loadPrivateDnsOptions();
    loadAutoDnsOptions();
    syncManualDnsToModule();
    initCustomBanner();
    setBannerSrc();

    initSwitch("switch-auto-mode", "auto_mode.txt", "auto-on", "auto-off", "Smart Auto Mode");
    initSwitch("switch-netstate", "state.txt", "state", "unstate", "Network State");
    initSwitch("switch-saver", "saver.txt", "saver", "unsaver", "Data Saver");
    initSwitch("switch-ipv6", "ipv6_state.txt", "disable", "enable", "Disable IPv6");
    initSwitch("switch-nicoffload", "nic.txt", "nic-offload", "nic-on", "NIC Offloading");
    initSwitch("switch-irq", "irq.txt", "irq-affinity", "unirq-affinity", "IRQ Affinity");
    initSwitch("switch-conntrack", "conntrack.txt", "conntrack-on", "conntrack-off", "ConnTrack Optimization");
    initSwitch("switch-wifips", "wifips.txt", "wifi-ps-off", "wifi-ps-on", "Wi-Fi Low-Jitter");
    initSwitch("switch-ksoft", "ksoft.txt", "boost-ksoft", "unboost-ksoft", "Ksoftirqd Boost");

    const btnSaveLog = document.getElementById('opt-savelog');
    if (btnSaveLog) {
      btnSaveLog.addEventListener('click', async () => {
        try {
          await exec('cp /data/adb/modules/PingPimp/log.txt /sdcard/PingPimp.log');
          toast('Log berhasil disimpan ke /sdcard/PingPimp.log');
        } catch (e) { toast('Gagal menyimpan file log.'); }
      });
    }

    rotateBannerMessage();
    setInterval(rotateBannerMessage, 7000);

    document.querySelectorAll('.nav-button').forEach(item => {
      item.addEventListener('click', () => showTab(item.dataset.target));
    });

    const navEl = document.getElementById('bottomNav');
    let lastScrollY = 0;
    window.addEventListener('scroll', () => {
      if (document.documentElement.getAttribute('data-nav') !== 'floating') {
        navEl.classList.remove('nav-hidden');
        return;
      }
      const y = window.scrollY;
      if (y > lastScrollY + 6 && y > 80) navEl.classList.add('nav-hidden');
      else if (y < lastScrollY - 6) navEl.classList.remove('nav-hidden');
      lastScrollY = y;
    }, { passive: true });

    const btnUserIso = document.getElementById('btn-user-apps-isolate');
    const btnSysIso = document.getElementById('btn-system-apps-isolate');
    if (btnUserIso && btnSysIso) {
      btnUserIso.addEventListener('click', () => {
        currentIsolateView = 'user';
        btnUserIso.classList.add('active'); btnSysIso.classList.remove('active');
        renderAppList('isolate');
      });
      btnSysIso.addEventListener('click', () => {
        currentIsolateView = 'system';
        btnSysIso.classList.add('active'); btnUserIso.classList.remove('active');
        renderAppList('isolate');
      });
    }

    const autoSwitch = document.getElementById('switch-auto-mode');
    if (autoSwitch) {
      autoSwitch.addEventListener('change', () => applyAutoModeUIState(autoSwitch.checked));
      setTimeout(() => applyAutoModeUIState(autoSwitch.checked), 300);
    }

    const infoDialog = document.getElementById('infoDialog');
    const infoTitle = document.getElementById('infoDialogTitle');
    const infoDesc = document.getElementById('infoDialogDesc');
    const infoIconBox = document.getElementById('infoDialogIcon');
    const closeInfoBtn = document.getElementById('closeInfoDialog');

    if (closeInfoBtn && infoDialog) {
      closeInfoBtn.addEventListener('click', () => infoDialog.classList.remove('active'));
      infoDialog.addEventListener('click', (e) => {
        if (e.target === infoDialog) infoDialog.classList.remove('active');
      });
    }

    document.querySelectorAll('.pp-option-text').forEach(block => {
      block.addEventListener('click', () => {
        const titleEl = block.querySelector('.pp-option-title');
        const iconBox = block.previousElementSibling;
        if (titleEl && iconBox) {
          const key = titleEl.getAttribute('data-i18n');
          const extKey = key ? key + 'Ext' : null;
          if (extKey && currentTranslations[extKey]) {
            infoTitle.textContent = titleEl.textContent;
            infoDesc.textContent = currentTranslations[extKey];
            const iconName = iconBox.querySelector('.pp-icon')?.dataset.icon;
            const infoIcon = infoIconBox?.querySelector('.pp-icon');
            if (iconName && infoIcon) IconManager.render(infoIcon, iconName);
            infoDialog.classList.add('active');
          }
        }
      });
    });

    const btnUserPrio = document.getElementById('btn-user-apps-prioritize');
    const btnSysPrio = document.getElementById('btn-system-apps-prioritize');
    if (btnUserPrio && btnSysPrio) {
      btnUserPrio.addEventListener('click', () => {
        currentPrioritizeView = 'user';
        btnUserPrio.classList.add('active'); btnSysPrio.classList.remove('active');
        renderAppList('prioritize');
      });
      btnSysPrio.addEventListener('click', () => {
        currentPrioritizeView = 'system';
        btnSysPrio.classList.add('active'); btnUserPrio.classList.remove('active');
        renderAppList('prioritize');
      });
    }

    const isolateContainer = document.getElementById('isolate-list-container');
    const prioritizeContainer = document.getElementById('prioritize-list-container');
    const loadingText = currentTranslations['loadingApps'] || "Loading apps, please wait...";
    if (isolateContainer) isolateContainer.innerHTML = `<div class="list-state">${loadingText}</div>`;
    if (prioritizeContainer) prioritizeContainer.innerHTML = `<div class="list-state">${loadingText}</div>`;

    loadAppConfigs().then(() => fetchUserPackagesInfo()).then(() => {
      renderAppList('isolate');
      renderAppList('prioritize');
    }).catch(err => {
      console.warn("Failed to load apps", err);
      if (isolateContainer) isolateContainer.innerHTML = `<div class="list-state list-error">Failed to load apps.</div>`;
      if (prioritizeContainer) prioritizeContainer.innerHTML = `<div class="list-state list-error">Failed to load apps.</div>`;
    });

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

    setHeaderTitle('PingPimp', 'Stacked_Line_Chart');
  } catch (err) {
    console.error('Feature init error:', err);
  }
});