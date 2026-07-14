# CLI DECOR

A minimal, config-driven neofetch alternative. Shows the system info you actually care about,
alongside your own image or the default ASCII logo, every time you open a terminal.

```
    ___        sujith@nixos
   /   \       ------------
  | O O |      OS:       NixOS 24.11 (Vicuna)
  |  ^  |      Kernel:   6.12.28
   \___/       Uptime:   2 hours, 14 minutes
  CLIDECOR     Shell:    zsh
               CPU:      AMD Ryzen 5 5600X
               Memory:   4891MiB / 15929MiB
               Disk:     42G / 117G (37%)
               Local IP: 192.168.1.10

               Welcome back, boss.
```

---

## Why

neofetch and fastfetch show things you didn't ask for (DE, WM theme, icons, terminal font...)
and skip things that are actually useful (battery, local IP, disk). CLI DECOR is a
stripped-down replacement — every field is individually toggleable, and you can drop
in any image as the logo using real truecolor half-block rendering.

---

## Requirements

- **bash** — any modern version
- **python3 + Pillow** — only needed for image logos. Install with:
  ```bash
  pip install Pillow
  ```
  Without Pillow, the default ASCII logo and all info fields work fine.

---

## Install

```bash
git clone git@github.com:sujith-himself/CLIdecor.git
cd CLIdecor
chmod +x install.sh
./install.sh
```

The installer:
1. Copies `clidecor.sh` and `src/imgrender.py` to `~/.config/clidecor/`
2. Copies `config.conf` to `~/.config/clidecor/config.conf` (only if one doesn't exist yet — existing config is never overwritten)
3. Appends the following block to your `.zshrc`, `.bashrc`, or `.bash_profile` (whichever it finds first):
   ```bash
   # CLI DECOR - runs on new terminal
   bash $HOME/.config/clidecor/clidecor.sh
   ```
4. Open a new terminal — it runs automatically.

> **Note:** python3 not found or Pillow missing? The installer will warn you. Text info still works without either.

---

## Manual run (without installing)

```bash
bash clidecor.sh
```

Or after installing:

```bash
bash ~/.config/clidecor/clidecor.sh
```

---

## Configuring

Edit `~/.config/clidecor/config.conf` (after install) or `config.conf` (in the repo, before install):

```ini
# --- Info fields ---
# Set to 0 to hide, 1 to show. Comment out or delete to hide too.
show_os=1
show_kernel=1
show_uptime=1
show_packages=1
show_shell=1
show_cpu=1
show_gpu=1
show_memory=1
show_disk=1
show_battery=0      # off by default — won't show on desktops without a battery
show_localip=1

# --- Custom text ---
# Printed at the bottom of the info panel. Can be anything.
custom_text=Welcome back, boss.

# --- Logo image ---
# Leave blank to use the default ASCII logo.
image_path=

# color  = full truecolor half-block rendering (recommended, looks like pixel art)
# ascii  = classic brightness-mapped character art
image_style=color

# How many terminal columns wide the logo should be.
# Typical values: 20–24 for narrow windows, 28–36 for wide ones.
image_width=28
```

### Field reference

| Key | Values | What it does |
|---|---|---|
| `show_os` | `0` / `1` | Distro name from `/etc/os-release` |
| `show_kernel` | `0` / `1` | Kernel version from `uname -r` |
| `show_uptime` | `0` / `1` | Human-readable uptime |
| `show_packages` | `0` / `1` | Package count (dpkg / rpm / pacman) |
| `show_shell` | `0` / `1` | Current shell |
| `show_cpu` | `0` / `1` | CPU model |
| `show_gpu` | `0` / `1` | GPU via `lspci` |
| `show_memory` | `0` / `1` | Used / total RAM |
| `show_disk` | `0` / `1` | Used / total disk on `/` |
| `show_battery` | `0` / `1` | Battery % and status (laptop only) |
| `show_localip` | `0` / `1` | Local IP address |
| `custom_text` | any string | Extra line at bottom of info panel |
| `image_path` | file path | Path to your logo image |
| `image_style` | `color` / `ascii` | Rendering mode for the image |
| `image_width` | integer | Logo width in terminal columns |

---

## Using a custom image logo

### Step 1 — Install Pillow

```bash
pip install Pillow
```

### Step 2 — Pick your image

Best results come from:
- **Small pixel art or icons** — 64×64 to 128×128 px PNGs look sharpest
- **Logos with flat colors** — distro logos, game sprites, anything vector-style
- **PNG over JPG** — compression artifacts in JPGs look bad at terminal resolution

Large photographs work but turn into color noise. Smaller, simpler images = better.

### Step 3 — Edit config

```ini
image_path=/home/youruser/pictures/logo.png
image_style=color
image_width=28
```

- Use an **absolute path** — relative paths work when run from the script's dir but break when called from your shell rc.
- Adjust `image_width` to taste. If the info panel is getting pushed off screen, lower it.

### Step 4 — Test immediately

```bash
bash ~/.config/clidecor/clidecor.sh
```

No need to open a new terminal. Tweak `image_width` until it looks right.

### Width guide

| Terminal width | Recommended `image_width` |
|---|---|
| Narrow (< 80 cols) | 18–22 |
| Standard (80 cols) | 24–28 |
| Wide (100+ cols) | 32–40 |
| Ultrawide / tiled | 44–56 |

### Style comparison

| `image_style=color` | `image_style=ascii` |
|---|---|
| Truecolor half-block (▀) — each character = 2 stacked pixels | Brightness-mapped characters from ` .:-=+*#%@` |
| Best for any image with color | Best for monochrome art or retro look |
| Requires truecolor terminal support | Works in any terminal |

---

## How it works

1. `clidecor.sh` reads `config.conf` and calls standard Linux commands and files
   (`/etc/os-release`, `/proc/cpuinfo`, `/proc/meminfo`, `uname`, `df`, `lspci`, etc.)
   to gather each enabled info field.

2. If `image_path` is set and the file exists, it calls:
   ```bash
   python3 src/imgrender.py <image_path> <image_width> <image_style>
   ```
   `imgrender.py` uses Pillow to resize the image, then renders it using the
   **half-block trick**: the `▀` Unicode character with its foreground color set
   to the top pixel and background set to the bottom pixel via ANSI truecolor codes.
   One character cell = 2 image pixels of vertical resolution.

3. The logo block (left) and info block (right) are printed side by side,
   line by line, with a 3-space gap.

4. If no image is set or the file doesn't exist, the built-in ASCII logo is used.

No network calls. No telemetry. Nothing runs except what's in these two files.

---

## Uninstall

Remove the CLI DECOR block from your `.bashrc` / `.zshrc`. It looks like this:

```
# CLI DECOR - runs on new terminal
bash $HOME/.config/clidecor/clidecor.sh
```

Then delete the install directory:

```bash
rm -rf ~/.config/clidecor
```
