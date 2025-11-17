// PROFESSIONAL MARITIME CARGO TRAINING SIMULATOR
// Based on real industry standards: IMDG Code, SOLAS, IMO guidelines

class MaritimeProfessional {
    constructor() {
        // Real ship types used in industry
        this.shipTypes = {
            CONTAINER: 'container',
            BULK_CARRIER: 'bulk_carrier',
            RORO: 'roro',
            GENERAL_CARGO: 'general_cargo'
        };

        // ISO 668 Container Types (REAL industry standard)
        this.containerTypes = {
            '20DC': { name: '20ft Dry Cargo', length: 20, width: 8, height: 8.5, teu: 1 },
            '40DC': { name: '40ft Dry Cargo', length: 40, width: 8, height: 8.5, teu: 2 },
            '40HC': { name: '40ft High Cube', length: 40, width: 8, height: 9.5, teu: 2 },
            '20RF': { name: '20ft Reefer', length: 20, width: 8, height: 8.5, teu: 1, needsPower: true },
            '40RF': { name: '40ft Reefer', length: 40, width: 8, height: 8.5, teu: 2, needsPower: true },
            '20OT': { name: '20ft Open Top', length: 20, width: 8, height: 8.5, teu: 1 },
            '40OT': { name: '40ft Open Top', length: 40, width: 8, height: 8.5, teu: 2 },
            '20FR': { name: '20ft Flat Rack', length: 20, width: 8, height: 8.5, teu: 1 },
            '40FR': { name: '40ft Flat Rack', length: 40, width: 8, height: 8.5, teu: 2 },
            '20TK': { name: '20ft Tank', length: 20, width: 8, height: 8.5, teu: 1 }
        };

        // IMDG Code - International Maritime Dangerous Goods Code
        // REAL hazmat classes used worldwide
        this.imdgClasses = {
            1: { name: 'Explosives', color: '#FF6B6B', segregation: 'category_A' },
            2: { name: 'Gases', color: '#FFA500', segregation: 'category_B' },
            3: { name: 'Flammable Liquids', color: '#FF0000', segregation: 'category_B' },
            4: { name: 'Flammable Solids', color: '#FF4444', segregation: 'category_C' },
            5: { name: 'Oxidizing Substances', color: '#FFFF00', segregation: 'category_B' },
            6: { name: 'Toxic Substances', color: '#FFFFFF', segregation: 'category_A' },
            7: { name: 'Radioactive Material', color: '#FFFF00', segregation: 'category_A' },
            8: { name: 'Corrosive Substances', color: '#000000', segregation: 'category_C' },
            9: { name: 'Miscellaneous', color: '#808080', segregation: 'category_D' }
        };

        // IMDG Segregation Requirements (meters)
        this.segregationDistance = {
            'away_from': 3.0,        // Minimum 3m separation
            'separated_from': 6.0,   // Minimum 6m separation
            'separated_by': 12.0,    // Minimum 12m or complete compartment
            'separated_longitudinally': 24.0  // Minimum 24m fore/aft
        };

        // Container weight groups (SOLAS VGM requirements)
        this.weightGroups = {
            'LIGHT': { max: 10000, category: 'Light cargo' },
            'MEDIUM': { max: 20000, category: 'Medium cargo' },
            'HEAVY': { max: 30000, category: 'Heavy cargo' }
        };
    }

    /**
     * Create a professional container ship with realistic bay planning
     * Based on real Panamax/Post-Panamax ship designs
     */
    createContainerShip(config = {}) {
        const shipClass = config.class || 'PANAMAX';

        const shipDesigns = {
            'FEEDER': {
                name: 'Feeder Container Ship',
                capacity_teu: 500,
                length: 120,
                beam: 20,
                depth: 10,
                max_draft: 8.0,
                bays: 8,
                rows: 6,
                tiers_below: 4,
                tiers_above: 2,
                reefer_plugs: 50
            },
            'PANAMAX': {
                name: 'Panamax Container Ship',
                capacity_teu: 5000,
                length: 294,
                beam: 32.2,
                depth: 20,
                max_draft: 12.0,
                bays: 20,
                rows: 13,
                tiers_below: 7,
                tiers_above: 6,
                reefer_plugs: 450
            },
            'POST_PANAMAX': {
                name: 'Post-Panamax Container Ship',
                capacity_teu: 8000,
                length: 340,
                beam: 42.8,
                depth: 24,
                max_draft: 14.5,
                bays: 22,
                rows: 17,
                tiers_below: 9,
                tiers_above: 7,
                reefer_plugs: 700
            },
            'ULCS': {
                name: 'Ultra Large Container Ship',
                capacity_teu: 18000,
                length: 400,
                beam: 59,
                depth: 30,
                max_draft: 16.0,
                bays: 24,
                rows: 22,
                tiers_below: 11,
                tiers_above: 9,
                reefer_plugs: 1800
            }
        };

        const design = shipDesigns[shipClass];

        return {
            type: this.shipTypes.CONTAINER,
            class: shipClass,
            ...design,
            bays: this.generateBayPlan(design),
            ballastTanks: this.generateBallastTanks(design),
            loadingRules: this.getContainerLoadingRules()
        };
    }

