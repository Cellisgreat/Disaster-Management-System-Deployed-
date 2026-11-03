/* ============================================================================
   core.js — shared frontend runtime
   ----------------------------------------------------------------------------
   * API client for the C++ REST backend (cookies are HttpOnly; we never touch
     the token from JavaScript).
   * Session guard + role routing.
   * UI primitives: toasts, dialogs, icons, formatting, small DOM helpers.
   Everything the pages use that is not page-specific lives here.
   ========================================================================== */
'use strict';

/* ------------------------------------------------------------------ icons -- */
const ICONS = {
  shield: '<path d="M12 3l7 3v5.5c0 4.3-2.9 7.9-7 9.5-4.1-1.6-7-5.2-7-9.5V6l7-3z"/>',
  grid: '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
  users: '<path d="M16 20v-1.6c0-1.9-1.6-3.4-3.5-3.4H6.5C4.6 15 3 16.5 3 18.4V20"/><circle cx="9.5" cy="8" r="3.4"/><path d="M21 20v-1.6c0-1.9-1.5-3.4-3.4-3.4"/><path d="M15.5 4.6a3.4 3.4 0 010 6.6"/>',
  clipboard: '<path d="M9 4.5h6M9 4.5A1.5 1.5 0 017.5 6v.5h-1A1.5 1.5 0 005 8v10a1.5 1.5 0 001.5 1.5h11A1.5 1.5 0 0019 18V8a1.5 1.5 0 00-1.5-1.5h-1V6A1.5 1.5 0 0015 4.5z"/><path d="M9 12h6M9 15.5h4"/>',
  map: '<path d="M9 4.5L3.5 6.6v12.9L9 17.4l6 2.1 5.5-2.1V4.5L15 6.6 9 4.5z"/><path d="M9 4.5v12.9M15 6.6v12.9"/>',
  route: '<circle cx="6" cy="18" r="2.4"/><circle cx="18" cy="6" r="2.4"/><path d="M8.4 18h5.1a3.5 3.5 0 000-7H10a3.5 3.5 0 010-7h5.6"/>',
  cone: '<path d="M12 3l6.5 15h-13L12 3z"/><path d="M8.6 12h6.8"/>',
  settings: '<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 00.34 1.87l.06.06a2 2 0 11-2.83 2.83l-.06-.06a1.7 1.7 0 00-1.87-.34 1.7 1.7 0 00-1 1.56V21a2 2 0 11-4 0v-.1A1.7 1.7 0 008 19.3a1.7 1.7 0 00-1.87.34l-.06.06a2 2 0 11-2.83-2.83l.06-.06A1.7 1.7 0 003.7 15a1.7 1.7 0 00-1.56-1H2a2 2 0 110-4h.1A1.7 1.7 0 003.7 9a1.7 1.7 0 00-.34-1.87l-.06-.06a2 2 0 112.83-2.83l.06.06A1.7 1.7 0 008 4.7 1.7 1.7 0 009 3.14V3a2 2 0 114 0v.1a1.7 1.7 0 001 1.56 1.7 1.7 0 001.87-.34l.06-.06a2 2 0 112.83 2.83l-.06.06A1.7 1.7 0 0019.3 9v.1a1.7 1.7 0 001.56 1H21a2 2 0 110 4h-.1a1.7 1.7 0 00-1.5 1z"/>',
  logout: '<path d="M15 17l5-5-5-5"/><path d="M20 12H9"/><path d="M12 4H6a2 2 0 00-2 2v12a2 2 0 002 2h6"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  search: '<circle cx="11" cy="11" r="6.5"/><path d="M16 16l4.5 4.5"/>',
  refresh: '<path d="M20 11a8 8 0 10-2.3 6.1"/><path d="M20 5v6h-6"/>',
  alert: '<path d="M12 8.5v5"/><path d="M12 17h.01"/><path d="M10.3 3.9L2.6 17.2A2 2 0 004.3 20h15.4a2 2 0 001.7-2.8L13.7 3.9a2 2 0 00-3.4 0z"/>',
  check: '<path d="M20 6L9 17l-5-5"/>',
  clock: '<circle cx="12" cy="12" r="8.5"/><path d="M12 7.5V12l3 2"/>',
  pin: '<path d="M12 21s6.5-6 6.5-11a6.5 6.5 0 10-13 0C5.5 15 12 21 12 21z"/><circle cx="12" cy="10" r="2.4"/>',
  heart: '<path d="M12 20s-7-4.4-7-9.3A4.2 4.2 0 0112 8a4.2 4.2 0 017 2.7C19 15.6 12 20 12 20z"/>',
  food: '<path d="M6 3v8M9 3v8M7.5 11v10"/><path d="M16 3c-1.5 1.6-2 3.6-2 5.5S15 12 16 12v9"/>',
  building: '<path d="M4 20V6l8-3 8 3v14"/><path d="M9 20v-5h6v5"/><path d="M9 9h.01M15 9h.01M9 12.5h.01M15 12.5h.01"/>',
  chevron: '<path d="M9 6l6 6-6 6"/>',
  close: '<path d="M6 6l12 12M18 6L6 18"/>',
  menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
  key: '<circle cx="8" cy="15" r="4"/><path d="M11 12l8-8M15 4h4v4"/>',
  eye: '<path d="M2.5 12S6 6 12 6s9.5 6 9.5 6-3.5 6-9.5 6-9.5-6-9.5-6z"/><circle cx="12" cy="12" r="2.6"/>',
  db: '<ellipse cx="12" cy="6" rx="7.5" ry="3"/><path d="M4.5 6v12c0 1.7 3.4 3 7.5 3s7.5-1.3 7.5-3V6"/><path d="M4.5 12c0 1.7 3.4 3 7.5 3s7.5-1.3 7.5-3"/>',
  send: '<path d="M21 3L10.5 13.5"/><path d="M21 3l-6.5 18-4-7.5-7.5-4L21 3z"/>'
};

