#!/bin/bash
# ╔══════════════════════════════════════════════════════════╗
# ║           CLI DECOR — Installer                         ║
# ║  Works two ways:                                        ║
# ║    1. curl -sSL https://raw.githubusercontent.com/      ║
# ║         sujith-himself/CLIdecor/main/install.sh | bash  ║
# ║    2. git clone ... && bash install.sh                  ║
# ╚══════════════════════════════════════════════════════════╝
set -e

REPO="https://github.com/sujith-himself/CLIdecor.git"
INSTALL_DIR="$HOME/.config/clidecor"
BIN_DIR="$HOME/.local/bin"
TMP_DIR=""

# ── Colour helpers ───────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
info()  { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()    { echo -e "${GREEN}[ OK ]${RESET}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
die()   { echo -e "${RED}[FAIL]${RESET}  $*" >&2; exit 1; }

echo -e "${BOLD}"
echo "  ██████╗██╗     ██╗██████╗ ███████╗ ██████╗ ██████╗ ██████╗ "
echo " ██╔════╝██║     ██║██╔══██╗██╔════╝██╔════╝██╔═══██╗██╔══██╗"
echo " ██║     ██║     ██║██║  ██║█████╗  ██║     ██║   ██║██████╔╝"
echo " ██║     ██║     ██║██║  ██║██╔══╝  ██║     ██║   ██║██╔══██╗"
echo " ╚██████╗███████╗██║██████╔╝███████╗╚██████╗╚██████╔╝██║  ██║"
echo "  ╚═════╝╚══════╝╚═╝╚═════╝ ╚══════╝ ╚═════╝ ╚═════╝╚═╝  ╚═╝"
echo -e "${RESET}"
echo -e " ${CYAN}The neofetch replacement built in C++${RESET}"
echo ""

# ── Detect if running from inside a cloned repo or via curl ──
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd)"
if [ -f "$SCRIPT_DIR/src/main.cpp" ]; then
    SOURCE_DIR="$SCRIPT_DIR"
    info "Running from cloned repo at $SOURCE_DIR"
else
    # Running via curl — clone the repo to a temp dir
    info "Running via curl — cloning repo..."
    if ! command -v git &>/dev/null; then
        die "git is required. Install it with: sudo apt install git  (or your distro's equivalent)"
    fi
    TMP_DIR="$(mktemp -d)"
    git clone --depth=1 "$REPO" "$TMP_DIR" &>/dev/null || die "Failed to clone repository. Check your internet connection."
    SOURCE_DIR="$TMP_DIR"
    ok "Repository cloned."
fi

# ── Check dependencies ────────────────────────────────────────
info "Checking dependencies..."
HAS_GPP=false; HAS_CLANG=false
command -v g++     &>/dev/null && HAS_GPP=true
command -v clang++ &>/dev/null && HAS_CLANG=true

if ! $HAS_GPP && ! $HAS_CLANG; then
    echo ""
    warn "No C++ compiler found. Install one with:"
    echo "   Ubuntu/Debian : sudo apt install g++"
    echo "   Arch/Manjaro  : sudo pacman -S gcc"
    echo "   Fedora        : sudo dnf install gcc-c++"
    echo "   openSUSE      : sudo zypper install gcc-c++"
    echo ""
    die "Aborting. Please install a C++ compiler and re-run."
fi

# ── Create install directory ──────────────────────────────────
mkdir -p "$INSTALL_DIR" "$BIN_DIR"

# ── Compile ───────────────────────────────────────────────────
info "Compiling CLI DECOR (this takes about 10 seconds)..."
COMPILE_OK=false
if $HAS_GPP; then
    if g++ -O3 -std=c++17 "$SOURCE_DIR/src/main.cpp" -o "$INSTALL_DIR/clidecor" -pthread 2>/dev/null; then
        COMPILE_OK=true
    fi
fi
if ! $COMPILE_OK && $HAS_CLANG; then
    if clang++ -O3 -std=c++17 "$SOURCE_DIR/src/main.cpp" -o "$INSTALL_DIR/clidecor" -pthread 2>/dev/null; then
        COMPILE_OK=true
    fi
fi
if ! $COMPILE_OK; then
    die "Compilation failed. Please open an issue at: $REPO/issues"
fi
ok "Binary compiled successfully."
chmod +x "$INSTALL_DIR/clidecor"

# ── Copy assets (don't overwrite user's existing config) ──────
if [ ! -f "$INSTALL_DIR/config.conf" ]; then
    cp "$SOURCE_DIR/config.conf" "$INSTALL_DIR/config.conf"
    ok "Default config installed."
else
    info "Existing config preserved."
fi

if [ -f "$SOURCE_DIR/tux.png" ] && [ ! -f "$INSTALL_DIR/tux.png" ]; then
    cp "$SOURCE_DIR/tux.png" "$INSTALL_DIR/tux.png"
fi

# ── Install wrapper script to ~/.local/bin ────────────────────
cat > "$BIN_DIR/clidecor" <<EOF
#!/bin/sh
exec "$INSTALL_DIR/clidecor" "\$@"
EOF
chmod +x "$BIN_DIR/clidecor"
ok "Wrapper installed to $BIN_DIR/clidecor"

# ── Add ~/.local/bin to PATH and auto-run on terminal start ──
for RC in "$HOME/.zshrc" "$HOME/.bashrc" "$HOME/.bash_profile"; do
    [ -f "$RC" ] || continue

    # Add ~/.local/bin to PATH if missing
    if ! grep -qF 'HOME/.local/bin' "$RC" 2>/dev/null; then
        echo '' >> "$RC"
        echo '# Added by CLIdecor installer' >> "$RC"
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$RC"
        ok "Added ~/.local/bin to PATH in $RC"
    fi

    # Add auto-run on new terminal
    if ! grep -qF 'clidecor' "$RC" 2>/dev/null; then
        echo '' >> "$RC"
        echo '# CLI DECOR — shows system info on new terminal' >> "$RC"
        echo 'clidecor' >> "$RC"
        ok "Auto-run added to $RC"
    fi

    # Add bash completions
    if [[ "$RC" == *".bashrc"* ]] && ! grep -qF 'complete -W' "$RC" 2>/dev/null; then
        echo "complete -W \"--settings --update --version --help --remove -t --theme --remind --refresh\" clidecor" >> "$RC"
        ok "Bash tab completion added"
    fi

    break  # Only modify the first shell rc file found
done

# ── Cleanup temp dir ──────────────────────────────────────────
if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
    rm -rf "$TMP_DIR"
fi

# ── Done ──────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}╔══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}${BOLD}║   CLI DECOR installed successfully!      ║${RESET}"
echo -e "${GREEN}${BOLD}╚══════════════════════════════════════════╝${RESET}"
echo ""
echo -e "  ${CYAN}Run now  :${RESET} clidecor"
echo -e "  ${CYAN}Settings :${RESET} clidecor --settings"
echo -e "  ${CYAN}Config   :${RESET} ~/.config/clidecor/config.conf"
echo -e "  ${CYAN}Update   :${RESET} clidecor --update"
echo -e "  ${CYAN}Remove   :${RESET} clidecor --remove"
echo ""
echo -e " ${YELLOW}Open a new terminal or run: source ~/.bashrc${RESET}"
echo ""
