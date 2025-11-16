// Three.js 3D Cargo Visualizer - WITH REAL PHYSICS!
// Ship tilts, rolls, and can CAPSIZE based on cargo placement

class CargoVisualizer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.scene = new THREE.Scene();
        this.camera = new THREE.PerspectiveCamera(
            60,
            this.container.clientWidth / this.container.clientHeight,
            0.1,
            2000
        );

        this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
        this.renderer.setClearColor(0x87CEEB, 1.0); // Sky blue background
        this.container.appendChild(this.renderer.domElement);

        // Lighting
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.7);
        this.scene.add(ambientLight);

        const sunLight = new THREE.DirectionalLight(0xffffff, 1.0);
        sunLight.position.set(100, 200, 100);
        sunLight.castShadow = true;
        this.scene.add(sunLight);

        // Additional fill light
        const fillLight = new THREE.DirectionalLight(0xffffff, 0.3);
        fillLight.position.set(-50, 50, -50);
        this.scene.add(fillLight);

        // Camera position
        this.camera.position.set(200, 150, 200);
        this.camera.lookAt(0, 0, 0);

        // Orbit controls
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.05;
        this.controls.minDistance = 50;
        this.controls.maxDistance = 800;

        // Physics engine
        this.physics = new MaritimePhysics();

        // Ship container - ALL ship elements go in here (for rotation)
        this.shipContainer = new THREE.Group();
        this.scene.add(this.shipContainer);

        // Water plane (animated waves)
        this.waterPlane = null;
        this.waterTime = 0;

        // Storage
        this.shipMesh = null;
        this.shipOutline = null;
        this.cargoMeshes = [];
        this.cgMarker = null;
        this.warningOverlay = null;

        // Ship data
        this.currentShip = null;
        this.currentCargo = [];
        this.currentStability = null;

        // Animation
        this.clock = new THREE.Clock();
        this.animate();

        // Handle window resize
        window.addEventListener('resize', () => this.onWindowResize());

        // Create ocean
        this.createOcean();

        // Initial empty ship
        this.showEmptyShip(150, 25);
    }

    animate() {
        requestAnimationFrame(() => this.animate());

        const deltaTime = this.clock.getDelta();

        // Update controls
        this.controls.update();

        // Animate waves
        this.animateWaves(deltaTime);

        // Update ship physics (rolling/pitching from waves)
        if (this.currentStability) {
            this.updateShipPhysics(deltaTime);
        }

        this.renderer.render(this.scene, this.camera);
    }

    animateWaves(deltaTime) {
        if (!this.waterPlane) return;

        this.waterTime += deltaTime;

        // Animate water vertices for wave effect
        const positions = this.waterPlane.geometry.attributes.position;
        const waveAmplitude = this.physics.seaState * 0.5;
        const waveFreq = 0.3;

        for (let i = 0; i < positions.count; i++) {
            const x = positions.getX(i);
            const z = positions.getZ(i);

            // Multiple sine waves for realistic ocean
            const wave1 = Math.sin(x * 0.05 + this.waterTime * waveFreq) * waveAmplitude;
            const wave2 = Math.sin(z * 0.03 + this.waterTime * waveFreq * 1.3) * waveAmplitude * 0.5;

            positions.setY(i, wave1 + wave2);
        }

        positions.needsUpdate = true;
        this.waterPlane.geometry.computeVertexNormals();
    }

    updateShipPhysics(deltaTime) {
        if (!this.currentStability) return;

        // Get wave-induced motion
        const waveMotion = this.physics.calculateWaveMotion(this.currentStability, deltaTime);

        // Base tilt from cargo placement
        const baseTiltX = this.currentStability.listAngle || 0;  // Roll (port/starboard)
        const baseTiltZ = this.currentStability.trimAngle || 0;  // Pitch (bow/stern)

        // Add wave motion
        const totalTiltX = baseTiltX + waveMotion.roll;
        const totalTiltZ = baseTiltZ + waveMotion.pitch;

        // Apply rotation to ship container (THIS IS THE MAGIC!)
        this.shipContainer.rotation.z = totalTiltX * (Math.PI / 180); // Convert to radians
        this.shipContainer.rotation.x = -totalTiltZ * (Math.PI / 180);

        // Add heave (vertical motion from waves)
        this.shipContainer.position.y = waveMotion.heave;

        // Update visual warnings based on tilt
        this.updateVisualWarnings();

        // Check for capsizing
        if (this.currentStability.isCapsized) {
            this.showCapsizedEffect();
        }
    }

    updateVisualWarnings() {
        if (!this.shipMesh || !this.currentStability) return;

        // Change ship color based on stability
        let shipColor = 0x2c3e50; // Normal blue-grey
        let opacity = 0.3;

        if (this.currentStability.stabilityStatus === 'critical') {
            shipColor = 0xff0000; // Red - DANGER!
            opacity = 0.5;
            // Pulse effect
            opacity = 0.3 + 0.2 * Math.sin(Date.now() / 200);
        } else if (this.currentStability.stabilityStatus === 'warning') {
            shipColor = 0xffa500; // Orange - WARNING
            opacity = 0.4;
        }

        this.shipMesh.material.color.setHex(shipColor);
        this.shipMesh.material.opacity = opacity;
    }

    showCapsizedEffect() {
        // Create dramatic capsized overlay
        if (!this.warningOverlay) {
            const overlay = document.createElement('div');
            overlay.id = 'capsize-overlay';
            overlay.style.cssText = `
                position: absolute;
                top: 50%;
                left: 50%;
                transform: translate(-50%, -50%);
                background: rgba(255, 0, 0, 0.9);
                color: white;
                padding: 40px 60px;
                border-radius: 15px;
                font-size: 48px;
                font-weight: bold;
                text-align: center;
                z-index: 100;
                box-shadow: 0 0 30px rgba(255, 0, 0, 0.8);
                animation: pulse 1s infinite;
            `;
            overlay.innerHTML = '⚠️ CAPSIZED! ⚠️<br><span style="font-size: 24px;">Ship has rolled over!</span>';
            this.container.appendChild(overlay);
            this.warningOverlay = overlay;

            // Add CSS animation
            const style = document.createElement('style');
            style.textContent = `
                @keyframes pulse {
                    0%, 100% { transform: translate(-50%, -50%) scale(1); }
                    50% { transform: translate(-50%, -50%) scale(1.05); }
                }
            `;
            document.head.appendChild(style);
        }
    }

    clearCapsizedEffect() {
        if (this.warningOverlay) {
            this.warningOverlay.remove();
            this.warningOverlay = null;
        }
    }

    createOcean() {
        // Create animated ocean surface
        const oceanGeometry = new THREE.PlaneGeometry(1000, 1000, 100, 100);
        const oceanMaterial = new THREE.MeshPhongMaterial({
            color: 0x006994,
            transparent: true,
            opacity: 0.7,
            side: THREE.DoubleSide,
            shininess: 100,
            specular: 0x222222
        });

        this.waterPlane = new THREE.Mesh(oceanGeometry, oceanMaterial);
        this.waterPlane.rotation.x = -Math.PI / 2;
        this.waterPlane.position.y = 0;
        this.scene.add(this.waterPlane);
    }

    onWindowResize() {
        if (!this.container) return;
        this.camera.aspect = this.container.clientWidth / this.container.clientHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
    }

    clear() {
        // Remove all cargo from ship container
        this.cargoMeshes.forEach(mesh => this.shipContainer.remove(mesh));
        this.cargoMeshes = [];

        // Remove CG marker
        if (this.cgMarker) {
            this.shipContainer.remove(this.cgMarker);
            this.cgMarker = null;
        }

        // Reset ship rotation
        this.shipContainer.rotation.set(0, 0, 0);
        this.shipContainer.position.set(0, 0, 0);

        // Clear capsized effect
        this.clearCapsizedEffect();
    }

    showEmptyShip(length, width) {
        // Clear existing
        if (this.shipMesh) {
            this.shipContainer.remove(this.shipMesh);
            this.shipContainer.remove(this.shipOutline);
        }

        // Store ship dimensions
        this.currentShip = { length, width };

        // Create realistic ship hull
        const hullGeometry = new THREE.BoxGeometry(length, 12, width);
        const hullMaterial = new THREE.MeshPhongMaterial({
            color: 0x2c3e50,
            transparent: true,
            opacity: 0.3,
            shininess: 30
        });

        this.shipMesh = new THREE.Mesh(hullGeometry, hullMaterial);
        this.shipMesh.position.set(length/2, -4, width/2);
        this.shipMesh.castShadow = true;
        this.shipContainer.add(this.shipMesh);

        // Ship outline (edges)
        const edges = new THREE.EdgesGeometry(hullGeometry);
        const lineMaterial = new THREE.LineBasicMaterial({ color: 0x4dabf7, linewidth: 2 });
        this.shipOutline = new THREE.LineSegments(edges, lineMaterial);
        this.shipOutline.position.copy(this.shipMesh.position);
        this.shipContainer.add(this.shipOutline);

        // Add deck markings
        this.addDeckMarkings(length, width);

        // Adjust camera to frame ship
        this.camera.position.set(length * 1.5, length * 0.8, width * 1.5);
        this.controls.target.set(length/2, 0, width/2);
    }

    addDeckMarkings(length, width) {
        // Add center line
        const centerLineGeom = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(0, 2, width/2),
            new THREE.Vector3(length, 2, width/2)
        ]);
        const centerLineMat = new THREE.LineBasicMaterial({ color: 0xffff00, linewidth: 2 });
        const centerLine = new THREE.Line(centerLineGeom, centerLineMat);
        this.shipContainer.add(centerLine);

        // Add midship marker
        const midshipGeom = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(length/2, 2, 0),
            new THREE.Vector3(length/2, 2, width)
        ]);
        const midshipLine = new THREE.Line(midshipGeom, centerLineMat);
        this.shipContainer.add(midshipLine);
    }

    /**
     * Update visualization with real-time physics
     * This is called continuously as cargo is placed
     */
    updatePhysics(ship, cargo) {
        this.currentShip = ship;
        this.currentCargo = cargo;

        // Calculate real-time stability
        this.currentStability = this.physics.calculateStability(ship, cargo);

        // Update visual elements
        this.updateCGMarker();
        this.updateVisualWarnings();

        return this.currentStability;
    }

    updateCGMarker() {
        if (!this.currentStability) return;

        // Remove old marker
        if (this.cgMarker) {
            this.shipContainer.remove(this.cgMarker);
        }

        // Create new CG marker
        const cg = this.currentStability.cg;
        const sphereGeometry = new THREE.SphereGeometry(2, 16, 16);
        const sphereMaterial = new THREE.MeshBasicMaterial({
            color: 0xff0000,
            transparent: true,
            opacity: 0.8
        });

        this.cgMarker = new THREE.Mesh(sphereGeometry, sphereMaterial);
        this.cgMarker.position.set(cg.x, cg.z + 2, cg.y);
        this.shipContainer.add(this.cgMarker);

        // Add CG marker label
        const label = this.createTextSprite('CG', 0xff0000);
        label.position.set(cg.x, cg.z + 6, cg.y);
        this.shipContainer.add(label);
        this.cargoMeshes.push(label);
    }

    /**
     * Place a cargo item visually
     */
    placeCargo(cargoItem) {
        if (!cargoItem.position) return;

        const [l, w, h] = cargoItem.dimensions;
        const pos = cargoItem.position;

        // Color map for cargo types
        const colorMap = {
            'standard': 0x667eea,
            'hazardous': 0xff6b6b,
            'reefer': 0x4dabf7,
            'fragile': 0xffd43b,
            'heavy': 0x495057,
            'bulk': 0x8c7ae6,
            'general': 0x20bf6b
        };

        // Create cargo box
        const geometry = new THREE.BoxGeometry(l, h, w);
        const color = colorMap[cargoItem.type] || 0x667eea;
        const material = new THREE.MeshPhongMaterial({
            color: color,
            transparent: false,
            shininess: 50
        });

        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.set(pos.x + l/2, pos.z + h/2, pos.y + w/2);
        mesh.castShadow = true;
        mesh.receiveShadow = true;

        this.shipContainer.add(mesh);
        this.cargoMeshes.push(mesh);

        // Add edges for better visibility
        const edges = new THREE.EdgesGeometry(geometry);
        const lineMat = new THREE.LineBasicMaterial({ color: 0xffffff, linewidth: 1 });
        const outline = new THREE.LineSegments(edges, lineMat);
        outline.position.copy(mesh.position);
        this.shipContainer.add(outline);
        this.cargoMeshes.push(outline);

        // Add label
        const label = this.createTextSprite(cargoItem.id, color);
        label.position.set(mesh.position.x, mesh.position.y + h/2 + 3, mesh.position.z);
        this.shipContainer.add(label);
        this.cargoMeshes.push(label);

        // Update physics
        return this.updatePhysics(this.currentShip, this.currentCargo);
    }

    createTextSprite(text, color) {
        const canvas = document.createElement('canvas');
        const context = canvas.getContext('2d');
        canvas.width = 256;
        canvas.height = 64;

        context.fillStyle = 'rgba(0, 0, 0, 0.7)';
        context.fillRect(0, 0, canvas.width, canvas.height);

        context.font = 'Bold 20px Arial';
        context.fillStyle = `#${color.toString(16).padStart(6, '0')}`;
        context.textAlign = 'center';
        context.fillText(text, canvas.width/2, canvas.height/2 + 7);

        const texture = new THREE.CanvasTexture(canvas);
        const material = new THREE.SpriteMaterial({ map: texture });
        const sprite = new THREE.Sprite(material);
        sprite.scale.set(10, 2.5, 1);

        return sprite;
    }

    /**
     * Set sea state for wave simulation
     */
    setSeaState(state) {
        this.physics.setSeaState(state);
    }

    /**
     * Get current stability info
     */
    getStability() {
        return this.currentStability;
    }
}
