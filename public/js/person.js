/* ============================================================================
   person.js — citizen console
   Views: #/overview  #/submit  #/my-requests  #/route  #/settings
   Requests are always scoped server-side to the signed-in account.
   ========================================================================== */
'use strict';

const PState = {
  user: null,
  graph: null,
  areas: [],
  myRequests: [],
  maps: []
};

const root = document.getElementById('view-root');
const pageTitle = document.getElementById('page-title');
const pageCrumbs = document.getElementById('page-crumbs');

/* ---------------------------------------------------------------- helpers */
function disposeMaps() {
  PState.maps.forEach((m) => m.destroy());
  PState.maps = [];
}

function errorCard(message) {
  return `<div class="card"><div class="card-body">
    <div class="notice notice-danger"><span>${escapeHtml(message || 'The backend request failed.')}</span></div>
    <button class="btn btn-outline btn-sm" style="margin-top:12px" onclick="location.reload()">Reload</button>
  </div></div>`;
}

function statCard({ label, value, suffix, meta, tone, iconName }) {
  return `<div class="stat ${tone ? 'tone-' + tone : ''}">
    <div class="label">${iconName ? icon(iconName, 13) : ''}${escapeHtml(label)}</div>
    <div class="value">${escapeHtml(String(value))}${suffix ? `<small> ${escapeHtml(suffix)}</small>` : ''}</div>
    ${meta ? `<div class="meta">${meta}</div>` : ''}
  </div>`;
}

function greet() {
  const h = new Date().getHours();
  return h < 12 ? 'morning' : h < 17 ? 'afternoon' : 'evening';
}

async function loadMine() {
  const res = await API.get('/api/requests/my');
  PState.myRequests = res.ok ? (res.data.requests || []) : [];
  const badge = document.querySelector('[data-badge="requests"]');
  if (badge) {
    badge.textContent = fmtInt(PState.myRequests.length);
    badge.classList.toggle('hidden', PState.myRequests.length === 0);
  }
  return PState.myRequests;
}

/* ==========================================================================
 *  OVERVIEW
 * ========================================================================*/
