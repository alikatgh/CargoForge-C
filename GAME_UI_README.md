# 🎮 CargoForge - Modern Game UI

**A completely reimagined gaming experience for maritime cargo training**

---

## 🚀 Quick Start

### Option 1: Use the Launcher (Easiest)
```bash
./PLAY_GAME.sh
```

### Option 2: Manual Start
```bash
cd web/frontend
python3 -m http.server 8000
# Open http://localhost:8000/game.html
```

---

## ✨ What's New

### Complete UI Overhaul
The interface has been completely redesigned with a modern, game-style UX:

#### **Before:**
- Basic HTML forms
- No visual feedback
- Poor user experience
- Confusing controls

#### **After:**
- Full-screen game layout
- Real-time HUD displays
- Modern card-based cargo inventory
- Smooth animations and transitions
- Professional game aesthetics
- Intuitive click-to-place controls

---

## 🎯 Features

### 1. **Modern Game HUD**
- **Top Bar**: Level, Score, Time, GM, List Angle, Cargo Count
- **Real-time Updates**: All stats update as you place cargo
- **Color-coded Warnings**: Green (good), Yellow (warning), Red (danger)

### 2. **Interactive 3D Viewport**
- Ship tilts in real-time based on cargo placement
- Orbital camera controls (drag to rotate, scroll to zoom)
- Wave animations
- Visual stability indicators

### 3. **Card-based Cargo Inventory**
- Modern cargo cards at bottom of screen
- Hover effects and animations
- Click to select, click ship to place
- Visual feedback when selected/placed
- Type badges (Standard, Heavy, Reefer, Hazardous)

### 4. **Stability Monitoring**
- Live stability meter
- GM (Metacentric Height) display
- List angle monitoring
- Status indicators (Optimal, Good, Warning, Critical)

### 5. **Tutorial System**
- Welcome overlay on first launch
- Help button (? icon) available anytime
- Clear instructions and goals

### 6. **Level Progression**
- 3 Progressive levels with increasing difficulty
- Level 1: Getting Started (3 containers)
- Level 2: Balance Master (5 containers, harder goals)
- Level 3: Heavy Load Challenge (6 containers, strict requirements)

### 7. **Scoring System**
- **Placement Score**: 100 points per cargo placed
- **Stability Score**: Up to 500 points for optimal stability
- **Time Bonus**: Faster completion = higher bonus
- **Total Score**: Displayed on level complete

### 8. **Notifications**
- Success notifications (green)
- Warning notifications (yellow)
- Error notifications (red)
- Auto-dismiss after 3 seconds

---

## 🎮 How to Play

### Step 1: Select Cargo
- Look at the cargo inventory bar at the bottom
- Click on any cargo card to select it
- Selected card will glow green

### Step 2: Place Cargo
- Click anywhere on the ship in the 3D viewport
- Cargo will be placed and physics will update
- Watch the ship tilt based on your placement!

### Step 3: Monitor Stability
- Keep **GM > 1.0m** (shown in top HUD)
- Keep **List < 2°** (shown in top HUD)
- Watch the stability meter (right side)

### Step 4: Complete Level
- Place all cargo items
- Meet the level goals:
  - Minimum GM (varies by level)
  - Maximum List angle (varies by level)
  - All cargo placed
- View your score breakdown
- Progress to next level!

---

## 🎨 UI Layout

