/* ============================================================================
   landing.js — landing page behaviour
   Pulls live counters and the real city graph from the C++ backend.
   ========================================================================== */
'use strict';

(function () {
  function setText(sel, value) {
    const el = document.querySelector(sel);
    if (el) el.textContent = value;
  }

  async function loadSummary() {
    const res = await API.get('/api/public/summary');
    if (!res.ok) {
      setText('#backend-status', 'backend unreachable');
      document.querySelectorAll('[data-stat]').forEach((n) => { n.textContent = '—'; });
      return;
    }
    const d = res.data;
    document.querySelectorAll('[data-stat]').forEach((node) => {
      const key = node.getAttribute('data-stat');
      node.textContent = fmtInt(d[key]);
    });
    setText('#hero-net-summary',
      `${d.areas} areas · ${d.roads} roads · ${d.availableVolunteers}/${d.volunteers} volunteers free`);
    setText('[data-net="areas"]', fmtInt(d.areas));
    setText('[data-net="roads"]', fmtInt(d.roads));
    setText('#backend-status', 'backend online');
  }

  async function loadGraph() {
    const res = await API.get('/api/graph');
    if (!res.ok) return;
    const graph = res.data;

    // hero preview — non-interactive, compact
    const heroEl = document.getElementById('hero-map');
    if (heroEl) {
      const heroHeight = Math.max(240, Math.round(heroEl.clientWidth * 0.62));
      const heroMap = new CityMap(heroEl, {
        height: heroHeight,
        interactive: false,
        showLegend: false,
        showToolbar: false,
        showRoadLabels: false
      });
      heroMap.setData(graph);
    }

    // full network diagram
    const netEl = document.getElementById('network-map');
    if (netEl) {
      const netMap = new CityMap(netEl, {
        height: 470,
        interactive: true,
        showLegend: true,
        showRoadLabels: true
      });
      netMap.setData(graph);
    }

    setText('[data-net="blocked"]', fmtInt(graph.stats.blockedRoads));
    setText('[data-net="connected"]', graph.stats.connected ? 'Yes' : 'No');
  }

  document.addEventListener('DOMContentLoaded', () => {
    loadSummary();
    loadGraph();
  });
})();
