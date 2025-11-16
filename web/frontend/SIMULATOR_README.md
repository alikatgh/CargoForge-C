# 🚢 CargoForge-C INTERACTIVE PHYSICS SIMULATOR

## 🎮 What's New?

The simulator has been completely rebuilt with **REAL PHYSICS** and **INTERACTIVE GAMEPLAY**!

### Key Features:

#### ✅ Real Maritime Physics
- **Ship actually tilts** based on cargo placement (list/trim angles)
- **Metacentric height (GM)** calculated in real-time using IMO guidelines
- **Wave simulation** - watch the ship roll and pitch in different sea states
- **Capsizing detection** - ship can actually flip over if unstable!
- **Real-time feedback** - visual warnings when stability is compromised

#### ✅ Interactive Manual Placement
- **Click to select** cargo from the list
- **Click to place** on the ship deck
- **Instant physics updates** - ship tilts as you place each item
- **Visual warnings** - ship glows red/orange when unstable
- **Game scoring** - rated on stability + efficiency + time

#### ✅ Realistic Stability Calculations
- **Block coefficient** - accurate hull displacement
- **Center of gravity** - 3D position calculated from cargo placement
- **Buoyancy calculations** - KB, BM, GM metrics
- **List angle** - transverse tilt from off-center loading
- **Trim angle** - longitudinal tilt from fore/aft imbalance
- **Draft calculation** - based on actual displacement

#### ✅ Wave Effects
- **Sea state control** - 0 (calm) to 6 (very high seas)
- **Rolling motion** - frequency depends on GM (lower GM = slower, larger rolls)
- **Pitching motion** - bow/stern oscillation in waves
- **Heave** - vertical up/down motion
- **Animated ocean surface** - realistic wave geometry

## 🎯 How to Play

### Setup Mode:
1. Configure ship dimensions (length, width, max weight)
2. Review cargo list (or add/remove items)
3. Set sea state (0-6) for wave difficulty
4. Click **"START INTERACTIVE MODE"**

### Playing Mode:
1. **Click on a cargo item** to select it
2. **Click on the ship** to place it (currently grid-based)
3. **Watch the ship tilt** in real-time based on weight distribution
4. **Monitor stability metrics**:
   - List Angle (Roll) - should be < 5°
   - Trim Angle (Pitch) - should be < 2°
   - GM (Metacentric Height) - should be 0.5-2.5m
   - Draft - increases as cargo is loaded
5. **Avoid capsizing** - if list > 25° or GM < 0, ship capsizes!
6. **Place all cargo** to complete the mission

### Scoring:
- **Placement Score**: % of cargo successfully placed
- **Stability Score**: Based on GM, list, and trim (optimal ranges)
- **Time Bonus**: Faster completion = higher bonus

## 📊 Stability Indicators

### Ship Color Coding:
- **Blue-Grey (transparent)**: Optimal stability - all good!
- **Orange (pulsing)**: Warning - approaching limits
- **Red (bright pulse)**: Critical - near capsizing!

### Stability Status:
- **Optimal**: GM 0.5-2.5m, List < 5°, Trim < 2°
- **Warning**: GM < 0.5m or > 3.0m, List 5-10°, Trim 2-5°
- **Critical**: GM < 0.3m, List > 10° - DANGER!

### Warnings:
- ⚠️ **GM too low** - "Tender ship" - slow, large rolling
- ⚠️ **GM too high** - "Stiff ship" - excessive acceleration
- ⚠️ **Excessive list** - Redistribute cargo transversely
- ⚠️ **Excessive trim** - Redistribute cargo longitudinally

## 🌊 Sea State Scale

- **0 - Calm**: Flat ocean, no motion
- **1 - Light**: Gentle ripples
- **2 - Moderate**: Small waves, light rolling
- **3 - Rough**: Moderate waves, noticeable motion
- **4 - Very Rough**: Large waves, significant rolling
- **5 - High**: Very large waves, heavy motion
- **6 - Very High/Phenomenal**: Extreme conditions

