/* ============================================================================
   admin.js — administrator console
   ----------------------------------------------------------------------------
   Hash-routed views on top of the C++ REST API:
     #/dashboard  #/volunteers  #/requests  #/city
     #/route      #/roads       #/accounts  #/settings
   Every list, statistic and mutation comes from the backend. No mock data.
   ========================================================================== */
'use strict';

const State = {
  user: null,
  graph: null,
  areas: [],
  volunteers: [],
  requests: [],
  accounts: [],
  dashboard: null,
  maps: [],
  routeFinder: null,
  routePrefill: null,
  filters: {
    volunteers: { q: '', service: '', area: '', status: 'all', sortKey: 'id', sortDir: 'asc' },
    requests: { tab: 'all', q: '' },
    roads: { q: '', status: 'all' }
  }
};

const root = document.getElementById('view-root');
const pageTitle = document.getElementById('page-title');
const pageCrumbs = document.getElementById('page-crumbs');

/* ==========================================================================
 *  generic helpers
 * ========================================================================*/
function disposeMaps() {
  State.maps.forEach((m) => m.destroy());
  State.maps = [];
  State.routeFinder = null;
}

function errorCard(message, code) {
  return `<div class="card"><div class="card-body">
    <div class="notice notice-danger">
      <div>
        <strong>Something went wrong.</strong><br>
        ${escapeHtml(message || 'The backend request failed.')}
        ${code ? `<span class="mono tiny muted">(${escapeHtml(code)})</span>` : ''}
      </div>
    </div>
    <button class="btn btn-outline btn-sm" style="margin-top:12px" onclick="reloadView()">Try again</button>
  </div></div>`;
}

function reloadView() {
  location.reload();
}

function statCard({ label, value, suffix, meta, tone, iconName, compact }) {
  return `<div class="stat ${tone ? 'tone-' + tone : ''}">
    <div class="label">${iconName ? icon(iconName, 13) : ''}${escapeHtml(label)}</div>
    <div class="value"${compact ? ' style="font-size:19px"' : ''}>${escapeHtml(String(value))}${suffix ? `<small> ${escapeHtml(suffix)}</small>` : ''}</div>
    ${meta ? `<div class="meta">${meta}</div>` : ''}
  </div>`;
}

function updateBadges(counts) {
  const set = (name, value) => {
    const el = document.querySelector(`[data-badge="${name}"]`);
    if (!el) return;
    el.textContent = fmtInt(value);
    el.classList.remove('hidden');
  };
  if (counts.volunteers !== undefined) set('volunteers', counts.volunteers);
  if (counts.requests !== undefined) set('requests', counts.requests);
  if (counts.alerts !== undefined) {
    const el = document.querySelector('[data-badge="alerts"]');
    if (el) {
      el.textContent = fmtInt(counts.alerts);
      el.classList.toggle('hidden', counts.alerts === 0);
    }
  }
}

/** Donut chart for volunteer availability (server numbers only). */
function donutSvg(available, assigned, size = 132) {
  const total = Math.max(1, available + assigned);
  const r = size / 2 - 12;
  const c = 2 * Math.PI * r;
  const availLen = (available / total) * c;
  const assignedLen = c - availLen;
  return `<svg class="donut" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}" role="img"
      aria-label="${available} available and ${assigned} assigned volunteers">
    <g transform="translate(${size / 2},${size / 2}) rotate(-90)">
      <circle r="${r}" fill="none" stroke="#e2e8f0" stroke-width="16"></circle>
      <circle r="${r}" fill="none" stroke="#16a34a" stroke-width="16"
              stroke-dasharray="${availLen} ${c}" stroke-linecap="butt"></circle>
      <circle r="${r}" fill="none" stroke="#94a3b8" stroke-width="16"
              stroke-dasharray="${assignedLen} ${c}" stroke-dashoffset="${-availLen}"
              stroke-linecap="butt"></circle>
    </g>
    <text x="50%" y="47%" text-anchor="middle" font-size="20" font-weight="700" fill="#0b1524">${available}</text>
    <text x="50%" y="61%" text-anchor="middle" font-size="10" fill="#64748b">AVAILABLE</text>
  </svg>`;
}

function serviceBarRow(label, values, maxAll) {
  const total = values.volunteers;
  const avail = values.available;
  const assigned = Math.max(0, total - avail);
  const w = (v) => (maxAll > 0 ? (v / maxAll) * 100 : 0);
  return `<div class="bar-row">
    <span class="small" style="font-weight:600">${escapeHtml(label)}</span>
    <div class="bar-track" title="${avail} available · ${assigned} assigned">
      <span class="bar-fill avail" style="width:${w(avail)}%"></span>
      <span class="bar-fill assigned" style="width:${w(assigned)}%"></span>
    </div>
    <span class="bar-val">${avail}/${total}</span>
  </div>`;
}

/* ==========================================================================
 *  DASHBOARD
 * ========================================================================*/
