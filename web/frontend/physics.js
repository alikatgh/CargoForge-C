// Maritime Physics Engine - REALISTIC ship stability simulation
// Based on naval architecture principles and IMO stability guidelines

class MaritimePhysics {
    constructor() {
        // Physical constants
        this.SEAWATER_DENSITY = 1.025; // tonnes per m³
        this.GRAVITY = 9.81; // m/s²

        // Stability thresholds (based on IMO guidelines)
        this.GM_CRITICAL = 0.3; // meters - below this is dangerous
        this.GM_OPTIMAL_MIN = 0.5; // meters
        this.GM_OPTIMAL_MAX = 2.5; // meters
        this.GM_STIFF = 3.0; // meters - above this is too stiff

        // List (roll) angle thresholds in degrees
        this.LIST_WARNING = 5.0; // degrees
        this.LIST_DANGEROUS = 10.0; // degrees
        this.LIST_CRITICAL = 15.0; // degrees - approaching capsizing
        this.LIST_CAPSIZE = 25.0; // degrees - ship capsizes

        // Trim (pitch) angle thresholds in degrees
        this.TRIM_WARNING = 2.0;
        this.TRIM_DANGEROUS = 5.0;
        this.TRIM_CRITICAL = 8.0;

        // Wave simulation parameters
        this.waveTime = 0;
        this.seaState = 0; // 0=calm, 1-9=wave height scale
    }

    /**
     * Calculate ship stability in real-time based on cargo placement
     * Returns complete stability analysis with tilt angles
     */
    calculateStability(ship, cargo) {
        // Calculate total weight and center of gravity
        let totalWeight = ship.lightship_weight || 0;
        let momentX = 0; // Longitudinal (fore-aft)
        let momentY = 0; // Transverse (port-starboard)
        let momentZ = 0; // Vertical

        // Account for lightship CG
        const lightshipKG = ship.lightship_kg || 5.0;
        momentZ = totalWeight * lightshipKG;

        // Sum up all placed cargo
        cargo.forEach(item => {
            if (!item.position) return; // Skip unplaced cargo

            const w = item.weight;
            const [l, w_dim, h] = item.dimensions;
            const p = item.position;

            // Calculate CG of this cargo item (center of box)
            const cgX = p.x + l / 2;
            const cgY = p.y + w_dim / 2;
            const cgZ = p.z + h / 2;

            totalWeight += w;
            momentX += w * cgX;
            momentY += w * cgY;
            momentZ += w * cgZ;
        });

        // Avoid division by zero
        if (totalWeight < 0.01) {
            return this.getEmptyShipStability(ship);
        }

        // Calculate center of gravity position
        const cgX = momentX / totalWeight; // Longitudinal position
        const cgY = momentY / totalWeight; // Transverse position
        const cgZ = momentZ / totalWeight; // KG - height above keel

        // Calculate ship centerline positions
        const shipCenterX = ship.length / 2;
        const shipCenterY = ship.width / 2;

        // Calculate displacement and draft
        const displacement = totalWeight / 1000; // tonnes
        const blockCoefficient = 0.75; // Typical cargo ship
        const waterplaneCoefficient = 0.85;

        // Draft calculation: Volume = Displacement / Density = L × B × T × Cb
        const displacedVolume = displacement / this.SEAWATER_DENSITY;
        const draft = displacedVolume / (ship.length * ship.width * blockCoefficient);

        // KB: Vertical center of buoyancy (typically 0.53 × draft for cargo ships)
        const kb = 0.53 * draft;

        // BM: Metacentric radius (transverse)
        // IT = second moment of waterplane area = L × B³ / 12 × Cw
        const inertiaT = (ship.length * Math.pow(ship.width, 3) / 12) * waterplaneCoefficient;
        const bm = inertiaT / displacedVolume;

        // GM: Metacentric height (THE KEY STABILITY METRIC)
        const gm = kb + bm - cgZ;

        // Calculate list angle (transverse tilt) - this is CRITICAL
        // When CG is off-center transversely, ship will list (roll)
        const cgOffsetY = cgY - shipCenterY; // Positive = starboard, negative = port

        // List angle calculation using moment equilibrium
        // For small angles: tan(θ) ≈ θ (radians) = offset / GM
        // But we need to be accurate for large angles too
        let listAngleRad = 0;
        if (gm > 0.01) {
            // Simple approximation for initial list
            listAngleRad = Math.atan(cgOffsetY / gm);
        } else if (gm < 0) {
            // Negative GM = unstable equilibrium = immediate capsize
            listAngleRad = Math.PI / 2; // 90 degrees = capsized
        }

        const listAngleDeg = listAngleRad * (180 / Math.PI);

        // Calculate trim angle (longitudinal tilt)
        const cgOffsetX = cgX - shipCenterX;

        // BML: Longitudinal metacentric radius (typically 10-20x larger than BM)
        const inertiaL = (Math.pow(ship.length, 3) * ship.width / 12) * waterplaneCoefficient;
        const bml = inertiaL / displacedVolume;
        const gml = kb + bml - cgZ; // Longitudinal GM

        let trimAngleRad = 0;
        if (gml > 0.01) {
            trimAngleRad = Math.atan(cgOffsetX / gml);
        }

        const trimAngleDeg = trimAngleRad * (180 / Math.PI);

        // Determine stability status
        let stabilityStatus = 'optimal';
        if (gm < this.GM_CRITICAL || Math.abs(listAngleDeg) > this.LIST_CRITICAL) {
            stabilityStatus = 'critical';
        } else if (gm < this.GM_OPTIMAL_MIN || gm > this.GM_STIFF ||
                   Math.abs(listAngleDeg) > this.LIST_WARNING ||
                   Math.abs(trimAngleDeg) > this.TRIM_WARNING) {
            stabilityStatus = 'warning';
        }

        // Check for capsizing
        const isCapsized = Math.abs(listAngleDeg) > this.LIST_CAPSIZE || gm < 0;

        return {
            totalWeight: totalWeight,
            displacement: displacement,
            draft: draft,

            // Center of gravity
            cg: {
                x: cgX,
                y: cgY,
                z: cgZ,
                longitudinal_percent: (cgX / ship.length) * 100,
                transverse_percent: (cgY / ship.width) * 100
            },

            // Stability metrics
            kb: kb,
            bm: bm,
            gm: gm,
            gml: gml,

            // Tilt angles (THE VISUAL FEEDBACK)
            listAngle: listAngleDeg, // Roll (port/starboard)
            trimAngle: trimAngleDeg,  // Pitch (bow/stern)

            // Status assessment
            stabilityStatus: stabilityStatus,
            isCapsized: isCapsized,

            // Warnings
            warnings: this.generateWarnings(gm, listAngleDeg, trimAngleDeg, totalWeight, ship.max_weight)
        };
    }

