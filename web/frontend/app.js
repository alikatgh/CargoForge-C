// CargoForge-C INTERACTIVE Simulator - REAL PHYSICS + MANUAL PLACEMENT!
// Place cargo by hand and watch the ship tilt in real-time

const API_URL = 'http://localhost:5000/api';

let cargoItems = [];
let visualizer = null;
let currentResults = null;

// Game state
let gameState = {
    mode: 'setup', // 'setup', 'playing', 'finished'
    placedCargo: [],
    remainingCargo: [],
    startTime: null,
    score: 0,
    seaState: 0
};

// Initialize application
document.addEventListener('DOMContentLoaded', () => {
    visualizer = new CargoVisualizer('viewport');
    loadExample();
    setupEventListeners();
    updateUI();
});

function setupEventListeners() {
    // Sea state slider
    const seaStateSlider = document.getElementById('sea-state');
    if (seaStateSlider) {
        seaStateSlider.addEventListener('input', (e) => {
            gameState.seaState = parseInt(e.target.value);
            visualizer.setSeaState(gameState.seaState);
            updateSeaStateDisplay();
        });
    }
}

function updateSeaStateDisplay() {
    const display = document.getElementById('sea-state-value');
    if (display) {
        const descriptions = [
            'Calm (0)',
            'Light (1)',
            'Moderate (2)',
            'Rough (3)',
            'Very Rough (4)',
            'High (5)',
            'Very High (6)',
            'Phenomenal (7-9)'
        ];
        const index = Math.min(gameState.seaState, descriptions.length - 1);
        display.textContent = descriptions[index];
    }
}

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

    const displayItems = gameState.mode === 'playing' ? gameState.remainingCargo : cargoItems;

    displayItems.forEach((cargo, index) => {
        const item = document.createElement('div');
        item.className = 'cargo-item';

        if (gameState.mode === 'playing') {
            item.classList.add('cargo-draggable');
            item.onclick = () => selectCargoForPlacement(cargo);
        }

        item.innerHTML = `
            <div>
                <strong>${cargo.id}</strong><br>
                <small>${cargo.weight/1000}t | ${cargo.dimensions.join('×')}m</small>
                <span class="cargo-badge badge-${cargo.type}">${cargo.type}</span>
            </div>
            ${gameState.mode === 'setup' ? `<button onclick="removeCargo(${index})" style="width: auto; padding: 5px 10px; font-size: 12px; margin: 0;">×</button>` : ''}
        `;
        listEl.appendChild(item);
    });

    if (gameState.mode === 'playing' && gameState.remainingCargo.length === 0) {
        listEl.innerHTML = '<div style="text-align: center; padding: 20px; color: #51cf66; font-weight: bold;">✓ All cargo placed!</div>';
        finishGame();
    }
}

// START INTERACTIVE MODE
function startInteractiveMode() {
    const ship = {
        length: parseFloat(document.getElementById('ship-length').value),
        width: parseFloat(document.getElementById('ship-width').value),
        max_weight: parseFloat(document.getElementById('ship-max-weight').value),
        lightship_weight: parseFloat(document.getElementById('ship-max-weight').value) * 0.3,
        lightship_kg: 5.0
    };

    gameState.mode = 'playing';
    gameState.placedCargo = [];
    gameState.remainingCargo = [...cargoItems];
    gameState.startTime = Date.now();
    gameState.selectedCargo = null;

    // Setup ship
    visualizer.clear();
    visualizer.showEmptyShip(ship.length, ship.width);
    visualizer.currentShip = ship;

    updateCargoList();
    updateUI();
    updateStatusDisplay();

    alert('🎮 INTERACTIVE MODE\n\nClick on a cargo item, then click on the ship to place it.\nWatch the ship tilt in real-time!\n\nTry to keep it stable!');
}

let selectedCargo = null;

