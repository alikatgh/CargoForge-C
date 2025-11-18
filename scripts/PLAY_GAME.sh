#!/bin/bash
# CargoForge-C Game Launcher

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  🚢 CargoForge - Maritime Cargo Training Simulator      ║"
echo "║  Game Launcher                                           ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Check if we're in the right directory
if [ ! -d "web/frontend" ]; then
    echo "❌ Error: Please run this script from the CargoForge-C root directory"
    exit 1
fi

echo "🎮 Launching CargoForge Game..."
echo ""
echo "Starting web server on http://localhost:8000"
echo ""

cd web/frontend

echo "🌐 Opening game in your browser..."
echo ""
echo "Controls:"
echo "  • Click cargo cards to select"
echo "  • Click on ship to place cargo"
echo "  • Watch the ship tilt in real-time!"
echo "  • Keep GM > 1.0m and List < 2°"
echo ""

python3 -m http.server 8000 &
SERVER_PID=$!

sleep 2

# Try to open in browser
xdg-open http://localhost:8000/game.html 2>/dev/null || \
open http://localhost:8000/game.html 2>/dev/null || \
echo "Please open: http://localhost:8000/game.html"

echo ""
echo "✅ Game is running!"
echo "Press Ctrl+C to stop the server"
echo ""

wait $SERVER_PID
