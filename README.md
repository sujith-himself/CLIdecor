<div align="center">

<h1>⚡ CLI DECOR</h1>

<p><strong>The blazing-fast, config-driven Neofetch replacement written in C++17</strong></p>

<p>
  <img src="https://img.shields.io/badge/language-C%2B%2B17-blue?style=for-the-badge&logo=cplusplus" />
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=for-the-badge" />
  <img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/speed-%3C5ms-orange?style=for-the-badge" />
</p>

<p>Shows your system info with live image rendering every time you open a terminal — in under 5ms.</p>

</div>

---

## 🚀 Install

### ⚡ One-line install (curl)
```bash
curl -sSL https://raw.githubusercontent.com/sujith-himself/CLIdecor/main/install.sh | bash
```

### 📦 Git clone
```bash
git clone https://github.com/sujith-himself/CLIdecor.git
cd CLIdecor
bash install.sh
```

### 🪟 Windows (PowerShell)
```powershell
git clone https://github.com/sujith-himself/CLIdecor.git
cd CLIdecor
.\install.ps1
```

> **Dependencies**: Only needs `g++` (GCC) or `clang++`. The installer tells you exactly how to get it if missing.

---

## ✨ Features

| Feature | Details |
|---|---|
| ⚡ **Sub-5ms startup** | Native C++17, no Python, no shell forks |
| 🖼️ **Custom Image Rendering** | PNG/JPG/BMP displayed as half-block TrueColor art |
| 🎨 **Live Themes** | 6 built-in themes, changes update the preview in real time |
| 🦎 **Chameleon Mode** | Auto-extracts dominant color from your image and adapts the UI |
| 🖥️ **Cross-platform** | Linux, macOS, Windows Terminal |
| ⚙️ **Interactive Settings** | Full TUI settings menu — no config editing needed |
| 🔔 **Reminder Box** | Pin a sticky note to your terminal |
| 📐 **Live Image Resizer** | WASD/arrow keys to resize & reposition your image live |
| 🔄 **Auto-updater** | `clidecor --update` fetches and recompiles in one step |
| 🎨 **Figlet Headers** | Built-in 3×5 font engine for big ASCII art text |

---

## 🎛️ All Commands

```bash
clidecor                  # Run (shows your system info)
clidecor --settings       # Open interactive settings TUI
clidecor --update         # Pull latest version and recompile
clidecor --version        # Show version number
clidecor --help           # Full usage
clidecor -t <theme>       # Set theme directly: default|hacker|dracula|nord|fire|gold
clidecor --remind "text"  # Set reminder text
clidecor --remove         # Uninstall CLI DECOR completely
```

---

## ⚙️ Configuration

Config lives at `~/.config/clidecor/config.conf`. Every option is documented inline:

```ini
# ── Info Modules ──────────────────────────────────────
show_os=1          # Show OS name
show_host=1        # Show hostname / machine model
show_kernel=1      # Show kernel version
show_uptime=1      # Show system uptime
show_packages=1    # Show installed package count
show_shell=1       # Show current shell + version
show_terminal=1    # Show terminal emulator
show_resolution=1  # Show screen resolution
show_cpu=1         # Show CPU name + usage bar
show_gpu=1         # Show GPU name
show_memory=1      # Show RAM usage bar
show_swap=1        # Show swap usage
show_disk=1        # Show disk usage
show_localip=1     # Show local IP address

# ── Appearance ────────────────────────────────────────
theme=default      # default | hacker | dracula | nord | fire | gold | chameleon
custom_text=       # Pipe-separated quotes shown at the bottom

# ── Custom Image ──────────────────────────────────────
image_path=        # Absolute path to your PNG/JPG image
image_width=28     # Width in terminal columns
image_style=color  # color (half-block) | ascii

# ── Reminder Box ──────────────────────────────────────
reminder_text=     # Text pinned to the reminder box
reminder_show=1    # 1 to show, 0 to hide
```

---

## 🎨 Themes

| Theme | Description |
|---|---|
| `default` | Cyan gradient — clean and universal |
| `hacker` | Matrix green on black |
| `dracula` | Purple/pink — the Dracula palette |
| `nord` | Cool arctic blues |
| `fire` | Orange/red flame gradient |
| `gold` | Yellow/orange luxury |
| `chameleon` | Auto-adapts to your custom image colors |

---

## 🐧 Supported Distros (Auto Logo)

Ubuntu · Arch Linux · Fedora · Debian · Kali Linux · Linux Mint · Pop!\_OS · Manjaro · openSUSE · EndeavourOS · *(any other distro shows Tux)*

---

## 🗑️ Uninstall

```bash
clidecor --remove
```

Or manually:
```bash
rm -rf ~/.config/clidecor
rm -f ~/.local/bin/clidecor
```
Then remove the `clidecor` line from your `~/.bashrc` or `~/.zshrc`.

---

## 🏗️ Build from Source

```bash
git clone https://github.com/sujith-himself/CLIdecor.git
cd CLIdecor
make            # or: g++ -O3 -std=c++17 src/main.cpp -o clidecor
./clidecor
```

**Requirements**: `g++` with C++17 support. No other dependencies.

---

## 📄 License

MIT License — free to use, modify, and distribute.

---

<div align="center">
  <sub>Built with ❤️ and C++17 · <a href="https://github.com/sujith-himself/CLIdecor/issues">Report a Bug</a> · <a href="https://github.com/sujith-himself/CLIdecor/issues">Request a Feature</a></sub>
</div>
