// CargoForge Game UI Controller
// Connects modern game UI with physics and visualization

class GameController {
    constructor() {
        // Core systems
        this.visualizer = null;
        this.physics = new MaritimePhysics();

        // Game state
        this.currentLevel = 1;
        this.score = 0;
        this.startTime = null;
        this.gameTimer = null;
        this.selectedCargo = null;
        this.placedCargo = [];
        this.remainingCargo = [];

        // Level data
        this.levels = this.createLevels();

        // UI elements
        this.elements = {
            loadingScreen: document.getElementById('loading-screen'),
            tutorialOverlay: document.getElementById('tutorial-overlay'),
            levelComplete: document.getElementById('level-complete'),
            viewport: document.getElementById('viewport'),
            cargoGrid: document.getElementById('cargo-grid'),
            // HUD displays
            levelDisplay: document.getElementById('level-display'),
            scoreDisplay: document.getElementById('score-display'),
            timeDisplay: document.getElementById('time-display'),
            gmDisplay: document.getElementById('gm-display'),
            listDisplay: document.getElementById('list-display'),
            cargoDisplay: document.getElementById('cargo-display'),
            // Stability indicator
            stabilityStatus: document.getElementById('stability-status'),
            stabilityMeterFill: document.getElementById('stability-meter-fill'),
            // Level complete
            finalScoreDisplay: document.getElementById('final-score-display'),
            placementScore: document.getElementById('placement-score'),
            stabilityScore: document.getElementById('stability-score'),
            timeScore: document.getElementById('time-score')
        };

        // Initialize
        this.init();
    }

    async init() {
        console.log('🎮 Initializing CargoForge...');

        // Wait for visualizer to load
        setTimeout(() => {
            try {
                this.visualizer = new CargoVisualizer('viewport');
                console.log('✅ Visualizer loaded');

                // Hide loading screen
                this.elements.loadingScreen.classList.add('hidden');

                // Show tutorial
                this.showTutorial();

                // Start first level
                this.loadLevel(1);
            } catch (error) {
                console.error('❌ Failed to initialize:', error);
                this.showNotification('Failed to initialize game', 'error');
            }
        }, 1000);
    }

    createLevels() {
        return [
            {
                id: 1,
                name: 'Getting Started',
                ship: {
                    length: 100,
                    width: 20,
                    max_weight: 10000000,
                    lightship_weight: 2000000,
                    lightship_kg: 6.0
                },
                cargo: [
                    { id: 'C1', weight: 250000, dimensions: [12, 2.4, 2.6], type: 'standard' },
                    { id: 'C2', weight: 250000, dimensions: [12, 2.4, 2.6], type: 'standard' },
                    { id: 'C3', weight: 200000, dimensions: [10, 2.4, 2.6], type: 'standard' }
                ],
                goals: {
                    min_gm: 1.0,
                    max_list: 2.0,
                    all_placed: true
                }
            },
            {
                id: 2,
                name: 'Balance Master',
                ship: {
                    length: 150,
                    width: 25,
                    max_weight: 50000000,
                    lightship_weight: 5000000,
                    lightship_kg: 7.0
                },
                cargo: [
                    { id: 'Heavy', weight: 800000, dimensions: [15, 5, 4], type: 'heavy' },
                    { id: 'Light1', weight: 200000, dimensions: [8, 3, 2], type: 'standard' },
                    { id: 'Light2', weight: 200000, dimensions: [8, 3, 2], type: 'standard' },
                    { id: 'Mid', weight: 400000, dimensions: [10, 4, 3], type: 'standard' },
                    { id: 'Bal', weight: 600000, dimensions: [12, 4, 3], type: 'standard' }
                ],
                goals: {
                    min_gm: 1.5,
                    max_list: 1.5,
                    all_placed: true
                }
            },
            {
                id: 3,
                name: 'Heavy Load Challenge',
                ship: {
                    length: 180,
                    width: 28,
                    max_weight: 80000000,
                    lightship_weight: 8000000,
                    lightship_kg: 8.0
                },
                cargo: [
                    { id: 'H1', weight: 1200000, dimensions: [20, 6, 5], type: 'heavy' },
                    { id: 'H2', weight: 1200000, dimensions: [20, 6, 5], type: 'heavy' },
                    { id: 'Reef', weight: 300000, dimensions: [12, 2.4, 2.6], type: 'reefer' },
                    { id: 'S1', weight: 250000, dimensions: [12, 2.4, 2.6], type: 'standard' },
                    { id: 'S2', weight: 250000, dimensions: [12, 2.4, 2.6], type: 'standard' },
                    { id: 'S3', weight: 250000, dimensions: [12, 2.4, 2.6], type: 'standard' }
                ],
                goals: {
                    min_gm: 1.2,
                    max_list: 1.0,
                    all_placed: true
                }
            }
        ];
    }

