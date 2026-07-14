#!/bin/bash
# CLI DECOR - a neofetch-style system info tool

if [ "$1" = "--help" ]; then
    echo "CLI DECOR — neofetch replacement"
    echo ""
    echo "Usage:"
    echo "  clidecor.sh              run normally"
    echo "  clidecor.sh --refresh    clear image cache and re-render"
    echo "  clidecor.sh --help       show this message"
    echo ""
    echo "Config: ~/.config/clidecor/config.conf"
    exit 0
fi

if [ "$1" = "--refresh" ]; then
    rm -f "$HOME/.cache/clidecor/"img_*.cache 2>/dev/null
    echo "Cache cleared."
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OS_TYPE=$(uname -s)

if [ -f "$HOME/.config/clidecor/config.conf" ]; then
    CONFIG="$HOME/.config/clidecor/config.conf"
else
    CONFIG="$SCRIPT_DIR/config.conf"
fi

declare -A CFG
CFG[show_os]=1
CFG[show_kernel]=1
CFG[show_uptime]=1
CFG[show_packages]=1
CFG[show_shell]=1
CFG[show_terminal]=1
CFG[show_resolution]=1
CFG[show_cpu]=1
CFG[show_gpu]=1
CFG[show_memory]=1
CFG[show_disk]=1
CFG[show_battery]=0
CFG[show_localip]=1
CFG[show_publicip]=0
CFG[show_weather]=0
CFG[show_git]=1
CFG[show_bars]=1
CFG[show_palette]=1
CFG[theme]="default"
CFG[accent_color]="cyan"
CFG[image_width]=28
CFG[image_style]="color"
CFG[pixel_size]=1

while IFS= read -r line; do
    [[ "$line" =~ ^[[:space:]]*# || -z "${line// }" ]] && continue
    key="${line%%=*}"
    value="${line#*=}"
    key="$(printf '%s' "$key" | xargs)"
    value="$(printf '%s' "$value" | xargs)"
    [ -n "$key" ] && CFG["$key"]="$value"
done < "$CONFIG"

BOLD=$'\033[1m'
RESET=$'\033[0m'
VAL_COLOR=$'\033[37m'

THEME="${CFG[theme]}"
[ -z "$THEME" ] && THEME="default"
case "$THEME" in
    hacker) ACCENT_COLOR="green" ;;
    dracula) ACCENT_COLOR="magenta" ;;
    nord) ACCENT_COLOR="blue" ;;
    fire) ACCENT_COLOR="red" ;;
    gold) ACCENT_COLOR="yellow" ;;
    *) ACCENT_COLOR="${CFG[accent_color]}" ;;
esac
case "$ACCENT_COLOR" in
    red) AC=$'\033[31m' ;;
    green) AC=$'\033[32m' ;;
    yellow) AC=$'\033[33m' ;;
    blue) AC=$'\033[34m' ;;
    magenta) AC=$'\033[35m' ;;
    cyan) AC=$'\033[36m' ;;
    white) AC=$'\033[37m' ;;
    *) AC=$'\033[36m' ;;
esac

USER_HOST="${USER}@$(hostname)"

# ─── Temp dir for parallel results ───────────────────────────────────────────
_TMP=$(mktemp -d)
trap 'rm -rf "$_TMP"' EXIT

# ─── Info functions (each writes its result to a temp file) ──────────────────

_get_os() {
    if [ "$OS_TYPE" = "Darwin" ]; then
        command -v sw_vers >/dev/null && echo "macOS $(sw_vers -productVersion)" || echo "macOS"
    elif [ -f /etc/os-release ]; then
        . /etc/os-release; echo "$PRETTY_NAME"
    else
        uname -s
    fi
}

_get_kernel() { uname -r; }

_get_uptime() {
    uptime -p 2>/dev/null | sed 's/^up //' \
    || uptime 2>/dev/null | sed 's/.*up \([^,]*\).*/\1/' | xargs
}

