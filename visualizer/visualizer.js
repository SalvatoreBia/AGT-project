/**
 * AGT Algorithm Visualizer
 * Main JavaScript logic for parsing logs and visualizing algorithm steps
 */

class AGTVisualizer {
    constructor() {
        this.logData = [];
        this.currentStep = 0;
        this.isPlaying = false;
        this.playInterval = null;
        this.speed = 500;
        this.nodeStates = new Map(); // node_id -> strategy (0 or 1)
        this.nodeUtilities = new Map(); // node_id -> utility
        this.simulation = null;
        this.graphData = { nodes: [], links: [] };
        // this.graphStructure = null; // Store loaded graph structure - REMOVED

        this.initEventListeners();
    }

    initEventListeners() {
        // File upload
        const dropZone = document.getElementById('dropZone');
        const fileInput = document.getElementById('fileInput');

        dropZone.addEventListener('click', () => fileInput.click());
        dropZone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropZone.classList.add('dragover');
        });
        dropZone.addEventListener('dragleave', () => {
            dropZone.classList.remove('dragover');
        });
        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.classList.remove('dragover');
            const file = e.dataTransfer.files[0];
            if (file) this.loadFile(file);
        });
        fileInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (file) this.loadFile(file);
        });

        // Playback controls
        document.getElementById('btnFirst').addEventListener('click', () => this.goToStep(0));
        document.getElementById('btnPrev').addEventListener('click', () => this.prevStep());
        document.getElementById('btnPlay').addEventListener('click', () => this.togglePlay());
        document.getElementById('btnNext').addEventListener('click', () => this.nextStep());
        document.getElementById('btnLast').addEventListener('click', () => this.goToStep(this.logData.length - 1));

        // Progress slider
        document.getElementById('progressSlider').addEventListener('input', (e) => {
            this.goToStep(parseInt(e.target.value));
        });

        // Speed slider
        document.getElementById('speedSlider').addEventListener('input', (e) => {
            this.speed = parseInt(e.target.value);
            document.getElementById('speedLabel').textContent = `${this.speed}ms`;
            if (this.isPlaying) {
                this.stopPlay();
                this.startPlay();
            }
        });

        // Algorithm Selector
        document.getElementById('algoSelect').addEventListener('change', (e) => {
            const step = parseInt(e.target.value);
            if (!isNaN(step)) {
                this.goToStep(step);
            }
        });
    }

    // loadGraphFile removed

    async loadFile(file) {
        const text = await file.text();
        const lines = text.trim().split('\n').filter(line => line.trim());

        this.logData = [];

        for (const line of lines) {
            try {
                const parsed = JSON.parse(line);

                // Expand MATCHING and VCG entries
                if (parsed.algorithm === 'MATCHING' && parsed.events) {
                    // Create a sequence of steps for the matching algorithm

                    // Initial State
                    let currentMatches = [];
                    let flowStats = { iter: 0, flow: 0, cost: 0 };

                    // Add an initial "Start" step
                    this.logData.push({
                        ...parsed,
                        _stepDesc: "Start Matching",
                        currentMatches: [],
                        flowStats: { ...flowStats }
                    });

                    for (const event of parsed.events) {
                        if (event.type === 'iter') {
                            flowStats = { iter: event.it, flow: event.flow, cost: event.cost };
                            this.logData.push({
                                ...parsed,
                                _stepDesc: `Min-Cost Flow Iteration ${event.it}`,
                                currentMatches: [...currentMatches], // Copy
                                flowStats: { ...flowStats }
                            });
                        } else if (event.type === 'match') {
                            currentMatches.push(event);
                            this.logData.push({
                                ...parsed,
                                _stepDesc: `Match Found: B${event.buyer} ↔ V${event.vendor}`,
                                currentMatches: [...currentMatches], // Copy
                                flowStats: { ...flowStats }
                            });
                        }
                    }

                } else if (parsed.algorithm === 'VCG' && parsed.events) {

                    let foundPath = null;
                    let calculatedPayments = [];

                    this.logData.push({
                        ...parsed,
                        _stepDesc: "VCG Request",
                        foundPath: null,
                        calculatedPayments: []
                    });

                    for (const event of parsed.events) {
                        if (event.type === 'path') {
                            foundPath = event;
                            this.logData.push({
                                ...parsed,
                                _stepDesc: "Shortest Path Found",
                                foundPath: event, // Object ref is fine, it's static
                                calculatedPayments: [...calculatedPayments]
                            });
                        } else if (event.type === 'payment') {
                            calculatedPayments.push(event);
                            this.logData.push({
                                ...parsed,
                                _stepDesc: `Calculating Payment for Node ${event.node}`,
                                foundPath: foundPath,
                                calculatedPayments: [...calculatedPayments]
                            });
                        }
                    }

                } else {
                    // Normal entry (Strategic Game)
                    this.logData.push(parsed);
                }

            } catch (e) {
                console.warn('Failed to parse line:', line);
            }
        }

        if (this.logData.length === 0) {
            alert('No valid log entries found!');
            return;
        }

        document.getElementById('fileInfo').textContent = `✅ Loaded ${file.name} (${this.logData.length} steps)`;

        // Show controls and visualization
        document.getElementById('controlsSection').style.display = 'flex';
        document.getElementById('vizArea').style.display = 'grid';

        // Setup slider
        const slider = document.getElementById('progressSlider');
        slider.max = this.logData.length - 1;
        slider.value = 0;

        // Populate Algorithm Selector
        const algoSelect = document.getElementById('algoSelect');
        algoSelect.innerHTML = '<option value="" disabled selected>Jump to Algorithm...</option>';

        let lastAlgo = null;
        this.logData.forEach((entry, index) => {
            if (entry.algorithm && entry.algorithm !== lastAlgo) {
                const option = document.createElement('option');
                option.value = index;
                // Nice mapping name
                let name = entry.algorithm;
                if (name === 'FP' || name === 'BRD' || name === 'RM') name = "Strategic Game (Part 1)";
                if (name === 'MATCHING') name = "Matching Market (Part 3)";
                if (name === 'VCG') name = "VCG Auction (Part 4)";

                option.textContent = `${name} (Step ${index})`;
                algoSelect.appendChild(option);
                lastAlgo = entry.algorithm;
            }
        });

        // Initialize visualization
        this.initVisualization();
        this.goToStep(0);
    }

    initVisualization() {
        // Reset state
        this.nodeStates.clear();
        this.nodeUtilities.clear();
        this.currentStep = 0;

        // Analyze log to determine node count and initial setup
        const firstEntry = this.logData[0];

        if (firstEntry.algorithm === 'MATCHING' || firstEntry.algorithm === 'VCG') {
            // No global graph needed
        } else {
            this.buildGraphFromLog();
        }
    }

    buildGraphFromLog() {
        // Fallback: Build simplified graph from log node IDs
        console.log("Building simplified graph from logs...");

        const nodeIds = new Set();
        for (const entry of this.logData) {
            if (entry.updates) {
                for (const update of entry.updates) {
                    nodeIds.add(update.id);
                }
            }
        }

        const nodes = Array.from(nodeIds).map(id => ({ id, strategy: 1 }));

        // Create random edges for visualization (simplified ring)
        const links = [];
        const nodeArray = Array.from(nodeIds).sort((a, b) => a - b);

        for (let i = 0; i < nodeArray.length; i++) {
            if (i < nodeArray.length - 1) {
                links.push({ source: nodeArray[i], target: nodeArray[i + 1] });
            }
        }
        if (nodeArray.length > 2) {
            links.push({ source: nodeArray[nodeArray.length - 1], target: nodeArray[0] });
        }

        this.graphData = { nodes, links };

        for (const node of this.graphData.nodes) {
            this.nodeStates.set(node.id, 1);
            this.nodeUtilities.set(node.id, 0.0);
        }

        this.renderGraph();
    }

    renderGraph() {
        const svg = d3.select('#graphSvg');
        svg.selectAll('*').remove();

        const container = document.querySelector('.graph-container');
        const width = container.clientWidth || 800;
        const height = container.clientHeight || 500;

        svg.attr('width', width).attr('height', height);

        // Create zoom container
        const g = svg.append('g');

        // Add zoom behavior
        const zoom = d3.zoom()
            .scaleExtent([0.1, 4])
            .on('zoom', (event) => {
                g.attr('transform', event.transform);
            });
        svg.call(zoom);

        // Force simulation
        this.simulation = d3.forceSimulation(this.graphData.nodes)
            .force('link', d3.forceLink(this.graphData.links).id(d => d.id).distance(50))
            .force('charge', d3.forceManyBody().strength(-100))
            .force('center', d3.forceCenter(width / 2, height / 2))
            .force('collision', d3.forceCollide().radius(15));

        // Render links
        const links = g.append('g')
            .attr('class', 'links')
            .selectAll('line')
            .data(this.graphData.links)
            .enter()
            .append('line')
            .attr('class', 'link');

        // Render nodes
        const nodes = g.append('g')
            .attr('class', 'nodes')
            .selectAll('circle')
            .data(this.graphData.nodes)
            .enter()
            .append('circle')
            .attr('class', 'node')
            .attr('r', 8)
            .attr('data-id', d => d.id)
            .call(d3.drag()
                .on('start', (event, d) => {
                    if (!event.active) this.simulation.alphaTarget(0.3).restart();
                    d.fx = d.x;
                    d.fy = d.y;
                })
                .on('drag', (event, d) => {
                    d.fx = event.x;
                    d.fy = event.y;
                })
                .on('end', (event, d) => {
                    if (!event.active) this.simulation.alphaTarget(0);
                    d.fx = null;
                    d.fy = null;
                }));

        // Add tooltips
        nodes.append('title').text(d => `Node ${d.id} `);

        // Update positions on tick
        this.simulation.on('tick', () => {
            links
                .attr('x1', d => d.source.x)
                .attr('y1', d => d.source.y)
                .attr('x2', d => d.target.x)
                .attr('y2', d => d.target.y);

            nodes
                .attr('cx', d => d.x)
                .attr('cy', d => d.y);
        });

        this.updateNodeColors();
    }

    updateNodeColors(changedNodes = []) {
        const changedSet = new Set(changedNodes);

        d3.selectAll('.node')
            .classed('secure', d => this.nodeStates.get(d.id) === 1)
            .classed('insecure', d => this.nodeStates.get(d.id) === 0)
            .classed('changed', d => changedSet.has(d.id))
            .attr('fill', d => {
                if (changedSet.has(d.id)) return '#ffaa00';
                return this.nodeStates.get(d.id) === 1 ? '#00ff88' : '#ff4757';
            });
    }

    goToStep(step) {
        if (step < 0 || step >= this.logData.length) return;

        // Reset state and replay from beginning
        this.nodeStates.clear();
        for (const node of this.graphData.nodes) {
            this.nodeStates.set(node.id, 1); // Start all secure by default or strategy 1
        }

        // Apply all updates up to current step
        const changedThisStep = [];
        for (let i = 0; i <= step; i++) {
            const entry = this.logData[i];

            if (entry.updates) {
                for (const update of entry.updates) {
                    this.nodeStates.set(update.id, update.new);
                    if (i === step) {
                        changedThisStep.push(update.id);
                    }
                }
            }
        }

        this.currentStep = step;
        this.updateUI(changedThisStep);
    }

    updateUI(changedNodes = []) {
        const entry = this.logData[this.currentStep];

        // Update slider
        document.getElementById('progressSlider').value = this.currentStep;

        let subInfo = "";
        if (entry._stepDesc) {
            subInfo = ` - ${entry._stepDesc}`;
        }
        document.getElementById('stepLabel').textContent = `Step ${this.currentStep + 1} / ${this.logData.length}${subInfo}`;

        // Determine view type
        if (entry.algorithm === 'MATCHING') {
            this.showMatchingView(entry);
        } else if (entry.algorithm === 'VCG') {
            this.showVCGView(entry);
        } else {
            this.showGraphView(entry, changedNodes);
        }
    }

    showGraphView(entry, changedNodes) {
        document.getElementById('vizArea').style.display = 'grid';
        document.getElementById('matchingView').style.display = 'none';
        document.getElementById('vcgView').style.display = 'none';

        // Update info panel
        document.getElementById('algoName').textContent = `Algorithm: ${entry.algorithm || 'Unknown'}`;
        document.getElementById('statIteration').textContent = entry.iteration !== undefined ? entry.iteration : '-';

        let secureCount = 0;
        for (const [, strat] of this.nodeStates) {
            if (strat === 1) secureCount++;
        }
        document.getElementById('statSecure').textContent = secureCount;
        document.getElementById('statChanges').textContent = changedNodes.length;

        // Update node colors
        this.updateNodeColors(changedNodes);
    }

    showMatchingView(entry) {
        document.getElementById('vizArea').style.display = 'none';
        document.getElementById('matchingView').style.display = 'block';
        document.getElementById('vcgView').style.display = 'none';

        const buyersCol = document.getElementById('buyersColumn');
        const vendorsCol = document.getElementById('vendorsColumn');
        const statsDiv = document.getElementById('matchingStats');

        // Use the expanded data
        const matches = entry.currentMatches || [];
        const flowStats = entry.flowStats || { iter: 0, flow: 0, cost: 0 };

        // Clear previous
        buyersCol.innerHTML = '<h4>Buyers</h4>';
        vendorsCol.innerHTML = '<h4>Vendors</h4>';

        // Collect matches for display
        const buyers = new Map();
        const vendors = new Map();
        let totalWelfare = 0;

        for (const m of matches) {
            buyers.set(m.buyer, { vendor: m.vendor, budget: m.budget, utility: m.u });
            if (!vendors.has(m.vendor)) {
                vendors.set(m.vendor, { buyers: [], price: m.price });
            }
            vendors.get(m.vendor).buyers.push(m.buyer);
            totalWelfare += m.u;
        }

        // Render buyers 
        const maxDisplay = 50;
        let count = 0;
        for (const [buyerId, info] of buyers) {
            if (count++ >= maxDisplay) break;
            const div = document.createElement('div');
            div.className = 'buyer-item matched';
            div.innerHTML = `<strong>B${buyerId}</strong><br>Budget: ${info.budget}<br>Utility: ${info.utility.toFixed(0)}`;
            buyersCol.appendChild(div);
        }

        // Render vendors
        count = 0;
        for (const [vendorId, info] of vendors) {
            if (count++ >= maxDisplay) break;
            const div = document.createElement('div');
            div.className = 'vendor-item matched';
            div.innerHTML = `<strong>V${vendorId}</strong><br>Price: ${info.price}<br>Sales: ${info.buyers.length}`;
            vendorsCol.appendChild(div);
        }

        // Stats with Flow info
        statsDiv.innerHTML = `
            <div class="stat">
                <div class="stat-value">${matches.length}</div>
                <div class="stat-label">Matches Found</div>
            </div>
            <div class="stat">
                <div class="stat-value">${totalWelfare.toFixed(0)}</div>
                <div class="stat-label">Total Welfare</div>
            </div>
            <div class="stat">
                <div class="stat-value" style="font-size: 0.9em;">F:${flowStats.flow} <br> C:${flowStats.cost.toFixed(0)}</div>
                <div class="stat-label">Flow / Cost</div>
            </div>
        `;

        // D3 Graph
        this.renderFlowGraph(matches, flowStats);
    }

    renderFlowGraph(matches, flowStats) {
        const svg = d3.select('#matchingSvg');
        svg.selectAll('*').remove();

        const container = document.querySelector('.matching-links');
        const width = container.clientWidth || 800;
        const height = 500;
        svg.attr('height', height);

        const margin = { top: 30, right: 30, bottom: 30, left: 30 };
        const graphWidth = width - margin.left - margin.right;
        const graphHeight = height - margin.top - margin.bottom;

        const g = svg.append('g').attr('transform', `translate(${margin.left},${margin.top})`);

        // Collect all potential nodes from matches
        const buyerSet = new Set();
        const vendorSet = new Set();

        matches.forEach(m => {
            buyerSet.add(m.buyer);
            vendorSet.add(m.vendor);
        });

        const buyers = Array.from(buyerSet).sort((a, b) => a - b);
        const vendors = Array.from(vendorSet).sort((a, b) => a - b);

        // Define Layers: Source -> Buyers -> Vendors -> Sink
        // X coordinates
        const xSource = 0;
        const xBuyers = graphWidth * 0.33;
        const xVendors = graphWidth * 0.66;
        const xSink = graphWidth;

        // Y scales
        const yScaleBuyer = d3.scalePoint().domain(buyers).range([50, graphHeight - 50]).padding(0.5);
        const yScaleVendor = d3.scalePoint().domain(vendors).range([50, graphHeight - 50]).padding(0.5);
        const yCenter = graphHeight / 2;

        // Draw Edges

        // 1. Source -> Buyers
        // In Min-Cost Flow, Source connects to all buyers.
        // We highlight if the buyer is involved in a match (flow > 0).
        const matchedBuyerIds = new Set(matches.map(m => m.buyer));

        g.selectAll('.edge-source-buyer')
            .data(buyers)
            .enter()
            .append('line')
            .attr('x1', xSource)
            .attr('y1', yCenter)
            .attr('x2', xBuyers)
            .attr('y2', d => yScaleBuyer(d))
            .attr('stroke', d => matchedBuyerIds.has(d) ? '#00ff88' : '#333')
            .attr('stroke-width', d => matchedBuyerIds.has(d) ? 3 : 1)
            .attr('stroke-opacity', d => matchedBuyerIds.has(d) ? 1 : 0.3)
            .attr('marker-end', d => matchedBuyerIds.has(d) ? 'url(#arrow-active)' : '');

        // 2. Buyers -> Vendors
        // Only draw matched edges (flow > 0)
        g.selectAll('.edge-buyer-vendor')
            .data(matches)
            .enter()
            .append('line')
            .attr('x1', xBuyers)
            .attr('y1', d => yScaleBuyer(d.buyer))
            .attr('x2', xVendors)
            .attr('y2', d => yScaleVendor(d.vendor))
            .attr('stroke', '#00ff88')
            .attr('stroke-width', 3)
            .attr('marker-end', 'url(#arrow-active)');

        // 3. Vendors -> Sink
        // Flow exists if vendor has at least one match
        const vendorFlow = {};
        matches.forEach(m => {
            vendorFlow[m.vendor] = (vendorFlow[m.vendor] || 0) + 1;
        });

        g.selectAll('.edge-vendor-sink')
            .data(vendors)
            .enter()
            .append('line')
            .attr('x1', xVendors)
            .attr('y1', d => yScaleVendor(d))
            .attr('x2', xSink)
            .attr('y2', yCenter)
            .attr('stroke', d => (vendorFlow[d] > 0) ? '#00ff88' : '#333')
            .attr('stroke-width', d => (vendorFlow[d] > 0) ? 3 : 1)
            .attr('stroke-opacity', d => (vendorFlow[d] > 0) ? 1 : 0.3)
            .attr('marker-end', d => (vendorFlow[d] > 0) ? 'url(#arrow-active)' : '');

        // Draw Nodes

        // Source
        const sourceNode = g.append('g').attr('transform', `translate(${xSource}, ${yCenter})`);
        sourceNode.append('circle').attr('r', 15).attr('fill', '#fff').attr('stroke', '#000');
        sourceNode.append('text').attr('dy', 5).attr('text-anchor', 'middle').text('S').style('font-weight', 'bold');

        // Sink
        const sinkNode = g.append('g').attr('transform', `translate(${xSink}, ${yCenter})`);
        sinkNode.append('circle').attr('r', 15).attr('fill', '#fff').attr('stroke', '#000');
        sinkNode.append('text').attr('dy', 5).attr('text-anchor', 'middle').text('T').style('font-weight', 'bold');

        // Buyers
        const buyerGroups = g.selectAll('.g-buyer')
            .data(buyers)
            .enter()
            .append('g')
            .attr('transform', d => `translate(${xBuyers}, ${yScaleBuyer(d)})`);

        buyerGroups.append('circle')
            .attr('r', 12)
            .attr('fill', '#00d4ff');
        buyerGroups.append('text').attr('dy', 4).attr('text-anchor', 'middle').text(d => `B${d}`).attr('fill', '#fff').style('font-size', '10px');

        // Vendors
        const vendorGroups = g.selectAll('.g-vendor')
            .data(vendors)
            .enter()
            .append('g')
            .attr('transform', d => `translate(${xVendors}, ${yScaleVendor(d)})`);

        vendorGroups.append('rect')
            .attr('width', 24).attr('height', 24).attr('x', -12).attr('y', -12)
            .attr('fill', '#7b2cbf');
        vendorGroups.append('text').attr('dy', 4).attr('text-anchor', 'middle').text(d => `V${d}`).attr('fill', '#fff').style('font-size', '10px');

        // Definitions for arrows
        const defs = svg.append('defs');

        defs.append('marker')
            .attr('id', 'arrow-active')
            .attr('viewBox', '0 -5 10 10')
            .attr('refX', 20)
            .attr('refY', 0)
            .attr('markerWidth', 6)
            .attr('markerHeight', 6)
            .attr('orient', 'auto')
            .append('path')
            .attr('d', 'M0,-5L10,0L0,5')
            .attr('fill', '#00ff88');

        defs.append('marker')
            .attr('id', 'arrow-inactive')
            .attr('viewBox', '0 -5 10 10')
            .attr('refX', 20)
            .attr('refY', 0)
            .attr('markerWidth', 6)
            .attr('markerHeight', 6)
            .attr('orient', 'auto')
            .append('path')
            .attr('d', 'M0,-5L10,0L0,5')
            .attr('fill', '#555');
    }

    showVCGView(entry) {
        document.getElementById('vizArea').style.display = 'none';
        document.getElementById('matchingView').style.display = 'none';
        document.getElementById('vcgView').style.display = 'block';

        const requestDiv = document.getElementById('vcgRequest');
        const pathDiv = document.getElementById('vcgPath');
        const paymentsDiv = document.getElementById('vcgPayments');

        if (entry.request) {
            requestDiv.innerHTML = `
                <p style="color: var(--text-muted); margin-bottom: 10px;">Path Request</p>
                <div style="font-size: 1.1em;">Find shortest path from <strong>${entry.request.source || 'S'}</strong> to <strong>${entry.request.target || 'T'}</strong></div>
            `;
        } else {
            requestDiv.innerHTML = '<p style="color: var(--text-muted);">VCG Processing...</p>';
        }

        if (entry.foundPath) {
            let nodes = entry.foundPath.nodes;
            if (Array.isArray(nodes)) {
                nodes = nodes.join(' → ');
            }
            pathDiv.innerHTML = `
                <p style="color: var(--text-muted); margin-bottom: 10px;">Shortest Path Found</p>
                <div class="path-sequence">
                    ${nodes}
                </div>
                <div style="margin-top: 5px; color: #00ff88;">Cost: ${entry.foundPath.cost}</div>
            `;
        } else {
            pathDiv.innerHTML = '<p style="color: var(--text-muted);">Finding path...</p>';
        }

        paymentsDiv.innerHTML = '';
        if (entry.calculatedPayments && entry.calculatedPayments.length > 0) {
            paymentsDiv.innerHTML = '<p style="color: var(--text-muted); margin-bottom: 10px;">VCG Payments</p>';
            entry.calculatedPayments.forEach(p => {
                const card = document.createElement('div');
                card.className = 'payment-card';
                card.innerHTML = `
                    <div class="node-id">Node ${p.node}</div>
                    <div class="payment-value">${p.payment}</div>
                `;
                paymentsDiv.appendChild(card);
            });
        }
    }

    nextStep() {
        if (this.currentStep < this.logData.length - 1) {
            this.goToStep(this.currentStep + 1);
        }
    }

    prevStep() {
        if (this.currentStep > 0) {
            this.goToStep(this.currentStep - 1);
        }
    }

    togglePlay() {
        if (this.isPlaying) {
            this.stopPlay();
        } else {
            this.startPlay();
        }
    }

    startPlay() {
        this.isPlaying = true;
        document.getElementById('btnPlay').textContent = '⏸️';

        this.playInterval = setInterval(() => {
            if (this.currentStep < this.logData.length - 1) {
                this.nextStep();
            } else {
                this.stopPlay();
            }
        }, this.speed);
    }

    stopPlay() {
        this.isPlaying = false;
        document.getElementById('btnPlay').textContent = '▶️';
        if (this.playInterval) {
            clearInterval(this.playInterval);
            this.playInterval = null;
        }
    }
}

// Initialize on load
document.addEventListener('DOMContentLoaded', () => {
    window.visualizer = new AGTVisualizer();
});