function selectCargoForPlacement(cargo) {
    selectedCargo = cargo;

    // Highlight selected cargo
    document.querySelectorAll('.cargo-item').forEach(el => {
        el.style.border = 'none';
    });

    event.target.closest('.cargo-item').style.border = '3px solid #667eea';

    updateStatusDisplay();
    alert(`Selected: ${cargo.id}\n\nNow click on the ship deck to place it!`);

    // Enable click-to-place on viewport
    enableShipClick();
}

function enableShipClick() {
    const viewport = document.getElementById('viewport');
    viewport.style.cursor = 'crosshair';

    viewport.onclick = (event) => {
        if (!selectedCargo) return;

        const ship = visualizer.currentShip;

        // Simple placement: put cargo at a grid position
        // In a full version, this would use raycasting to place at mouse position
        const placedCount = gameState.placedCargo.length;
        const gridX = (placedCount % 3) * (ship.length / 3);
        const gridY = Math.floor(placedCount / 3) * (ship.width / 3);

        // Add position to cargo
        selectedCargo.position = {
            x: gridX,
            y: gridY,
            z: -8.0 // Below deck
        };

        // Place cargo visually
        visualizer.placeCargo(selectedCargo);

        // Move cargo from remaining to placed
        gameState.placedCargo.push(selectedCargo);
        gameState.remainingCargo = gameState.remainingCargo.filter(c => c !== selectedCargo);

        // Update physics
        const stability = visualizer.updatePhysics(ship, gameState.placedCargo);

        // Update UI
        updateCargoList();
        updateStatusDisplay();
        displayStability(stability);

        // Clear selection
        selectedCargo = null;
        viewport.style.cursor = 'default';
        viewport.onclick = null;

        // Check if capsized
        if (stability.isCapsized) {
            setTimeout(() => {
                alert('⚠️ SHIP CAPSIZED! ⚠️\n\nThe ship has rolled over due to poor weight distribution!\n\nGame Over!');
                resetGame();
            }, 1000);
        }
    };
}

function displayStability(stability) {
    const resultsEl = document.getElementById('results');
    const metricsEl = document.getElementById('metrics');

    resultsEl.style.display = 'block';

    const statusClass = stability.stabilityStatus === 'optimal' ? 'status-optimal' :
                        stability.stabilityStatus === 'critical' ? 'status-critical' : 'status-warning';

    metricsEl.innerHTML = `
        <div class="metric">
            <span class="metric-label">List Angle (Roll)</span>
            <span class="metric-value ${statusClass}">${stability.listAngle.toFixed(2)}°</span>
        </div>
        <div class="metric">
            <span class="metric-label">Trim Angle (Pitch)</span>
            <span class="metric-value">${stability.trimAngle.toFixed(2)}°</span>
        </div>
        <div class="metric">
            <span class="metric-label">Metacentric Height (GM)</span>
            <span class="metric-value">${stability.gm.toFixed(2)}m</span>
        </div>
        <div class="metric">
            <span class="metric-label">Draft</span>
            <span class="metric-value">${stability.draft.toFixed(2)}m</span>
        </div>
        <div class="metric">
            <span class="metric-label">Stability Status</span>
            <span class="metric-value ${statusClass}">${stability.stabilityStatus.toUpperCase()}</span>
        </div>
        <div class="metric">
            <span class="metric-label">Total Weight</span>
            <span class="metric-value">${(stability.totalWeight/1000).toFixed(1)}t</span>
        </div>
    `;

    // Show warnings
    if (stability.warnings && stability.warnings.length > 0) {
        const warningsEl = document.createElement('div');
        warningsEl.style.cssText = 'margin-top: 15px; padding: 15px; background: #fff3cd; border-radius: 5px; border-left: 4px solid #ffc107;';
        warningsEl.innerHTML = '<strong>⚠️ Warnings:</strong><br>' +
            stability.warnings.join('<br>');
        metricsEl.appendChild(warningsEl);
    }
}

