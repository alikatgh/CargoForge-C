// CargoForge-C Web Application Logic

const API_URL = 'http://localhost:5000/api';

let cargoItems = [];
let visualizer = null;
let currentResults = null;

// Initialize application
document.addEventListener('DOMContentLoaded', () => {
    visualizer = new CargoVisualizer('viewport');
    loadExample();
    loadChallenges();
});

// Tab switching
function switchTab(tabName) {
    const tabs = document.querySelectorAll('.tab');
    const contents = document.querySelectorAll('.tab-content');

    tabs.forEach(t => t.classList.remove('active'));
    contents.forEach(c => c.classList.remove('active'));

    document.querySelector(`.tab:nth-child(${tabName === 'simulator' ? 1 : tabName === 'challenges' ? 2 : 3})`).classList.add('active');
    document.getElementById(`${tabName}-tab`).classList.add('active');
}

// Load example configuration
function loadExample() {
    cargoItems = [
        {id: 'HeavyMachinery', weight: 550000, dimensions: [20, 5, 3], type: 'heavy'},
        {id: 'ContainerA', weight: 250000, dimensions: [12.2, 2.4, 2.6], type: 'reefer'},
        {id: 'ContainerB', weight: 250000, dimensions: [12.2, 2.4, 2.6], type: 'reefer'},
        {id: 'SteelBeams', weight: 400000, dimensions: [18, 2, 2], type: 'bulk'},
        {id: 'SmallCrate', weight: 50000, dimensions: [2, 2, 2], type: 'general'}
    ];
    updateCargoList();
}

// Add new cargo item
function addCargo() {
    const id = prompt('Cargo ID:', `Cargo${cargoItems.length + 1}`);
    if (!id) return;

    const weight = parseFloat(prompt('Weight (kg):', '250000'));
    const length = parseFloat(prompt('Length (m):', '12'));
    const width = parseFloat(prompt('Width (m):', '2.4'));
    const height = parseFloat(prompt('Height (m):', '2.6'));
    const type = prompt('Type (standard/hazardous/reefer/fragile/heavy):', 'standard');

    cargoItems.push({
        id: id,
        weight: weight,
        dimensions: [length, width, height],
        type: type
    });

    updateCargoList();
}

// Remove cargo item
function removeCargo(index) {
    cargoItems.splice(index, 1);
    updateCargoList();
}

// Update cargo list display
function updateCargoList() {
    const listEl = document.getElementById('cargo-list');
    listEl.innerHTML = '';

    cargoItems.forEach((cargo, index) => {
        const item = document.createElement('div');
        item.className = 'cargo-item';
        item.innerHTML = `
            <div>
                <strong>${cargo.id}</strong><br>
                <small>${cargo.weight/1000}t | ${cargo.dimensions.join('×')}m</small>
                <span class="cargo-badge badge-${cargo.type}">${cargo.type}</span>
            </div>
            <button onclick="removeCargo(${index})" style="width: auto; padding: 5px 10px; font-size: 12px; margin: 0;">×</button>
        `;
        listEl.appendChild(item);
    });
}