function icon(name, size = 16, cls = '') {
  const body = ICONS[name] || ICONS.alert;
  return `<svg class="${cls}" width="${size}" height="${size}" viewBox="0 0 24 24" fill="none"
    stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"
    aria-hidden="true">${body}</svg>`;
}

/* -------------------------------------------------------------------- API -- */
const API = {
  /** Performs a JSON request against the C++ backend. */
  async request(method, path, body) {
    const opts = {
      method,
      headers: { Accept: 'application/json' },
      credentials: 'same-origin'
    };
    if (body !== undefined) {
      opts.headers['Content-Type'] = 'application/json';
      opts.body = JSON.stringify(body);
    }
    let res;
    try {
      res = await fetch(path, opts);
    } catch (err) {
      return { ok: false, status: 0, data: { error: 'Cannot reach the C++ backend. Is the server running?', code: 'network' } };
    }
    let data = null;
    const text = await res.text();
    try { data = text ? JSON.parse(text) : {}; } catch (e) { data = { error: text || 'Unexpected response' }; }
    return { ok: res.ok, status: res.status, data: data || {} };
  },
  get(path) { return this.request('GET', path); },
  post(path, body) { return this.request('POST', path, body === undefined ? {} : body); },
  patch(path, body) { return this.request('PATCH', path, body === undefined ? {} : body); }
};

/* --------------------------------------------------------------- helpers -- */
const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

