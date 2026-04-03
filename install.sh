#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${CYAN}⚡ CM Language Installer for POSIX (Linux/macOS)${NC}"
echo -e "${CYAN}================================================${NC}"

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}❌ CMake is missing.${NC}"
    echo -e "${YELLOW}Attempting to install CMake...${NC}"
    if command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y cmake
    elif command -v brew &> /dev/null; then
        brew install cmake
    else
        echo -e "${RED}Could not auto-install cmake. Please install it manually.${NC}"
        exit 1
    fi
fi

# Check for gcc or clang
if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
    echo -e "${RED}❌ A C compiler (gcc/clang) is missing.${NC}"
    echo -e "${YELLOW}Attempting to install build essentials...${NC}"
    if command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y build-essential curl
    elif command -v brew &> /dev/null; then
        # macOS usually prompts for xcode CLI tools automatically, but brew gcc is fallback
        xcode-select --install || echo "Xcode CLI tools already installed"
    else
        echo -e "${RED}Could not auto-install C compiler. Please install it manually.${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}✅ Dependencies found. Building CM...${NC}"
cmake -B build
cmake --build build

echo -e "${GREEN}✨ CM Compiler successfully built!${NC}"
cp build/cm cm
chmod +x cm
echo -e "${GREEN}✅ Done! The 'cm' binary is now in your root directory.${NC}"
echo -e "${CYAN}Try it: ./cm run tests/oop_test.curium${NC}"