async function renderDashboard() {
  const res = await API.get('/api/admin/dashboard');
  if (!res.ok) { root.innerHTML = errorCard(res.data.error, res.data.code); return; }
  const d = res.data;
  State.dashboard = d;
  const s = d.stats;

  updateBadges({
    volunteers: s.volunteers.total,
    requests: s.requests.total,
    alerts: (d.alerts || []).filter((a) => a.severity !== 'info').length
  });

  const alertsHtml = (d.alerts || []).length
    ? d.alerts.map((a) => `
      <div class="alert-item ${escapeHtml(a.severity)}">
        <div class="icon">${icon(a.severity === 'critical' ? 'alert' : a.severity === 'warning' ? 'clock' : 'map', 15)}</div>
        <div>
          <h4>${escapeHtml(a.title)}</h4>
          <p>${escapeHtml(a.message)}</p>
        </div>
        ${a.requestId ? `<button class="btn btn-sm btn-outline act" data-open-request="${a.requestId}">Open</button>` : ''}
      </div>`).join('')
    : `<div class="empty">${icon('check', 26)}<h3>No outstanding alerts</h3>
       <p class="small">Every active request is fully covered by available volunteer capacity.</p></div>`;

  const recentRows = (d.recentRequests || []).map((r) => `
    <tr class="${r.underCovered ? 'is-alert' : ''}">
      <td class="mono">#${r.id}</td>
      <td>${serviceBadge(r.service)}</td>
      <td>${escapeHtml(r.areaName)}</td>
      <td class="nowrap">${fmtInt(r.peopleCovered)} / ${fmtInt(r.peopleAffected)}
        <div style="margin-top:5px">${progressBar(r.peopleCovered, r.peopleAffected)}</div></td>
      <td class="mono">${r.assignedVolunteerCount}</td>
      <td>${statusBadge(r.status)}</td>
      <td class="cell-actions"><button class="btn btn-sm btn-ghost" data-open-request="${r.id}">Details</button></td>
    </tr>`).join('');

  const maxService = Math.max(1, ...(d.serviceDistribution || []).map((x) => x.volunteers));

  root.innerHTML = `
    <div class="page-intro">
      <h2>Good ${greeting()}, ${escapeHtml(State.user.username)}</h2>
      <p>Live snapshot of the volunteer pool, citizen requests and road network. All figures are
         computed by the C++ backend from the CSV databases.</p>
    </div>

    <div class="stat-grid">
      ${statCard({ label: 'Total volunteers', value: fmtInt(s.volunteers.total), tone: 'info', iconName: 'users' })}
      ${statCard({ label: 'Available', value: fmtInt(s.volunteers.available), tone: 'ok', iconName: 'check', meta: 'ready to dispatch' })}
      ${statCard({ label: 'Assigned', value: fmtInt(s.volunteers.assigned), tone: 'warn', iconName: 'clock', meta: 'on active requests' })}
      ${statCard({ label: 'Total requests', value: fmtInt(s.requests.total), tone: 'info', iconName: 'clipboard' })}
      ${statCard({ label: 'Active requests', value: fmtInt(s.requests.active), tone: 'warn', iconName: 'clock' })}
      ${statCard({ label: 'Completed', value: fmtInt(s.requests.completed), tone: 'ok', iconName: 'check' })}
      ${statCard({ label: 'People affected', value: fmtInt(s.people.affectedTotal), tone: 'danger', iconName: 'alert', meta: `${fmtInt(s.people.affectedActive)} in active requests` })}
      ${statCard({ label: 'People covered', value: fmtInt(s.people.coveredTotal), tone: 'ok', iconName: 'heart', meta: `${s.people.coveragePct}% of all affected` })}
      ${statCard({ label: 'City areas', value: fmtInt(s.city.areas), tone: 'info', iconName: 'pin', meta: `${fmtInt(s.city.roads)} roads mapped` })}
      ${statCard({ label: 'Blocked roads', value: fmtInt(s.city.blockedRoads), tone: s.city.blockedRoads > 0 ? 'danger' : 'ok', iconName: 'cone', meta: `${fmtInt(s.city.openRoads)} open` })}
    </div>

    <div class="grid grid-2" style="margin-bottom:16px;align-items:start">
      <div class="card">
        <div class="card-head">
          <div>
            <h2>Action required</h2>
            <div class="sub">Requests that still need assistance, plus network warnings</div>
          </div>
          <span class="badge ${(d.alerts || []).length ? 'badge-warn' : 'badge-ok'}">${(d.alerts || []).length} alert(s)</span>
        </div>
        <div class="alert-list">${alertsHtml}</div>
      </div>

      <div class="card">
        <div class="card-head">
          <div><h2>Volunteer availability</h2><div class="sub">Pool state right now</div></div>
        </div>
        <div class="card-body">
          <div class="donut-wrap">
            ${donutSvg(s.volunteers.available, s.volunteers.assigned)}
            <div class="donut-legend">
              <div class="row"><span class="sw" style="background:#16a34a"></span> Available
                <span class="val">${fmtInt(s.volunteers.available)}</span></div>
              <div class="row"><span class="sw" style="background:#94a3b8"></span> Assigned
                <span class="val">${fmtInt(s.volunteers.assigned)}</span></div>
              <div class="row" style="border-top:1px solid var(--slate-200);padding-top:8px">
                <span class="small muted">Coverage of active need</span>
                <span class="val">${s.people.affectedActive > 0
                  ? Math.round((s.people.coveredActive / s.people.affectedActive) * 100) + '%' : '—'}</span>
              </div>
            </div>
          </div>

          <div style="margin-top:20px">
            <div class="small muted" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:10px">
              Service-wise volunteers (available / total)
            </div>
            <div class="bars">
              ${(d.serviceDistribution || []).map((row) =>
                serviceBarRow(row.service, row, maxService)).join('')}
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="card" style="margin-bottom:16px">
      <div class="card-head">
        <div><h2>Recent disaster requests</h2><div class="sub">Newest six, straight from requests_db.csv</div></div>
        <a class="btn btn-sm btn-outline" href="#/requests">View all requests</a>
      </div>
      <div class="table-wrap">
        <table class="data">
          <thead><tr>
            <th>ID</th><th>Service</th><th>Area</th><th>Coverage</th><th>Vols</th><th>Status</th><th></th>
          </tr></thead>
          <tbody>${recentRows || `<tr><td colspan="7">${emptyState('No requests yet', 'Citizen submissions will appear here.')}</td></tr>`}</tbody>
        </table>
      </div>
    </div>

    <div class="card">
      <div class="card-head"><div><h2>Quick actions</h2><div class="sub">Common operations</div></div></div>
      <div class="card-body">
        <div class="btn-row">
          <a class="btn btn-primary" href="#/volunteers">${icon('plus', 15)} Add volunteer</a>
          <a class="btn btn-outline" href="#/requests">${icon('clipboard', 15)} Triage requests</a>
          <a class="btn btn-outline" href="#/city">${icon('map', 15)} Open city network</a>
          <a class="btn btn-outline" href="#/route">${icon('route', 15)} Find shortest route</a>
          <a class="btn btn-outline" href="#/roads">${icon('cone', 15)} Manage roads</a>
          <button class="btn btn-outline" id="dash-reset">${icon('refresh', 15)} Reset availability</button>
        </div>
      </div>
    </div>`;

  root.querySelectorAll('[data-open-request]').forEach((btn) => {
    btn.addEventListener('click', () => openRequestDetail(Number(btn.getAttribute('data-open-request'))));
  });
  const resetBtn = root.querySelector('#dash-reset');
  if (resetBtn) resetBtn.addEventListener('click', resetAvailability);
}

function greeting() {
  const h = new Date().getHours();
  if (h < 12) return 'morning';
  if (h < 17) return 'afternoon';
  return 'evening';
}

/* ==========================================================================
 *  VOLUNTEERS
 * ========================================================================*/
