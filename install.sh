#!/bin/bash
# CLI DECOR installer (C++ high-performance engine)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="$HOME/.config/clidecor"

echo "Installing CLI DECOR (C++ engine) to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR"

LIBS=""
if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* || "$OSTYPE" == "win32"* ]]; then
    LIBS="-lws2_32"
fi

if command -v g++ >/dev/null 2>&1; then
    echo "Compiling C++ binary..."
    g++ -O3 -std=c++17 "$SCRIPT_DIR/src/main.cpp" -o "$INSTALL_DIR/clidecor" $LIBS || make -C "$SCRIPT_DIR"
elif command -v clang++ >/dev/null 2>&1; then
    echo "Compiling C++ binary with clang++..."
    clang++ -O3 -std=c++17 "$SCRIPT_DIR/src/main.cpp" -o "$INSTALL_DIR/clidecor" $LIBS || make -C "$SCRIPT_DIR"
fi

if [ -f "$SCRIPT_DIR/clidecor.exe" ]; then
    cp "$SCRIPT_DIR/clidecor.exe" "$INSTALL_DIR/clidecor.exe"
fi

# don't overwrite an existing user config
if [ ! -f "$INSTALL_DIR/config.conf" ]; then
    cp "$SCRIPT_DIR/config.conf" "$INSTALL_DIR/config.conf"
fi

# install default side art
if [ -f "$SCRIPT_DIR/tux.png" ]; then
    cp "$SCRIPT_DIR/tux.png" "$INSTALL_DIR/tux.png"
fi

chmod +x "$INSTALL_DIR/clidecor" 2>/dev/null || true

EXE_NAME="clidecor"
if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* || "$OSTYPE" == "win32"* ]]; then
    EXE_NAME="clidecor.exe"
fi

# Make globally accessible for the user
BIN_DIR="$HOME/.local/bin"
mkdir -p "$BIN_DIR"
if [ -f "$INSTALL_DIR/$EXE_NAME" ]; then
    echo '#!/bin/sh' > "$BIN_DIR/clidecor"
    echo "exec \"$INSTALL_DIR/$EXE_NAME\" \"\$@\"" >> "$BIN_DIR/clidecor"
    chmod +x "$BIN_DIR/clidecor"
fi

RC_FILE=""
if [ -f "$HOME/.zshrc" ]; then
    RC_FILE="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    RC_FILE="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile" ]; then
    RC_FILE="$HOME/.bash_profile"
else
    # Fallback: Create .bashrc
    RC_FILE="$HOME/.bashrc"
    touch "$RC_FILE"
fi

LINE="\$HOME/.config/clidecor/$EXE_NAME"
ALIAS_LINE="alias clidecor=\"\$HOME/.config/clidecor/$EXE_NAME\""

if [ -n "$RC_FILE" ]; then
    # Add auto-start on terminal launch
    if ! grep -qF "\$HOME/.config/clidecor" "$RC_FILE" 2>/dev/null; then
        echo "" >> "$RC_FILE"
        echo "# CLI DECOR - runs on new terminal" >> "$RC_FILE"
        echo "$LINE" >> "$RC_FILE"
        echo "Added CLI DECOR auto-start to $RC_FILE"
    fi
    
    # Add alias for global command access if PATH doesn't work out of the box
    if ! grep -qF "alias clidecor" "$RC_FILE" 2>/dev/null; then
        echo "$ALIAS_LINE" >> "$RC_FILE"
        echo "Added 'clidecor' alias to $RC_FILE"
    fi
fi

echo ""
echo "Done. Edit ~/.config/clidecor/config.conf to customize what shows."
echo -e "\033[1;32mIMPORTANT: Run 'source $RC_FILE' or open a new terminal to use the 'clidecor' command globally!\033[0m"