async function renderOverview() {
  const [graphRes, areaRes] = await Promise.all([API.get('/api/graph'), API.get('/api/areas')]);
  if (!graphRes.ok) { root.innerHTML = errorCard(graphRes.data.error); return; }
  PState.graph = graphRes.data;
  PState.areas = areaRes.ok ? (areaRes.data.areas || []) : [];
  const mine = await loadMine();

  const active = mine.filter((r) => r.status === 'Active');
  const completed = mine.filter((r) => r.status === 'Completed');
  const covered = mine.reduce((sum, r) => sum + r.peopleCovered, 0);
  const affected = mine.reduce((sum, r) => sum + r.peopleAffected, 0);

  const rows = mine.slice(0, 5).map((r) => `
    <tr class="${r.underCovered ? 'is-alert' : ''}">
      <td class="mono">#${r.id}</td>
      <td>${serviceBadge(r.service)}</td>
      <td>${escapeHtml(r.areaName)}</td>
      <td style="min-width:150px">
        <div class="mono small">${fmtInt(r.peopleCovered)} / ${fmtInt(r.peopleAffected)}</div>
        <div style="margin-top:5px">${progressBar(r.peopleCovered, r.peopleAffected)}</div>
      </td>
      <td>${statusBadge(r.status)}</td>
      <td class="cell-actions"><button class="btn btn-sm btn-ghost" data-view="${r.id}">Details</button></td>
    </tr>`).join('');

  root.innerHTML = `
    <div class="page-intro">
      <h2>Good ${greet()}, ${escapeHtml(PState.user.username)}</h2>
      <p>Report an emergency, follow how many people have been covered by volunteers, or find the
         shortest route between two city areas.</p>
    </div>

    <div class="stat-grid">
      ${statCard({ label: 'My requests', value: fmtInt(mine.length), tone: 'info', iconName: 'clipboard' })}
      ${statCard({ label: 'Active', value: fmtInt(active.length), tone: 'warn', iconName: 'clock' })}
      ${statCard({ label: 'Completed', value: fmtInt(completed.length), tone: 'ok', iconName: 'check' })}
      ${statCard({ label: 'People covered', value: fmtInt(covered), tone: 'ok', iconName: 'heart', meta: affected ? `of ${fmtInt(affected)} reported` : 'no reports yet' })}
    </div>

    <div class="grid grid-2" style="align-items:start;margin-bottom:16px">
      <div class="card">
        <div class="card-head">
          <div><h2>Need help?</h2><div class="sub">Submit a disaster service request</div></div>
        </div>
        <div class="card-body">
          <p class="small muted">Choose a service, the affected area and how many people need
             assistance. The dispatch engine assigns the closest available volunteers
             automatically.</p>
          <div class="btn-row" style="margin-top:14px">
            <a class="btn btn-danger" href="#/submit">${icon('send', 15)} Submit request</a>
            <a class="btn btn-outline" href="#/route">${icon('route', 15)} Find shortest route</a>
          </div>
          ${affected > covered ? `<div class="notice notice-warn" style="margin-top:14px">
            <span><strong>${fmtInt(affected - covered)} people</strong> across your requests are
            still waiting for assistance. Administrators are working on it.</span></div>` : ''}
        </div>
      </div>

      <div class="card">
        <div class="card-head"><div><h2>Service categories</h2><div class="sub">What you can request</div></div></div>
        <div class="card-body">
          <div class="grid" style="gap:10px">
            <div style="display:flex;gap:10px;align-items:flex-start">
              ${serviceBadge('Rescue')}<span class="small muted">Search &amp; rescue teams, evacuation support.</span>
            </div>
            <div style="display:flex;gap:10px;align-items:flex-start">
              ${serviceBadge('Medical Help')}<span class="small muted">Paramedics, first aid and medicine.</span>
            </div>
            <div style="display:flex;gap:10px;align-items:flex-start">
              ${serviceBadge('Food')}<span class="small muted">Food packets, water and rations.</span>
            </div>
          </div>
          <div class="notice notice-info" style="margin-top:14px">
            <span>Coverage depends on how many volunteers are free at the time of your request.</span>
          </div>
        </div>
      </div>
    </div>

    <div class="card" style="margin-bottom:16px">
      <div class="card-head">
        <div><h2>My recent requests</h2><div class="sub">Only requests created by your account</div></div>
        <a class="btn btn-sm btn-outline" href="#/my-requests">View all</a>
      </div>
      <div class="table-wrap">
        <table class="data">
          <thead><tr><th>ID</th><th>Service</th><th>Area</th><th>Coverage</th><th>Status</th><th></th></tr></thead>
          <tbody>${rows || `<tr><td colspan="6">${emptyState('No requests yet',
            'Use “Submit request” to report an emergency in your area.', 'clipboard')}</td></tr>`}</tbody>
        </table>
      </div>
    </div>

    <div class="card" style="overflow:hidden">
      <div class="card-head"><div><h2>City network</h2>
        <div class="sub">Your affected areas are highlighted in red</div></div></div>
      <div id="person-map"></div>
    </div>`;

  const map = new CityMap(document.getElementById('person-map'), {
    height: 480, interactive: true, showLegend: true
  });
  map.setData(PState.graph);
  map.setAffected(Array.from(new Set(active.map((r) => r.areaId))));
  PState.maps.push(map);

  bindRequestDetailButtons();
}

function bindRequestDetailButtons() {
  root.querySelectorAll('[data-view]').forEach((btn) => {
    btn.addEventListener('click', () => openMyRequest(Number(btn.getAttribute('data-view'))));
  });
}

/* ==========================================================================
 *  SUBMIT REQUEST
 * ========================================================================*/