    getEmptyShipStability(ship) {
        return {
            totalWeight: ship.lightship_weight || 0,
            displacement: 0,
            draft: 0,
            cg: { x: ship.length/2, y: ship.width/2, z: 5,
                  longitudinal_percent: 50, transverse_percent: 50 },
            kb: 0, bm: 0, gm: 1.5, gml: 15,
            listAngle: 0,
            trimAngle: 0,
            stabilityStatus: 'optimal',
            isCapsized: false,
            warnings: []
        };
    }

    generateWarnings(gm, listAngle, trimAngle, totalWeight, maxWeight) {
        const warnings = [];

        if (gm < 0) {
            warnings.push('⚠️ CRITICAL: Negative GM - Ship will capsize!');
        } else if (gm < this.GM_CRITICAL) {
            warnings.push('⚠️ CRITICAL: GM below safe minimum - Unstable!');
        } else if (gm < this.GM_OPTIMAL_MIN) {
            warnings.push('⚠️ WARNING: GM too low - Tender ship, slow rolling');
        } else if (gm > this.GM_STIFF) {
            warnings.push('⚠️ WARNING: GM too high - Stiff ship, excessive rolling acceleration');
        }

        if (Math.abs(listAngle) > this.LIST_CRITICAL) {
            warnings.push(`⚠️ CRITICAL: Excessive list (${listAngle.toFixed(1)}°) - Near capsizing!`);
        } else if (Math.abs(listAngle) > this.LIST_DANGEROUS) {
            warnings.push(`⚠️ DANGER: Severe list (${listAngle.toFixed(1)}°) - Redistribute cargo!`);
        } else if (Math.abs(listAngle) > this.LIST_WARNING) {
            warnings.push(`⚠️ WARNING: List detected (${listAngle.toFixed(1)}°) - Balance cargo`);
        }

        if (Math.abs(trimAngle) > this.TRIM_CRITICAL) {
            warnings.push(`⚠️ CRITICAL: Excessive trim (${trimAngle.toFixed(1)}°)`);
        } else if (Math.abs(trimAngle) > this.TRIM_DANGEROUS) {
            warnings.push(`⚠️ WARNING: High trim (${trimAngle.toFixed(1)}°)`);
        }

        if (totalWeight > maxWeight) {
            warnings.push('⚠️ CRITICAL: Ship overloaded - Exceeds maximum weight!');
        } else if (totalWeight > maxWeight * 0.95) {
            warnings.push('⚠️ WARNING: Near maximum capacity');
        }

        return warnings;
    }

