/* ============================================================================
   city-map.js — interactive schematic city-network renderer (SVG)
   ----------------------------------------------------------------------------
   Draws exactly what the C++ backend reports: the 20 areas, the 38 roads, their
   distances, blocked/open state and any shortest path returned by Dijkstra.
   Nothing here computes routes — it only visualises /api/graph data.

   Usage:
     const map = new CityMap(el, { height: 520, onAreaClick(area){}, onRoadClick(road){} });
     map.setData(await API.get('/api/graph').then(r => r.data));
     map.setRoute(routeJson);        // highlight path + road ids
     map.setSelection(fromId, toId); // highlight endpoints
     map.setAffected([8, 12]);       // areas with active requests
   ========================================================================== */
'use strict';

class CityMap {
  constructor(container, options = {}) {
    this.container = container;
    this.options = Object.assign({
      height: 520,
      interactive: true,
      showLegend: true,
      showToolbar: true,
      showRoadLabels: true,
      extraTools: '',
      onAreaClick: null,
      onRoadClick: null
    }, options);

    this.areas = [];
    this.roads = [];
    this.areaIndex = {};
    this.route = null;
    this.sourceId = null;
    this.destId = null;
    this.affectedIds = [];
    this.scale = 1;
    this.tx = 0;
    this.ty = 0;
    this.hovered = null;
    this._build();
  }

  /* ------------------------------------------------------------- structure */
  _build() {
    const { height, showLegend, showToolbar, extraTools, interactive } = this.options;
    this.container.innerHTML = `
      <div class="map-shell">
        ${showToolbar ? `<div class="map-toolbar">
          <div class="btn-row" style="gap:6px">
            <button class="btn btn-sm btn-outline" data-map="zoom-in" title="Zoom in">+</button>
            <button class="btn btn-sm btn-outline" data-map="zoom-out" title="Zoom out">&minus;</button>
            <button class="btn btn-sm btn-outline" data-map="fit">Fit</button>
          </div>
          <div class="btn-row" style="gap:6px;margin-left:auto">${extraTools}</div>
        </div>` : ''}
        <div class="map-canvas" style="height:${height}px">
          <svg role="img" aria-label="Schematic city network of areas and roads">
            <g class="viewport">
              <g class="layer-edges"></g>
              <g class="layer-route"></g>
              <g class="layer-nodes"></g>
            </g>
          </svg>
          <div class="map-tooltip hidden"></div>
          ${interactive ? '<div class="map-hint">Drag to pan · scroll to zoom · click areas</div>' : ''}
        </div>
        ${showLegend ? `
        <div class="map-legend">
          <span class="item"><span class="swatch"></span> Open road</span>
          <span class="item"><span class="swatch blocked"></span> Blocked road</span>
          <span class="item"><span class="swatch route"></span> Shortest route (Dijkstra)</span>
          <span class="item"><span class="node"></span> Area node</span>
          <span class="item"><span class="node sel"></span> Selected area</span>
          <span class="item"><span class="node aff"></span> Area with active request</span>
        </div>` : ''}
      </div>`;

    this.svg = this.container.querySelector('svg');
    this.viewport = this.container.querySelector('.viewport');
    this.edgeLayer = this.container.querySelector('.layer-edges');
    this.routeLayer = this.container.querySelector('.layer-route');
    this.nodeLayer = this.container.querySelector('.layer-nodes');
    this.canvas = this.container.querySelector('.map-canvas');
    this.tooltip = this.container.querySelector('.map-tooltip');

    if (showToolbar) {
      this.container.querySelector('[data-map="zoom-in"]').addEventListener('click', () => this.zoomBy(1.25));
      this.container.querySelector('[data-map="zoom-out"]').addEventListener('click', () => this.zoomBy(0.8));
      this.container.querySelector('[data-map="fit"]').addEventListener('click', () => this.fit());
    }

    if (interactive) this._attachPanZoom();
  }

  /* ------------------------------------------------------------ data input */
  setData(graph) {
    this.areas = (graph.areas || []).slice();
    this.roads = (graph.roads || []).slice();
    this.width = (graph.canvas && graph.canvas.width) || 1000;
    this.height = (graph.canvas && graph.canvas.height) || 700;
    this.stats = graph.stats || {};

    this.areaIndex = {};
    this.areas.forEach((a) => { this.areaIndex[a.id] = a; });

    this.svg.setAttribute('viewBox', `0 0 ${this.width} ${this.height}`);
    this._renderEdges();
    this._renderNodes();
    this._renderRoute();
    this.fit();
  }