async function renderVolunteers() {
  const [volRes, areaRes] = await Promise.all([API.get('/api/admin/volunteers'), API.get('/api/areas')]);
  if (!volRes.ok) { root.innerHTML = errorCard(volRes.data.error, volRes.data.code); return; }
  State.volunteers = volRes.data.volunteers || [];
  State.areas = (areaRes.data.areas || []);

  updateBadges({ volunteers: State.volunteers.length });
  const f = State.filters.volunteers;

  root.innerHTML = `
    <div class="page-intro">
      <h2>Volunteer management</h2>
      <p>Every registered volunteer with the service they provide and how many people they can
         handle. Volunteers assigned to an active request are never marked available.</p>
    </div>

    <div class="card" style="margin-bottom:16px">
      <div class="filters">
        <div class="search-wrap">
          ${icon('search', 15)}
          <input type="search" id="vol-search" placeholder="Search name, area or service…" value="${escapeHtml(f.q)}">
          </div>
          <select id="vol-service" style="width:auto;min-width:150px">
            <option value="">All services</option>
            <option ${f.service === 'Rescue' ? 'selected' : ''}>Rescue</option>
            <option ${f.service === 'Medical Help' ? 'selected' : ''}>Medical Help</option>
            <option ${f.service === 'Food' ? 'selected' : ''}>Food</option>
          </select>
          <select id="vol-area" style="width:auto;min-width:160px">
            <option value="">All areas</option>
            ${State.areas.map((a) => `<option value="${a.id}" ${String(f.area) === String(a.id) ? 'selected' : ''}>${escapeHtml(a.name)}</option>`).join('')}
          </select>
          <div class="chip-row">
            ${['all', 'available', 'assigned'].map((k) => `
              <button class="chip ${f.status === k ? 'on' : ''}" data-vol-status="${k}">
                ${k === 'all' ? 'All' : k === 'available' ? 'Available' : 'Assigned'}
              </button>`).join('')}
          </div>
        </div>
      <div class="table-wrap" id="vol-table"></div>
    </div>

    <div class="grid grid-2" style="align-items:start">
        <div class="card">
          <div class="card-head"><div><h2>Add volunteer</h2><div class="sub">Saved to volunteers_db.csv</div></div></div>
          <div class="card-body">
            <form id="add-vol-form" novalidate>
              <div class="field">
                <label for="nv-name">Full name</label>
                <input type="text" id="nv-name" maxlength="60" placeholder="e.g. Asha Kumari" required>
                <div class="err hidden" data-err="name"></div>
              </div>
              <div class="field">
                <label for="nv-area">Base area</label>
                <select id="nv-area" required>
                  ${State.areas.map((a) => `<option value="${a.id}">${escapeHtml(a.name)} (#${a.id})</option>`).join('')}
                </select>
              </div>
              <div class="field">
                <label for="nv-service">Service specialization</label>
                <select id="nv-service-new" required>
                  <option>Rescue</option>
                  <option>Medical Help</option>
                  <option>Food</option>
                </select>
              </div>
              <div class="field">
                <label for="nv-capacity">People-handling capacity</label>
                <input type="number" id="nv-capacity" min="1" max="100000" step="1" value="25" required>
                <div class="hint">How many affected people this volunteer can assist.</div>
                <div class="err hidden" data-err="capacity"></div>
              </div>
              <button class="btn btn-primary btn-block" type="submit" id="add-vol-btn">Register volunteer</button>
            </form>
          </div>
        </div>

        <div class="card">
          <div class="card-head"><div><h2>Bulk availability</h2><div class="sub">Release idle volunteers</div></div></div>
          <div class="card-body">
            <p class="small muted">Marks every volunteer as available <strong>except</strong> those still
              assigned to an active request — the backend protects active assignments.</p>
            <button class="btn btn-outline btn-block" id="reset-vol" style="margin-top:12px">
              ${icon('refresh', 15)} Reset availability
            </button>
          </div>
        </div>
      </div>
    </div>`;

  renderVolunteerTable();

  const search = root.querySelector('#vol-search');
  search.addEventListener('input', debounce((e) => {
    State.filters.volunteers.q = e.target.value.trim().toLowerCase();
    renderVolunteerTable();
  }, 120));
  root.querySelector('#vol-service').addEventListener('change', (e) => {
    State.filters.volunteers.service = e.target.value;
    renderVolunteerTable();
  });
  root.querySelector('#vol-area').addEventListener('change', (e) => {
    State.filters.volunteers.area = e.target.value;
    renderVolunteerTable();
  });
  root.querySelectorAll('[data-vol-status]').forEach((btn) => {
    btn.addEventListener('click', () => {
      State.filters.volunteers.status = btn.getAttribute('data-vol-status');
      root.querySelectorAll('[data-vol-status]').forEach((b) => b.classList.toggle('on', b === btn));
      renderVolunteerTable();
    });
  });

  root.querySelector('#add-vol-form').addEventListener('submit', addVolunteer);
  root.querySelector('#reset-vol').addEventListener('click', resetAvailability);
}

function volunteerSort(list) {
  const { sortKey, sortDir } = State.filters.volunteers;
  const dir = sortDir === 'asc' ? 1 : -1;
  return list.slice().sort((a, b) => {
    let av = a[sortKey], bv = b[sortKey];
    if (sortKey === 'areaName' || sortKey === 'name' || sortKey === 'service' || sortKey === 'status') {
      return String(av).localeCompare(String(bv)) * dir;
    }
    return (Number(av) - Number(bv)) * dir;
  });
}

function renderVolunteerTable() {
  const f = State.filters.volunteers;
  let list = State.volunteers.filter((v) => {
    if (f.service && v.service !== f.service) return false;
    if (f.area !== '' && String(v.areaId) !== String(f.area)) return false;
    if (f.status === 'available' && !v.available) return false;
    if (f.status === 'assigned' && v.available) return false;
    if (f.q) {
      const hay = `${v.name} ${v.areaName} ${v.service} #${v.id}`.toLowerCase();
      if (hay.indexOf(f.q) === -1) return false;
    }
    return true;
  });
  list = volunteerSort(list);

  const th = (key, label) => {
    const active = f.sortKey === key;
    const arrow = active ? (f.sortDir === 'asc' ? '▲' : '▼') : '';
    return `<th class="sortable" data-sort="${key}">${label}<span class="arrow">${arrow}</span></th>`;
  };

  const rows = list.map((v) => `
    <tr>
      <td class="mono">#${v.id}</td>
      <td class="cell-strong nowrap">${escapeHtml(v.name)}</td>
      <td class="nowrap">${escapeHtml(v.areaName)}<div class="cell-sub">id #${v.areaId}</div></td>
      <td>${serviceBadge(v.service)}</td>
      <td class="mono">${fmtInt(v.capacity)}</td>
      <td>${statusBadge(v.status)}
        ${v.assignedAreas && v.assignedAreas.length
          ? `<div class="cell-sub">helping: ${v.assignedAreas.map(escapeHtml).join(', ')}</div>` : ''}</td>
    </tr>`).join('');

  const table = document.getElementById('vol-table');
  if (!table) return;
  if (!list.length) {
    table.innerHTML = emptyState('No volunteers match those filters',
      'Adjust the search or filters, or register a new volunteer.');
    return;
  }
  table.innerHTML = `<table class="data">
    <thead><tr>
      ${th('id', 'ID')}${th('name', 'Name')}${th('areaName', 'Base area')}
      ${th('service', 'Service')}${th('capacity', 'Capacity')}${th('status', 'Status')}
    </tr></thead>
    <tbody>${rows}</tbody></table>
    <div class="card-foot">Showing ${list.length} of ${State.volunteers.length} volunteers</div>`;

  table.querySelectorAll('[data-sort]').forEach((cell) => {
    cell.addEventListener('click', () => {
      const key = cell.getAttribute('data-sort');
      const f2 = State.filters.volunteers;
      if (f2.sortKey === key) f2.sortDir = f2.sortDir === 'asc' ? 'desc' : 'asc';
      else { f2.sortKey = key; f2.sortDir = 'asc'; }
      renderVolunteerTable();
    });
  });
}

async function addVolunteer(event) {
  event.preventDefault();
  const name = document.getElementById('nv-name');
  const areaId = document.getElementById('nv-area').value;
  const service = document.getElementById('nv-service-new').value;
  const capacity = document.getElementById('nv-capacity').value;

  ['name', 'capacity'].forEach((k) => {
    const n = document.querySelector(`[data-err="${k}"]`);
    if (n) { n.classList.add('hidden'); }
    const input = document.getElementById(k === 'name' ? 'nv-name' : 'nv-capacity');
    if (input) input.classList.remove('invalid');
  });

  let invalid = false;
  if (!name.value.trim() || name.value.trim().length < 2) {
    const n = document.querySelector('[data-err="name"]');
    n.textContent = 'Enter the volunteer name (at least 2 characters).';
    n.classList.remove('hidden');
    name.classList.add('invalid');
    invalid = true;
  }
  if (!capacity || Number(capacity) < 1) {
    const n = document.querySelector('[data-err="capacity"]');
    n.textContent = 'Capacity must be at least 1.';
    n.classList.remove('hidden');
    document.getElementById('nv-capacity').classList.add('invalid');
    invalid = true;
  }
  if (invalid) return;

  const btn = document.getElementById('add-vol-btn');
  btn.disabled = true; btn.classList.add('is-busy'); btn.textContent = 'Saving…';

  const res = await API.post('/api/admin/volunteers', {
    name: name.value.trim(),
    areaId: Number(areaId),
    service,
    capacity: Number(capacity)
  });

  btn.disabled = false; btn.classList.remove('is-busy'); btn.textContent = 'Register volunteer';

  if (!res.ok) {
    toast(res.data.error || 'Could not add volunteer.', 'error');
    return;
  }
  toast(`${res.data.volunteer.name} registered in ${res.data.volunteer.areaName}.`, 'ok');
  renderVolunteers();
}