    /**
     * Calculate rolling and pitching motion based on sea state
     * Simulates ship motion in waves
     */
    calculateWaveMotion(stability, deltaTime) {
        this.waveTime += deltaTime;

        // Wave parameters based on sea state (0-9 scale)
        const waveAmplitude = this.seaState * 0.5; // degrees per sea state level
        const waveFrequency = 0.3 + (this.seaState * 0.05); // Hz

        // Natural rolling period depends on GM (smaller GM = slower rolling)
        // T = 2π × √(k²/g×GM) where k is radius of gyration
        const k = stability.gm > 0 ? Math.sqrt(Math.abs(stability.gm)) : 1;
        const naturalPeriod = 2 * Math.PI * k / Math.sqrt(this.GRAVITY * Math.max(stability.gm, 0.1));
        const naturalFreq = 1 / naturalPeriod;

        // Calculate wave-induced rolling (sinusoidal motion)
        const rollAmplitude = waveAmplitude / Math.max(stability.gm, 0.3); // Lower GM = more rolling
        const rollAngle = rollAmplitude * Math.sin(2 * Math.PI * naturalFreq * this.waveTime);

        // Calculate pitching (typically 1/3 of rolling)
        const pitchAngle = (waveAmplitude / 3) * Math.sin(2 * Math.PI * waveFrequency * this.waveTime + Math.PI/4);

        return {
            roll: rollAngle,  // Additional roll from waves
            pitch: pitchAngle, // Additional pitch from waves
            heave: waveAmplitude * 0.5 * Math.sin(2 * Math.PI * waveFrequency * this.waveTime) // Vertical motion
        };
    }

    /**
     * Set sea state (0 = calm, 9 = phenomenal)
     */
    setSeaState(state) {
        this.seaState = Math.max(0, Math.min(9, state));
    }

    /**
     * Calculate score based on stability, efficiency, and time
     */
    calculateScore(stability, placedCount, totalCount, timeSeconds) {
        if (stability.isCapsized) return 0;

        // Placement efficiency (0-100 points)
        const placementScore = (placedCount / totalCount) * 100;

        // Stability score (0-100 points)
        let stabilityScore = 0;
        if (stability.gm >= this.GM_OPTIMAL_MIN && stability.gm <= this.GM_OPTIMAL_MAX) {
            stabilityScore = 100; // Perfect GM
        } else if (stability.gm >= this.GM_CRITICAL) {
            stabilityScore = 50; // Acceptable but not optimal
        }

        // Reduce score based on list/trim
        stabilityScore -= Math.abs(stability.listAngle) * 3; // -3 points per degree of list
        stabilityScore -= Math.abs(stability.trimAngle) * 2; // -2 points per degree of trim
        stabilityScore = Math.max(0, stabilityScore);

        // Time bonus (faster = better, but capped)
        const timeBonus = Math.max(0, 100 - timeSeconds);

        // Total score
        const totalScore = Math.round(placementScore * 0.5 + stabilityScore * 0.4 + timeBonus * 0.1);

        return {
            total: totalScore,
            placement: Math.round(placementScore),
            stability: Math.round(stabilityScore),
            timeBonus: Math.round(timeBonus)
        };
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = MaritimePhysics;
}