function updateStatusDisplay() {
    const statusEl = document.getElementById('game-status');
    if (!statusEl) return;

    if (gameState.mode === 'playing') {
        const elapsed = Math.floor((Date.now() - gameState.startTime) / 1000);
        statusEl.innerHTML = `
            <strong>Time:</strong> ${elapsed}s |
            <strong>Placed:</strong> ${gameState.placedCargo.length}/${cargoItems.length} |
            ${selectedCargo ? `<strong style="color: #667eea;">Selected: ${selectedCargo.id}</strong>` : '<strong>Click a cargo to select</strong>'}
        `;
    } else {
        statusEl.innerHTML = '';
    }
}

function finishGame() {
    if (!visualizer.currentStability) return;

    const elapsed = Math.floor((Date.now() - gameState.startTime) / 1000);
    const stability = visualizer.currentStability;

    // Calculate final score
    const scoreData = visualizer.physics.calculateScore(
        stability,
        gameState.placedCargo.length,
        cargoItems.length,
        elapsed
    );

    gameState.score = scoreData.total;

    setTimeout(() => {
        alert(`🎉 MISSION COMPLETE! 🎉\n\n` +
              `Total Score: ${scoreData.total}/100\n\n` +
              `Placement: ${scoreData.placement}/100\n` +
              `Stability: ${scoreData.stability}/100\n` +
              `Time Bonus: ${scoreData.timeBonus}/100\n\n` +
              `Final Status: ${stability.stabilityStatus.toUpperCase()}\n` +
              `GM: ${stability.gm.toFixed(2)}m\n` +
              `List: ${stability.listAngle.toFixed(2)}°`);

        gameState.mode = 'finished';
        updateUI();
    }, 500);
}

function resetGame() {
    gameState.mode = 'setup';
    gameState.placedCargo = [];
    gameState.remainingCargo = [];
    selectedCargo = null;

    visualizer.clear();
    const ship = {
        length: parseFloat(document.getElementById('ship-length').value),
        width: parseFloat(document.getElementById('ship-width').value)
    };
    visualizer.showEmptyShip(ship.length, ship.width);

    updateCargoList();
    updateUI();
    document.getElementById('results').style.display = 'none';
}

function updateUI() {
    const startBtn = document.getElementById('start-interactive-btn');
    const resetBtn = document.getElementById('reset-btn');
    const addCargoBtn = document.querySelector('button[onclick="addCargo()"]');

    if (gameState.mode === 'setup') {
        if (startBtn) startBtn.style.display = 'block';
        if (resetBtn) resetBtn.style.display = 'none';
        if (addCargoBtn) addCargoBtn.style.display = 'block';
    } else {
        if (startBtn) startBtn.style.display = 'none';
        if (resetBtn) resetBtn.style.display = 'block';
        if (addCargoBtn) addCargoBtn.style.display = 'none';
    }
}

// Update status every second
setInterval(() => {
    if (gameState.mode === 'playing') {
        updateStatusDisplay();
    }
}, 1000);

// Load challenges (keeping original functionality)
async function loadChallenges() {
    // Stub for now - can be implemented later
    const cardsEl = document.getElementById('challenge-cards');
    if (!cardsEl) return;

    cardsEl.innerHTML = `
        <div class="challenge-card">
            <span class="challenge-difficulty diff-beginner">Beginner</span>
            <h3 style="margin: 10px 0;">Basic Balance</h3>
            <p style="color: #666;">Place 3 containers without tilting the ship more than 2°</p>
        </div>
        <div class="challenge-card">
            <span class="challenge-difficulty diff-intermediate">Intermediate</span>
            <h3 style="margin: 10px 0;">Rough Seas</h3>
            <p style="color: #666;">Maintain stability in Sea State 4 with mixed cargo</p>
        </div>
        <div class="challenge-card">
            <span class="challenge-difficulty diff-advanced">Advanced</span>
            <h3 style="margin: 10px 0;">Emergency Rebalance</h3>
            <p style="color: #666;">Ship is already listing 8° - redistribute cargo to fix it!</p>
        </div>
    `;
}

loadChallenges();