    loadLevel(levelNum) {
        const level = this.levels[levelNum - 1];
        if (!level) {
            this.showNotification('No more levels!', 'success');
            return;
        }

        console.log(`📦 Loading Level ${levelNum}: ${level.name}`);

        // Reset state
        this.currentLevel = levelNum;
        this.placedCargo = [];
        this.remainingCargo = [...level.cargo];
        this.selectedCargo = null;
        this.startTime = Date.now();

        // Update HUD
        this.elements.levelDisplay.textContent = levelNum;
        this.updateCargoDisplay();

        // Load ship
        this.visualizer.loadShip(level.ship, []);

        // Create cargo cards
        this.createCargoCards(level.cargo);

        // Start timer
        this.startGameTimer();

        this.showNotification(`Level ${levelNum}: ${level.name}`, 'success');
    }

    createCargoCards(cargoList) {
        this.elements.cargoGrid.innerHTML = '';

        cargoList.forEach(cargo => {
            const card = document.createElement('div');
            card.className = 'cargo-card';
            card.dataset.cargoId = cargo.id;

            // Icon based on type
            const icon = this.getCargoIcon(cargo.type);

            // Type badge
            const badge = cargo.type !== 'standard'
                ? `<div class="cargo-type-badge ${cargo.type}">${cargo.type}</div>`
                : '';

            card.innerHTML = `
                ${badge}
                <div class="cargo-icon">${icon}</div>
                <div class="cargo-name">${cargo.id}</div>
                <div class="cargo-specs">
                    <span>${(cargo.weight / 1000).toFixed(0)}t</span>
                    <span>${cargo.dimensions[0]}×${cargo.dimensions[1]}m</span>
                </div>
            `;

            // Click handler
            card.addEventListener('click', () => this.selectCargo(cargo, card));

            this.elements.cargoGrid.appendChild(card);
        });
    }

    getCargoIcon(type) {
        const icons = {
            'standard': '📦',
            'heavy': '🔩',
            'reefer': '❄️',
            'hazardous': '☢️'
        };
        return icons[type] || '📦';
    }

    selectCargo(cargo, cardElement) {
        // Check if already placed
        if (this.placedCargo.find(c => c.id === cargo.id)) {
            this.showNotification('Cargo already placed!', 'warning');
            return;
        }

        // Deselect previous
        document.querySelectorAll('.cargo-card.selected').forEach(el => {
            el.classList.remove('selected');
        });

        // Select this cargo
        this.selectedCargo = cargo;
        cardElement.classList.add('selected');

        this.showNotification(`✅ Selected ${cargo.id} - Click on ship deck to place`, 'success');

        // Enable ship click and preview
        this.enableShipPlacement();
        this.enableGhostPreview();
    }

    enableShipPlacement() {
        // Add click handler to viewport
        const handleClick = (event) => {
            if (!this.selectedCargo) return;

            // Get mouse position in normalized device coordinates
            const rect = this.elements.viewport.getBoundingClientRect();
            const mouseX = ((event.clientX - rect.left) / rect.width) * 2 - 1;
            const mouseY = -((event.clientY - rect.top) / rect.height) * 2 + 1;

            // Use raycasting to get actual deck position
            const position = this.visualizer.getDeckClickPosition(mouseX, mouseY);

            if (position) {
                this.placeCargo(position);
            } else {
                this.showNotification('Click on the ship deck!', 'warning');
            }
        };

        // Remove old listener if exists
        if (this.placementHandler) {
            this.elements.viewport.removeEventListener('click', this.placementHandler);
        }

        this.placementHandler = handleClick;
        this.elements.viewport.addEventListener('click', handleClick);
    }