  /* ---------------------------------------------------------------- render */
  _renderEdges() {
    const parts = [];
    this.roads.forEach((road) => {
      const a = this.areaIndex[road.fromAreaId];
      const b = this.areaIndex[road.toAreaId];
      if (!a || !b) return;
      const mx = (a.x + b.x) / 2;
      const my = (a.y + b.y) / 2;
      const dx = b.x - a.x, dy = b.y - a.y;
      const len = Math.max(1, Math.hypot(dx, dy));
      const nx = -dy / len, ny = dx / len;   // perpendicular for label offset
      const offset = 13;
      const lx = mx + nx * offset;
      const ly = my + ny * offset;
      const showLabel = this.options.showRoadLabels && len > 78;

      parts.push(`
        <g class="edge" data-road-id="${road.id}">
          <line class="edge-hit" x1="${a.x}" y1="${a.y}" x2="${b.x}" y2="${b.y}"></line>
          <line class="edge-line ${road.blocked ? 'blocked' : ''}" x1="${a.x}" y1="${a.y}" x2="${b.x}" y2="${b.y}"></line>
          ${showLabel ? `<text class="edge-label ${road.blocked ? 'blocked' : ''}" x="${lx}" y="${ly}"
             text-anchor="middle">${road.distanceKm} km</text>` : ''}
        </g>`);
    });
    this.edgeLayer.innerHTML = parts.join('');

    if (this.options.interactive && this.options.onRoadClick) {
      this.edgeLayer.querySelectorAll('.edge').forEach((g) => {
        const id = Number(g.getAttribute('data-road-id'));
        g.addEventListener('click', (e) => {
          e.stopPropagation();
          const road = this.roads.find((r) => r.id === id);
          if (road) this.options.onRoadClick(road);
        });
        g.addEventListener('mousemove', (e) => {
          const road = this.roads.find((r) => r.id === id);
          if (!road) return;
          this._showTooltip(e,
            `${road.fromAreaName} → ${road.toAreaName}`,
            `Road ${road.id} · ${road.distanceKm} km · ${road.status}`);
        });
        g.addEventListener('mouseleave', () => this._hideTooltip());
      });
    }
  }