// Run optimization
async function runOptimization() {
    const shipData = {
        ship: {
            length: parseFloat(document.getElementById('ship-length').value),
            width: parseFloat(document.getElementById('ship-width').value),
            max_weight: parseFloat(document.getElementById('ship-max-weight').value)
        },
        cargo: cargoItems
    };

    // Show loading
    document.getElementById('loading').classList.add('show');

    try {
        const response = await fetch(`${API_URL}/optimize`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(shipData)
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        currentResults = await response.json();
        displayResults(currentResults);
        visualizer.visualize(currentResults);

    } catch (error) {
        alert(`Optimization failed: ${error.message}`);
        console.error(error);
    } finally {
        document.getElementById('loading').classList.remove('show');
    }
}

// Display results
function displayResults(data) {
    const resultsEl = document.getElementById('results');
    const metricsEl = document.getElementById('metrics');

    resultsEl.style.display = 'block';

    const analysis = data.analysis;

    // Determine status classes
    const stabilityClass = analysis.stability_status === 'optimal' ? 'status-optimal' :
                          analysis.stability_status === 'critical' ? 'status-critical' : 'status-warning';

    const balanceClass = analysis.balance_status === 'good' ? 'status-optimal' : 'status-warning';

    metricsEl.innerHTML = `
        <div class="metric">
            <span class="metric-label">Placement Rate</span>
            <span class="metric-value">${analysis.placed_count}/${analysis.total_count} items</span>
        </div>
        <div class="metric">
            <span class="metric-label">Total Weight</span>
            <span class="metric-value">${(analysis.total_ship_weight/1000).toFixed(1)}t (${analysis.capacity_used_percent.toFixed(1)}%)</span>
        </div>
        <div class="metric">
            <span class="metric-label">Center of Gravity</span>
            <span class="metric-value">${analysis.center_of_gravity.longitudinal_percent.toFixed(1)}% / ${analysis.center_of_gravity.transverse_percent.toFixed(1)}%</span>
        </div>
        <div class="metric">
            <span class="metric-label">Balance</span>
            <span class="metric-value ${balanceClass}">${analysis.balance_status.toUpperCase()}</span>
        </div>
        <div class="metric">
            <span class="metric-label">Metacentric Height (GM)</span>
            <span class="metric-value">${analysis.metacentric_height ? analysis.metacentric_height.toFixed(2) + 'm' : 'N/A'}</span>
        </div>
        <div class="metric">
            <span class="metric-label">Stability</span>
            <span class="metric-value ${stabilityClass}">${analysis.stability_status.toUpperCase()}</span>
        </div>
    `;

    // Show warnings if any
    if (data.execution && data.execution.warnings.length > 0) {
        const warningsEl = document.createElement('div');
        warningsEl.style.marginTop = '15px';
        warningsEl.style.padding = '10px';
        warningsEl.style.background = '#fff3cd';
        warningsEl.style.borderRadius = '5px';
        warningsEl.innerHTML = '<strong>Warnings:</strong><br>' +
            data.execution.warnings.filter(w => w.trim()).join('<br>');
        metricsEl.appendChild(warningsEl);
    }
}

// Load challenges
async function loadChallenges() {
    try {
        const response = await fetch(`${API_URL}/challenges`);
        const data = await response.json();

        const cardsEl = document.getElementById('challenge-cards');
        cardsEl.innerHTML = '';

        data.challenges.forEach(challenge => {
            const card = document.createElement('div');
            card.className = 'challenge-card';
            card.onclick = () => loadChallenge(challenge);

            card.innerHTML = `
                <span class="challenge-difficulty diff-${challenge.difficulty}">${challenge.difficulty}</span>
                <h3 style="margin: 10px 0;">${challenge.title}</h3>
                <p style="color: #666; line-height: 1.4;">${challenge.description}</p>
                <p style="margin-top: 10px; font-size: 14px; color: #999;">
                    ${challenge.cargo.length} cargo items
                </p>
            `;

            cardsEl.appendChild(card);
        });
    } catch (error) {
        console.error('Failed to load challenges:', error);
    }
}

// Load a specific challenge
function loadChallenge(challenge) {
    // Switch to simulator tab
    switchTab('simulator');

    // Load challenge data
    document.getElementById('ship-length').value = challenge.ship.length;
    document.getElementById('ship-width').value = challenge.ship.width;
    document.getElementById('ship-max-weight').value = challenge.ship.max_weight;

    cargoItems = JSON.parse(JSON.stringify(challenge.cargo));
    updateCargoList();

    alert(`Challenge loaded: ${challenge.title}\n\nGoal: ${challenge.description}\n\nClick "Optimize Placement" to solve!`);
}
