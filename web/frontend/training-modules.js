// PROFESSIONAL MARITIME CARGO PLANNER TRAINING MODULES
// Based on actual maritime training curricula

class TrainingModules {
    constructor() {
        this.modules = this.createTrainingCurriculum();
        this.currentModule = null;
        this.progress = {};
    }

    createTrainingCurriculum() {
        return [
            // MODULE 1: BASIC SHIP STABILITY
            {
                id: 'M01',
                title: 'Basic Ship Stability Principles',
                difficulty: 'beginner',
                duration: '30 minutes',
                certification: 'Ship Stability Fundamentals',
                lessons: [
                    {
                        id: 'M01L01',
                        title: 'Understanding GM (Metacentric Height)',
                        theory: `
**What is GM?**
GM (Metacentric Height) is THE most important stability measure.
It's the distance between the ship's center of gravity (G) and the metacenter (M).

**Why it matters:**
- Positive GM = Stable ship (returns to upright)
- Negative GM = Unstable ship (WILL CAPSIZE!)
- Too low GM = Slow, large rolling (tender ship)
- Too high GM = Fast, jerky rolling (stiff ship)

**Professional Standards:**
- Minimum GM: 0.15m (absolute minimum)
- Recommended GM: 0.5m - 2.5m for cargo ships
- Critical GM: < 0.3m (dangerous!)

**Formula:**
GM = KB + BM - KG

Where:
- KB = Vertical center of buoyancy (≈ 0.53 × draft)
- BM = Metacentric radius (depends on waterplane area)
- KG = Vertical center of gravity (from keel)
                        `,
                        practice: {
                            task: 'Place 3 containers at ship centerline. Maintain GM > 1.0m',
                            ship: 'FEEDER',
                            containers: [
                                { type: '20DC', weight: 15000, id: 'C001' },
                                { type: '20DC', weight: 15000, id: 'C002' },
                                { type: '20DC', weight: 15000, id: 'C003' }
                            ],
                            success_criteria: {
                                min_gm: 1.0,
                                max_list: 1.0,
                                all_placed: true
                            }
                        }
                    },
                    {
                        id: 'M01L02',
                        title: 'List and Trim - Controlling Ship Angle',
                        theory: `
**List (Transverse Tilt):**
When cargo is off-center port/starboard, ship lists (rolls to one side).

**Professional Limits:**
- Normal operations: < 0.5° list
- Warning: 2° - 5° list
- Dangerous: > 5° list
- Emergency: > 10° list

**Trim (Longitudinal Tilt):**
When cargo is unbalanced fore/aft, ship trims (bow or stern down).

**Professional Limits:**
- Normal: < 0.5° trim
- Acceptable: 0.5° - 2° trim
- Excessive: > 2° trim

**How to Control:**
1. Balance cargo port/starboard (prevents list)
2. Balance cargo fore/aft (prevents trim)
3. Use ballast tanks to correct imbalance
                        `,
                        practice: {
                            task: 'Load 4 containers. Keep list < 0.5° and trim < 0.5°',
                            ship: 'FEEDER',
                            containers: [
                                { type: '40DC', weight: 25000, id: 'C001' },
                                { type: '40DC', weight: 25000, id: 'C002' },
                                { type: '20DC', weight: 12000, id: 'C003' },
                                { type: '20DC', weight: 12000, id: 'C004' }
                            ],
                            success_criteria: {
                                max_list: 0.5,
                                max_trim: 0.5,
                                all_placed: true
                            }
                        }
                    }
                ]
            },

            // MODULE 2: CONTAINER TYPES AND HANDLING
            {
                id: 'M02',
                title: 'Container Types and ISO Standards',
                difficulty: 'beginner',
                duration: '45 minutes',
                certification: 'Container Identification',
                lessons: [
                    {
                        id: 'M02L01',
                        title: 'ISO 668 Container Types',
                        theory: `
**Standard Container Sizes (ISO 668):**

**20ft Containers (1 TEU):**
- 20DC: Dry Cargo (most common)
- 20RF: Reefer (refrigerated, needs power!)
- 20OT: Open Top (for tall cargo)
- 20FR: Flat Rack (for wide/oversized cargo)
- 20TK: Tank (liquids)

**40ft Containers (2 TEU):**
- 40DC: Dry Cargo
- 40HC: High Cube (9.5ft tall vs 8.5ft)
- 40RF: Reefer
- 40OT: Open Top
- 40FR: Flat Rack

**TEU (Twenty-foot Equivalent Unit):**
- 20ft container = 1 TEU
- 40ft container = 2 TEU
- Ship capacity measured in TEU

**Critical Rules:**
1. Reefers MUST be on power plugs
2. Open tops cannot be stacked under
3. Flat racks need special lashing
4. Tanks have weight limits
                        `,
                        practice: {
                            task: 'Identify and place containers: 2× 20DC, 1× 40HC, 1× 20RF (on reefer plug!)',
                            ship: 'FEEDER',
                            containers: [
                                { type: '20DC', weight: 18000, id: 'C001' },
                                { type: '20DC', weight: 18000, id: 'C002' },
                                { type: '40HC', weight: 28000, id: 'C003' },
                                { type: '20RF', weight: 22000, id: 'C004', needs_power: true }
                            ],
                            success_criteria: {
                                reefer_on_plug: true,
                                all_placed: true
                            }
                        }
                    },
                    {
                        id: 'M02L02',
                        title: 'SOLAS VGM (Verified Gross Mass)',
                        theory: `
**SOLAS VGM Requirement:**
Since July 2016, ALL containers must have verified weight before loading.

**Why it matters:**
- Prevents overloading (ship capsizing risk)
- Prevents deck collapse
- Prevents stack crushing
- Legal requirement worldwide

**Weight Categories:**
- Light: < 10 tonnes
- Medium: 10-20 tonnes
- Heavy: 20-30 tonnes
- Super Heavy: > 30 tonnes (special handling)

**Professional Rules:**
1. NEVER exceed slot weight limit
2. Heavy containers at BOTTOM of stack
3. Maximum stack weight: 192 tonnes (SOLAS)
4. Deck containers: lower weight limit

**Stack Weight Calculation:**
Bottom container bears weight of all containers above!
                        `,
                        practice: {
                            task: 'Stack containers correctly: Heavy at bottom, light on top',
                            ship: 'FEEDER',
                            containers: [
                                { type: '40DC', weight: 28000, id: 'HEAVY1' },
                                { type: '40DC', weight: 18000, id: 'MED1' },
                                { type: '40DC', weight: 8000, id: 'LIGHT1' }
                            ],
                            success_criteria: {
                                correct_stacking_order: true,
                                all_placed: true
                            }
                        }
                    }
                ]
            },

            // MODULE 3: IMDG CODE HAZMAT
            {
                id: 'M03',
                title: 'IMDG Code - Dangerous Goods Handling',
                difficulty: 'intermediate',
                duration: '60 minutes',
                certification: 'Dangerous Goods Handling',
                lessons: [
                    {
                        id: 'M03L01',
                        title: 'IMDG Classes and Segregation',
                        theory: `
**IMDG Code Classes (REAL industry standard):**

Class 1: EXPLOSIVES
- MOST dangerous
- Requires MAXIMUM segregation
- Never near accommodation
- Example: Fireworks, ammunition

Class 2: GASES
- Compressed, liquefied, or dissolved
- Flammable, non-flammable, toxic
- Example: LPG, oxygen, nitrogen

Class 3: FLAMMABLE LIQUIDS
- Flashpoint < 60°C
- Keep away from heat/oxidizers
- Example: Gasoline, paint, alcohols

Class 4: FLAMMABLE SOLIDS
- Can ignite from friction
- Example: Matches, metal powders

Class 5: OXIDIZING SUBSTANCES
- Can cause/intensify fire
- Keep away from flammables
- Example: Hydrogen peroxide, fertilizers

Class 6: TOXIC SUBSTANCES
- Poisonous if inhaled/ingested
- Example: Pesticides, medical waste

Class 7: RADIOACTIVE MATERIAL
- Special segregation from all
- Example: Medical isotopes, uranium

Class 8: CORROSIVE SUBSTANCES
- Can damage skin/metal
- Example: Acids, batteries

Class 9: MISCELLANEOUS
- Other dangers
- Example: Lithium batteries

**Segregation Terms:**
- "Away from" = 3m minimum
- "Separated from" = 6m minimum
- "Separated by" = 12m OR complete compartment

**CRITICAL:**
Explosives and flammables MUST be separated by 12m minimum!
                        `,
                        practice: {
                            task: 'Load containers with Classes 1 (explosive) and 3 (flammable). Keep 12m apart!',
                            ship: 'PANAMAX',
                            containers: [
                                { type: '20DC', weight: 15000, id: 'EXPL', imdg_class: 1 },
                                { type: '20DC', weight: 15000, id: 'FLAM', imdg_class: 3 },
                                { type: '20DC', weight: 15000, id: 'GEN1' },
                                { type: '20DC', weight: 15000, id: 'GEN2' }
                            ],
                            success_criteria: {
                                imdg_segregation_correct: true,
                                min_distance_class_1_3: 12.0,
                                all_placed: true
                            }
                        }
                    }
                ]
            },

            // MODULE 4: BAY PLANNING
            {
                id: 'M04',
                title: 'Professional Bay Planning',
                difficulty: 'intermediate',
                duration: '90 minutes',
                certification: 'Cargo Planner (Bay)',
                lessons: [
                    {
                        id: 'M04L01',
                        title: 'Bay/Row/Tier System',
                        theory: `
**Container Slot Identification:**
Format: BBRRTT (6 digits)
- BB = Bay (01, 02, 03...)
- RR = Row (01=Port, 00=Centerline, 02=Starboard, etc.)
- TT = Tier (02, 04, 06... below deck; 82, 84, 86... above deck)

**Example:**
- Slot "010204" = Bay 01, Row 02 (starboard), Tier 04 (2nd tier below deck)
- Slot "100082" = Bay 10, Row 00 (centerline), Tier 82 (1st tier above deck)

**Bay Numbering:**
- ODD numbers (01, 03, 05...) = 20ft bay positions
- EVEN numbers (02, 04, 06...) = 40ft bay positions
- This allows 20ft or 40ft containers

**Loading Sequence:**
1. Bottom-up (lower tiers first)
2. Center-out (balance as you go)
3. Heavy-first (stability)
4. Discharge port consideration
                        `,
                        practice: {
                            task: 'Load containers using proper bay/row/tier notation',
                            ship: 'PANAMAX',
                            containers: [
                                { type: '40DC', weight: 25000, id: 'C001', target_slot: '020082' },
                                { type: '20DC', weight: 15000, id: 'C002', target_slot: '010104' },
                                { type: '20DC', weight: 15000, id: 'C003', target_slot: '030104' }
                            ],
                            success_criteria: {
                                correct_slots: true,
                                all_placed: true
                            }
                        }
                    }
                ]
            },

            // MODULE 5: BALLAST MANAGEMENT
            {
                id: 'M05',
                title: 'Ballast Tank Operations',
                difficulty: 'advanced',
                duration: '60 minutes',
                certification: 'Ballast Operations',
                lessons: [
                    {
                        id: 'M05L01',
                        title: 'Using Ballast to Correct List/Trim',
                        theory: `
**Ballast Tanks:**
Ships have ballast tanks to correct stability issues.

**Tank Locations:**
- Fore Peak: Front of ship (controls trim)
- Aft Peak: Rear of ship (controls trim)
- Port Wing: Left side (controls list)
- Starboard Wing: Right side (controls list)
- Double Bottom: Under cargo holds (increases GM)

**How to Correct List:**
Ship listing to PORT → Fill STARBOARD tanks
Ship listing to STARBOARD → Fill PORT tanks

**How to Correct Trim:**
Bow down (trim by bow) → Fill AFT PEAK
Stern down (trim by stern) → Fill FORE PEAK

**Professional Practice:**
1. Never fill/empty too quickly (free surface effect)
2. Keep tanks pressed up (full) or empty (avoid slack tanks)
3. Monitor stability continuously
4. Calculate ballast needed BEFORE pumping
                        `,
                        practice: {
                            task: 'Load cargo that creates 3° port list, then correct with ballast',
                            ship: 'PANAMAX',
                            containers: [
                                { type: '40DC', weight: 28000, id: 'C001', row_offset: -5 }, // Port side
                                { type: '40DC', weight: 28000, id: 'C002', row_offset: -5 },
                                { type: '20DC', weight: 10000, id: 'C003', row_offset: 5 }  // Starboard (lighter)
                            ],
                            success_criteria: {
                                initial_list_detected: true,
                                ballast_corrected: true,
                                final_list: 0.5,
                                all_placed: true
                            }
                        }
                    }
                ]
            },

            // MODULE 6: ADVANCED SCENARIOS
            {
                id: 'M06',
                title: 'Real-World Cargo Planning Scenarios',
                difficulty: 'advanced',
                duration: '120 minutes',
                certification: 'Professional Cargo Planner',
                lessons: [
                    {
                        id: 'M06L01',
                        title: 'Multi-Port Loading',
                        theory: `
**Real-World Challenge:**
Ship visits 3 ports:
1. Port A (load 50 containers)
2. Port B (discharge 20, load 30)
3. Port C (discharge 60)

**Key Principle:**
Containers for Port C must be ACCESSIBLE at Port B!
(Cannot bury Port C containers under Port B cargo)

**Stowage Planning:**
1. Plan BACKWARDS (last port first)
2. Keep discharge ports on top/accessible
3. Minimize restowing (moving containers unnecessarily)
4. Optimize crane time (fast operations = $$$)

**Overstowage:**
When you MUST move containers to access others.
- Costs time and money
- Can damage containers
- Professional planners minimize this!
                        `,
                        practice: {
                            task: 'Load for 2 ports. Port B containers must be accessible.',
                            ship: 'PANAMAX',
                            containers: [
                                { type: '40DC', weight: 25000, id: 'PORTA-1', discharge_port: 'A' },
                                { type: '40DC', weight: 25000, id: 'PORTA-2', discharge_port: 'A' },
                                { type: '40DC', weight: 25000, id: 'PORTB-1', discharge_port: 'B' },
                                { type: '40DC', weight: 25000, id: 'PORTB-2', discharge_port: 'B' }
                            ],
                            success_criteria: {
                                no_overstowage: true,
                                port_b_accessible: true,
                                all_placed: true
                            }
                        }
                    }
                ]
            }
        ];
    }