async function renderSubmit() {
  const graphRes = await API.get('/api/graph');
  if (!graphRes.ok) { root.innerHTML = errorCard(graphRes.data.error); return; }
  PState.graph = graphRes.data;
  const areas = graphRes.data.areas || [];
  PState.areas = areas;

  root.innerHTML = `
    <div class="page-intro">
      <h2>Submit a disaster request</h2>
      <p>The C++ dispatch engine allocates volunteers from the affected area first, then the
         nearest suitable volunteers from other areas.</p>
    </div>

    <div class="grid grid-2" style="align-items:start">
      <div class="card">
        <div class="card-head"><div><h2>Request details</h2><div class="sub">All fields except the message are required</div></div></div>
        <div class="card-body">
          <form id="req-form" novalidate>
            <div class="field">
              <label for="rq-service">Service category</label>
              <select id="rq-service" required>
                <option value="Rescue">Rescue</option>
                <option value="Medical Help">Medical Help</option>
                <option value="Food">Food</option>
              </select>
            </div>
            <div class="field">
              <label for="rq-area">Affected area</label>
              <select id="rq-area" required>
                ${areas.map((a) => `<option value="${a.id}">${escapeHtml(a.name)} (#${a.id})</option>`).join('')}
              </select>
              <div class="hint">Only areas in the city graph can be serviced.</div>
            </div>
            <div class="field">
              <label for="rq-people">Number of affected people</label>
              <input type="number" id="rq-people" min="1" max="100000" step="1" value="25" required>
              <div class="err hidden" data-err="people"></div>
            </div>
            <div class="field">
              <label for="rq-message">Additional message (optional)</label>
              <textarea id="rq-message" maxlength="500"
                placeholder="Access details, landmarks, vulnerable people, medicines required…"></textarea>
              <div class="hint"><span id="rq-count">0</span>/500 characters</div>
            </div>
            <button class="btn btn-danger btn-block" type="submit" id="rq-submit">
              ${icon('send', 15)} Submit request
            </button>
          </form>
          <div class="notice notice-warn" style="margin-top:14px">
            <span>Demonstration system — do not rely on it in a real emergency.</span>
          </div>
        </div>
      </div>

      <div class="card" id="rq-outcome">
        <div class="card-head"><div><h2>What happens next</h2><div class="sub">Dispatch pipeline</div></div></div>
        <div class="card-body">
          <ol class="small" style="padding-left:18px;display:grid;gap:9px;color:var(--slate-600)">
            <li>Your request is recorded with a unique ID and linked to your account.</li>
            <li>Volunteers of the required service already in the area are dispatched first.</li>
            <li>If more help is needed, Dijkstra's algorithm ranks volunteers in other areas by
                shortest travel distance.</li>
            <li>Every assigned volunteer is marked unavailable until an administrator closes the
                request.</li>
            <li>You can follow the coverage progress under “My requests”.</li>
          </ol>
        </div>
        <div class="card-foot">Toggle to the “My requests” page afterwards to track progress.</div>
      </div>
    </div>`;

  const msg = document.getElementById('rq-message');
  const counter = document.getElementById('rq-count');
  msg.addEventListener('input', () => { counter.textContent = msg.value.length; });

  document.getElementById('req-form').addEventListener('submit', submitRequest);
}

async function submitRequest(event) {
  event.preventDefault();
  const service = document.getElementById('rq-service').value;
  const areaId = Number(document.getElementById('rq-area').value);
  const peopleInput = document.getElementById('rq-people');
  const message = document.getElementById('rq-message').value;
  const people = Number(peopleInput.value);

  const errNode = document.querySelector('[data-err="people"]');
  errNode.classList.add('hidden');
  peopleInput.classList.remove('invalid');
  if (!people || people < 1 || people > 100000) {
    errNode.textContent = 'Enter a number between 1 and 100000.';
    errNode.classList.remove('hidden');
    peopleInput.classList.add('invalid');
    return;
  }

  const btn = document.getElementById('rq-submit');
  btn.disabled = true; btn.classList.add('is-busy');
  const res = await API.post('/api/requests', { service, areaId, peopleAffected: people, message });
  btn.disabled = false; btn.classList.remove('is-busy');

  if (!res.ok) { toast(res.data.error || 'Could not submit the request.', 'error'); return; }

  const r = res.data.request;
  const allocations = res.data.allocations || [];
  toast(`Request #${r.id} submitted.`, 'ok');

  const outcome = document.getElementById('rq-outcome');
  outcome.innerHTML = `
    <div class="card-head">
      <div><h2>Request #${r.id} submitted</h2>
        <div class="sub">${escapeHtml(r.service)} · ${escapeHtml(r.areaName)}</div></div>
      <span class="badge badge-ok"><span class="dot"></span>Recorded</span>
    </div>
    <div class="card-body">
      <dl class="kv">
        <dt>Request ID</dt><dd class="mono">#${r.id}</dd>
        <dt>Service</dt><dd>${serviceBadge(r.service)}</dd>
        <dt>Affected area</dt><dd>${escapeHtml(r.areaName)}</dd>
        <dt>People affected</dt><dd class="mono">${fmtInt(r.peopleAffected)}</dd>
        <dt>People covered</dt><dd class="mono">${fmtInt(r.peopleCovered)}</dd>
        <dt>Remaining need</dt><dd class="mono">${fmtInt(r.peopleRemaining)}</dd>
        <dt>Volunteers assigned</dt><dd class="mono">${allocations.length}</dd>
      </dl>
      <div style="margin-top:12px">${progressBar(r.peopleCovered, r.peopleAffected)}</div>

      <div style="margin-top:16px">
        <div class="small muted" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:8px">
          Volunteers dispatched by the C++ engine
        </div>
        ${allocations.length ? allocations.map((a) => `
          <div class="vol-pill">
            <span class="av">${initials(a.name)}</span>
            <span>${escapeHtml(a.name)}</span>
            <span class="cap">${escapeHtml(a.areaName)} · ${a.capacity} ppl
              · ${a.local ? 'already in area' : fmtKm(a.distanceKm) + ' km away'}</span>
          </div>`).join('')
          : '<p class="small muted">No suitable volunteer was available. An administrator will review this request.</p>'}
      </div>

      ${r.peopleRemaining > 0 ? `<div class="notice notice-warn" style="margin-top:14px">
        <span><strong>${fmtInt(r.peopleRemaining)} people still need assistance.</strong>
        All reachable volunteers of this service are already assigned.</span></div>`
        : `<div class="notice notice-ok" style="margin-top:14px">
        <span>All affected people are covered by the volunteer capacity dispatched.</span></div>`}
    </div>
    <div class="card-foot">
      <div class="btn-row">
        <a class="btn btn-sm btn-primary" href="#/my-requests">Track my requests</a>
        <button class="btn btn-sm btn-outline" id="rq-again">Submit another request</button>
      </div>
    </div>`;

  document.getElementById('rq-again').addEventListener('click', () => renderSubmit());
  await loadMine();
}

/* ==========================================================================
 *  MY REQUESTS
 * ========================================================================*/
async function renderMyRequests() {
  const mine = await loadMine();

  root.innerHTML = `
    <div class="page-intro">
      <h2>My requests</h2>
      <p>Requests submitted by ${escapeHtml(PState.user.username)}. Coverage updates as
         administrators complete operations and release volunteers.</p>
    </div>

    <div class="stat-grid">
      ${statCard({ label: 'Total', value: fmtInt(mine.length), tone: 'info', iconName: 'clipboard' })}
      ${statCard({ label: 'Active', value: fmtInt(mine.filter((r) => r.status === 'Active').length), tone: 'warn', iconName: 'clock' })}
      ${statCard({ label: 'Needs assistance', value: fmtInt(mine.filter((r) => r.underCovered).length), tone: mine.some((r) => r.underCovered) ? 'danger' : 'ok', iconName: 'alert' })}
      ${statCard({ label: 'Completed', value: fmtInt(mine.filter((r) => r.status === 'Completed').length), tone: 'ok', iconName: 'check' })}
    </div>

    <div class="card">
      <div class="card-head">
        <div><h2>Request history</h2><div class="sub">Newest first</div></div>
        <a class="btn btn-sm btn-danger" href="#/submit">${icon('plus', 14)} New request</a>
      </div>
      <div class="table-wrap">
        <table class="data">
          <thead><tr>
            <th>ID</th><th>Service</th><th>Area</th><th>Affected</th><th>Coverage</th>
            <th>Volunteers</th><th>Status</th><th></th>
          </tr></thead>
          <tbody>
          ${mine.length ? mine.map((r) => `
            <tr class="${r.underCovered ? 'is-alert' : ''}">
              <td class="mono">#${r.id}</td>
              <td>${serviceBadge(r.service)}</td>
              <td class="cell-strong">${escapeHtml(r.areaName)}</td>
              <td class="mono">${fmtInt(r.peopleAffected)}</td>
              <td style="min-width:160px">
                <div class="mono small">${fmtInt(r.peopleCovered)} / ${fmtInt(r.peopleAffected)}
                  ${r.underCovered ? `<span class="badge badge-danger" style="margin-left:6px">${fmtInt(r.peopleRemaining)} waiting</span>` : ''}</div>
                <div style="margin-top:5px">${progressBar(r.peopleCovered, r.peopleAffected)}</div>
              </td>
              <td class="mono">${r.assignedVolunteerCount}</td>
              <td>${statusBadge(r.status)}</td>
              <td class="cell-actions"><button class="btn btn-sm btn-ghost" data-view="${r.id}">Details</button></td>
            </tr>`).join('')
            : `<tr><td colspan="8">${emptyState('You have not submitted any requests',
                'When you report an emergency it will appear here with live coverage.', 'clipboard')}</td></tr>`}
          </tbody>
        </table>
      </div>
      <div class="card-foot">Requests are stored in <span class="mono">requests_db.csv</span> with your account as owner.</div>
    </div>`;

  bindRequestDetailButtons();
}

async function openMyRequest(id) {
  const res = await API.get(`/api/requests/${id}`);
  if (!res.ok) { toast(res.data.error || 'Could not load the request.', 'error'); return; }
  const r = res.data.request;

  const volunteers = (r.assignedVolunteers || []).map((v) => `
    <div class="vol-pill">
      <span class="av">${initials(v.name)}</span>
      <span>${escapeHtml(v.name)}</span>
      <span class="cap">${v.capacity ? v.capacity + ' ppl' : ''}${v.areaName ? ' · ' + escapeHtml(v.areaName) : ''}</span>
    </div>`).join('');

  await dialog({
    title: `Request #${r.id}`,
    subtitle: `${r.service} · ${r.areaName} · ${r.status}`,
    confirmLabel: 'Close',
    hideCancel: true,
    wide: true,
    body: `
      <dl class="kv">
        <dt>Status</dt><dd>${statusBadge(r.status)}</dd>
        <dt>People affected</dt><dd class="mono">${fmtInt(r.peopleAffected)}</dd>
        <dt>People covered</dt><dd class="mono">${fmtInt(r.peopleCovered)} (${r.progressPct}%)</dd>
        <dt>Still waiting</dt><dd class="mono">${fmtInt(r.peopleRemaining)}</dd>
        <dt>Volunteers</dt><dd class="mono">${r.assignedVolunteerCount}</dd>
      </dl>
      <div style="margin-top:12px">${progressBar(r.peopleCovered, r.peopleAffected)}</div>
      <div style="margin-top:16px">
        <div class="small muted" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:8px">
          Assigned volunteers
        </div>
        ${volunteers || '<p class="small muted">No volunteers assigned yet.</p>'}
      </div>
      ${r.message ? `<div style="margin-top:16px">
        <div class="small muted" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:6px">Your message</div>
        <div class="notice notice-info">${escapeHtml(r.message)}</div></div>` : ''}`
  });
}

/* ==========================================================================
 *  ROUTE FINDER
 * ========================================================================*/
async function renderRouteView() {
  const [graphRes, areaRes] = await Promise.all([API.get('/api/graph'), API.get('/api/areas')]);
  if (!graphRes.ok) { root.innerHTML = errorCard(graphRes.data.error); return; }
  PState.graph = graphRes.data;
  PState.areas = areaRes.ok ? (areaRes.data.areas || []) : [];
  const mine = await loadMine();

  root.innerHTML = `
    <div class="page-intro">
      <h2>Shortest route</h2>
      <p>Distance between two city areas, calculated by the C++ Dijkstra implementation.
         Blocked roads are automatically avoided.</p>
    </div>

    <div class="card" style="margin-bottom:16px">
      <div class="card-head"><div><h2>Route calculation</h2><div class="sub">Pick a source and destination</div></div></div>
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
            <button class="btn btn-outline" type="button" id="route-swap" title="Swap">⇄</button>
            <button class="btn btn-primary" type="submit">Find route</button>
          </div>
        </form>
        <div id="route-result"></div>
      </div>
    </div>

    <div class="card" style="overflow:hidden">
      <div id="route-map"></div>
    </div>`;

  const map = new CityMap(document.getElementById('route-map'), {
    height: 500, interactive: true, showLegend: true
  });
  map.setData(PState.graph);
  map.setAffected(Array.from(new Set(mine.filter((r) => r.status === 'Active').map((r) => r.areaId))));
  PState.maps.push(map);

  const finder = initRouteFinder({
    formId: 'route-form', fromId: 'route-from', toId: 'route-to',
    resultId: 'route-result', swapId: 'route-swap', map
  });
  finder.setAreas(PState.areas);
}

/* ==========================================================================
 *  SETTINGS
 * ========================================================================*/
async function renderPersonSettings() {
  const metaRes = await API.get('/api/meta');
  const meta = metaRes.ok ? metaRes.data : {};

  root.innerHTML = `
    <div class="page-intro">
      <h2>Account settings</h2>
      <p>Manage your password and review the demonstration notice.</p>
    </div>

    <div class="grid grid-2" style="align-items:start">
      <div class="card">
        <div class="card-head"><div><h2>Change password</h2>
          <div class="sub">Signed in as ${escapeHtml(PState.user.username)}</div></div></div>
        <div class="card-body">
          <form id="pw-form" novalidate>
            <div class="field">
              <label for="pw-current">Current password</label>
              <input type="password" id="pw-current" autocomplete="current-password" required>
            </div>
            <div class="field">
              <label for="pw-new">New password</label>
              <input type="password" id="pw-new" autocomplete="new-password" required>
              <div class="hint">At least 6 characters.</div>
            </div>
            <div class="field">
              <label for="pw-confirm">Confirm new password</label>
              <input type="password" id="pw-confirm" autocomplete="new-password" required>
            </div>
            <button class="btn btn-primary" type="submit" id="pw-btn">Update password</button>
          </form>
        </div>
      </div>

      <div class="card">
        <div class="card-head"><div><h2>Session &amp; notice</h2><div class="sub">Role: PERSON</div></div></div>
        <div class="card-body">
          <p class="small muted">You are signed in with a server-side session. Signing out ends it
             immediately.</p>
          <div class="btn-row" style="margin-top:12px">
            <button class="btn btn-outline" data-logout>${icon('logout', 15)} Sign out</button>
          </div>
          <div class="notice notice-warn" style="margin-top:16px">
            <span>${escapeHtml(meta.disclaimer || 'Demonstration system — not a certified emergency dispatch service.')}</span>
          </div>
        </div>
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
}

/* ==========================================================================
 *  ROUTER + BOOT
 * ========================================================================*/
const VIEWS = {
  overview: { title: 'Overview', crumbs: 'CivicShield · citizen console', render: renderOverview },
  submit: { title: 'Submit request', crumbs: 'Citizen · new disaster request', render: renderSubmit },
  'my-requests': { title: 'My requests', crumbs: 'Citizen · request history', render: renderMyRequests },
  route: { title: 'Shortest route', crumbs: 'City · shortest path', render: renderRouteView },
  settings: { title: 'Account settings', crumbs: 'Account · security', render: renderPersonSettings }
};

function currentViewName() {
  const hash = location.hash.replace(/^#\/?/, '').split('?')[0];
  return VIEWS[hash] ? hash : 'overview';
}

async function navigate() {
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
    </div></div>`;
  try {
    await view.render();
  } catch (err) {
    root.innerHTML = errorCard('Unexpected frontend error: ' + err.message);
  }
  const sidebar = document.querySelector('.sidebar');
  if (sidebar) sidebar.classList.remove('open');
  const scrim = document.querySelector('.sidebar-scrim');
  if (scrim) scrim.remove();
}

(async function boot() {
  const user = await Session.guard('PERSON');
  if (!user) return;
  PState.user = user;
  setUserChrome(user);
  initShell();
  window.addEventListener('hashchange', navigate);
  navigate();
})();
