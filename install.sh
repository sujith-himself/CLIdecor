#!/bin/bash
# CLI DECOR installer (C++ high-performance engine)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="$HOME/.config/clidecor"

echo "Installing CLI DECOR (C++ engine) to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR"

if command -v g++ >/dev/null 2>&1; then
    echo "Compiling C++ binary..."
    g++ -O3 -std=c++17 "$SCRIPT_DIR/src/main.cpp" -o "$INSTALL_DIR/clidecor" || make -C "$SCRIPT_DIR"
elif command -v clang++ >/dev/null 2>&1; then
    echo "Compiling C++ binary with clang++..."
    clang++ -O3 -std=c++17 "$SCRIPT_DIR/src/main.cpp" -o "$INSTALL_DIR/clidecor" || make -C "$SCRIPT_DIR"
fi

if [ -f "$SCRIPT_DIR/clidecor.exe" ]; then
    cp "$SCRIPT_DIR/clidecor.exe" "$INSTALL_DIR/clidecor.exe"
fi

# don't overwrite an existing user config
if [ ! -f "$INSTALL_DIR/config.conf" ]; then
    cp "$SCRIPT_DIR/config.conf" "$INSTALL_DIR/config.conf"
fi

chmod +x "$INSTALL_DIR/clidecor" 2>/dev/null || true

# Make globally accessible for the user
BIN_DIR="$HOME/.local/bin"
mkdir -p "$BIN_DIR"
if [ -f "$INSTALL_DIR/clidecor" ]; then
    ln -sf "$INSTALL_DIR/clidecor" "$BIN_DIR/clidecor"
    echo "Made 'clidecor' command globally accessible via $BIN_DIR"
fi

RC_FILE=""
if [ -f "$HOME/.zshrc" ]; then
    RC_FILE="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    RC_FILE="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile" ]; then
    RC_FILE="$HOME/.bash_profile"
fi

LINE="\$HOME/.config/clidecor/clidecor"

if [ -n "$RC_FILE" ]; then
    if ! grep -qF "$LINE" "$RC_FILE" 2>/dev/null; then
        echo "" >> "$RC_FILE"
        echo "# CLI DECOR - runs on new terminal" >> "$RC_FILE"
        echo "$LINE" >> "$RC_FILE"
        echo "Added CLI DECOR to $RC_FILE"
    else
        echo "CLI DECOR already present in $RC_FILE"
    fi
else
    echo "Could not detect .bashrc/.zshrc — add this line to your shell rc manually:"
    echo "  $LINE"
fi

echo ""
echo "Done. Edit ~/.config/clidecor/config.conf to customize what shows."
echo "Open a new terminal to see it in action!"