**Tip**: Higher sea states amplify poor stability! A ship with low GM will roll much more in rough seas.

## 🎓 Maritime Physics Explained

### Metacentric Height (GM):
The distance between the center of gravity (G) and the metacenter (M).
- **Positive GM** = Stable (ship returns to upright after tilting)
- **Negative GM** = Unstable (ship capsizes)
- **Optimal** = 0.5-2.5m for cargo ships

Formula: `GM = KB + BM - KG`
- KB = Center of buoyancy height (≈ 0.53 × draft)
- BM = Metacentric radius (depends on waterplane area)
- KG = Center of gravity height

### List Angle (Roll):
Transverse tilt caused by off-center cargo placement.
- Calculated from: `tan(θ) = offset_y / GM`
- **Safe**: < 5°
- **Dangerous**: 10-15°
- **Capsizing**: > 25°

### Trim Angle (Pitch):
Longitudinal tilt from unbalanced fore/aft loading.
- Uses longitudinal GM (GML, much larger than transverse GM)
- **Safe**: < 2°
- **Warning**: 2-5°
- **Critical**: > 5°

## 🚀 Running the Simulator

### Quick Start (No Server Required):
```bash
# Just open in a browser!
open SIMULATOR_DEMO.html
# or
firefox SIMULATOR_DEMO.html
# or
chrome SIMULATOR_DEMO.html
```

### With Python Server:
```bash
cd web/frontend
python3 -m http.server 8000
# Open http://localhost:8000/SIMULATOR_DEMO.html
```

### With the Full Backend:
```bash
cd web
./START.sh
# Open http://localhost:5000/index.html
```

## 🔧 Technical Details

### Physics Engine:
- **File**: `physics.js`
- **Class**: `MaritimePhysics`
- **Based on**: IMO stability guidelines, naval architecture principles
- **Calculations**: Real-time (every frame)

### 3D Visualization:
- **File**: `visualizer.js`
- **Class**: `CargoVisualizer`
- **Library**: Three.js (WebGL)
- **Features**: Ship rotation, wave animation, cargo placement, CG marker

### Game Logic:
- **File**: `app.js`
- **Modes**: setup → playing → finished
- **Placement**: Click-based (grid positioning)
- **Scoring**: Multi-factor algorithm

## 🎯 Next Steps (Phase 2 & 3)

### Phase 2 - Enhanced Gameplay:
- [ ] **Real drag-and-drop** - Place cargo with precise mouse positioning
- [ ] **3D raycasting** - Click exact position on ship deck
- [ ] **Cargo rotation** - Manually rotate containers
- [ ] **Undo/redo** - Fix mistakes
- [ ] **Ballast tanks** - Pump water to compensate for list
- [ ] **Challenge modes** - Predefined scenarios with goals

### Phase 3 - Realistic Constraints:
- [ ] **Container bay planning** - TEU/FEU slot system
- [ ] **Lashing points** - Limited securing positions
- [ ] **Stacking rules** - Can't place cargo in mid-air
- [ ] **Load sequences** - Bottom-up placement required
- [ ] **Different ship types** - Container ship, bulk carrier, RoRo
- [ ] **Realistic holds** - Proper cargo holds with hatch covers

## 📝 Notes

- This simulator uses **realistic naval architecture** formulas
- Physics calculations are **frame-rate independent**
- All metrics are based on **IMO stability guidelines**
- Ship dimensions and cargo weights are **realistic** for cargo vessels
- The C backend is **not required** for the interactive simulator (pure JavaScript)

## 🐛 Known Limitations

- Cargo placement is currently **grid-based** (not free placement)
- No **collision detection** between cargo items
- Simplified **hull geometry** (rectangular box)
- No **dynamic ballast** management
- Wave simulation is **cosmetic** (doesn't affect actual physics much)

---

**Built with passion for maritime simulation realism!** 🌊
