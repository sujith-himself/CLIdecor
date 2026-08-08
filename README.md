# CLI DECOR ⚡ (C++ Ultra-Fast Engine)

A minimal, config-driven, sub-millisecond Neofetch alternative written in **C++17**. Cross-platform for **Linux, macOS, and Windows** (Windows Terminal, PowerShell, CMD, Git Bash). Shows system info alongside custom image half-block pixel art or ASCII logo every time you open a terminal.

![CLI DECOR Terminal Preview](C:/Users/ADMIN/.gemini/antigravity-ide/brain/24dca69b-f0df-49d9-ba9c-cf640fcdb13d/clidecor_terminal_preview_1786185079401.png)

```text
              ADMIN@JITH
              ----------
              OS:          Microsoft Windows 11 Pro
              Kernel:      Version 10.0.22631.5624
              Uptime:      10 hours, 54 minutes
    ___       Shell:       cmd.exe
   /   \      Terminal:    Windows Terminal / Console
  | O O |     Resolution:  1920x1080
  |  ^  |     CPU:         AMD Ryzen 7 5700G with Radeon Graphics [░░░░░░░░░░░░░░░░] 0%
   \___/      Memory:      21602MiB / 22395MiB [███████████████░] 96%
  CLIDECOR    Disk:        591G / 930G (63%)
              Local IP:    192.168.191.1

              Welcome back, boss.
```

---

## Why CLI DECOR?

- **Sub-Millisecond Execution (< 5ms)**: Written in native C++17. Eliminates slow shell script subprocess forks and Python interpreter overhead.
- **Cross-Platform (Linux, macOS, Windows)**: Uses native OS APIs (`Win32` on Windows, `procfs` on Linux, `sysctl` on macOS).
- **Zero Python Dependencies**: Image logos (PNG, JPEG, BMP) are decoded and rendered natively in C++ using `stb_image.h`.
- **Config-Driven & Fully Modular**: Every field is individually toggleable via `~/.config/clidecor/config.conf`.
- **Truecolor & Pixel Art Engine**: Real truecolor half-block (`▀` / `▄`) rendering for high-resolution terminal image logos and custom pixel sizes.

---

## Quick Install

### Linux & macOS (Bash / Zsh)

```bash
git clone https://github.com/sujith-himself/CLIdecor.git
cd CLIdecor
chmod +x install.sh
./install.sh
```

### Windows (PowerShell)

```powershell
git clone https://github.com/sujith-himself/CLIdecor.git
cd CLIdecor
.\install.ps1
```

### Windows (CMD / Batch)

```cmd
git clone https://github.com/sujith-himself/CLIdecor.git
cd CLIdecor
install.bat
```

---

## Configuration

Edit `~/.config/clidecor/config.conf` (or `%USERPROFILE%\.config\clidecor\config.conf` on Windows):

```ini
# --- Info fields ---
show_os=1
show_kernel=1
show_uptime=1
show_packages=1
show_shell=1
show_cpu=1
show_gpu=1
show_memory=1
show_disk=1
show_battery=0
show_localip=1

# --- Styling & Themes ---
theme=default          # default | hacker | dracula | nord | fire | gold
accent_color=cyan      # cyan | red | green | yellow | blue | magenta | white

# --- Custom text / Quotes ---
custom_text=Welcome back, boss.|Hack the planet.|Stay sharp.

# --- Logo Image ---
image_path=/path/to/your/image.png
image_style=color      # color (half-block ANSI) | ascii
image_width=28         # columns wide
pixel_size=1           # 1 = smooth, 2-4 = chunky retro pixel-art blocks
```

---

## License

MIT License. Designed & maintained for maximum speed and simplicity.
