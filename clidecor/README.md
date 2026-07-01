# CLI DECOR

A neofetch-style tool that shows system info (and optionally an image) every time you open a terminal.

## Why

neofetch/fastfetch show a bunch of stuff you don't care about (DE, WM theme, icons...)
and don't show some stuff you might (battery, local IP, disk usage). CLI DECOR is a
stripped-down, config-driven alternative where you pick what shows, and you can drop
in your own image as the logo instead of a distro ASCII logo.

## Install

```bash
chmod +x install.sh
./install.sh
```

This copies everything to `~/.config/clidecor/` and adds a line to your `.bashrc`/`.zshrc`
so it runs on every new terminal.

## Requirements

- bash
- python3 + Pillow (`pip install Pillow`) — only needed if you want to use a custom image logo.
  Without it, the plain ASCII logo + info panel still works fine.

## Configuring

Edit `~/.config/clidecor/config.conf`:

```
show_os=1
show_kernel=1
show_uptime=1
show_packages=1
show_shell=1
show_cpu=1
show_gpu=1
show_memory=1
show_disk=1
show_battery=1
show_localip=1

custom_text=Welcome back, boss.

image_path=
image_style=color
image_width=28
```

- Set any `show_x=0` (or delete/comment the line) to hide that field.
- `custom_text` is any line(s) you want printed at the bottom.
- `image_path` — point this at a `.png`/`.jpg` to use it as your logo instead of the
  default ASCII art. Leave blank for the default logo.
- `image_style` — `color` for full-color half-block rendering (recommended, looks like
  real pixel art in the terminal), or `ascii` for classic character-density art.
- `image_width` — how many terminal columns wide the logo should be. Smaller terminals
  need a smaller number (try 20-24 for narrow windows, 28-36 for wide ones).

## Manual run

```bash
bash ~/.config/clidecor/clidecor.sh
```

## Uninstall

Remove the `# CLI DECOR` block from your `.bashrc`/`.zshrc`, then:

```bash
rm -rf ~/.config/clidecor
```

## How it works (short version)

- `clidecor.sh` gathers system info using standard Linux commands/files
  (`/etc/os-release`, `/proc/cpuinfo`, `/proc/meminfo`, `uname`, `df`, etc.) —
  same category of sources neofetch itself uses.
- If `image_path` is set, it shells out to `src/imgrender.py`, which uses Pillow to
  resize the image and print it as colored terminal blocks using the half-block
  trick (▀ character with separate foreground/background truecolor ANSI codes —
  each character cell shows 2 image pixels stacked vertically).
- The two blocks (logo + info) are then printed side by side.

No network calls, no telemetry, nothing runs except what's in these two files.