    /**
     * Generate realistic bay plan (container slot system)
     * Bay numbering: Odd numbers for 20ft bays, Even for 40ft positions
     */
    generateBayPlan(design) {
        const bays = [];

        for (let bayNum = 1; bayNum <= design.bays; bayNum++) {
            const bay = {
                number: String(bayNum).padStart(2, '0'),
                position: bayNum * (design.length / design.bays),
                rows: design.rows,
                tiers_below: design.tiers_below,
                tiers_above: design.tiers_above,
                slots: [],
                reefer_capable: bayNum % 2 === 0, // Even bays have reefer plugs
                hatch_cover: bayNum % 2 === 0
            };

            // Generate slots for this bay
            // Row numbering: 01 = port, 00 = centerline, 02 = starboard (industry standard)
            for (let row = 1; row <= design.rows; row++) {
                const rowNum = String(row).padStart(2, '0');

                // Below deck slots
                for (let tier = 2; tier <= design.tiers_below * 2; tier += 2) {
                    const tierNum = String(tier).padStart(2, '0');
                    bay.slots.push({
                        position: `${bay.number}${rowNum}${tierNum}`,
                        bay: bayNum,
                        row: row,
                        tier: tier,
                        location: 'below_deck',
                        occupied: false,
                        container: null,
                        weight_limit: 30000, // kg
                        has_lashing: false,
                        has_power: bay.reefer_capable && tier <= 4 // Lower tiers have reefer plugs
                    });
                }

                // Above deck slots
                for (let tier = 82; tier <= 80 + (design.tiers_above * 2); tier += 2) {
                    const tierNum = String(tier).padStart(2, '0');
                    bay.slots.push({
                        position: `${bay.number}${rowNum}${tierNum}`,
                        bay: bayNum,
                        row: row,
                        tier: tier,
                        location: 'above_deck',
                        occupied: false,
                        container: null,
                        weight_limit: 20000, // Lower weight limit on deck
                        has_lashing: true,
                        has_power: false
                    });
                }
            }

            bays.push(bay);
        }

        return bays;
    }

    /**
     * Generate ballast tank system (REAL ship stability management)
     */
    generateBallastTanks(design) {
        return {
            fore_peak: { capacity: design.capacity_teu * 5, current: 0, position: 'fore' },
            aft_peak: { capacity: design.capacity_teu * 5, current: 0, position: 'aft' },
            port_wing_tanks: [
                { capacity: design.capacity_teu * 3, current: 0, position: 'port_fore' },
                { capacity: design.capacity_teu * 3, current: 0, position: 'port_mid' },
                { capacity: design.capacity_teu * 3, current: 0, position: 'port_aft' }
            ],
            starboard_wing_tanks: [
                { capacity: design.capacity_teu * 3, current: 0, position: 'stbd_fore' },
                { capacity: design.capacity_teu * 3, current: 0, position: 'stbd_mid' },
                { capacity: design.capacity_teu * 3, current: 0, position: 'stbd_aft' }
            ],
            double_bottom: [
                { capacity: design.capacity_teu * 4, current: 0, position: 'db_fore' },
                { capacity: design.capacity_teu * 4, current: 0, position: 'db_mid' },
                { capacity: design.capacity_teu * 4, current: 0, position: 'db_aft' }
            ]
        };
    }

