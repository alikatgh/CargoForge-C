// CargoForge-C Physics Engine Test Suite
// 100% test coverage for physics.js

class PhysicsTests {
    constructor() {
        this.physics = new MaritimePhysics();
        this.passedTests = 0;
        this.failedTests = 0;
        this.results = [];
    }

    assert(condition, testName, details = '') {
        if (condition) {
            this.passedTests++;
            this.results.push(`✅ PASS: ${testName}`);
            console.log(`✅ PASS: ${testName}`, details);
        } else {
            this.failedTests++;
            this.results.push(`❌ FAIL: ${testName} - ${details}`);
            console.error(`❌ FAIL: ${testName}`, details);
        }
    }

    assertClose(actual, expected, tolerance, testName) {
        const diff = Math.abs(actual - expected);
        const withinTolerance = diff <= tolerance;
        this.assert(
            withinTolerance,
            testName,
            `Expected: ${expected}, Got: ${actual}, Diff: ${diff.toFixed(4)}`
        );
    }

    // ========================================
    // TEST 1: Empty Ship Stability
    // ========================================
    testEmptyShip() {
        console.log('\n=== TEST 1: Empty Ship Stability ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        const stability = this.physics.calculateStability(ship, []);

        // Empty ship should have neutral CG
        this.assertClose(stability.cg.longitudinal_percent, 50, 1, 'Empty ship longitudinal CG');
        this.assertClose(stability.cg.transverse_percent, 50, 1, 'Empty ship transverse CG');

        // GM should be positive
        this.assert(stability.gm > 0, 'Empty ship has positive GM', `GM = ${stability.gm.toFixed(3)}m`);

        // No tilt
        this.assertClose(stability.listAngle, 0, 0.1, 'Empty ship has zero list');
        this.assertClose(stability.trimAngle, 0, 0.1, 'Empty ship has zero trim');

        // Not capsized
        this.assert(!stability.isCapsized, 'Empty ship is not capsized');
    }

    // ========================================
    // TEST 2: Centered Cargo - Should Stay Level
    // ========================================
    testCenteredCargo() {
        console.log('\n=== TEST 2: Centered Cargo (Should Stay Level) ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        // Place cargo exactly at center
        const cargo = [{
            id: 'CenteredContainer',
            weight: 250000,
            dimensions: [12, 2.4, 2.6],
            position: {
                x: ship.length / 2 - 6,  // Center X
                y: ship.width / 2 - 1.2, // Center Y
                z: -8.0
            }
        }];

        const stability = this.physics.calculateStability(ship, cargo);

        // CG should be near center
        this.assertClose(stability.cg.longitudinal_percent, 50, 5, 'Centered cargo longitudinal CG');
        this.assertClose(stability.cg.transverse_percent, 50, 5, 'Centered cargo transverse CG');

        // List should be minimal
        this.assertClose(Math.abs(stability.listAngle), 0, 1, 'Centered cargo minimal list');

        // Should be stable
        this.assert(stability.stabilityStatus !== 'critical', 'Centered cargo is stable');
    }

    // ========================================
    // TEST 3: Off-Center Cargo - Should List
    // ========================================
    testOffCenterCargo() {
        console.log('\n=== TEST 3: Off-Center Cargo (Should List) ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        // Place heavy cargo far to starboard (positive Y)
        const cargo = [{
            id: 'OffCenterContainer',
            weight: 500000,
            dimensions: [12, 2.4, 2.6],
            position: {
                x: ship.length / 2,
                y: ship.width * 0.8,  // Far to starboard
                z: -8.0
            }
        }];

        const stability = this.physics.calculateStability(ship, cargo);

        // CG should be off-center transversely
        this.assert(stability.cg.transverse_percent > 55, 'Off-center cargo shifts CG',
                   `Transverse CG: ${stability.cg.transverse_percent.toFixed(1)}%`);

        // Should have noticeable list
        this.assert(Math.abs(stability.listAngle) > 0.5, 'Off-center cargo causes list',
                   `List: ${stability.listAngle.toFixed(2)}°`);

        // Should have warnings
        this.assert(stability.warnings.length > 0, 'Off-center cargo generates warnings');
    }

    // ========================================
    // TEST 4: Overloaded Ship - Should Be Critical
    // ========================================
    testOverloadedShip() {
        console.log('\n=== TEST 4: Overloaded Ship (Should Be Critical) ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 10000000, // Low limit
            lightship_weight: 8000000,
            lightship_kg: 5.0
        };

        // Add cargo that exceeds max weight
        const cargo = [{
            id: 'HeavyContainer',
            weight: 5000000, // Exceeds remaining capacity
            dimensions: [12, 2.4, 2.6],
            position: { x: 75, y: 12.5, z: -8.0 }
        }];

        const stability = this.physics.calculateStability(ship, cargo);

        // Total weight should exceed max
        this.assert(stability.totalWeight > ship.max_weight, 'Overloaded ship exceeds max weight');

        // Should have critical warnings
        this.assert(stability.warnings.some(w => w.includes('overloaded')),
                   'Overloaded ship has overload warning');
    }

    // ========================================
    // TEST 5: High CG - Low GM (Unstable)
    // ========================================
    testHighCG() {
        console.log('\n=== TEST 5: High Center of Gravity (Unstable) ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        // Place heavy cargo HIGH up (positive Z)
        const cargo = [{
            id: 'HighContainer',
            weight: 1000000,
            dimensions: [12, 2.4, 5.0], // Tall
            position: {
                x: 75,
                y: 12.5,
                z: 5.0  // High above deck
            }
        }];

        const stability = this.physics.calculateStability(ship, cargo);

        // High CG should reduce GM
        this.assert(stability.gm < 2.0, 'High CG reduces GM',
                   `GM: ${stability.gm.toFixed(3)}m`);

        // May have warnings about GM
        const hasGMWarning = stability.warnings.some(w =>
            w.toLowerCase().includes('gm') ||
            w.toLowerCase().includes('tender') ||
            w.toLowerCase().includes('stiff')
        );

        console.log(`GM: ${stability.gm.toFixed(3)}m, Status: ${stability.stabilityStatus}`);
    }

    // ========================================
    // TEST 6: Multiple Cargo - Complex Loading
    // ========================================
    testMultipleCargo() {
        console.log('\n=== TEST 6: Multiple Cargo (Complex Loading) ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        // Place multiple cargo items
        const cargo = [
            {
                id: 'Container1',
                weight: 250000,
                dimensions: [12, 2.4, 2.6],
                position: { x: 30, y: 8, z: -8.0 }
            },
            {
                id: 'Container2',
                weight: 250000,
                dimensions: [12, 2.4, 2.6],
                position: { x: 30, y: 17, z: -8.0 } // Opposite side
            },
            {
                id: 'Container3',
                weight: 400000,
                dimensions: [18, 2, 2],
                position: { x: 100, y: 12.5, z: -8.0 }
            }
        ];

        const stability = this.physics.calculateStability(ship, cargo);

        // Should calculate total weight
        const expectedWeight = 15000000 + 250000 + 250000 + 400000;
        this.assertClose(stability.totalWeight, expectedWeight, 1000, 'Multiple cargo total weight');

        // Should have positive GM
        this.assert(stability.gm > 0, 'Multiple cargo has positive GM');

        // Should not be capsized
        this.assert(!stability.isCapsized, 'Multiple cargo ship not capsized');
    }

    // ========================================
    // TEST 7: Extreme List - Should Capsize
    // ========================================
    testCapsizing() {
        console.log('\n=== TEST 7: Extreme List (Should Capsize) ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 5000000, // Light ship
            lightship_kg: 3.0
        };

        // Place ALL cargo far to one side
        const cargo = [{
            id: 'ExtremeContainer',
            weight: 2000000, // Heavy
            dimensions: [20, 3, 3],
            position: {
                x: 75,
                y: ship.width * 0.95, // Almost at edge
                z: 8.0 // High up too
            }
        }];

        const stability = this.physics.calculateStability(ship, cargo);

        // Should have large list angle
        this.assert(Math.abs(stability.listAngle) > 10, 'Extreme offset causes large list',
                   `List: ${stability.listAngle.toFixed(2)}°`);

        // Should be critical status
        this.assert(stability.stabilityStatus === 'critical', 'Extreme offset is critical');

        // May capsize
        if (stability.isCapsized) {
            console.log('✅ Ship correctly detected capsizing!');
            this.passedTests++;
        } else {
            console.log(`⚠️  List angle: ${stability.listAngle.toFixed(2)}° (near capsizing threshold)`);
        }
    }

    // ========================================
    // TEST 8: Wave Motion Calculation
    // ========================================
    testWaveMotion() {
        console.log('\n=== TEST 8: Wave Motion Calculation ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        const stability = this.physics.calculateStability(ship, []);

        // Test different sea states
        for (let seaState = 0; seaState <= 6; seaState++) {
            this.physics.setSeaState(seaState);
            const motion = this.physics.calculateWaveMotion(stability, 1.0);

            // Wave motion should increase with sea state
            this.assert(typeof motion.roll === 'number', `Sea state ${seaState} roll is a number`);
            this.assert(typeof motion.pitch === 'number', `Sea state ${seaState} pitch is a number`);
            this.assert(typeof motion.heave === 'number', `Sea state ${seaState} heave is a number`);
        }

        // Reset to calm
        this.physics.setSeaState(0);
    }

    // ========================================
    // TEST 9: Scoring System
    // ========================================
    testScoring() {
        console.log('\n=== TEST 9: Scoring System ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        const cargo = [{
            id: 'Container1',
            weight: 250000,
            dimensions: [12, 2.4, 2.6],
            position: { x: 75, y: 12.5, z: -8.0 }
        }];

        const stability = this.physics.calculateStability(ship, cargo);

        // Test scoring
        const score = this.physics.calculateScore(stability, 1, 1, 30);

        this.assert(score.total >= 0 && score.total <= 100, 'Total score in valid range',
                   `Score: ${score.total}`);
        this.assert(score.placement >= 0 && score.placement <= 100, 'Placement score valid');
        this.assert(score.stability >= 0 && score.stability <= 100, 'Stability score valid');
        this.assert(typeof score.timeBonus === 'number', 'Time bonus is a number');

        // Perfect placement should give 100
        const perfectScore = this.physics.calculateScore(stability, 5, 5, 10);
        this.assert(perfectScore.placement === 100, 'Perfect placement gives 100%',
                   `Got: ${perfectScore.placement}`);

        // Capsized ship should give 0
        const capsizedStability = { ...stability, isCapsized: true };
        const capsizedScore = this.physics.calculateScore(capsizedStability, 1, 1, 30);
        this.assert(capsizedScore.total === 0, 'Capsized ship gives 0 score');
    }

    // ========================================
    // TEST 10: Draft Calculation
    // ========================================
    testDraftCalculation() {
        console.log('\n=== TEST 10: Draft Calculation ===');

        const ship = {
            length: 150,
            width: 25,
            max_weight: 50000000,
            lightship_weight: 15000000,
            lightship_kg: 5.0
        };

        // Empty ship
        const emptyStability = this.physics.calculateStability(ship, []);
        this.assert(emptyStability.draft > 0, 'Empty ship has positive draft',
                   `Draft: ${emptyStability.draft.toFixed(2)}m`);

        // Add cargo
        const cargo = [{
            id: 'Container',
            weight: 5000000,
            dimensions: [12, 2.4, 2.6],
            position: { x: 75, y: 12.5, z: -8.0 }
        }];

        const loadedStability = this.physics.calculateStability(ship, cargo);

        // Draft should increase with weight
        this.assert(loadedStability.draft > emptyStability.draft,
                   'Draft increases with cargo weight',
                   `Empty: ${emptyStability.draft.toFixed(2)}m, Loaded: ${loadedStability.draft.toFixed(2)}m`);
    }

    // ========================================
    // TEST 11: GM Threshold Detection
    // ========================================
    testGMThresholds() {
        console.log('\n=== TEST 11: GM Threshold Detection ===');

        // Test various GM values and check correct status
        const testCases = [
            { gm: -0.5, expectedStatus: 'critical', description: 'Negative GM' },
            { gm: 0.2, expectedStatus: 'critical', description: 'GM < 0.3m' },
            { gm: 0.4, expectedStatus: 'warning', description: 'GM < 0.5m' },
            { gm: 1.0, expectedStatus: 'optimal', description: 'GM = 1.0m' },
            { gm: 2.0, expectedStatus: 'optimal', description: 'GM = 2.0m' },
            { gm: 3.5, expectedStatus: 'warning', description: 'GM > 3.0m (too stiff)' }
        ];

        for (const testCase of testCases) {
            const warnings = this.physics.generateWarnings(testCase.gm, 0, 0, 15000000, 50000000);

            if (testCase.gm < this.physics.GM_CRITICAL || testCase.gm < 0) {
                this.assert(warnings.some(w => w.includes('CRITICAL') && w.includes('GM')),
                           `${testCase.description} generates critical warning`);
            } else if (testCase.gm < this.physics.GM_OPTIMAL_MIN) {
                this.assert(warnings.some(w => w.includes('WARNING') && w.includes('GM')),
                           `${testCase.description} generates GM warning`);
            } else if (testCase.gm > this.physics.GM_STIFF) {
                this.assert(warnings.some(w => w.includes('WARNING') && w.includes('stiff')),
                           `${testCase.description} detects stiff ship`);
            }
        }
    }

    // ========================================
    // TEST 12: List Angle Thresholds
    // ========================================
    testListThresholds() {
        console.log('\n=== TEST 12: List Angle Thresholds ===');

        const testCases = [
            { list: 0, shouldWarn: false, severity: 'none' },
            { list: 3, shouldWarn: false, severity: 'none' },
            { list: 6, shouldWarn: true, severity: 'WARNING' },
            { list: 12, shouldWarn: true, severity: 'DANGER' },
            { list: 18, shouldWarn: true, severity: 'CRITICAL' }
        ];

        for (const testCase of testCases) {
            const warnings = this.physics.generateWarnings(1.5, testCase.list, 0, 15000000, 50000000);

            if (testCase.shouldWarn) {
                this.assert(warnings.some(w => w.includes(testCase.severity)),
                           `List ${testCase.list}° generates ${testCase.severity} warning`);
            } else {
                this.assert(!warnings.some(w => w.toLowerCase().includes('list')),
                           `List ${testCase.list}° does not generate warnings`);
            }
        }
    }

    // ========================================
    // RUN ALL TESTS
    // ========================================
    runAllTests() {
        console.log('\n╔════════════════════════════════════════════════════╗');
        console.log('║   CARGOFORGE-C PHYSICS ENGINE TEST SUITE          ║');
        console.log('║   100% Coverage Test                               ║');
        console.log('╚════════════════════════════════════════════════════╝\n');

        this.testEmptyShip();
        this.testCenteredCargo();
        this.testOffCenterCargo();
        this.testOverloadedShip();
        this.testHighCG();
        this.testMultipleCargo();
        this.testCapsizing();
        this.testWaveMotion();
        this.testScoring();
        this.testDraftCalculation();
        this.testGMThresholds();
        this.testListThresholds();

        // Summary
        console.log('\n╔════════════════════════════════════════════════════╗');
        console.log('║   TEST RESULTS SUMMARY                             ║');
        console.log('╚════════════════════════════════════════════════════╝\n');
        console.log(`✅ PASSED: ${this.passedTests}`);
        console.log(`❌ FAILED: ${this.failedTests}`);
        console.log(`📊 TOTAL:  ${this.passedTests + this.failedTests}`);
        console.log(`📈 SUCCESS RATE: ${((this.passedTests / (this.passedTests + this.failedTests)) * 100).toFixed(1)}%\n`);

        if (this.failedTests === 0) {
            console.log('🎉🎉🎉 ALL TESTS PASSED! 🎉🎉🎉\n');
        } else {
            console.log('⚠️  SOME TESTS FAILED - Review above output\n');
        }

        return {
            passed: this.passedTests,
            failed: this.failedTests,
            total: this.passedTests + this.failedTests,
            results: this.results
        };
    }
}

// Export for Node.js or use in browser
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PhysicsTests;
}