async function resetAvailability() {
  const ok = await confirmAction({
    title: 'Reset volunteer availability?',
    message: 'Every volunteer not currently assigned to an active request becomes available. ' +
             'Volunteers on active requests keep their assignment.',
    confirmLabel: 'Reset availability'
  });
  if (!ok) return;
  const res = await API.post('/api/admin/volunteers/reset');
  if (!res.ok) { toast(res.data.error || 'Reset failed.', 'error'); return; }
  toast(res.data.message, res.data.skippedAssigned > 0 ? 'warn' : 'ok');
  if (location.hash.indexOf('/volunteers') !== -1) renderVolunteers();
  else navigate(true);
}

/* ==========================================================================
 *  REQUESTS
 * ========================================================================*/
async function renderRequests() {
  const res = await API.get('/api/admin/requests');
  if (!res.ok) { root.innerHTML = errorCard(res.data.error, res.data.code); return; }
  State.requests = res.data.requests || [];
  updateBadges({ requests: State.requests.length });

  const pending = State.requests.filter((r) => r.status === 'Active' && r.peopleCovered < r.peopleAffected).length;

  root.innerHTML = `
    <div class="page-intro">
      <h2>Disaster request management</h2>
      <p>Triage every citizen request, inspect which volunteers were dispatched by the allocation
         algorithm, and close requests to release their volunteers.</p>
    </div>

    <div class="stat-grid">
      ${statCard({ label: 'Total requests', value: fmtInt(State.requests.length), tone: 'info', iconName: 'clipboard' })}
      ${statCard({ label: 'Active', value: fmtInt(State.requests.filter((r) => r.status === 'Active').length), tone: 'warn', iconName: 'clock' })}
      ${statCard({ label: 'Completed', value: fmtInt(State.requests.filter((r) => r.status === 'Completed').length), tone: 'ok', iconName: 'check' })}
      ${statCard({ label: 'Needs more help', value: fmtInt(pending), tone: pending ? 'danger' : 'ok', iconName: 'alert', meta: 'covered < affected' })}
    </div>

    <div class="card">
      <div class="tabs" id="req-tabs">
        <button class="tab active" data-tab="all">All <span class="count">${State.requests.length}</span></button>
        <button class="tab" data-tab="active">Active <span class="count">${State.requests.filter((r) => r.status === 'Active').length}</span></button>
        <button class="tab" data-tab="completed">Completed <span class="count">${State.requests.filter((r) => r.status === 'Completed').length}</span></button>
        <button class="tab" data-tab="needs">Needs assistance <span class="count">${pending}</span></button>
      </div>
      <div class="filters">
        <div class="search-wrap">
          ${icon('search', 15)}
          <input type="search" id="req-search" placeholder="Search area, service, owner or ID…"
                 value="${escapeHtml(State.filters.requests.q)}">
        </div>
      </div>
      <div class="table-wrap" id="req-table"></div>
    </div>`;

  renderRequestTable();

  root.querySelectorAll('#req-tabs .tab').forEach((tab) => {
    tab.addEventListener('click', () => {
      State.filters.requests.tab = tab.getAttribute('data-tab');
      root.querySelectorAll('#req-tabs .tab').forEach((t) => t.classList.toggle('active', t === tab));
      renderRequestTable();
    });
  });
  const search = root.querySelector('#req-search');
  search.addEventListener('input', debounce((e) => {
    State.filters.requests.q = e.target.value.trim().toLowerCase();
    renderRequestTable();
  }, 120));
}

function renderRequestTable() {
  const { tab, q } = State.filters.requests;
  const list = State.requests.filter((r) => {
    if (tab === 'active' && r.status !== 'Active') return false;
    if (tab === 'completed' && r.status !== 'Completed') return false;
    if (tab === 'needs' && !(r.status === 'Active' && r.peopleCovered < r.peopleAffected)) return false;
    if (q) {
      const hay = `${r.id} ${r.service} ${r.areaName} ${r.owner} ${r.message}`.toLowerCase();
      if (hay.indexOf(q) === -1) return false;
    }
    return true;
  });

  const table = document.getElementById('req-table');
  if (!table) return;
  if (!list.length) {
    table.innerHTML = emptyState('No requests in this view', 'Try a different tab or clear the search.');
    return;
  }

  table.innerHTML = `<table class="data">
    <thead><tr>
      <th>ID</th><th>Service</th><th>Area</th><th>Affected</th><th>Coverage</th>
      <th>Volunteers</th><th>Status</th><th>Owner</th><th></th>
    </tr></thead>
    <tbody>
    ${list.map((r) => `
      <tr class="${r.underCovered ? 'is-alert' : ''}">
        <td class="mono">#${r.id}</td>
        <td>${serviceBadge(r.service)}</td>
        <td class="cell-strong">${escapeHtml(r.areaName)}<div class="cell-sub">area #${r.areaId}</div></td>
        <td class="mono">${fmtInt(r.peopleAffected)}</td>
        <td style="min-width:150px">
          <div class="mono small">${fmtInt(r.peopleCovered)} / ${fmtInt(r.peopleAffected)}
            ${r.underCovered ? `<span class="badge badge-danger" style="margin-left:6px">${fmtInt(r.peopleRemaining)} waiting</span>` : ''}</div>
          <div style="margin-top:5px">${progressBar(r.peopleCovered, r.peopleAffected)}</div>
        </td>
        <td class="mono">${r.assignedVolunteerCount}</td>
        <td>${statusBadge(r.status)}</td>
        <td class="small">${r.owner ? escapeHtml(r.owner) : '<span class="muted">legacy</span>'}</td>
        <td class="cell-actions">
          <button class="btn btn-sm btn-ghost" data-view="${r.id}">Details</button>
          ${r.status === 'Active' ? `<button class="btn btn-sm btn-ok" data-complete="${r.id}">Complete</button>` : ''}
        </td>
      </tr>`).join('')}
    </tbody></table>
    <div class="card-foot">Showing ${list.length} request(s)</div>`;

  table.querySelectorAll('[data-view]').forEach((b) =>
    b.addEventListener('click', () => openRequestDetail(Number(b.getAttribute('data-view')))));
  table.querySelectorAll('[data-complete]').forEach((b) =>
    b.addEventListener('click', () => completeRequest(Number(b.getAttribute('data-complete')))));
}

