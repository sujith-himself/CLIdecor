#!/bin/bash
# CLI DECOR - a neofetch-style system info tool
# Reads config.conf next to this script (or ~/.config/clidecor/config.conf)

if [ "$1" = "--refresh" ]; then
    rm -f "$HOME/.cache/clidecor/"img_*.cache 2>/dev/null
    echo "Cache cleared."
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f "$HOME/.config/clidecor/config.conf" ]; then
    CONFIG="$HOME/.config/clidecor/config.conf"
else
    CONFIG="$SCRIPT_DIR/config.conf"
fi

# --- load config (key=value lines, ignore # comments/blank lines) ---
# IFS= + splitting on first '=' only so values that contain '=' are preserved
declare -A CFG
while IFS= read -r line; do
    [[ "$line" =~ ^[[:space:]]*# || -z "${line// }" ]] && continue
    key="${line%%=*}"
    value="${line#*=}"
    key="$(printf '%s' "$key" | xargs)"
    value="$(printf '%s' "$value" | xargs)"
    [ -n "$key" ] && CFG["$key"]="$value"
done < "$CONFIG"

# --- color codes & accent ---
BOLD='\033[1m'
RESET='\033[0m'
ACCENT_COLOR="${CFG[accent_color]:-cyan}"
case "$ACCENT_COLOR" in
    red) AC='\033[31m' ;;
    green) AC='\033[32m' ;;
    yellow) AC='\033[33m' ;;
    blue) AC='\033[34m' ;;
    magenta) AC='\033[35m' ;;
    cyan) AC='\033[36m' ;;
    white) AC='\033[37m' ;;
    *) AC='\033[36m' ;;
esac

USER_HOST="${USER}@$(hostname)"
lines=()

get_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$PRETTY_NAME"
    else
        uname -s
    fi
}

get_kernel() {
    uname -r
}

get_uptime() {
    if command -v uptime >/dev/null; then
        # uptime -p is GNU-only; fall back to parsing raw uptime output
        uptime -p 2>/dev/null | sed 's/^up //' || \
        uptime 2>/dev/null | sed 's/.*up \([^,]*\).*/\1/' | xargs
    fi
}

get_packages() {
    if command -v dpkg >/dev/null; then
        dpkg -l 2>/dev/null | grep -c '^ii' 
    elif command -v rpm >/dev/null; then
        rpm -qa 2>/dev/null | wc -l
    elif command -v pacman >/dev/null; then
        pacman -Q 2>/dev/null | wc -l
    else
        echo "unknown"
    fi
}

get_shell() {
    basename "$SHELL"
}

get_terminal() {
    local term="${TERM_PROGRAM:-$TERM}"
    [ -n "$XTERM_VERSION" ] && term="xterm"
    echo "$term"
}

get_resolution() {
    if command -v xdpyinfo >/dev/null 2>&1; then
        xdpyinfo 2>/dev/null | grep dimensions | awk '{print $2}'
    elif command -v xrandr >/dev/null 2>&1; then
        xrandr 2>/dev/null | grep '\*' | head -1 | awk '{print $1}'
    fi
}

get_cpu() {
    if [ -f /proc/cpuinfo ]; then
        grep -m1 "model name" /proc/cpuinfo | cut -d: -f2 | xargs
    elif command -v sysctl >/dev/null; then
        sysctl -n machdep.cpu.brand_string
    fi
}

get_gpu() {
    if command -v lspci >/dev/null; then
        local gpus
        gpus=$(lspci 2>/dev/null | grep -i 'vga\|3d\|display')
        local real_gpu
        real_gpu=$(echo "$gpus" | grep -i 'nvidia\|amd\|radeon\|intel\|geforce\|rx\|rtx' | head -1 | cut -d: -f3 | xargs)
        if [ -n "$real_gpu" ]; then
            echo "$real_gpu"
        else
            local virt_gpu
            virt_gpu=$(echo "$gpus" | head -1 | cut -d: -f3 | xargs)
            if [ -n "$virt_gpu" ]; then
                echo "$virt_gpu (virtual)"
            fi
        fi
    fi
}

get_memory() {
    if [ -f /proc/meminfo ]; then
        local total used
        total=$(awk '/MemTotal/{print int($2/1024)}' /proc/meminfo)
        local avail=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
        used=$((total - avail))
        echo "${used}MiB / ${total}MiB"
    fi
}

get_disk() {
    df -h / 2>/dev/null | awk 'NR==2{print $3" / "$2" ("$5")"}'
}

get_battery() {
    local bat_path
    bat_path=$(ls -d /sys/class/power_supply/BAT* 2>/dev/null | head -1)
    if [ -n "$bat_path" ] && [ -f "$bat_path/capacity" ]; then
        echo "$(cat "$bat_path/capacity")% ($(cat "$bat_path/status" 2>/dev/null))"
    fi
}

