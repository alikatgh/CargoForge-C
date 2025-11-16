// Three.js 3D Cargo Visualizer

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

        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
        this.renderer.setClearColor(0x1a1a2e);
        this.container.appendChild(this.renderer.domElement);

        // Lighting
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
        this.scene.add(ambientLight);

        const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
        directionalLight.position.set(50, 100, 50);
        this.scene.add(directionalLight);

        // Camera position
        this.camera.position.set(200, 150, 200);
        this.camera.lookAt(0, 0, 0);

        // Orbit controls
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.05;

        // Grid helper
        const gridHelper = new THREE.GridHelper(400, 40, 0x444444, 0x222222);
        gridHelper.position.y = -20;
        this.scene.add(gridHelper);

        // Axes helper
        const axesHelper = new THREE.AxesHelper(100);
        this.scene.add(axesHelper);

        // Storage
        this.shipMesh = null;
        this.cargoMeshes = [];

        // Animation loop
        this.animate();

        // Handle window resize
        window.addEventListener('resize', () => this.onWindowResize());

        // Initial render
        this.showEmptyShip(150, 25);
    }

    animate() {
        requestAnimationFrame(() => this.animate());
        this.controls.update();
        this.renderer.render(this.scene, this.camera);
    }

    onWindowResize() {
        if (!this.container) return;
        this.camera.aspect = this.container.clientWidth / this.container.clientHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
    }

    clear() {
        // Remove existing ship and cargo
        if (this.shipMesh) {
            this.scene.remove(this.shipMesh);
            this.shipMesh = null;
        }

        this.cargoMeshes.forEach(mesh => this.scene.remove(mesh));
        this.cargoMeshes = [];
    }

    showEmptyShip(length, width) {
        this.clear();

        // Create ship hull (semi-transparent box)
        const hullGeometry = new THREE.BoxGeometry(length, 12, width);
        const hullMaterial = new THREE.MeshPhongMaterial({
            color: 0x2c3e50,
            transparent: true,
            opacity: 0.2,
            wireframe: false
        });
        this.shipMesh = new THREE.Mesh(hullGeometry, hullMaterial);
        this.shipMesh.position.set(length/2, -4, width/2);
        this.scene.add(this.shipMesh);

        // Ship outline
        const edges = new THREE.EdgesGeometry(hullGeometry);
        const lineMaterial = new THREE.LineBasicMaterial({ color: 0x4dabf7, linewidth: 2 });
        const outline = new THREE.LineSegments(edges, lineMaterial);
        outline.position.copy(this.shipMesh.position);
        this.scene.add(outline);

        // Waterline
        const waterGeometry = new THREE.PlaneGeometry(length, width);
        const waterMaterial = new THREE.MeshPhongMaterial({
            color: 0x1e90ff,
            transparent: true,
            opacity: 0.3,
            side: THREE.DoubleSide
        });
        const water = new THREE.Mesh(waterGeometry, waterMaterial);
        water.rotation.x = -Math.PI / 2;
        water.position.set(length/2, 0, width/2);
        this.scene.add(water);

        // Adjust camera
        this.camera.position.set(length * 1.5, length * 0.8, width * 1.5);
        this.controls.target.set(length/2, 0, width/2);
    }

    visualize(data) {
        this.clear();

        const ship = data.ship;
        const cargo = data.cargo;

        // Draw ship hull
        this.showEmptyShip(ship.length, ship.width);

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

        // Draw each cargo item
        cargo.forEach(item => {
            if (!item.placed || !item.position) return;

            const [length, width, height] = item.dimensions;
            const pos = item.position;

            // Create cargo box
            const geometry = new THREE.BoxGeometry(length, height, width);
            const color = colorMap[item.type] || 0x667eea;
            const material = new THREE.MeshPhongMaterial({
                color: color,
                transparent: false,
                opacity: 0.9
            });

            const mesh = new THREE.Mesh(geometry, material);
            mesh.position.set(
                pos.x + length/2,
                pos.z + height/2,
                pos.y + width/2
            );

            this.scene.add(mesh);
            this.cargoMeshes.push(mesh);

            // Add edges for better visibility
            const edges = new THREE.EdgesGeometry(geometry);
            const lineMat = new THREE.LineBasicMaterial({ color: 0xffffff, linewidth: 1 });
            const outline = new THREE.LineSegments(edges, lineMat);
            outline.position.copy(mesh.position);
            this.scene.add(outline);
            this.cargoMeshes.push(outline);

            // Add label
            this.addLabel(item.id, mesh.position);
        });

        // Add center of gravity marker
        if (data.analysis && data.analysis.center_of_gravity) {
            const cg = data.analysis.center_of_gravity;
            const cgX = (ship.length * cg.longitudinal_percent) / 100;
            const cgY = (ship.width * cg.transverse_percent) / 100;

            const sphereGeometry = new THREE.SphereGeometry(2, 16, 16);
            const sphereMaterial = new THREE.MeshBasicMaterial({ color: 0xff0000 });
            const cgMarker = new THREE.Mesh(sphereGeometry, sphereMaterial);
            cgMarker.position.set(cgX, 5, cgY);
            this.scene.add(cgMarker);
            this.cargoMeshes.push(cgMarker);
        }
    }

    addLabel(text, position) {
        // Create canvas for text
        const canvas = document.createElement('canvas');
        const context = canvas.getContext('2d');
        canvas.width = 256;
        canvas.height = 64;

        context.fillStyle = 'rgba(0, 0, 0, 0.7)';
        context.fillRect(0, 0, canvas.width, canvas.height);

        context.font = 'Bold 20px Arial';
        context.fillStyle = 'white';
        context.textAlign = 'center';
        context.fillText(text, canvas.width/2, canvas.height/2 + 7);

        const texture = new THREE.CanvasTexture(canvas);
        const material = new THREE.SpriteMaterial({ map: texture });
        const sprite = new THREE.Sprite(material);

        sprite.position.set(position.x, position.y + 5, position.z);
        sprite.scale.set(10, 2.5, 1);

        this.scene.add(sprite);
        this.cargoMeshes.push(sprite);
    }
}
