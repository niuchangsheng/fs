#!/bin/bash
# KVFS Development Environment Initialization Script
# Run this script to set up the development environment

set -e

echo "=== KVFS Development Environment Setup ==="

# Check for required tools
echo "Checking required tools..."

if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ not found. Please install a C++ compiler."
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake not found. Please install CMake 3.16+."
    exit 1
fi

echo "  [OK] g++ found: $(g++ --version | head -1)"
echo "  [OK] cmake found: $(cmake --version | head -1)"

# Create build directory
echo ""
echo "Setting up build directory..."
if [ ! -d "build" ]; then
    mkdir -p build
    echo "  [OK] Created build/"
else
    echo "  [OK] build/ already exists"
fi

# Configure and build
echo ""
echo "Configuring and building..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo ""
echo "Available targets:"
echo "  ./kvfs_example      - Basic initialization test"
echo ""
echo "Development workflow:"
echo "  1. Check .features.json for failing features"
echo "  2. Pick ONE feature to implement"
echo "  3. Write a test in examples/harness_test.cpp"
echo "  4. Implement the feature"
echo "  5. Rebuild: cd build && make -j"
echo "  6. Run tests: ./kvfs_harness_test"
echo "  7. Update .features.json and commit"
echo ""