function escapeHtml(value) {
  return String(value === null || value === undefined ? '' : value)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function fmtInt(n) {
  const v = Number(n || 0);
  return v.toLocaleString('en-IN');
}

function fmtKm(n) {
  const v = Number(n || 0);
  return (Math.round(v * 100) / 100).toString();
}

function fmtDateTime(seconds) {
  if (!seconds) return '—';
  const d = new Date(seconds * 1000);
  return d.toLocaleString(undefined, { day: '2-digit', month: 'short', hour: '2-digit', minute: '2-digit' });
}

function initials(name) {
  const parts = String(name || '?').replace(/[._-]/g, ' ').trim().split(/\s+/);
  const a = parts[0] ? parts[0][0] : '?';
  const b = parts.length > 1 ? parts[parts.length - 1][0] : '';
  return (a + b).toUpperCase();
}

const SERVICE_CLASS = { 'Rescue': 'rescue', 'Medical Help': 'medical', 'Food': 'food' };

function serviceBadge(service) {
  const cls = SERVICE_CLASS[service] || 'neutral';
  const tone = cls === 'rescue' ? 'badge-danger' : cls === 'medical' ? 'badge-info' : 'badge-ok';
  return `<span class="badge ${tone}"><span class="dot"></span>${escapeHtml(service)}</span>`;
}

function statusBadge(status) {
  if (status === 'Completed') return '<span class="badge badge-ok"><span class="dot"></span>Completed</span>';
  if (status === 'Active') return '<span class="badge badge-warn"><span class="dot"></span>Active</span>';
  if (status === 'Available') return '<span class="badge badge-ok"><span class="dot"></span>Available</span>';
  if (status === 'Assigned') return '<span class="badge badge-warn"><span class="dot"></span>Assigned</span>';
  return `<span class="badge badge-neutral">${escapeHtml(status || '—')}</span>`;
}

function progressBar(covered, affected) {
  const pct = affected > 0 ? Math.min(100, (covered / affected) * 100) : 0;
  const tone = pct >= 100 ? '' : pct >= 50 ? 'warn' : 'danger';
  return `<div class="progress ${tone}" role="progressbar" aria-valuenow="${Math.round(pct)}"
      aria-valuemin="0" aria-valuemax="100" title="${covered} of ${affected} people covered">
      <span style="width:${pct}%"></span></div>`;
}

/* ----------------------------------------------------------------- toasts -- */
function toastStack() {
  let stack = $('.toast-stack');
  if (!stack) {
    stack = document.createElement('div');
    stack.className = 'toast-stack';
    stack.setAttribute('role', 'status');
    stack.setAttribute('aria-live', 'polite');
    document.body.appendChild(stack);
  }
  return stack;
}

function toast(message, type = 'info', ms = 4200) {
  const stack = toastStack();
  const node = document.createElement('div');
  node.className = 'toast ' + type;
  const ico = type === 'ok' ? 'check' : type === 'error' ? 'alert' : type === 'warn' ? 'alert' : 'clock';
  node.innerHTML = `${icon(ico, 16)}<div class="msg">${escapeHtml(message)}</div>
    <button class="close" aria-label="Dismiss">&times;</button>`;
  stack.appendChild(node);
  const close = () => node.remove();
  node.querySelector('.close').addEventListener('click', close);
  setTimeout(close, ms);
}

/* ---------------------------------------------------------------- dialogs -- */
function dialog({ title, subtitle = '', body = '', confirmLabel = 'Confirm', cancelLabel = 'Cancel',
                  danger = false, wide = false, hideCancel = false }) {
  return new Promise((resolve) => {
    const backdrop = document.createElement('div');
    backdrop.className = 'modal-backdrop';
    backdrop.innerHTML = `
      <div class="modal ${wide ? 'wide' : ''}" role="dialog" aria-modal="true" aria-label="${escapeHtml(title)}">
        <div class="modal-head">
          <div style="flex:1">
            <h3>${escapeHtml(title)}</h3>
            ${subtitle ? `<div class="sub">${escapeHtml(subtitle)}</div>` : ''}
          </div>
          <button class="icon-btn" data-close aria-label="Close">${icon('close', 16)}</button>
        </div>
        <div class="modal-body">${body}</div>
        <div class="modal-foot">
          ${hideCancel ? '' : `<button class="btn btn-outline" data-close>${escapeHtml(cancelLabel)}</button>`}
          <button class="btn ${danger ? 'btn-danger' : 'btn-primary'}" data-confirm>${escapeHtml(confirmLabel)}</button>
        </div>
      </div>`;
    document.body.appendChild(backdrop);

    const finish = (value) => {
      backdrop.remove();
      document.removeEventListener('keydown', onKey);
      resolve(value);
    };
    const onKey = (e) => {
      if (e.key === 'Escape') finish(false);
      if (e.key === 'Enter' && !e.target.matches('textarea')) {
        e.preventDefault();
        finish(true);
      }
    };
    backdrop.addEventListener('click', (e) => {
      if (e.target === backdrop) finish(false);
      if (e.target.closest('[data-close]')) finish(false);
      if (e.target.closest('[data-confirm]')) finish(true);
    });
    document.addEventListener('keydown', onKey);
    const focusTarget = backdrop.querySelector('input, select, textarea, [data-confirm]');
    if (focusTarget) focusTarget.focus();
  });
}

function confirmAction(opts) {
  const message = typeof opts === 'string' ? opts : (opts.message || 'Are you sure?');
  const title = typeof opts === 'string' ? 'Please confirm' : (opts.title || 'Please confirm');
  return dialog({
    title,
    subtitle: typeof opts === 'string' ? '' : (opts.subtitle || ''),
    body: `<p style="font-size:13.5px;color:#334155">${escapeHtml(message)}</p>`,
    confirmLabel: (typeof opts === 'object' && opts.confirmLabel) || 'Confirm',
    danger: !!(typeof opts === 'object' && opts.danger)
  });
}

/* --------------------------------------------------------------- sessions -- */
const Session = {
  current: null,
  async load() {
    const res = await API.get('/api/auth/me');
    this.current = res.ok && res.data.authenticated ? res.data.user : null;
    return this.current;
  },
  /** Guards a page: requires login, and optionally a specific role. */
  async guard(requiredRole) {
    const user = await this.load();
    if (!user) {
      const next = encodeURIComponent(location.pathname + location.hash);
      location.replace('/login.html?next=' + next);
      return null;
    }
    if (requiredRole && user.role !== requiredRole) {
      location.replace(user.isAdmin ? '/admin.html' : '/person.html');
      return null;
    }
    return user;
  },
  async logout() {
    await API.post('/api/auth/logout');
    location.replace('/login.html?signedout=1');
  }
};

/* --------------------------------------------------------------- app shell -- */
function initShell(options) {
  const sidebar = $('.sidebar');
  const toggle = $('.hamburger');
  if (toggle && sidebar) {
    toggle.addEventListener('click', () => {
      const open = sidebar.classList.toggle('open');
      let scrim = $('.sidebar-scrim');
      if (open && !scrim) {
        scrim = document.createElement('div');
        scrim.className = 'sidebar-scrim';
        scrim.addEventListener('click', () => {
          sidebar.classList.remove('open');
          scrim.remove();
        });
        document.body.appendChild(scrim);
      } else if (!open && scrim) {
        scrim.remove();
      }
    });
  }
  $$('[data-logout]').forEach((btn) => btn.addEventListener('click', async () => {
    const ok = await confirmAction({ title: 'Sign out?', message: 'You will be returned to the sign-in page.', confirmLabel: 'Sign out' });
    if (ok) Session.logout();
  }));
  if (options && options.title) {
    const h1 = $('.topbar h1');
    if (h1) h1.textContent = options.title;
  }
}

function setUserChrome(user) {
  $$('[data-user-name]').forEach((n) => { n.textContent = user.username; });
  $$('[data-user-role]').forEach((n) => { n.textContent = user.role; });
  $$('[data-user-initials]').forEach((n) => { n.textContent = initials(user.username); });
}

function emptyState(title, message, iconName = 'clipboard') {
  return `<div class="empty">${icon(iconName, 28)}<h3>${escapeHtml(title)}</h3>
    <p class="small">${escapeHtml(message)}</p></div>`;
}

function debounce(fn, ms = 180) {
  let timer = null;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
}
