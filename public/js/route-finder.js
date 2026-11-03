/* ============================================================================
   route-finder.js — shared shortest-route widget (admin + citizen console)
   ----------------------------------------------------------------------------
   The frontend never computes a route: it asks /api/routes, which runs the
   original C++ Dijkstra implementation (custom priority queue, blocked roads
   excluded) and renders exactly what comes back.
   ========================================================================== */
'use strict';

function initRouteFinder(opts) {
  const form = document.getElementById(opts.formId);
  const fromSel = document.getElementById(opts.fromId);
  const toSel = document.getElementById(opts.toId);
  const resultEl = document.getElementById(opts.resultId);
  const swapBtn = opts.swapId ? document.getElementById(opts.swapId) : null;
  const submitBtn = form ? form.querySelector('[type="submit"]') : null;
  let areas = [];

  function optionHtml(area) {
    return `<option value="${area.id}">${escapeHtml(area.name)} (#${area.id})</option>`;
  }

  function setAreas(list) {
    areas = list || [];
    const previousFrom = fromSel ? fromSel.value : '';
    const hadPreviousTo = !!(toSel && toSel.value && toSel.dataset.touched === '1');
    const previousTo = hadPreviousTo ? toSel.value : '';
    const html = areas.map(optionHtml).join('');
    if (fromSel) { fromSel.innerHTML = html; }
    if (toSel) { toSel.innerHTML = html; }
    if (fromSel && previousFrom) fromSel.value = previousFrom;
    if (toSel && hadPreviousTo && previousTo && areas.some((a) => String(a.id) === previousTo)) {
      toSel.value = previousTo;
    } else if (toSel && areas.length > 1) {
      // default the destination to a different area than the source
      const alt = areas.find((a) => String(a.id) !== (fromSel ? fromSel.value : ''));
      if (alt) toSel.value = String(alt.id);
    }
    if (toSel) toSel.addEventListener('change', () => { toSel.dataset.touched = '1'; });
  }

  async function refreshAreas() {
    const res = await API.get('/api/areas');
    if (res.ok) setAreas(res.data.areas);
    return areas;
  }

  function renderMessage(kind, title, body) {
    resultEl.innerHTML = `<div class="route-result ${kind}">
      <div class="small" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em">${escapeHtml(title)}</div>
      <div class="small" style="margin-top:4px">${escapeHtml(body)}</div></div>`;
  }

  function renderResult(route) {
    if (!route.found) {
      resultEl.innerHTML = `<div class="route-result fail">
        <div class="small" style="font-weight:700;text-transform:uppercase;letter-spacing:.06em">No open route</div>
        <div class="path">${escapeHtml(route.from.name)} ✕ ${escapeHtml(route.to.name)}</div>
        <div class="small">${escapeHtml(route.message || 'These areas are not connected by open roads.')}</div>
      </div>`;
      return;
    }
    const steps = (route.steps || []).map((s, i) => `
      <div class="route-step">
        <span class="idx">${i + 1}</span>
        <span>${escapeHtml(s.fromAreaName)} → ${escapeHtml(s.toAreaName)}</span>
        <span class="km">${fmtKm(s.distanceKm)} km</span>
      </div>`).join('');

    resultEl.innerHTML = `<div class="route-result">
      <div class="small muted" style="text-transform:uppercase;letter-spacing:.06em;font-weight:700">
        Shortest route · computed by C++ Dijkstra
      </div>
      <div class="path">${escapeHtml(route.pathText)}</div>
      <div class="small"><strong>Total distance:</strong> ${fmtKm(route.totalKm)} km
        · ${route.path.length} areas · ${route.steps.length} road segment(s)</div>
      <div class="route-steps">${steps || '<div class="route-step"><span>Source equals destination — no travel required.</span></div>'}</div>
    </div>`;
  }

  async function submit(event) {
    if (event) event.preventDefault();
    if (!fromSel || !toSel) return;
    const from = fromSel.value;
    const to = toSel.value;
    if (from === '' || to === '') { renderMessage('fail', 'Missing selection', 'Choose both a source and a destination area.'); return; }

    if (submitBtn) { submitBtn.disabled = true; submitBtn.classList.add('is-busy'); }
    const res = await API.get(`/api/routes?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
    if (submitBtn) { submitBtn.disabled = false; submitBtn.classList.remove('is-busy'); }

    if (!res.ok) {
      renderMessage('fail', 'Route unavailable', res.data.error || 'The backend rejected the request.');
      return;
    }
    renderResult(res.data);
    if (opts.map) {
      opts.map.setSelection(Number(from), Number(to));
      opts.map.setRoute(res.data);
    }
    if (typeof opts.onRoute === 'function') opts.onRoute(res.data);
    if (typeof opts.onFocusMap === 'function') opts.onFocusMap();
  }

  if (form) form.addEventListener('submit', submit);
  if (swapBtn) {
    swapBtn.addEventListener('click', () => {
      const a = fromSel.value;
      fromSel.value = toSel.value;
      toSel.value = a;
    });
  }

  return { refreshAreas, setAreas, submit, getAreas: () => areas };
}

window.initRouteFinder = initRouteFinder;
