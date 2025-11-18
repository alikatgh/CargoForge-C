#!/bin/bash
# CargoForge-C Testing & Demo Script

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  CargoForge-C Interactive Physics Simulator             ║"
echo "║  Testing & Demo Launcher                                ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Check if we're in the right directory
if [ ! -d "web/frontend" ]; then
    echo "❌ Error: Please run this script from the CargoForge-C root directory"
    exit 1
fi

echo "Select an option:"
echo ""
echo "  1) 🧪 Run Quick Smoke Tests (30 seconds)"
echo "  2) 🧪 Run Full Test Suite (2 minutes)"
echo "  3) 🎮 Launch Interactive Simulator"
echo "  4) 📊 View Test Report"
echo "  5) 🚀 All Tests + Simulator (recommended)"
echo ""
read -p "Enter choice [1-5]: " choice

case $choice in
    1)
        echo ""
        echo "🧪 Starting quick smoke tests..."
        cd web/frontend
        echo "Starting web server on http://localhost:8000"
        echo ""
        echo "📋 Opening test-simple.html in your browser..."
        echo "   (Tests will run automatically)"
        echo ""
        python3 -m http.server 8000 &
        SERVER_PID=$!
        sleep 2
        xdg-open http://localhost:8000/test-simple.html 2>/dev/null || \
        open http://localhost:8000/test-simple.html 2>/dev/null || \
        echo "Please open: http://localhost:8000/test-simple.html"
        echo ""
        echo "Press Ctrl+C to stop the server when done"
        wait $SERVER_PID
        ;;

    2)
        echo ""
        echo "🧪 Starting full test suite..."
        cd web/frontend
        echo "Starting web server on http://localhost:8000"
        echo ""
        echo "📋 Opening TEST_RUNNER.html in your browser..."
        echo "   Click 'RUN ALL TESTS' to start"
        echo ""
        python3 -m http.server 8000 &
        SERVER_PID=$!
        sleep 2
        xdg-open http://localhost:8000/TEST_RUNNER.html 2>/dev/null || \
        open http://localhost:8000/TEST_RUNNER.html 2>/dev/null || \
        echo "Please open: http://localhost:8000/TEST_RUNNER.html"
        echo ""
        echo "Press Ctrl+C to stop the server when done"
        wait $SERVER_PID
        ;;

    3)
        echo ""
        echo "🎮 Launching Interactive Simulator..."
        cd web/frontend
        echo "Starting web server on http://localhost:8000"
        echo ""
        echo "🚢 Opening SIMULATOR_DEMO.html in your browser..."
        echo ""
        echo "HOW TO PLAY:"
        echo "  1. Click 'START INTERACTIVE MODE'"
        echo "  2. Click on a cargo item to select it"
        echo "  3. Click on the ship to place it"
        echo "  4. WATCH THE SHIP TILT in real-time!"
        echo "  5. Try placing cargo on one side vs evenly"
        echo "  6. Adjust the sea state slider to see waves"
        echo ""
        python3 -m http.server 8000 &
        SERVER_PID=$!
        sleep 2
        xdg-open http://localhost:8000/SIMULATOR_DEMO.html 2>/dev/null || \
        open http://localhost:8000/SIMULATOR_DEMO.html 2>/dev/null || \
        echo "Please open: http://localhost:8000/SIMULATOR_DEMO.html"
        echo ""
        echo "Press Ctrl+C to stop the server when done"
        wait $SERVER_PID
        ;;

    4)
        echo ""
        echo "📊 Viewing Test Report..."
        if command -v less &> /dev/null; then
            less ../docs/TESTING_COMPLETE.md
        else
            cat ../docs/TESTING_COMPLETE.md
        fi
        ;;

    5)
        echo ""
        echo "🚀 Running all tests + simulator..."
        cd web/frontend
        echo "Starting web server on http://localhost:8000"
        echo ""
        python3 -m http.server 8000 &
        SERVER_PID=$!
        sleep 2

        echo "📋 Opening tests..."
        xdg-open http://localhost:8000/test-simple.html 2>/dev/null || \
        open http://localhost:8000/test-simple.html 2>/dev/null || \
        echo "Please open: http://localhost:8000/test-simple.html"

        sleep 3

        echo "🎮 Opening simulator..."
        xdg-open http://localhost:8000/SIMULATOR_DEMO.html 2>/dev/null || \
        open http://localhost:8000/SIMULATOR_DEMO.html 2>/dev/null || \
        echo "Please open: http://localhost:8000/SIMULATOR_DEMO.html"

        echo ""
        echo "✅ Tests should be running in one tab"
        echo "🎮 Simulator ready in another tab"
        echo ""
        echo "Press Ctrl+C to stop the server when done"
        wait $SERVER_PID
        ;;

    *)
        echo "❌ Invalid choice. Please run the script again and select 1-5."
        exit 1
        ;;
esac

echo ""
echo "✅ Done!"