_get_packages() {
    local out=()
    if [ "$OS_TYPE" = "Darwin" ]; then
        command -v brew >/dev/null && { local c; c=$(brew list 2>/dev/null | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (brew)"); }
        command -v port >/dev/null && { local c; c=$(port installed 2>/dev/null | tail -n +2 | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (port)"); }
    else
        # dpkg-query is faster than dpkg -l | grep
        command -v dpkg-query >/dev/null && { local c; c=$(dpkg-query -f '.\n' -W 2>/dev/null | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (dpkg)"); }
        command -v pacman >/dev/null    && { local c; c=$(pacman -Qq 2>/dev/null | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (pacman)"); }
        command -v rpm >/dev/null       && { local c; c=$(rpm -qa 2>/dev/null | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (rpm)"); }
        command -v snap >/dev/null      && { local c; c=$(snap list 2>/dev/null | tail -n +2 | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (snap)"); }
        command -v flatpak >/dev/null   && { local c; c=$(flatpak list 2>/dev/null | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (flatpak)"); }
    fi
    # pip: use freeze (much faster than pip list)
    command -v pip >/dev/null && { local c; c=$(pip list --format=freeze 2>/dev/null | wc -l | tr -d ' '); [ "$c" -gt 0 ] && out+=("$c (pip)"); }
    if [ ${#out[@]} -gt 0 ]; then
        local IFS=', '; echo "${out[*]}"
    else
        echo "unknown"
    fi
}

_get_shell()    { basename "$SHELL"; }

_get_terminal() {
    if   [ -n "$TERM_PROGRAM" ]; then echo "$TERM_PROGRAM" | sed 's/iTerm.app/iTerm2/'
    elif [ -n "$TERMINAL" ];     then echo "$TERMINAL"
    elif [ -n "$XTERM_VERSION" ];then echo "XTerm"
    elif [[ "$TERM" == *kitty* ]];    then echo "kitty"
    elif [[ "$TERM" == *alacritty* ]];then echo "alacritty"
    else
        local p; p=$(ps -p $PPID -o comm= 2>/dev/null)
        case "$p" in
            kitty|alacritty|gnome-terminal|konsole|xterm|tilix|terminator|wezterm) echo "$p" ;;
            tmux*) echo "tmux" ;;
            *) echo "${TERM:-unknown}" ;;
        esac
    fi
}

_get_resolution() {
    if [ "$OS_TYPE" = "Darwin" ]; then
        system_profiler SPDisplaysDataType 2>/dev/null | awk '/Resolution/{print $2"x"$4; exit}'
    elif command -v xdpyinfo >/dev/null 2>&1; then
        xdpyinfo 2>/dev/null | awk '/dimensions/{print $2}'
    elif command -v xrandr >/dev/null 2>&1; then
        xrandr 2>/dev/null | awk '/\*/{print $1; exit}'
    fi
}

_get_cpu() {
    local cpu=""
    if [ "$OS_TYPE" = "Darwin" ]; then
        cpu=$(sysctl -n machdep.cpu.brand_string 2>/dev/null)
    elif [ -f /proc/cpuinfo ]; then
        cpu=$(grep -m1 "model name" /proc/cpuinfo | cut -d: -f2 | xargs)
    fi
    [ -n "$cpu" ] && echo "$cpu" \
        | sed -e 's/(R)//g' -e 's/(TM)//g' -e 's/ CPU//g' \
              -e 's/ Processor//g' -e 's/ Core / /g' \
              -e 's/ with Radeon Graphics//g' \
        | xargs
}

_get_cpu_usage() {
    # Single-sample instant idle ratio (no sleep) — fast approximation
    if [ "$OS_TYPE" = "Darwin" ]; then
        top -l 1 2>/dev/null | awk '/CPU usage/{gsub(/%/,"",$3); print int($3)}'
        return
    fi
    [ -f /proc/stat ] || { echo 0; return; }
    local cpu; cpu=($(head -n1 /proc/stat))
    local idle=${cpu[4]}
    local total=0; local v
    for v in "${cpu[@]:1}"; do total=$((total + v)); done
    # brief sleep so delta is meaningful (runs in background so doesn't block)
    sleep 0.15
    local cpu2; cpu2=($(head -n1 /proc/stat))
    local idle2=${cpu2[4]}
    local total2=0
    for v in "${cpu2[@]:1}"; do total2=$((total2 + v)); done
    local di=$((idle2 - idle)) dt=$((total2 - total))
    [ "$dt" -gt 0 ] && echo $(( 100 * (dt - di) / dt )) || echo 0
}

_get_gpu() {
    if [ "$OS_TYPE" = "Darwin" ]; then
        system_profiler SPDisplaysDataType 2>/dev/null | awk -F': ' '/Chipset Model/{print $2; exit}'
        return
    fi
    command -v lspci >/dev/null || return
    local gpus; gpus=$(lspci 2>/dev/null | grep -i 'vga\|3d\|display')
    local real; real=$(echo "$gpus" | grep -i 'nvidia\|amd\|radeon\|intel\|geforce\|rx\|rtx' | head -1 | cut -d: -f3 | xargs)
    if [ -n "$real" ]; then echo "$real"
    else
        local virt; virt=$(echo "$gpus" | grep -i 'vmware\|virtualbox\|svga\|bochs\|qxl' | head -1 | cut -d: -f3 | xargs)
        [ -n "$virt" ] && echo "$virt"
    fi
}

_get_memory_raw() {
    # Writes: used_mb total_mb
    if [ "$OS_TYPE" = "Darwin" ]; then
        local tb; tb=$(sysctl -n hw.memsize 2>/dev/null)
        local total_mb=$(( tb / 1024 / 1024 ))
        local pages; pages=$(vm_stat 2>/dev/null | awk '/Pages active/{print $3}' | tr -d '.')
        local used_mb=$(( pages * 4096 / 1024 / 1024 ))
        echo "$used_mb $total_mb"
    elif [ -f /proc/meminfo ]; then
        local total_mb; total_mb=$(awk '/MemTotal/{print int($2/1024)}' /proc/meminfo)
        local avail; avail=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
        echo "$((total_mb - avail)) $total_mb"
    fi
}

_get_disk() { df -h / 2>/dev/null | awk 'NR==2{print $3" / "$2" ("$5")"}'; }

_get_battery() {
    if [ "$OS_TYPE" = "Darwin" ]; then
        local pct stat
        pct=$(pmset -g batt 2>/dev/null | grep -Eo '[0-9]+%' | head -1)
        stat=$(pmset -g batt 2>/dev/null | grep -o 'discharging\|charging\|charged' | head -1)
        [ -n "$pct" ] && echo "$pct ($stat)"
    else
        local bp; bp=$(ls -d /sys/class/power_supply/BAT* 2>/dev/null | head -1)
        [ -n "$bp" ] && [ -f "$bp/capacity" ] && echo "$(cat "$bp/capacity")% ($(cat "$bp/status" 2>/dev/null))"
    fi
}

_get_localip() {
    if [ "$OS_TYPE" = "Darwin" ]; then
        ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null
    elif command -v ip >/dev/null 2>&1; then
        ip route get 1.1.1.1 2>/dev/null | awk '/src/{for(i=1;i<=NF;i++) if($i=="src") print $(i+1); exit}'
    elif hostname -I >/dev/null 2>&1; then
        hostname -I 2>/dev/null | awk '{print $1}'
    elif command -v ifconfig >/dev/null 2>&1; then
        ifconfig 2>/dev/null | awk '/inet /{if($2!="127.0.0.1"){print $2;exit}}'
    fi
}

_get_publicip() {
    command -v curl >/dev/null && curl -s --max-time 3 https://api.ipify.org 2>/dev/null
}

_get_weather() {
    command -v curl >/dev/null || return
    local loc="${CFG[weather_location]}"
    curl -s --max-time 2 "wttr.in/${loc}?format=3" 2>/dev/null
}

_get_git() {
    git rev-parse --is-inside-work-tree >/dev/null 2>&1 || return
    local branch; branch=$(git branch --show-current 2>/dev/null)
    # Use --short for faster status check
    local dirty; dirty=$(git status --short 2>/dev/null | wc -l | tr -d ' ')
    if [ "$dirty" -gt 0 ]; then echo "$branch * (dirty)"
    else echo "$branch ✓ (clean)"
    fi
}

# ─── Bar builder ─────────────────────────────────────────────────────────────
build_bar() {
    local pct=$1
    local filled=$(( pct * 16 / 100 ))
    local empty=$(( 16 - filled ))
    local bar="[${AC}"
    local i; for ((i=0; i<filled; i++)); do bar+="█"; done
    bar+="${RESET}${VAL_COLOR}"
    for ((i=0; i<empty; i++)); do bar+="░"; done
    bar+="] $pct%"
    printf '%s' "$bar"
}

# ─── Kick off all slow getters in PARALLEL ───────────────────────────────────
[ "${CFG[show_os]}" = "1" ]         && { _get_os         > "$_TMP/os"         & }
[ "${CFG[show_kernel]}" = "1" ]     && { _get_kernel      > "$_TMP/kernel"     & }
[ "${CFG[show_uptime]}" = "1" ]     && { _get_uptime      > "$_TMP/uptime"     & }
[ "${CFG[show_packages]}" = "1" ]   && { _get_packages    > "$_TMP/packages"   & }
[ "${CFG[show_terminal]}" = "1" ]   && { _get_terminal    > "$_TMP/terminal"   & }
[ "${CFG[show_resolution]}" = "1" ] && { _get_resolution  > "$_TMP/resolution" & }
[ "${CFG[show_cpu]}" = "1" ]        && { _get_cpu         > "$_TMP/cpu"        & }
[ "${CFG[show_bars]}" = "1" ]       && { _get_cpu_usage   > "$_TMP/cpu_usage"  & }
[ "${CFG[show_gpu]}" = "1" ]        && { _get_gpu         > "$_TMP/gpu"        & }
[ "${CFG[show_disk]}" = "1" ]       && { _get_disk        > "$_TMP/disk"       & }
[ "${CFG[show_battery]}" = "1" ]    && { _get_battery     > "$_TMP/battery"    & }
[ "${CFG[show_localip]}" = "1" ]    && { _get_localip     > "$_TMP/localip"    & }
[ "${CFG[show_publicip]}" = "1" ]   && { _get_publicip    > "$_TMP/publicip"   & }
[ "${CFG[show_weather]}" = "1" ]    && { _get_weather     > "$_TMP/weather"    & }
[ "${CFG[show_git]}" = "1" ]        && { _get_git         > "$_TMP/git"        & }

# Memory raw (used + total) for bar calculation
[ "${CFG[show_memory]}" = "1" ]     && { _get_memory_raw  > "$_TMP/memraw"     & }

# Wait for all background jobs to finish
wait

# ─── Read results & build line list ──────────────────────────────────────────
_r() { [ -f "$_TMP/$1" ] && cat "$_TMP/$1" || true; }

add_line() {
    local label="$1" value="$2"
    [ -n "$value" ] && lines+=("$(printf "${AC}${BOLD}%-12s${RESET} ${VAL_COLOR}%s${RESET}" "$label" "$value")")
}

lines=()

[ "${CFG[show_os]}" = "1" ]         && add_line "OS:"         "$(_r os)"
[ "${CFG[show_kernel]}" = "1" ]     && add_line "Kernel:"     "$(_r kernel)"
[ "${CFG[show_uptime]}" = "1" ]     && add_line "Uptime:"     "$(_r uptime)"
[ "${CFG[show_packages]}" = "1" ]   && add_line "Packages:"   "$(_r packages)"
[ "${CFG[show_shell]}" = "1" ]      && add_line "Shell:"      "$(basename "$SHELL")"
[ "${CFG[show_terminal]}" = "1" ]   && add_line "Terminal:"   "$(_r terminal)"
[ "${CFG[show_resolution]}" = "1" ] && add_line "Resolution:" "$(_r resolution)"

# CPU + bar
if [ "${CFG[show_cpu]}" = "1" ]; then
    cpu_str=$(_r cpu)
    if [ "${CFG[show_bars]}" = "1" ] && [ -n "$cpu_str" ]; then
        pct=$(_r cpu_usage); pct=${pct:-0}
        cpu_str+=" $(build_bar "$pct")"
    fi
    add_line "CPU:" "$cpu_str"
fi

[ "${CFG[show_gpu]}" = "1" ]        && add_line "GPU:"        "$(_r gpu)"

# Memory + bar
if [ "${CFG[show_memory]}" = "1" ]; then
    read -r used_mb total_mb <<< "$(_r memraw)"
    if [ -n "$total_mb" ] && [ "$total_mb" -gt 0 ] 2>/dev/null; then
        pct=$(( used_mb * 100 / total_mb ))
        mem_str="${used_mb}MiB / ${total_mb}MiB"
        [ "${CFG[show_bars]}" = "1" ] && mem_str+=" $(build_bar "$pct")"
        add_line "Memory:" "$mem_str"
    fi
fi

[ "${CFG[show_disk]}" = "1" ]       && add_line "Disk:"       "$(_r disk)"
[ "${CFG[show_battery]}" = "1" ]    && add_line "Battery:"    "$(_r battery)"
[ "${CFG[show_localip]}" = "1" ]    && add_line "Local IP:"   "$(_r localip)"

v=$(_r publicip); [ "${CFG[show_publicip]}" = "1" ] && [ -n "$v" ] && add_line "Public IP:" "$v"
v=$(_r weather);  [ "${CFG[show_weather]}" = "1" ]  && [ -n "$v" ] && add_line "Weather:"   "$v"
v=$(_r git);      [ "${CFG[show_git]}" = "1" ]      && [ -n "$v" ] && add_line "Git:"       "$v"

# ─── Palette bar ─────────────────────────────────────────────────────────────
if [ "${CFG[show_palette]}" = "1" ]; then
    lines+=("")
    p1="" p2=""
    for c in "0;0;0" "170;0;0" "0;170;0" "170;170;0" "0;0;170" "170;0;170" "0;170;170" "170;170;170"; do
        p1+="$(printf "\033[48;2;%sm   \033[0m" "$c")"
    done
    for c in "85;85;85" "255;85;85" "85;255;85" "255;255;85" "85;85;255" "255;85;255" "85;255;255" "255;255;255"; do
        p2+="$(printf "\033[48;2;%sm   \033[0m" "$c")"
    done
    lines+=("$p1")
    lines+=("$p2")
fi

# ─── Default logo ─────────────────────────────────────────────────────────────
DEFAULT_LOGO=(
"    ___    "
"   /   \\   "
"  | O O |  "
"  |  ^  |  "
"   \\___/   "
"  CLIDECOR "
)

# ─── Text block ───────────────────────────────────────────────────────────────
text_block=()
text_block+=("$(printf "${AC}${BOLD}%s${RESET}" "$USER_HOST")")
text_block+=("$(printf "${AC}%s${RESET}" "$(printf '%*s' "${#USER_HOST}" '' | tr ' ' '-')")")
for l in "${lines[@]}"; do text_block+=("$l"); done

if [ -n "${CFG[custom_text]}" ]; then
    text_block+=("")
    IFS='|' read -ra _texts <<< "${CFG[custom_text]}"
    _n=${#_texts[@]}
    if [ "$_n" -gt 0 ]; then
        _i=$(( RANDOM % _n ))
        text_block+=("$(printf "${AC}${BOLD}%s${RESET}" "${_texts[$_i]}")")
    fi
fi

# ─── Logo block ───────────────────────────────────────────────────────────────
logo_block=()
img_width="${CFG[image_width]:-28}"
img_style="${CFG[image_style]:-color}"
img_pixel_size="${CFG[pixel_size]:-1}"

if [ -n "${CFG[image_path]}" ] && [ -f "${CFG[image_path]}" ]; then
    mkdir -p "$HOME/.cache/clidecor"
    base=$(basename "${CFG[image_path]}")
    mtime=$(stat -c %Y "${CFG[image_path]}" 2>/dev/null || stat -f %m "${CFG[image_path]}" 2>/dev/null)
    cache_file="$HOME/.cache/clidecor/img_${base}_${img_width}_${img_style}_${img_pixel_size}.cache"
    use_cache=0
    if [ -f "$cache_file" ]; then
        cached_mtime=$(head -n 1 "$cache_file")
        [ "$cached_mtime" = "$mtime" ] && use_cache=1
    fi
    if [ "$use_cache" -eq 1 ]; then
        while IFS= read -r rline; do logo_block+=("$rline"); done < <(tail -n +2 "$cache_file")
    else
        echo "$mtime" > "$cache_file"
        while IFS= read -r rline; do
            logo_block+=("$rline")
            echo "$rline" >> "$cache_file"
        done < <(python3 "$SCRIPT_DIR/src/imgrender.py" "${CFG[image_path]}" "$img_width" "$img_style" "$img_pixel_size" 2>/dev/null)
    fi
fi

if [ "${#logo_block[@]}" -eq 0 ]; then
    for l in "${DEFAULT_LOGO[@]}"; do
        logo_block+=("$(printf "${AC}%s${RESET}" "$l")")
    done
    img_width=11
fi

# ─── Vertical centering & side-by-side print ─────────────────────────────────
max_lines=${#logo_block[@]}
[ ${#text_block[@]} -gt "$max_lines" ] && max_lines=${#text_block[@]}

pad_logo=$(( (max_lines - ${#logo_block[@]}) / 2 ))
pad_text=$(( (max_lines - ${#text_block[@]}) / 2 ))

new_logo=(); for ((i=0; i<pad_logo; i++)); do new_logo+=("$(printf "%*s" "$img_width" "")"); done
for l in "${logo_block[@]}"; do new_logo+=("$l"); done
logo_block=("${new_logo[@]}")

new_text=(); for ((i=0; i<pad_text; i++)); do new_text+=(""); done
for l in "${text_block[@]}"; do new_text+=("$l"); done
text_block=("${new_text[@]}")

max_lines=${#logo_block[@]}
[ ${#text_block[@]} -gt "$max_lines" ] && max_lines=${#text_block[@]}

for ((i=0; i<max_lines; i++)); do
    logo_line="${logo_block[$i]:-$(printf "%*s" "$img_width" "")}"
    printf "%s   %s\n" "$logo_line" "${text_block[$i]}"
done