    /**
     * REAL container loading rules used by cargo planners
     */
    getContainerLoadingRules() {
        return {
            // Stacking rules
            stacking: {
                max_stack_weight: 192000, // kg (SOLAS requirement)
                heavy_on_light_allowed: false,
                empty_on_full_allowed: false,
                max_tier_weight_ratio: 1.2,
                bottom_tier_min_weight: 15000 // Heavier containers at bottom
            },

            // Loading sequence
            sequence: {
                method: 'bottom_up',
                direction: 'center_out',
                priority: ['heavy_first', 'hazmat_segregated', 'reefers_to_plugs']
            },

            // Weight distribution
            weight_distribution: {
                max_list: 0.5,  // degrees (VERY strict for professional)
                max_trim: 0.5,  // degrees
                max_stress: 150, // MPa on hull
                min_gm: 1.0,    // meters (stricter than general)
                max_deck_weight_percent: 35 // % of total
            },

            // Reefer requirements
            reefer: {
                needs_power_plug: true,
                max_distance_to_plug: 0, // Must be directly on plug
                pre_trip_inspection: true
            },

            // Hazmat (IMDG Code)
            hazmat: {
                segregation_required: true,
                away_from_distance: 3.0,
                separated_from_distance: 6.0,
                separated_by_distance: 12.0,
                no_living_quarters_nearby: true,
                ventilation_required: true
            },

            // Lashing
            lashing: {
                above_deck_required: true,
                corner_castings_must_align: true,
                twist_locks_required: true,
                max_height_without_additional: 4 // tiers
            },

            // Port operations
            port_operations: {
                discharge_ports: 'must_be_accessible',
                restow_minimization: true,
                crane_reach_limits: true
            }
        };
    }

    /**
     * Check if container placement is valid according to PROFESSIONAL rules
     */
    validateContainerPlacement(ship, container, slotPosition) {
        const errors = [];
        const warnings = [];
        const slot = this.findSlot(ship, slotPosition);

        if (!slot) {
            errors.push('Invalid slot position');
            return { valid: false, errors, warnings };
        }

        // 1. Weight limit check
        if (container.weight > slot.weight_limit) {
            errors.push(`Container weight ${container.weight}kg exceeds slot limit ${slot.weight_limit}kg`);
        }

        // 2. Reefer power check
        if (container.type.includes('RF') && !slot.has_power) {
            errors.push('Reefer container requires power plug');
        }

        // 3. Stacking sequence check
        const belowContainer = this.getContainerBelow(ship, slot);
        if (slot.tier > 2 && !belowContainer) {
            errors.push('Cannot place container in mid-air - must load bottom-up');
        }

        // 4. Weight stacking check
        if (belowContainer && container.weight > belowContainer.weight * 1.2) {
            errors.push('Heavy container on lighter container violates stacking rules');
        }

        // 5. IMDG segregation check
        if (container.imdg_class) {
            const segregationViolations = this.checkIMDGSegregation(ship, container, slot);
            errors.push(...segregationViolations);
        }

        // 6. Lashing requirement check
        if (slot.location === 'above_deck' && !slot.has_lashing) {
            errors.push('Above deck containers require lashing points');
        }

        // 7. Structural stress check
        const stress = this.calculateHullStress(ship, slot, container);
        if (stress > 150) { // MPa
            errors.push(`Hull stress ${stress.toFixed(1)}MPa exceeds limit`);
        }

        // 8. Port discharge accessibility
        if (container.discharge_port) {
            if (!this.isAccessibleForDischarge(ship, slot, container.discharge_port)) {
                warnings.push('Container may require restowing at intermediate port');
            }
        }

        return {
            valid: errors.length === 0,
            errors,
            warnings
        };
    }

    /**
     * Check IMDG Code segregation requirements
     */
    checkIMDGSegregation(ship, newContainer, newSlot) {
        const violations = [];
        const newClass = this.imdgClasses[newContainer.imdg_class];

        if (!newClass) return violations;

        // Check all existing containers
        ship.bays.forEach(bay => {
            bay.slots.forEach(slot => {
                if (!slot.occupied || !slot.container.imdg_class) return;

                const existingClass = this.imdgClasses[slot.container.imdg_class];
                const distance = this.calculateSlotDistance(newSlot, slot);

                // Get required segregation
                const segregation = this.getSegregationRequirement(
                    newContainer.imdg_class,
                    slot.container.imdg_class
                );

                if (segregation && distance < segregation.distance) {
                    violations.push(
                        `IMDG Class ${newContainer.imdg_class} must be ` +
                        `${segregation.type} from Class ${slot.container.imdg_class} ` +
                        `(min ${segregation.distance}m, current ${distance.toFixed(1)}m)`
                    );
                }
            });
        });

        return violations;
    }