async function openRequestDetail(id) {
  const res = await API.get(`/api/admin/requests/${id}`);
  if (!res.ok) { toast(res.data.error || 'Could not load request.', 'error'); return; }
  const r = res.data.request;

  const volunteerRows = (r.assignedVolunteers || []).length
    ? r.assignedVolunteers.map((v) => `
      <div class="vol-pill">
        <span class="av">${initials(v.name)}</span>
        <span>${escapeHtml(v.name)}${v.recordFound ? '' : ' (missing record)'}</span>
        <span class="cap">${v.capacity ? v.capacity + ' ppl' : ''}${v.areaName ? ' · ' + escapeHtml(v.areaName) : ''}</span>
      </div>`).join('')
    : '<p class="small muted">No volunteers were allocated to this request.</p>';

  const body = `
    <div class="detail-grid">
      <div>
        <dl class="kv">
          <dt>Request</dt><dd class="mono">#${r.id}</dd>
          <dt>Service</dt><dd>${serviceBadge(r.service)}</dd>
          <dt>Affected area</dt><dd>${escapeHtml(r.areaName)} (#${r.areaId})</dd>
          <dt>Status</dt><dd>${statusBadge(r.status)}</dd>
          <dt>Owner</dt><dd>${r.owner ? escapeHtml(r.owner) : '<span class="muted">legacy record</span>'}</dd>
        </dl>
      </div>
      <div>
        <dl class="kv">
          <dt>People affected</dt><dd class="mono">${fmtInt(r.peopleAffected)}</dd>
          <dt>People covered</dt><dd class="mono">${fmtInt(r.peopleCovered)}</dd>
          <dt>Still waiting</dt><dd class="mono">${fmtInt(r.peopleRemaining)}</dd>
          <dt>Volunteers</dt><dd class="mono">${r.assignedVolunteerCount}</dd>
        </dl>
      </div>
    </div>
    <div style="margin-top:14px">
      <div class="small muted" style="margin-bottom:6px">Coverage ${r.progressPct}%</div>
      ${progressBar(r.peopleCovered, r.peopleAffected)}
    </div>
    <div style="margin-top:18px">
      <div class="small muted" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:8px">Assigned volunteers</div>
      ${volunteerRows}
    </div>
    ${r.message ? `<div style="margin-top:18px">
      <div class="small muted" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:6px">Citizen message</div>
      <div class="notice notice-info">${escapeHtml(r.message)}</div></div>` : ''}
    ${r.underCovered ? `<div class="notice notice-warn" style="margin-top:16px">
      <span><strong>${fmtInt(r.peopleRemaining)} people still need assistance.</strong>
      No further suitable volunteer was available when this request was processed.</span></div>` : ''}`;

  await dialog({
    title: `Request #${r.id} · ${r.service}`,
    subtitle: `${r.areaName} · ${r.status}`,
    body,
    confirmLabel: r.status === 'Active' ? 'Mark completed' : 'Close',
    cancelLabel: 'Close',
    wide: true
  }).then((confirmed) => {
    if (confirmed && r.status === 'Active') completeRequest(r.id);
  });
}

async function completeRequest(id) {
  const ok = await confirmAction({
    title: `Complete request #${id}?`,
    message: 'The request is marked completed, its assigned volunteers become available again, and both CSV databases are updated.',
    confirmLabel: 'Mark completed'
  });
  if (!ok) return;

  const res = await API.patch(`/api/admin/requests/${id}/complete`);
  if (!res.ok) { toast(res.data.error || 'Could not complete request.', 'error'); return; }
  toast(res.data.message, 'ok');
  if (location.hash.indexOf('/requests') !== -1) renderRequests();
  else navigate(true);
}

/* ==========================================================================
 *  CITY NETWORK
 * ========================================================================*/
async function renderCity() {
  const res = await API.get('/api/graph');
  if (!res.ok) { root.innerHTML = errorCard(res.data.error, res.data.code); return; }
  const graph = res.data;
  State.graph = graph;

  root.innerHTML = `
    <div class="page-intro">
      <h2>City network</h2>
      <p>All ${graph.stats.areas} areas and ${graph.stats.roads} roads loaded from the backend graph.
         Click an area to inspect it, click a road to block or re-open it.</p>
    </div>

    <div class="stat-grid">
      ${statCard({ label: 'Areas', value: fmtInt(graph.stats.areas), tone: 'info', iconName: 'pin' })}
      ${statCard({ label: 'Roads', value: fmtInt(graph.stats.roads), tone: 'info', iconName: 'map' })}
      ${statCard({ label: 'Open roads', value: fmtInt(graph.stats.openRoads), tone: 'ok', iconName: 'check' })}
      ${statCard({ label: 'Blocked roads', value: fmtInt(graph.stats.blockedRoads), tone: graph.stats.blockedRoads ? 'danger' : 'ok', iconName: 'cone' })}
      ${statCard({ label: 'Connectivity', value: graph.stats.connected ? 'Connected' : 'Disconnected', tone: graph.stats.connected ? 'ok' : 'danger', iconName: 'route', compact: true })}
    </div>

    <div class="card" style="overflow:hidden;margin-bottom:16px">
      <div id="city-map"></div>
    </div>

    <div class="grid grid-2" style="align-items:start">
      <div class="card">
        <div class="card-head"><div><h2>Area inspector</h2><div class="sub">Click any node on the map</div></div></div>
        <div class="card-body" id="area-inspector">
          ${emptyState('No area selected', 'Select a node to see volunteers based there and its roads.', 'pin')}
        </div>
      </div>

      <div class="card">
        <div class="card-head">
          <div><h2>Blocked roads</h2><div class="sub">Skipped by every route calculation</div></div>
          <a class="btn btn-sm btn-outline" href="#/roads">Manage all roads</a>
        </div>
        <div class="card-body" id="blocked-list"></div>
      </div>
    </div>`;

  const map = new CityMap(document.getElementById('city-map'), {
    height: 520,
    interactive: true,
    showLegend: true,
    onAreaClick: (area) => showAreaInspector(area, map),
    onRoadClick: (road) => toggleRoad(road, () => renderCity())
  });
  map.setData(graph);
  State.maps.push(map);

  renderBlockedList(graph.roads);

  // highlight areas that currently have active requests
  API.get('/api/admin/requests').then((r) => {
    if (!r.ok) return;
    const ids = Array.from(new Set((r.data.requests || [])
      .filter((x) => x.status === 'Active')
      .map((x) => x.areaId)));
    map.setAffected(ids);
  });
}

function showAreaInspector(area, map) {
  const box = document.getElementById('area-inspector');
  const volunteers = State.volunteers.filter((v) => v.areaId === area.id);
  const roads = (State.graph.roads || []).filter((r) => r.fromAreaId === area.id || r.toAreaId === area.id);

  box.innerHTML = `
    <div style="display:flex;align-items:flex-start;gap:12px;margin-bottom:14px">
      <span class="avatar" style="background:var(--navy-800);color:#fff">${area.id}</span>
      <div>
        <h3 style="font-size:16px">${escapeHtml(area.name)}</h3>
        <div class="small muted">Area #${area.id} · schematic position ${area.x}, ${area.y}</div>
      </div>
    </div>
    <dl class="kv">
      <dt>Volunteers based here</dt><dd class="mono">${volunteers.length}</dd>
      <dt>Available now</dt><dd class="mono">${volunteers.filter((v) => v.available).length}</dd>
      <dt>Connected roads</dt><dd class="mono">${roads.length}</dd>
      <dt>Open roads</dt><dd class="mono">${roads.filter((r) => !r.blocked).length}</dd>
    </dl>
    <div style="margin-top:14px">
      ${volunteers.length ? volunteers.map((v) => `
        <div class="vol-pill"><span class="av">${initials(v.name)}</span>
        <span>${escapeHtml(v.name)}</span>
        <span class="cap">${escapeHtml(v.service)} · ${v.capacity} ppl</span></div>`).join('')
        : '<p class="small muted">No volunteers are based in this area.</p>'}
    </div>
    <button class="btn btn-outline btn-sm" style="margin-top:14px" id="route-from-area">
      ${icon('route', 14)} Find route from ${escapeHtml(area.name)}
    </button>`;

  const btn = box.querySelector('#route-from-area');
  if (btn) btn.addEventListener('click', () => {
    State.routePrefill = { from: area.id };
    location.hash = '#/route';
  });
}

