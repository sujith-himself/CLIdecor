#!/bin/bash
# CLI DECOR installer
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="$HOME/.config/clidecor"

echo "Installing CLI DECOR to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR/src"
cp "$SCRIPT_DIR/clidecor.sh" "$INSTALL_DIR/clidecor.sh"
cp "$SCRIPT_DIR/src/imgrender.py" "$INSTALL_DIR/src/imgrender.py"

# don't overwrite an existing user config
if [ ! -f "$INSTALL_DIR/config.conf" ]; then
    cp "$SCRIPT_DIR/config.conf" "$INSTALL_DIR/config.conf"
fi

chmod +x "$INSTALL_DIR/clidecor.sh"
# imgrender.py is invoked as 'python3 imgrender.py', no execute bit needed

# check python3 + Pillow (needed only if you use image_path)
if ! command -v python3 >/dev/null; then
    echo "note: python3 not found — image logo feature won't work, but text info still will."
elif ! python3 -c "import PIL" >/dev/null 2>&1; then
    echo "note: Pillow not found — image logo won't work until you install it."
    echo "      Debian/Ubuntu/Kali:  sudo apt install python3-pil"
    echo "      Other / macOS:       pip install Pillow"
fi

RC_FILE=""
# Check for zsh first (by file presence), then bash
if [ -f "$HOME/.zshrc" ]; then
    RC_FILE="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    RC_FILE="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile" ]; then
    RC_FILE="$HOME/.bash_profile"
fi

LINE="bash \$HOME/.config/clidecor/clidecor.sh"

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
echo "Open a new terminal to see it in action, or run: bash ~/.config/clidecor/clidecor.sh"
