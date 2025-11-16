#!/usr/bin/env node

// CargoForge-C Test Runner for Node.js
// This runs the physics tests from the command line

const fs = require('fs');

// Load and evaluate physics.js
const physicsCode = fs.readFileSync(__dirname + '/physics.js', 'utf8')
    .replace('if (typeof module', '// if (typeof module'); // Disable module export check
eval(physicsCode);

// Load and evaluate test code
const testCode = fs.readFileSync(__dirname + '/test-physics.js', 'utf8')
    .replace('if (typeof module', '// if (typeof module'); // Disable module export check
eval(testCode);

// Run the tests
console.log('\n🚀 Running CargoForge-C Physics Tests...\n');

try {
    const tester = new PhysicsTests();
    const results = tester.runAllTests();

    // Exit with appropriate code
    if (results.failed === 0) {
        console.log('\n✅ All tests passed successfully!\n');
        process.exit(0);
    } else {
        console.log(`\n❌ ${results.failed} test(s) failed!\n`);
        process.exit(1);
    }
} catch (error) {
    console.error('\n❌ Test suite crashed:', error);
    process.exit(1);
}