function renderBlockedList(roads) {
  const box = document.getElementById('blocked-list');
  if (!box) return;
  const blocked = (roads || []).filter((r) => r.blocked);
  if (!blocked.length) {
    box.innerHTML = `<div class="notice notice-ok">
      <span>All ${(roads || []).length} roads are open. Dijkstra can use the full network.</span></div>`;
    return;
  }
  box.innerHTML = `<div class="table-wrap"><table class="data">
    <thead><tr><th>Road</th><th>Between</th><th>Km</th><th></th></tr></thead>
    <tbody>${blocked.map((r) => `
      <tr>
        <td class="mono">#${r.id}</td>
        <td>${escapeHtml(r.fromAreaName)} ↔ ${escapeHtml(r.toAreaName)}</td>
        <td class="mono">${fmtKm(r.distanceKm)}</td>
        <td class="cell-actions"><button class="btn btn-sm btn-outline" data-unblock="${r.id}">Unblock</button></td>
      </tr>`).join('')}</tbody></table></div>`;

  box.querySelectorAll('[data-unblock]').forEach((b) => {
    b.addEventListener('click', () => {
      const road = (State.graph.roads || []).find((r) => r.id === Number(b.getAttribute('data-unblock')));
      if (road) toggleRoad(road, () => renderCity());
    });
  });
}

async function toggleRoad(road, after) {
  const willBlock = !road.blocked;
  const ok = await confirmAction({
    title: willBlock ? `Block road #${road.id}?` : `Re-open road #${road.id}?`,
    message: willBlock
      ? `Traffic between ${road.fromAreaName} and ${road.toAreaName} will be closed. All shortest-path calculations will avoid this ${fmtKm(road.distanceKm)} km road until it is re-opened.`
      : `${road.fromAreaName} ↔ ${road.toAreaName} becomes usable again for route calculations.`,
    confirmLabel: willBlock ? 'Block road' : 'Re-open road',
    danger: willBlock
  });
  if (!ok) return;

  const res = await API.patch(`/api/admin/roads/${road.id}`, { blocked: willBlock });
  if (!res.ok) { toast(res.data.error || 'Could not update road.', 'error'); return; }
  toast(res.data.message, willBlock ? 'warn' : 'ok');
  if (typeof after === 'function') after();
  else renderCity();
}

/* ==========================================================================
 *  ROUTE FINDER
 * ========================================================================*/
async function renderRoute() {
  const [graphRes, areaRes] = await Promise.all([API.get('/api/graph'), API.get('/api/areas')]);
  if (!graphRes.ok) { root.innerHTML = errorCard(graphRes.data.error, graphRes.data.code); return; }
  State.graph = graphRes.data;
  State.areas = areaRes.data.areas || [];

  root.innerHTML = `
    <div class="page-intro">
      <h2>Shortest route finder</h2>
      <p>Routes are calculated by the original C++ Dijkstra implementation (custom linked-list
         priority queue over the adjacency list). Blocked roads are excluded automatically.</p>
    </div>

    <div class="card" style="margin-bottom:16px">
      <div class="card-head"><div><h2>Route calculation</h2>
        <div class="sub">Choose a source and a destination area</div></div>
        <span class="badge badge-neutral">GET /api/routes</span>
      </div>
      <div class="card-body">
        <form id="route-form" class="route-form" novalidate>
          <div class="field" style="margin:0">
            <label for="route-from">Source area</label>
            <select id="route-from"></select>
          </div>
          <div class="field" style="margin:0">
            <label for="route-to">Destination area</label>
            <select id="route-to"></select>
          </div>
          <div class="btn-row">
            <button class="btn btn-outline" type="button" id="route-swap" title="Swap source and destination">⇄</button>
            <button class="btn btn-primary" type="submit" id="route-submit">Find route</button>
          </div>
        </form>
        <div id="route-result"></div>
      </div>
      <div class="card-foot">
        The result shown here is exactly what the backend returns — the frontend never computes a path.
      </div>
    </div>

    <div class="card" style="overflow:hidden">
      <div id="route-map"></div>
    </div>`;

  const map = new CityMap(document.getElementById('route-map'), {
    height: 520, interactive: true, showLegend: true
  });
  map.setData(State.graph);
  State.maps.push(map);

  State.routeFinder = initRouteFinder({
    formId: 'route-form',
    fromId: 'route-from',
    toId: 'route-to',
    resultId: 'route-result',
    swapId: 'route-swap',
    map
  });
  State.routeFinder.setAreas(State.areas);

  if (State.routePrefill && State.routePrefill.from !== undefined) {
    const sel = document.getElementById('route-from');
    sel.value = String(State.routePrefill.from);
    State.routePrefill = null;
  }

  // mark areas with active requests for context
  const reqRes = await API.get('/api/admin/requests');
  if (reqRes.ok) {
    const ids = Array.from(new Set((reqRes.data.requests || [])
      .filter((r) => r.status === 'Active').map((r) => r.areaId)));
    map.setAffected(ids);
  }
}

/* ==========================================================================
 *  ROAD MANAGEMENT
 * ========================================================================*/
async function renderRoads() {
  const res = await API.get('/api/roads');
  if (!res.ok) { root.innerHTML = errorCard(res.data.error, res.data.code); return; }
  const roads = res.data.roads || [];
  State.graph = stateGraphWithRoads(roads);

  const blocked = roads.filter((r) => r.blocked).length;
  root.innerHTML = `
    <div class="page-intro">
      <h2>Road management</h2>
      <p>Blocking a road writes straight to <span class="mono">roads_db.csv</span> and immediately
         changes every shortest-path result. Only administrators can do this.</p>
    </div>

    <div class="stat-grid">
      ${statCard({ label: 'Roads', value: fmtInt(roads.length), tone: 'info', iconName: 'map' })}
      ${statCard({ label: 'Open', value: fmtInt(roads.length - blocked), tone: 'ok', iconName: 'check' })}
      ${statCard({ label: 'Blocked', value: fmtInt(blocked), tone: blocked ? 'danger' : 'ok', iconName: 'cone' })}
      ${statCard({ label: 'Total road length', value: fmtKm(roads.reduce((a, r) => a + r.distanceKm, 0)), suffix: 'km', tone: 'info', iconName: 'route' })}
    </div>

    <div class="card">
      <div class="filters">
        <div class="search-wrap">
          ${icon('search', 15)}
          <input type="search" id="road-search" placeholder="Search road id or area…" value="${escapeHtml(State.filters.roads.q)}">
        </div>
        <div class="chip-row">
          ${['all', 'open', 'blocked'].map((k) => `
            <button class="chip ${State.filters.roads.status === k ? 'on' : ''}" data-road-status="${k}">
              ${k.charAt(0).toUpperCase() + k.slice(1)}
            </button>`).join('')}
        </div>
      </div>
      <div class="table-wrap" id="road-table"></div>
    </div>`;

  const draw = () => {
    const { q, status } = State.filters.roads;
    let list = roads.filter((r) => {
      if (status === 'open' && r.blocked) return false;
      if (status === 'blocked' && !r.blocked) return false;
      if (q) {
        const hay = `${r.id} ${r.fromAreaName} ${r.toAreaName}`.toLowerCase();
        if (hay.indexOf(q) === -1) return false;
      }
      return true;
    });
    const table = document.getElementById('road-table');
    if (!list.length) {
      table.innerHTML = emptyState('No roads match', 'Clear the filters to see the whole network.');
      return;
    }
    table.innerHTML = `<table class="data">
      <thead><tr><th>Road</th><th>Starting area</th><th>Destination</th><th>Distance</th><th>Status</th><th></th></tr></thead>
      <tbody>${list.map((r) => `
        <tr class="${r.blocked ? 'is-critical' : ''}">
          <td class="mono">#${r.id}</td>
          <td class="cell-strong">${escapeHtml(r.fromAreaName)}<div class="cell-sub">area id #${r.fromAreaId}</div></td>
          <td class="cell-strong">${escapeHtml(r.toAreaName)}<div class="cell-sub">area id #${r.toAreaId}</div></td>
          <td class="mono">${fmtKm(r.distanceKm)} km</td>
          <td>${r.blocked
            ? '<span class="badge badge-danger"><span class="dot"></span>Blocked</span>'
            : '<span class="badge badge-ok"><span class="dot"></span>Open</span>'}</td>
          <td class="cell-actions">
            <button class="btn btn-sm ${r.blocked ? 'btn-ok' : 'btn-danger'}" data-road="${r.id}">
              ${r.blocked ? 'Unblock' : 'Block'}
            </button>
          </td>
        </tr>`).join('')}</tbody></table>
      <div class="card-foot">Showing ${list.length} of ${roads.length} roads</div>`;

    table.querySelectorAll('[data-road]').forEach((b) => {
      b.addEventListener('click', () => {
        const road = roads.find((r) => r.id === Number(b.getAttribute('data-road')));
        if (road) toggleRoad(road, () => renderRoads());
      });
    });
  };

  draw();
  root.querySelector('#road-search').addEventListener('input', debounce((e) => {
    State.filters.roads.q = e.target.value.trim().toLowerCase();
    draw();
  }, 120));
  root.querySelectorAll('[data-road-status]').forEach((chip) => {
    chip.addEventListener('click', () => {
      State.filters.roads.status = chip.getAttribute('data-road-status');
      root.querySelectorAll('[data-road-status]').forEach((c) => c.classList.toggle('on', c === chip));
      draw();
    });
  });
}