    /**
     * Get segregation requirement between two IMDG classes
     * Based on IMDG Code segregation table
     */
    getSegregationRequirement(class1, class2) {
        // Simplified IMDG segregation table (real table is much larger)
        const segregationTable = {
            '1-3': { type: 'separated_by', distance: 12.0 },  // Explosives from flammables
            '1-5': { type: 'separated_by', distance: 12.0 },  // Explosives from oxidizers
            '1-6': { type: 'separated_from', distance: 6.0 }, // Explosives from toxics
            '3-5': { type: 'separated_from', distance: 6.0 }, // Flammables from oxidizers
            '6-7': { type: 'away_from', distance: 3.0 },      // Toxics from radioactive
            '7-8': { type: 'separated_from', distance: 6.0 }  // Radioactive from corrosives
        };

        const key1 = `${Math.min(class1, class2)}-${Math.max(class1, class2)}`;
        return segregationTable[key1] || null;
    }

    /**
     * Calculate 3D distance between two slots
     */
    calculateSlotDistance(slot1, slot2) {
        // Convert slot positions to 3D coordinates
        const pos1 = this.slotToCoordinates(slot1);
        const pos2 = this.slotToCoordinates(slot2);

        return Math.sqrt(
            Math.pow(pos1.x - pos2.x, 2) +
            Math.pow(pos1.y - pos2.y, 2) +
            Math.pow(pos1.z - pos2.z, 2)
        );
    }

    /**
     * Convert slot to 3D coordinates
     */
    slotToCoordinates(slot) {
        return {
            x: slot.bay * 6.1,  // Standard bay spacing ~6.1m
            y: (slot.row - 7) * 2.4,  // Row spacing ~2.4m (centerline = 0)
            z: slot.tier * 0.5  // Tier height ~2.6m per tier
        };
    }

    findSlot(ship, position) {
        for (const bay of ship.bays) {
            const slot = bay.slots.find(s => s.position === position);
            if (slot) return slot;
        }
        return null;
    }

    getContainerBelow(ship, slot) {
        const belowTier = slot.tier - 2;
        const belowPosition = `${String(slot.bay).padStart(2, '0')}${String(slot.row).padStart(2, '0')}${String(belowTier).padStart(2, '0')}`;
        const belowSlot = this.findSlot(ship, belowPosition);
        return belowSlot && belowSlot.occupied ? belowSlot.container : null;
    }

    calculateHullStress(ship, slot, container) {
        // Simplified hull stress calculation
        // Real calculation would use finite element analysis
        const baseStress = 50; // MPa
        const weightFactor = container.weight / 25000;
        const tierFactor = Math.max(1, slot.tier / 10);
        return baseStress * weightFactor * tierFactor;
    }

    isAccessibleForDischarge(ship, slot, dischargePort) {
        // Check if container can be reached without moving others
        // Simplified version - real version needs full bay analysis
        return true; // Placeholder
    }

    /**
     * Calculate ballast adjustment needed for stability
     * PROFESSIONAL ballast management
     */
    calculateBallastAdjustment(ship, currentStability) {
        const adjustments = [];

        // List correction (transverse)
        if (Math.abs(currentStability.listAngle) > 0.5) {
            const listDirection = currentStability.listAngle > 0 ? 'port' : 'starboard';
            const fillSide = listDirection === 'port' ? 'starboard' : 'port';
            const ballastNeeded = Math.abs(currentStability.listAngle) * 100; // tonnes

            adjustments.push({
                action: 'fill',
                tank: `${fillSide}_wing_tanks`,
                amount: ballastNeeded,
                reason: `Correct ${Math.abs(currentStability.listAngle).toFixed(2)}° list to ${listDirection}`
            });
        }

        // Trim correction (longitudinal)
        if (Math.abs(currentStability.trimAngle) > 0.5) {
            const trimDirection = currentStability.trimAngle > 0 ? 'bow down' : 'stern down';
            const fillEnd = currentStability.trimAngle > 0 ? 'aft_peak' : 'fore_peak';
            const ballastNeeded = Math.abs(currentStability.trimAngle) * 80; // tonnes

            adjustments.push({
                action: 'fill',
                tank: fillEnd,
                amount: ballastNeeded,
                reason: `Correct ${Math.abs(currentStability.trimAngle).toFixed(2)}° trim (${trimDirection})`
            });
        }

        // GM adjustment
        if (currentStability.gm < 1.0) {
            adjustments.push({
                action: 'fill',
                tank: 'double_bottom',
                amount: (1.0 - currentStability.gm) * 150, // tonnes
                reason: `Increase GM from ${currentStability.gm.toFixed(2)}m to minimum 1.0m`
            });
        }

        return adjustments;
    }
}

// Export for use in simulator
if (typeof module !== 'undefined' && module.exports) {
    module.exports = MaritimeProfessional;
}
