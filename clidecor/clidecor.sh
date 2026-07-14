#!/bin/bash
# CLI DECOR - a neofetch-style system info tool
# Reads config.conf next to this script (or ~/.config/clidecor/config.conf)

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

# --- color codes ---
BOLD='\033[1m'
CYAN='\033[36m'
RESET='\033[0m'

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

get_cpu() {
    if [ -f /proc/cpuinfo ]; then
        grep -m1 "model name" /proc/cpuinfo | cut -d: -f2 | xargs
    elif command -v sysctl >/dev/null; then
        sysctl -n machdep.cpu.brand_string
    fi
}

get_gpu() {
    if command -v lspci >/dev/null; then
        lspci 2>/dev/null | grep -i 'vga\|3d\|display' | head -1 | cut -d: -f3 | xargs
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
    [ -n "$value" ] && lines+=("$(printf "${CYAN}${BOLD}%-11s${RESET} %s" "$label" "$value")")
}

[ "${CFG[show_os]}" = "1" ]        && add_line "OS:"       "$(get_os)"
[ "${CFG[show_kernel]}" = "1" ]    && add_line "Kernel:"   "$(get_kernel)"
[ "${CFG[show_uptime]}" = "1" ]    && add_line "Uptime:"   "$(get_uptime)"
[ "${CFG[show_packages]}" = "1" ]  && add_line "Packages:" "$(get_packages)"
[ "${CFG[show_shell]}" = "1" ]     && add_line "Shell:"    "$(get_shell)"
[ "${CFG[show_cpu]}" = "1" ]       && add_line "CPU:"      "$(get_cpu)"
[ "${CFG[show_gpu]}" = "1" ]       && add_line "GPU:"      "$(get_gpu)"
[ "${CFG[show_memory]}" = "1" ]    && add_line "Memory:"   "$(get_memory)"
[ "${CFG[show_disk]}" = "1" ]      && add_line "Disk:"     "$(get_disk)"
[ "${CFG[show_battery]}" = "1" ]   && add_line "Battery:"  "$(get_battery)"
[ "${CFG[show_localip]}" = "1" ]   && add_line "Local IP:" "$(get_localip)"

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
text_block+=("$(printf "${BOLD}%s${RESET}" "$USER_HOST")")
text_block+=("$(printf "${CYAN}%s${RESET}" "$(printf '%*s' "${#USER_HOST}" '' | tr ' ' '-')")")
for l in "${lines[@]}"; do
    text_block+=("$l")
done
if [ -n "${CFG[custom_text]}" ]; then
    text_block+=("")
    text_block+=("${CFG[custom_text]}")
fi

# --- build the left-hand logo block ---
logo_block=()
img_width="${CFG[image_width]:-28}"
img_style="${CFG[image_style]:-color}"
if [ -n "${CFG[image_path]}" ] && [ -f "${CFG[image_path]}" ]; then
    while IFS= read -r rline; do
        logo_block+=("$rline")
    done < <(python3 "$SCRIPT_DIR/src/imgrender.py" "${CFG[image_path]}" "$img_width" "$img_style" 2>/dev/null)
fi
if [ "${#logo_block[@]}" -eq 0 ]; then
    for l in "${DEFAULT_LOGO[@]}"; do
        logo_block+=("$(printf "${CYAN}%s${RESET}" "$l")")
    done
    img_width=11
fi

# --- print side by side ---
max_lines=${#logo_block[@]}
[ ${#text_block[@]} -gt "$max_lines" ] && max_lines=${#text_block[@]}

for ((i=0; i<max_lines; i++)); do
    logo_line="${logo_block[$i]}"
    text_line="${text_block[$i]}"
    printf "%s   %s\n" "$logo_line" "$text_line"
done