function stateGraphWithRoads(roads) {
  const g = State.graph ? Object.assign({}, State.graph) : { areas: [], roads: [] };
  g.roads = roads;
  return g;
}

/* ==========================================================================
 *  ACCOUNTS
 * ========================================================================*/
async function renderAccounts() {
  const res = await API.get('/api/admin/accounts');
  if (!res.ok) { root.innerHTML = errorCard(res.data.error, res.data.code); return; }
  const accounts = res.data.accounts || [];
  State.accounts = accounts;

  root.innerHTML = `
    <div class="page-intro">
      <h2>Account management</h2>
      <p>Only administrators can create further administrator accounts. Password hashes are never
         exposed by the API.</p>
    </div>

    <div class="split" style="align-items:start">
      <div class="card">
        <div class="card-head"><div><h2>Registered accounts</h2>
          <div class="sub">${accounts.length} account(s) · ${res.data.admins} administrator(s)</div></div></div>
        <div class="table-wrap">
          <table class="data">
            <thead><tr><th>Username</th><th>Role</th><th>Access</th></tr></thead>
            <tbody>${accounts.map((a) => `
              <tr>
                <td class="cell-strong">${escapeHtml(a.username)}
                  ${a.username === State.user.username ? '<span class="badge badge-neutral" style="margin-left:6px">you</span>' : ''}</td>
                <td>${a.isAdmin ? '<span class="badge badge-danger"><span class="dot"></span>ADMIN</span>'
                                : '<span class="badge badge-info"><span class="dot"></span>PERSON</span>'}</td>
                <td class="small muted">${a.isAdmin ? 'Full operations access' : 'Submit and track own requests'}</td>
              </tr>`).join('')}</tbody>
          </table>
        </div>
        <div class="card-foot">Accounts are stored in <span class="mono">users_db.csv</span> with PBKDF2 hashes.</div>
      </div>

      <div class="card">
        <div class="card-head"><div><h2>Create account</h2><div class="sub">Administrator action</div></div></div>
        <div class="card-body">
          <form id="create-account-form" novalidate>
            <div class="field">
              <label for="acc-username">Username</label>
              <input type="text" id="acc-username" autocomplete="off" placeholder="3–24 characters" required>
            </div>
            <div class="field">
              <label for="acc-password">Password</label>
              <input type="password" id="acc-password" autocomplete="new-password" placeholder="At least 6 characters" required>
            </div>
            <div class="field">
              <label for="acc-role">Role</label>
              <select id="acc-role">
                <option value="ADMIN">ADMIN — operations access</option>
                <option value="PERSON">PERSON — citizen account</option>
              </select>
            </div>
            <button class="btn btn-primary btn-block" type="submit" id="acc-btn">Create account</button>
          </form>
          <div class="notice notice-info" style="margin-top:14px">
            <span>Citizens can also self-register from the public page; self-registration always
              produces a PERSON account.</span>
          </div>
        </div>
      </div>
    </div>`;

  root.querySelector('#create-account-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('acc-username').value.trim();
    const password = document.getElementById('acc-password').value;
    const role = document.getElementById('acc-role').value;
    if (username.length < 3 || password.length < 6) {
      toast('Username needs 3+ characters and password 6+.', 'warn');
      return;
    }
    const btn = document.getElementById('acc-btn');
    btn.disabled = true; btn.classList.add('is-busy'); btn.textContent = 'Creating…';
    const r = await API.post('/api/admin/accounts', { username, password, role });
    btn.disabled = false; btn.classList.remove('is-busy'); btn.textContent = 'Create account';
    if (!r.ok) { toast(r.data.error || 'Could not create account.', 'error'); return; }
    toast(`${role} account "${username}" created.`, 'ok');
    renderAccounts();
  });
}

/* ==========================================================================
 *  SETTINGS
 * ========================================================================*/