  _renderNodes() {
    const parts = this.areas.map((area) => {
      const affected = this.affectedIds.indexOf(area.id) !== -1;
      const isSource = this.sourceId === area.id;
      const isDest = this.destId === area.id;
      let cls = '';
      if (isSource) cls = 'source';
      else if (isDest) cls = 'dest';
      else if (affected) cls = 'affected';
      const onDark = cls !== '';
      return `
        <g class="node-group" data-area-id="${area.id}" tabindex="0" role="button"
           aria-label="Area ${area.id}: ${area.name}">
          <circle class="node-circle ${cls}" cx="${area.x}" cy="${area.y}" r="16"></circle>
          <text class="node-id ${onDark ? 'on-dark' : ''}" x="${area.x}" y="${area.y + 3.6}">${area.id}</text>
          <text class="node-label" x="${area.x}" y="${area.y + 33}">${area.name}</text>
          ${affected && !isSource && !isDest ? `<text class="node-badge" x="${area.x}" y="${area.y - 22}" fill="#a91f17">NEED</text>` : ''}
        </g>`;
    }).join('');
    this.nodeLayer.innerHTML = parts;

    this.nodeLayer.querySelectorAll('.node-group').forEach((g) => {
      const id = Number(g.getAttribute('data-area-id'));
      const area = this.areaIndex[id];
      g.addEventListener('mousemove', (e) => {
        this._showTooltip(e, `Area ${area.id} · ${area.name}`,
          `${area.volunteers} volunteer(s) based here · ${area.availableVolunteers} available`);
      });
      g.addEventListener('mouseleave', () => this._hideTooltip());
      if (this.options.interactive && this.options.onAreaClick) {
        g.addEventListener('click', (e) => { e.stopPropagation(); this.options.onAreaClick(area); });
        g.addEventListener('keydown', (e) => {
          if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); this.options.onAreaClick(area); }
        });
      }
    });
  }

  _renderRoute() {
    if (!this.route || !this.route.found || !this.route.path || this.route.path.length < 2) {
      this.routeLayer.innerHTML = '';
      return;
    }
    const pts = this.route.path.map((p) => this.areaIndex[p.id]).filter(Boolean);
    if (pts.length < 2) { this.routeLayer.innerHTML = ''; return; }
    const d = pts.map((p, i) => `${i === 0 ? 'M' : 'L'}${p.x},${p.y}`).join(' ');

    const routeRoadIds = this.route.roadIds || [];
    this.edgeLayer.querySelectorAll('.edge').forEach((g) => {
      const rid = Number(g.getAttribute('data-road-id'));
      const line = g.querySelector('.edge-line');
      if (routeRoadIds.indexOf(rid) !== -1) line.classList.add('route');
      else line.classList.remove('route');
    });

    this.routeLayer.innerHTML = `
      <path d="${d}" fill="none" stroke="rgba(29,78,216,.16)" stroke-width="18"
            stroke-linecap="round" stroke-linejoin="round"></path>
      <path d="${d}" fill="none" stroke="#1d4ed8" stroke-width="6.5"
            stroke-linecap="round" stroke-linejoin="round"></path>
      <path class="route-flow" d="${d}" fill="none" stroke="#dbeafe" stroke-width="2.2"
            stroke-linecap="round" stroke-linejoin="round"
            stroke-dasharray="10 16"></path>`;

    const flow = this.routeLayer.querySelector('.route-flow');
    if (flow && !window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      let offset = 0;
      if (this._flowTimer) clearInterval(this._flowTimer);
      this._flowTimer = setInterval(() => {
        offset = (offset + 1.6) % 26;
        if (!flow.isConnected) { clearInterval(this._flowTimer); return; }
        flow.setAttribute('stroke-dashoffset', String(offset));
      }, 55);
    }
  }

  /* ------------------------------------------------------------------- API */
  setRoute(route) {
    this.route = route;
    this._renderRoute();
  }

  clearRoute() {
    this.route = null;
    this.routeLayer.innerHTML = '';
    this.edgeLayer.querySelectorAll('.edge-line.route').forEach((l) => l.classList.remove('route'));
  }

  setSelection(sourceId, destId) {
    this.sourceId = sourceId === undefined ? null : sourceId;
    this.destId = destId === undefined ? null : destId;
    this._renderNodes();
    this._renderRoute();
  }

  setAffected(ids) {
    this.affectedIds = ids || [];
    this._renderNodes();
  }

  area(id) { return this.areaIndex[id]; }

  road(id) { return this.roads.find((r) => r.id === id); }

  /* ------------------------------------------------------------- viewport */
  zoomBy(factor) {
    const cx = this.width / 2, cy = this.height / 2;
    this._zoomAt(cx, cy, factor);
  }

  _zoomAt(px, py, factor) {
    const next = Math.min(2.6, Math.max(0.55, this.scale * factor));
    const k = next / this.scale;
    this.tx = px - (px - this.tx) * k;
    this.ty = py - (py - this.ty) * k;
    this.scale = next;
    this._applyTransform();
  }

  _applyTransform() {
    this.viewport.setAttribute('transform', `translate(${this.tx.toFixed(2)},${this.ty.toFixed(2)}) scale(${this.scale.toFixed(3)})`);
  }

  fit() {
    this.scale = 1;
    this.tx = 0;
    this.ty = 0;
    this._applyTransform();
  }

  _attachPanZoom() {
    const svg = this.svg;
    let dragging = false, lastX = 0, lastY = 0;

    svg.addEventListener('wheel', (e) => {
      e.preventDefault();
      const rect = this.canvas.getBoundingClientRect();
      const px = ((e.clientX - rect.left) / rect.width) * this.width;
      const py = ((e.clientY - rect.top) / rect.height) * this.height;
      const factor = e.deltaY < 0 ? 1.12 : 0.89;
      // pointer position in viewBox space must account for the current transform
      const vx = (px - this.tx) / this.scale;
      const vy = (py - this.ty) / this.scale;
      const next = Math.min(2.6, Math.max(0.55, this.scale * factor));
      this.scale = next;
      this.tx = px - vx * next;
      this.ty = py - vy * next;
      this._applyTransform();
    }, { passive: false });

    svg.addEventListener('pointerdown', (e) => {
      if (e.button !== 0) return;
      dragging = true;
      lastX = e.clientX; lastY = e.clientY;
      svg.classList.add('dragging');
      svg.setPointerCapture(e.pointerId);
    });
    svg.addEventListener('pointermove', (e) => {
      if (!dragging) return;
      const rect = this.canvas.getBoundingClientRect();
      const dx = (e.clientX - lastX) * (this.width / rect.width);
      const dy = (e.clientY - lastY) * (this.height / rect.height);
      this.tx += dx; this.ty += dy;
      lastX = e.clientX; lastY = e.clientY;
      this._applyTransform();
    });
    const stop = () => { dragging = false; svg.classList.remove('dragging'); };
    svg.addEventListener('pointerup', stop);
    svg.addEventListener('pointercancel', stop);
    svg.addEventListener('pointerleave', stop);
  }

  /* ------------------------------------------------------------- tooltips */
  _showTooltip(event, title, subtitle) {
    const rect = this.canvas.getBoundingClientRect();
    this.tooltip.innerHTML = `<strong>${escapeHtml(title)}</strong><span>${escapeHtml(subtitle)}</span>`;
    this.tooltip.style.left = (event.clientX - rect.left) + 'px';
    this.tooltip.style.top = (event.clientY - rect.top) + 'px';
    this.tooltip.classList.remove('hidden');
  }

  _hideTooltip() { this.tooltip.classList.add('hidden'); }

  destroy() {
    if (this._flowTimer) clearInterval(this._flowTimer);
    this.container.innerHTML = '';
  }
}

window.CityMap = CityMap;