get_localip() {
    # ip route: modern Linux; hostname -I: older Linux; ifconfig: macOS/BSD
    if command -v ip >/dev/null 2>&1; then
        ip route get 1.1.1.1 2>/dev/null | awk '/src/{for(i=1;i<=NF;i++) if($i=="src") print $(i+1); exit}'
    elif command -v hostname >/dev/null 2>&1 && hostname -I >/dev/null 2>&1; then
        hostname -I 2>/dev/null | awk '{print $1}'
    elif command -v ifconfig >/dev/null 2>&1; then
        ifconfig 2>/dev/null | awk '/inet /{if($2!="127.0.0.1") {print $2; exit}}'
    fi
}

add_line() {
    local label="$1" value="$2"
    [ -n "$value" ] && lines+=("$(printf "${AC}${BOLD}%-12s${RESET} %s" "$label" "$value")")
}

[ "${CFG[show_os]}" = "1" ]         && add_line "OS:"         "$(get_os)"
[ "${CFG[show_kernel]}" = "1" ]     && add_line "Kernel:"     "$(get_kernel)"
[ "${CFG[show_uptime]}" = "1" ]     && add_line "Uptime:"     "$(get_uptime)"
[ "${CFG[show_packages]}" = "1" ]   && add_line "Packages:"   "$(get_packages)"
[ "${CFG[show_shell]}" = "1" ]      && add_line "Shell:"      "$(get_shell)"
[ "${CFG[show_terminal]}" = "1" ]   && add_line "Terminal:"   "$(get_terminal)"
[ "${CFG[show_resolution]}" = "1" ] && add_line "Resolution:" "$(get_resolution)"
[ "${CFG[show_cpu]}" = "1" ]        && add_line "CPU:"        "$(get_cpu)"
[ "${CFG[show_gpu]}" = "1" ]        && add_line "GPU:"        "$(get_gpu)"
[ "${CFG[show_memory]}" = "1" ]     && add_line "Memory:"     "$(get_memory)"
[ "${CFG[show_disk]}" = "1" ]       && add_line "Disk:"       "$(get_disk)"
[ "${CFG[show_battery]}" = "1" ]    && add_line "Battery:"    "$(get_battery)"
[ "${CFG[show_localip]}" = "1" ]    && add_line "Local IP:"   "$(get_localip)"

# --- Color palette bar ---
palette1=""
for c in {40..47}; do
    palette1+="$(printf "\033[${c}m   \033[0m")"
done
palette2=""
for c in {100..107}; do
    palette2+="$(printf "\033[${c}m   \033[0m")"
done
lines+=("")
lines+=("$palette1")
lines+=("$palette2")

# --- default ascii logo (used when no image_path is set) ---
DEFAULT_LOGO=(
"    ___    "
"   /   \\   "
"  | O O |  "
"  |  ^  |  "
"   \\___/   "
"  CLIDECOR "
)

# --- build the right-hand text block (header + info + custom text) ---
text_block=()
text_block+=("$(printf "${AC}${BOLD}%s${RESET}" "$USER_HOST")")
text_block+=("$(printf "${AC}%s${RESET}" "$(printf '%*s' "${#USER_HOST}" '' | tr ' ' '-')")")
for l in "${lines[@]}"; do
    text_block+=("$l")
done
if [ -n "${CFG[custom_text]}" ]; then
    text_block+=("")
    text_block+=("$(printf "${AC}${BOLD}%s${RESET}" "${CFG[custom_text]}")")
fi

# --- build the left-hand logo block ---
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
        if [ "$cached_mtime" = "$mtime" ]; then
            use_cache=1
        fi
    fi
    
    if [ "$use_cache" -eq 1 ]; then
        while IFS= read -r rline; do
            logo_block+=("$rline")
        done < <(tail -n +2 "$cache_file")
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

# --- Vertical centering ---
if [ ${#logo_block[@]} -gt ${#text_block[@]} ]; then
    pad=$(( (${#logo_block[@]} - ${#text_block[@]}) / 2 ))
    new_text=()
    for ((i=0; i<pad; i++)); do
        new_text+=("")
    done
    for l in "${text_block[@]}"; do
        new_text+=("$l")
    done
    text_block=("${new_text[@]}")
fi

# --- print side by side ---
max_lines=${#logo_block[@]}
[ ${#text_block[@]} -gt "$max_lines" ] && max_lines=${#text_block[@]}

for ((i=0; i<max_lines; i++)); do
    logo_line="${logo_block[$i]}"
    text_line="${text_block[$i]}"
    printf "%s   %s\n" "$logo_line" "$text_line"
done