```
┌─────────────────────────────────────────────────────────────┐
│  🚢 CARGOFORGE  │ Level │ Score │ Time │ GM │ List │ Cargo │ ← HUD
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                                                             │
│                   3D Ship Viewport                          │
│               (Click to place cargo)                        │
│                                                             │
│                     [Stability Meter] ←                     │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  📦 CARGO INVENTORY                                         │
│  ┌────┐  ┌────┐  ┌────┐  ┌────┐  ┌────┐                  │
│  │ C1 │  │ C2 │  │ C3 │  │ C4 │  │ C5 │  ← Cargo Cards    │
│  └────┘  └────┘  └────┘  └────┘  └────┘                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 Level Goals

### Level 1: Getting Started
- **Ship**: 100m × 20m small vessel
- **Cargo**: 3 standard containers
- **Goals**:
  - GM ≥ 1.0m
  - List ≤ 2.0°
  - All cargo placed

### Level 2: Balance Master
- **Ship**: 150m × 25m medium vessel
- **Cargo**: 5 containers (1 heavy, 4 standard)
- **Goals**:
  - GM ≥ 1.5m
  - List ≤ 1.5°
  - All cargo placed

### Level 3: Heavy Load Challenge
- **Ship**: 180m × 28m large vessel
- **Cargo**: 6 containers (2 heavy, 1 reefer, 3 standard)
- **Goals**:
  - GM ≥ 1.2m
  - List ≤ 1.0°
  - All cargo placed

---

## 🎯 Gameplay Tips

### 1. **Balance is Key**
- Place heavy cargo near the centerline
- Distribute weight evenly left-to-right
- Avoid placing all cargo on one side

### 2. **Watch the Ship Tilt**
- The ship tilts in real-time!
- If it leans too much, remove and reposition cargo
- Aim for a level ship (0° list)

### 3. **GM Management**
- Higher GM = More stable (but can be too stiff)
- Lower GM = Less stable (can capsize)
- Optimal range: 1.0m - 2.5m

### 4. **Time Bonus**
- Complete levels faster for higher scores
- But don't rush - stability is more important!

### 5. **Capsizing**
- If list exceeds 25°, the ship capsizes
- Level will reset - try again!

---

## 🛠️ Technical Architecture

### File Structure
```
web/frontend/
├── game.html           # Modern game UI (new!)
├── game-ui.js          # Game controller logic (new!)
├── physics.js          # Maritime physics engine (existing)
├── visualizer.js       # 3D visualization (existing)
└── index.html          # Old interface (deprecated)
```

### Key Components

#### **game.html**
- Full-screen grid layout
- Top HUD bar (60px)
- Central 3D viewport (flexible)
- Bottom cargo inventory (180px)
- Overlays (tutorial, level complete)
- Modern dark gradient theme

#### **game-ui.js** (GameController class)
- Level management
- Cargo selection/placement logic
- HUD updates
- Scoring system
- Tutorial flow
- Notification system
- Timer management

#### **physics.js** (MaritimePhysics class)
- Real IMO-compliant stability calculations
- GM, list, trim computations
- Capsizing detection
- Wave motion simulation

#### **visualizer.js** (CargoVisualizer class)
- Three.js 3D rendering
- Ship tilting animation
- Cargo placement visualization
- Ocean waves
- Camera controls

---

## 🎨 Design Decisions

### Color Palette
- **Background**: Dark gradient (#0a0e27 → #1a1f3a)
- **Primary Accent**: Maritime green (#34d399)
- **Warning**: Amber (#fbbf24)
- **Danger**: Red (#f87171)
- **Neutral**: Slate grays (#64748b, #94a3b8)

### Typography
- **Font**: Segoe UI (system font for performance)
- **HUD Stats**: 18px bold
- **Labels**: 10px uppercase with letter-spacing
- **Titles**: 32-48px bold

### Animations
- **Cargo Cards**: Hover lift (translateY -5px)
- **Notifications**: Slide down + fade out
- **Modals**: Scale in animation
- **Transitions**: 0.2s ease for smooth interactions

### Responsive Design
- Mobile breakpoint: 768px
- Adjusted grid layout for small screens
- Smaller cargo cards on mobile

---

## 🚀 Performance

### Optimizations
- **Hardware Acceleration**: CSS transforms and opacity
- **Efficient Rendering**: Three.js with antialiasing
- **Minimal DOM Updates**: Only update changed elements
- **Debounced Events**: Prevent excessive calculations

### Target Performance
- **60 FPS** 3D rendering
- **< 100ms** physics calculations
- **< 50ms** UI updates
- **< 1s** level loading

---

## 🔮 Future Enhancements

### Drag-and-Drop
- Real drag-and-drop instead of click-to-place
- Visual dragging ghost element
- Drop zones on ship deck

### Enhanced Visuals
- Particle effects for cargo placement
- Ship wake/foam effects
- Dynamic lighting based on time of day
- Weather effects (rain, fog)

### More Game Features
- Achievement system
- Leaderboard
- Challenge mode (time trials)
- Sandbox mode (free play)
- Professional training modules integration

### Multiplayer
- Compete with other players
- Shared leaderboards
- Co-op cargo loading

---

## 📱 Browser Compatibility

### Tested Browsers
- ✅ Chrome/Edge (recommended)
- ✅ Firefox
- ✅ Safari
- ⚠️  Mobile browsers (limited support)

### Requirements
- **WebGL**: Required for 3D rendering
- **ES6**: Modern JavaScript features
- **Canvas**: For Three.js rendering

---

## 🐛 Known Issues

1. **Cargo Placement**: Currently random position (no raycasting)
   - **Workaround**: Will be fixed with drag-and-drop

2. **Mobile Performance**: May lag on older devices
   - **Workaround**: Use desktop browser

3. **Resize Handling**: Canvas may not resize perfectly
   - **Workaround**: Refresh page after resizing

---

## 💡 Tips for Developers

### Adding New Levels
Edit `game-ui.js` → `createLevels()`:
```javascript
{
    id: 4,
    name: 'Your Level Name',
    ship: { length: 200, width: 30, max_weight: 100000000 },
    cargo: [
        { id: 'C1', weight: 500000, dimensions: [12, 2.4, 2.6], type: 'standard' }
    ],
    goals: { min_gm: 1.0, max_list: 2.0, all_placed: true }
}
```

### Customizing UI Colors
Edit `game.html` → `<style>` section:
```css
/* Change primary accent color */
.stat-value { color: #YOUR_COLOR; }

/* Change background gradient */
body { background: linear-gradient(135deg, #COLOR1, #COLOR2); }
```

### Adjusting Physics
Edit `physics.js` → Constructor thresholds:
```javascript
this.GM_OPTIMAL_MIN = 0.5; // Adjust stability thresholds
this.LIST_WARNING = 5.0;   // Adjust tilt warnings
```

---

## 📞 Support

For issues or questions:
1. Check this README first
2. Review the code comments
3. Test in Chrome/Edge browser
4. Check browser console for errors

---

## 🎓 Educational Value

This game teaches:
- **Maritime Stability**: Real GM calculations, list/trim concepts
- **Weight Distribution**: Center of gravity principles
- **IMO Standards**: Professional stability guidelines
- **Problem Solving**: Strategic cargo placement
- **Risk Management**: Balancing efficiency vs. safety

**Perfect for:**
- Maritime students
- Cargo planning trainees
- Naval architecture enthusiasts
- Game-based learning fans

---

## 📝 Changelog

### v2.0.0 (Latest) - Modern Game UI
- ✅ Complete UI redesign with modern game aesthetics
- ✅ Full-screen game layout with HUD
- ✅ Card-based cargo inventory
- ✅ Real-time stability monitoring
- ✅ Tutorial system
- ✅ Level progression (3 levels)
- ✅ Scoring system
- ✅ Notifications
- ✅ Loading screen
- ✅ Help system

### v1.0.0 - Physics Simulator
- Real-time physics engine
- 3D visualization with ship tilting
- Wave motion simulation
- Capsizing detection
- Basic HTML interface

---

**🚢 Happy Loading! May your cargo always be balanced! ⚖️**