    enableGhostPreview() {
        // Add mouse move handler for ghost preview
        const handleMove = (event) => {
            if (!this.selectedCargo) {
                this.visualizer.removeGhostCargo();
                return;
            }

            // Get mouse position
            const rect = this.elements.viewport.getBoundingClientRect();
            const mouseX = ((event.clientX - rect.left) / rect.width) * 2 - 1;
            const mouseY = -((event.clientY - rect.top) / rect.height) * 2 + 1;

            // Get deck position
            const position = this.visualizer.getDeckClickPosition(mouseX, mouseY);

            if (position) {
                // Show ghost at this position
                this.visualizer.showGhostCargo(
                    position,
                    this.selectedCargo.dimensions,
                    this.selectedCargo.type
                );
                this.elements.viewport.style.cursor = 'crosshair';
            } else {
                this.visualizer.removeGhostCargo();
                this.elements.viewport.style.cursor = 'default';
            }
        };

        // Remove old listener if exists
        if (this.previewHandler) {
            this.elements.viewport.removeEventListener('mousemove', this.previewHandler);
        }

        this.previewHandler = handleMove;
        this.elements.viewport.addEventListener('mousemove', handleMove);
    }

    disableGhostPreview() {
        if (this.previewHandler) {
            this.elements.viewport.removeEventListener('mousemove', this.previewHandler);
            this.previewHandler = null;
        }
        this.visualizer.removeGhostCargo();
        this.elements.viewport.style.cursor = 'default';
    }

    placeCargo(position) {
        if (!this.selectedCargo) return;

        const level = this.levels[this.currentLevel - 1];
        const ship = level.ship;

        // Create placed cargo object with REAL position from click
        const placedCargo = {
            ...this.selectedCargo,
            position: position  // Use actual clicked position!
        };

        // Add to placed list
        this.placedCargo.push(placedCargo);

        // Remove from remaining
        this.remainingCargo = this.remainingCargo.filter(c => c.id !== this.selectedCargo.id);

        // Update visualization
        this.visualizer.placeCargo(placedCargo);

        // Remove ghost
        this.visualizer.removeGhostCargo();

        // Calculate physics
        const stability = this.physics.calculateStability(ship, this.placedCargo);
        this.currentStability = stability;

        // Update HUD
        this.updateStabilityDisplay(stability);
        this.updateCargoDisplay();

        // Mark card as placed
        const card = document.querySelector(`[data-cargo-id="${this.selectedCargo.id}"]`);
        if (card) {
            card.classList.remove('selected');
            card.classList.add('placed');
        }

        // Show notification with position info
        this.showNotification(
            `✅ Placed ${this.selectedCargo.id} at (${position.x}m, ${position.y}m)`,
            'success'
        );

        // Check for capsizing
        if (stability.isCapsized) {
            this.handleCapsizing();
            return;
        }

        // Check for level complete
        if (this.remainingCargo.length === 0) {
            this.checkLevelComplete();
        }

        // Deselect
        this.selectedCargo = null;
        this.disableGhostPreview();
    }

    updateStabilityDisplay(stability) {
        // GM display
        const gm = stability.gm || 0;
        this.elements.gmDisplay.textContent = `${gm.toFixed(2)}m`;
        this.elements.gmDisplay.className = 'stat-value';
        if (gm < 0.5) this.elements.gmDisplay.classList.add('danger');
        else if (gm < 1.0) this.elements.gmDisplay.classList.add('warning');

        // List display
        const list = Math.abs(stability.listAngle || 0);
        this.elements.listDisplay.textContent = `${list.toFixed(1)}°`;
        this.elements.listDisplay.className = 'stat-value';
        if (list > 5.0) this.elements.listDisplay.classList.add('danger');
        else if (list > 2.0) this.elements.listDisplay.classList.add('warning');

        // Stability status
        const status = stability.stabilityStatus || 'optimal';
        this.elements.stabilityStatus.textContent = status.toUpperCase();
        this.elements.stabilityStatus.style.color =
            status === 'critical' ? '#f87171' :
            status === 'warning' ? '#fbbf24' :
            status === 'good' ? '#34d399' : '#34d399';

        // Stability meter
        const meterWidth = Math.min(100, Math.max(0, (gm / 2.5) * 100));
        this.elements.stabilityMeterFill.style.width = `${meterWidth}%`;
        this.elements.stabilityMeterFill.className = 'stability-meter-fill';
        if (status === 'critical') this.elements.stabilityMeterFill.classList.add('danger');
        else if (status === 'warning') this.elements.stabilityMeterFill.classList.add('warning');

        // Update score
        this.updateScore(stability);
    }

