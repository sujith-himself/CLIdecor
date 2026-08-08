#!/bin/bash
# CLI DECOR Uninstaller

echo "Removing CLI DECOR..."

if [ -d "$HOME/.config/clidecor" ]; then
    rm -rf "$HOME/.config/clidecor"
    echo "Removed ~/.config/clidecor"
fi

if [ -L "$HOME/.local/bin/clidecor" ]; then
    rm -f "$HOME/.local/bin/clidecor"
    echo "Removed symlink ~/.local/bin/clidecor"
elif [ -f "$HOME/.local/bin/clidecor" ]; then
    rm -f "$HOME/.local/bin/clidecor"
    echo "Removed binary ~/.local/bin/clidecor"
fi

echo "Cleaning up shell RC files..."
for RC_FILE in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.bash_profile"; do
    if [ -f "$RC_FILE" ]; then
        sed -i '/clidecor/d' "$RC_FILE"
        echo "Cleaned up $RC_FILE"
    fi
done

echo "CLI DECOR has been successfully uninstalled!"