async function renderSettings() {
  const [healthRes, metaRes] = await Promise.all([API.get('/api/health'), API.get('/api/meta')]);
  const health = healthRes.ok ? healthRes.data : {};
  const meta = metaRes.ok ? metaRes.data : {};

  root.innerHTML = `
    <div class="page-intro">
      <h2>Account settings</h2>
      <p>Change your password, review the demonstration disclaimer and check the backend status.</p>
    </div>

    <div class="grid grid-2" style="align-items:start">
      <div class="card">
        <div class="card-head"><div><h2>Change password</h2>
          <div class="sub">Signed in as ${escapeHtml(State.user.username)}</div></div></div>
        <div class="card-body">
          <form id="pw-form" novalidate>
            <div class="field">
              <label for="pw-current">Current password</label>
              <input type="password" id="pw-current" autocomplete="current-password" required>
            </div>
            <div class="field">
              <label for="pw-new">New password</label>
              <input type="password" id="pw-new" autocomplete="new-password" required>
              <div class="hint">At least 6 characters. All other sessions are signed out.</div>
            </div>
            <div class="field">
              <label for="pw-confirm">Confirm new password</label>
              <input type="password" id="pw-confirm" autocomplete="new-password" required>
            </div>
            <button class="btn btn-primary" type="submit" id="pw-btn">Update password</button>
          </form>
          <div class="notice notice-warn" style="margin-top:16px">
            <span>The demo administrator still uses the documented starter password
              (<span class="mono">admin123</span>). Change it before sharing this deployment.</span>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-head"><div><h2>System information</h2><div class="sub">Reported by the backend</div></div></div>
        <div class="card-body">
          <dl class="kv">
            <dt>Backend</dt><dd>${escapeHtml(meta.backend || 'C++ REST API')}</dd>
            <dt>Version</dt><dd class="mono">${escapeHtml(meta.version || '—')}</dd>
            <dt>Status</dt><dd>${health.status === 'ok'
              ? '<span class="badge badge-ok"><span class="dot"></span>online</span>'
              : '<span class="badge badge-danger">unreachable</span>'}</dd>
            <dt>Uptime</dt><dd class="mono">${health.uptimeSeconds !== undefined ? Math.floor(health.uptimeSeconds / 60) + ' min' : '—'}</dd>
            <dt>Areas / roads</dt><dd class="mono">${fmtInt(health.areas)} / ${fmtInt(health.roads)}</dd>
            <dt>Volunteers</dt><dd class="mono">${fmtInt(health.volunteers)}</dd>
            <dt>Requests</dt><dd class="mono">${fmtInt(health.requests)}</dd>
          </dl>
          <div class="notice notice-info" style="margin-top:16px">
            <span>${escapeHtml(meta.disclaimer || 'Demonstration system — not a certified emergency dispatch service.')}</span>
          </div>
        </div>
      </div>
    </div>

    <div class="card" style="margin-top:16px">
      <div class="card-head"><div><h2>Session</h2><div class="sub">Server-side session, HttpOnly cookie</div></div></div>
      <div class="card-body">
        <div class="btn-row">
          <button class="btn btn-outline" data-logout>${icon('logout', 15)} Sign out</button>
          <button class="btn btn-danger" id="pw-demo-reset">Reset demo password to admin123</button>
        </div>
        <p class="small muted" style="margin-top:10px">The reset button is a convenience for
          demonstration sessions only; it requires your current password.</p>
      </div>
    </div>`;

  root.querySelector('#pw-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const current = document.getElementById('pw-current').value;
    const next = document.getElementById('pw-new').value;
    const confirm = document.getElementById('pw-confirm').value;
    if (next.length < 6) { toast('New password must be at least 6 characters.', 'warn'); return; }
    if (next !== confirm) { toast('New passwords do not match.', 'warn'); return; }

    const btn = document.getElementById('pw-btn');
    btn.disabled = true; btn.classList.add('is-busy'); btn.textContent = 'Updating…';
    const res = await API.post('/api/auth/password', { currentPassword: current, newPassword: next });
    btn.disabled = false; btn.classList.remove('is-busy'); btn.textContent = 'Update password';
    if (!res.ok) { toast(res.data.error || 'Could not change password.', 'error'); return; }
    document.getElementById('pw-form').reset();
    toast(res.data.message, 'ok');
  });

  root.querySelector('#pw-demo-reset').addEventListener('click', async () => {
    const { value: current } = await inputDialog({
      title: 'Reset to the demo password',
      subtitle: 'Confirms your identity before restoring admin123',
      label: 'Current password',
      confirmLabel: 'Reset password'
    }) || {};
    if (!current) return;
    const res = await API.post('/api/auth/password', { currentPassword: current, newPassword: 'admin123' });
    if (!res.ok) { toast(res.data.error || 'Could not reset password.', 'error'); return; }
    toast('Password reset to the documented demo value.', 'warn');
  });
}

/** Small dialog with a single text input; resolves {value} or null. */
function inputDialog({ title, subtitle, label, type = 'password', confirmLabel = 'Save' }) {
  return new Promise((resolve) => {
    const backdrop = document.createElement('div');
    backdrop.className = 'modal-backdrop';
    backdrop.innerHTML = `
      <div class="modal" role="dialog" aria-modal="true">
        <div class="modal-head">
          <div style="flex:1"><h3>${escapeHtml(title)}</h3>
            ${subtitle ? `<div class="sub">${escapeHtml(subtitle)}</div>` : ''}</div>
          <button class="icon-btn" data-close aria-label="Close">${icon('close', 16)}</button>
        </div>
        <div class="modal-body">
          <div class="field" style="margin:0">
            <label for="dlg-input">${escapeHtml(label)}</label>
            <input type="${type}" id="dlg-input" autocomplete="off">
          </div>
        </div>
        <div class="modal-foot">
          <button class="btn btn-outline" data-close>Cancel</button>
          <button class="btn btn-primary" data-ok>${escapeHtml(confirmLabel)}</button>
        </div>
      </div>`;
    document.body.appendChild(backdrop);
    const input = backdrop.querySelector('#dlg-input');
    input.focus();
    const finish = (value) => { backdrop.remove(); resolve(value); };
    backdrop.addEventListener('click', (e) => {
      if (e.target === backdrop || e.target.closest('[data-close]')) finish(null);
      if (e.target.closest('[data-ok]')) finish({ value: input.value });
    });
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') finish({ value: input.value });
      if (e.key === 'Escape') finish(null);
    });
  });
}

/* ==========================================================================
 *  ROUTER
 * ========================================================================*/
const VIEWS = {
  dashboard: { title: 'Operations dashboard', crumbs: 'CivicShield · overview', render: renderDashboard },
  volunteers: { title: 'Volunteers', crumbs: 'Operations · volunteer pool', render: renderVolunteers },
  requests: { title: 'Disaster requests', crumbs: 'Operations · request triage', render: renderRequests },
  city: { title: 'City network', crumbs: 'City · network map', render: renderCity },
  route: { title: 'Route finder', crumbs: 'City · shortest path', render: renderRoute },
  roads: { title: 'Road management', crumbs: 'City · roads & closures', render: renderRoads },
  accounts: { title: 'Accounts', crumbs: 'Administration · accounts', render: renderAccounts },
  settings: { title: 'Account settings', crumbs: 'Administration · security', render: renderSettings }
};

function currentViewName() {
  const hash = location.hash.replace(/^#\/?/, '').split('?')[0];
  return VIEWS[hash] ? hash : 'dashboard';
}

async function navigate(force) {
  const name = currentViewName();
  const view = VIEWS[name];
  disposeMaps();
  document.querySelectorAll('[data-nav]').forEach((el) => {
    el.classList.toggle('active', el.getAttribute('data-nav') === name);
  });
  pageTitle.textContent = view.title;
  pageCrumbs.textContent = view.crumbs;
  document.title = view.title + ' · CivicShield';
  root.innerHTML = `<div class="card"><div class="card-body">
      <div class="skeleton" style="width:35%"></div>
      <div class="skeleton" style="width:65%;margin-top:10px"></div>
      <div class="skeleton" style="width:50%;margin-top:10px"></div>
    </div></div>`;
  try {
    await view.render(force);
  } catch (err) {
    root.innerHTML = errorCard('Unexpected frontend error: ' + err.message, 'frontend');
  }
  // close the mobile drawer after navigation
  const sidebar = document.querySelector('.sidebar');
  if (sidebar) sidebar.classList.remove('open');
  const scrim = document.querySelector('.sidebar-scrim');
  if (scrim) scrim.remove();
}

/* ==========================================================================
 *  BOOT
 * ========================================================================*/
(async function boot() {
  const user = await Session.guard('ADMIN');
  if (!user) return;
  State.user = user;
  setUserChrome(user);
  initShell();
  document.getElementById('refresh-btn').addEventListener('click', () => {
    toast('Refreshing from the backend…', 'info', 1400);
    navigate(true);
  });
  window.addEventListener('hashchange', () => navigate());
  navigate();
})();