    updateCargoDisplay() {
        const level = this.levels[this.currentLevel - 1];
        const total = level.cargo.length;
        const placed = this.placedCargo.length;
        this.elements.cargoDisplay.textContent = `${placed}/${total}`;
    }

    updateScore(stability) {
        // Calculate score based on stability quality
        const baseScore = this.placedCargo.length * 100;

        const gmBonus = stability.gm >= 1.0 ? 50 : 0;
        const listBonus = Math.abs(stability.listAngle) < 1.0 ? 50 : 0;
        const stabilityBonus = stability.stabilityStatus === 'optimal' ? 100 : 0;

        this.score = baseScore + gmBonus + listBonus + stabilityBonus;
        this.elements.scoreDisplay.textContent = this.score;
    }

    handleCapsizing() {
        this.showNotification('⚠️ SHIP CAPSIZED! Starting over...', 'error');

        setTimeout(() => {
            this.loadLevel(this.currentLevel);
        }, 2000);
    }

    checkLevelComplete() {
        const level = this.levels[this.currentLevel - 1];
        const goals = level.goals;

        // Check goals
        const gmOk = this.currentStability.gm >= goals.min_gm;
        const listOk = Math.abs(this.currentStability.listAngle) <= goals.max_list;
        const allPlaced = this.placedCargo.length === level.cargo.length;

        if (gmOk && listOk && allPlaced) {
            this.completeLevel();
        } else {
            let msg = 'Level goals not met:\n';
            if (!gmOk) msg += `• GM must be ≥ ${goals.min_gm}m (current: ${this.currentStability.gm.toFixed(2)}m)\n`;
            if (!listOk) msg += `• List must be ≤ ${goals.max_list}° (current: ${Math.abs(this.currentStability.listAngle).toFixed(1)}°)\n`;
            this.showNotification(msg, 'warning');
        }
    }

    completeLevel() {
        // Stop timer
        if (this.gameTimer) {
            clearInterval(this.gameTimer);
        }

        const elapsedTime = (Date.now() - this.startTime) / 1000; // seconds

        // Calculate scores
        const placementScore = this.placedCargo.length * 100;
        const stabilityScore = this.currentStability.stabilityStatus === 'optimal' ? 500 :
                              this.currentStability.stabilityStatus === 'good' ? 300 : 100;
        const timeBonus = Math.max(0, 300 - Math.floor(elapsedTime / 10) * 10);
        const totalScore = placementScore + stabilityScore + timeBonus;

        // Update displays
        this.elements.finalScoreDisplay.textContent = totalScore;
        this.elements.placementScore.textContent = placementScore;
        this.elements.stabilityScore.textContent = stabilityScore;
        this.elements.timeScore.textContent = timeBonus;

        // Show modal
        this.elements.levelComplete.classList.add('active');

        this.showNotification('🎉 LEVEL COMPLETE!', 'success');
    }

    startGameTimer() {
        if (this.gameTimer) {
            clearInterval(this.gameTimer);
        }

        this.gameTimer = setInterval(() => {
            const elapsed = Math.floor((Date.now() - this.startTime) / 1000);
            const minutes = Math.floor(elapsed / 60);
            const seconds = elapsed % 60;
            this.elements.timeDisplay.textContent =
                `${minutes}:${seconds.toString().padStart(2, '0')}`;
        }, 1000);
    }

    showTutorial() {
        this.elements.tutorialOverlay.classList.add('active');
    }

    showNotification(message, type = 'success') {
        // Remove existing notifications
        document.querySelectorAll('.notification').forEach(n => n.remove());

        // Create notification
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.textContent = message;
        document.body.appendChild(notification);

        // Auto-remove after 3 seconds
        setTimeout(() => {
            notification.remove();
        }, 3000);
    }
}

// Global functions for button handlers
function closeTutorial() {
    document.getElementById('tutorial-overlay').classList.remove('active');
}

function showTutorial() {
    document.getElementById('tutorial-overlay').classList.add('active');
}

function nextLevel() {
    document.getElementById('level-complete').classList.remove('active');
    game.currentLevel++;
    game.loadLevel(game.currentLevel);
}

// Initialize game on load
let game;
window.addEventListener('load', () => {
    console.log('🚢 CargoForge starting...');
    game = new GameController();
});