    /**
     * Get training module by ID
     */
    getModule(moduleId) {
        return this.modules.find(m => m.id === moduleId);
    }

    /**
     * Get lesson by ID
     */
    getLesson(moduleId, lessonId) {
        const module = this.getModule(moduleId);
        if (!module) return null;
        return module.lessons.find(l => l.id === lessonId);
    }

    /**
     * Start a lesson (prepare simulator for practice)
     */
    startLesson(lessonId) {
        for (const module of this.modules) {
            const lesson = module.lessons.find(l => l.id === lessonId);
            if (lesson) {
                return {
                    module: module,
                    lesson: lesson,
                    theory: lesson.theory,
                    practice: lesson.practice
                };
            }
        }
        return null;
    }

    /**
     * Check if lesson practice is completed successfully
     */
    checkLessonSuccess(lessonId, results) {
        const lessonData = this.startLesson(lessonId);
        if (!lessonData) return { passed: false, feedback: 'Lesson not found' };

        const criteria = lessonData.practice.success_criteria;
        const feedback = [];
        let passed = true;

        // Check each criterion
        if (criteria.min_gm && results.gm < criteria.min_gm) {
            passed = false;
            feedback.push(`❌ GM too low: ${results.gm.toFixed(2)}m (minimum: ${criteria.min_gm}m)`);
        }

        if (criteria.max_list && Math.abs(results.listAngle) > criteria.max_list) {
            passed = false;
            feedback.push(`❌ Excessive list: ${results.listAngle.toFixed(2)}° (maximum: ${criteria.max_list}°)`);
        }

        if (criteria.max_trim && Math.abs(results.trimAngle) > criteria.max_trim) {
            passed = false;
            feedback.push(`❌ Excessive trim: ${results.trimAngle.toFixed(2)}° (maximum: ${criteria.max_trim}°)`);
        }

        if (criteria.all_placed && results.unplacedCount > 0) {
            passed = false;
            feedback.push(`❌ Not all containers placed: ${results.unplacedCount} remaining`);
        }

        if (criteria.reefer_on_plug && !results.reefer_on_plug) {
            passed = false;
            feedback.push(`❌ Reefer container not on power plug!`);
        }

        if (criteria.imdg_segregation_correct && results.imdg_violations > 0) {
            passed = false;
            feedback.push(`❌ IMDG segregation violations: ${results.imdg_violations}`);
        }

        // Success feedback
        if (passed) {
            feedback.push(`✅ All criteria met!`);
            feedback.push(`✅ GM: ${results.gm.toFixed(2)}m`);
            feedback.push(`✅ List: ${results.listAngle.toFixed(2)}°`);
            feedback.push(`✅ Trim: ${results.trimAngle.toFixed(2)}°`);
        }

        return { passed, feedback, criteria };
    }

    /**
     * Get certification for completed module
     */
    issueCertification(moduleId, userName) {
        const module = this.getModule(moduleId);
        if (!module) return null;

        return {
            certificate_id: `CFMC-${moduleId}-${Date.now()}`,
            holder: userName,
            course: module.title,
            certification: module.certification,
            issue_date: new Date().toISOString(),
            valid_until: new Date(Date.now() + 365*24*60*60*1000).toISOString(), // 1 year
            issuer: 'CargoForge Maritime Training Academy',
            verification_url: `https://cargoforge.com/verify/${moduleId}`
        };
    }

    /**
     * Get complete training path
     */
    getTrainingPath() {
        return {
            beginner: this.modules.filter(m => m.difficulty === 'beginner'),
            intermediate: this.modules.filter(m => m.difficulty === 'intermediate'),
            advanced: this.modules.filter(m => m.difficulty === 'advanced')
        };
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = TrainingModules;
}
