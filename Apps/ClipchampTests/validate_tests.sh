#!/bin/bash

# Clipchamp BabylonNative Test Validation Script
echo "Clipchamp BabylonNative Test Validation"
echo "======================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$SCRIPT_DIR"

echo "Test directory: $TEST_DIR"
echo ""

# Check for required test files
echo "Checking required test files..."
ls -la *.cpp *.h 2>/dev/null || echo "No C++ files found"

# Count test cases
TOTAL_TESTS=$(grep -c "TEST_F" *.cpp 2>/dev/null || echo "0")
echo "Total test cases found: $TOTAL_TESTS"

# Check for key patterns
echo ""
echo "Checking for Clipchamp-specific patterns..."
if grep -q "MTLDevice\|MTKView" *.cpp 2>/dev/null; then
    echo "✓ Metal framework integration found"
fi

if grep -q "Babylon::Graphics::Device\|Babylon::AppRuntime" *.cpp 2>/dev/null; then
    echo "✓ BabylonNative core components found"
fi

if grep -q "createSourceCallback\|readFrameCallback" *.cpp 2>/dev/null; then
    echo "✓ Superfill integration found"
fi

echo ""
echo "✓ VALIDATION COMPLETE"
