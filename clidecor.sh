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

if [ -f "$HOME/.config/clidecor/config.conf" ]; then
    CONFIG="$HOME/.config/clidecor/config.conf"
else
    CONFIG="$SCRIPT_DIR/config.conf"
fi

declare -A CFG
# Defaults
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
lines=()

get_os() {
    local os=$(uname -s)
    if [ "$os" = "Darwin" ]; then
        if command -v sw_vers >/dev/null; then
            echo "macOS $(sw_vers -productVersion)"
        else
            echo "macOS"
        fi
    elif [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$PRETTY_NAME"
    else
        uname -s
    fi
}

get_kernel() { uname -r; }

get_uptime() {
    if command -v uptime >/dev/null; then
        uptime -p 2>/dev/null | sed 's/^up //' || \
        uptime 2>/dev/null | sed 's/.*up \([^,]*\).*/\1/' | xargs
    fi
}

get_packages() {
    local os=$(uname -s)
    local out=()
    if [ "$os" = "Darwin" ]; then
        if command -v brew >/dev/null; then
            local c=$(brew list 2>/dev/null | wc -l)
            [ "$c" -gt 0 ] && out+=("$c (brew)")
        fi
        if command -v port >/dev/null; then
            local c=$(port installed 2>/dev/null | wc -l)
            [ "$c" -gt 0 ] && out+=("$c (port)")
        fi
    else
        if command -v dpkg >/dev/null; then
            local c=$(dpkg -l 2>/dev/null | grep -c '^ii')
            [ "$c" -gt 0 ] && out+=("$c (dpkg)")
        fi
        if command -v pacman >/dev/null; then
            local c=$(pacman -Q 2>/dev/null | wc -l)
            [ "$c" -gt 0 ] && out+=("$c (pacman)")
        fi
        if command -v rpm >/dev/null; then
            local c=$(rpm -qa 2>/dev/null | wc -l)
            [ "$c" -gt 0 ] && out+=("$c (rpm)")
        fi
        if command -v snap >/dev/null; then
            local c=$(snap list 2>/dev/null | tail -n +2 | wc -l)
            [ "$c" -gt 0 ] && out+=("$c (snap)")
        fi
        if command -v flatpak >/dev/null; then
            local c=$(flatpak list 2>/dev/null | wc -l)
            [ "$c" -gt 0 ] && out+=("$c (flatpak)")
        fi
    fi
    if command -v pip >/dev/null; then
        local c=$(pip list 2>/dev/null | tail -n +3 | wc -l)
        [ "$c" -gt 0 ] && out+=("$c (pip)")
    fi
    
    if [ ${#out[@]} -gt 0 ]; then
        local ifs_save=$IFS
        IFS=', '
        echo "${out[*]}"
        IFS=$ifs_save
    else
        echo "unknown"
    fi
}

get_shell() { basename "$SHELL"; }

get_terminal() {
    if [ -n "$TERM_PROGRAM" ]; then
        echo "$TERM_PROGRAM" | sed 's/iTerm.app/iTerm2/'
    elif [ -n "$TERMINAL" ]; then
        echo "$TERMINAL"
    elif [ -n "$XTERM_VERSION" ]; then
        echo "XTerm"
    elif [[ "$TERM" == *"kitty"* ]]; then
        echo "kitty"
    elif [[ "$TERM" == *"alacritty"* ]]; then
        echo "alacritty"
    else
        local parent
        parent=$(ps -p $PPID -o comm= 2>/dev/null)
        if [[ "$parent" =~ (kitty|alacritty|gnome-terminal|konsole|xterm|tilix|terminator|wezterm|tmux) ]]; then
            if [ "$parent" = "tmux" ]; then
                echo "tmux"
            else
                echo "$parent"
            fi
        elif [ -n "$TERM" ]; then
            echo "$TERM"
        fi
    fi
}

get_resolution() {
    local os=$(uname -s)
    if [ "$os" = "Darwin" ]; then
        system_profiler SPDisplaysDataType 2>/dev/null | awk '/Resolution/{print $2"x"$4; exit}'
    else
        if command -v xdpyinfo >/dev/null 2>&1; then
            xdpyinfo 2>/dev/null | awk '/dimensions/{print $2}'
        elif command -v xrandr >/dev/null 2>&1; then
            xrandr 2>/dev/null | awk '/\*/{print $1; exit}'
        fi
    fi
}

get_cpu() {
    local cpu=""
    local os=$(uname -s)
    if [ "$os" = "Darwin" ]; then
        if command -v sysctl >/dev/null; then
            cpu=$(sysctl -n machdep.cpu.brand_string)
        fi
    elif [ -f /proc/cpuinfo ]; then
        cpu=$(grep -m1 "model name" /proc/cpuinfo | cut -d: -f2 | xargs)
    fi
    if [ -n "$cpu" ]; then
        echo "$cpu" | sed -e 's/(R)//g' -e 's/(TM)//g' -e 's/ CPU//g' -e 's/ Processor//g' -e 's/ Core / /g' -e 's/ with Radeon Graphics//g' | xargs
    fi
}

get_gpu() {
    local os=$(uname -s)
    if [ "$os" = "Darwin" ]; then
        system_profiler SPDisplaysDataType 2>/dev/null | awk -F': ' '/Chipset Model/{print $2; exit}'
        return
    fi
    if command -v lspci >/dev/null; then
        local gpus
        gpus=$(lspci 2>/dev/null | grep -i 'vga\|3d\|display')
        local real_gpu
        real_gpu=$(echo "$gpus" | grep -i 'nvidia\|amd\|radeon\|intel\|geforce\|rx\|rtx' | head -1 | cut -d: -f3 | xargs)
        if [ -n "$real_gpu" ]; then
            echo "$real_gpu"
        else
            local virt_gpu
            virt_gpu=$(echo "$gpus" | grep -i 'vmware\|virtualbox\|svga\|bochs\|qxl' | head -1 | cut -d: -f3 | xargs)
            if [ -n "$virt_gpu" ]; then
                echo "$virt_gpu"
            fi
        fi
    fi
}

build_bar() {
    local pct=$1
    local filled=$(( pct * 16 / 100 ))
    local empty=$(( 16 - filled ))
    local bar="["
    bar+="${AC}"
    for ((i=0; i<filled; i++)); do bar+="█"; done
    bar+="${RESET}${VAL_COLOR}"
    for ((i=0; i<empty; i++)); do bar+="░"; done
    bar+="] $pct%"
    echo "$bar"
}

get_cpu_usage() {
    local os=$(uname -s)
    local pct=0
    if [ "$os" = "Darwin" ]; then
        pct=$(top -l 1 | awk '/CPU usage/ {print int($3)}')
    elif [ -f /proc/stat ]; then
        local cpu1=($(head -n1 /proc/stat))
        local idle1=${cpu1[4]}
        local total1=0
        for val in "${cpu1[@]:1}"; do total1=$((total1 + val)); done
        sleep 0.1
        local cpu2=($(head -n1 /proc/stat))
        local idle2=${cpu2[4]}
        local total2=0
        for val in "${cpu2[@]:1}"; do total2=$((total2 + val)); done
        local idle_diff=$((idle2 - idle1))
        local total_diff=$((total2 - total1))
        if [ "$total_diff" -gt 0 ]; then
            pct=$(( 100 * (total_diff - idle_diff) / total_diff ))
        fi
    fi
    echo "$pct"
}

get_cpu_display() {
    local cpu=$(get_cpu)
    if [ "${CFG[show_bars]}" = "1" ]; then
        local pct=$(get_cpu_usage)
        cpu+=" $(build_bar $pct)"
    fi
    echo "$cpu"
}

get_memory() {
    local os=$(uname -s)
    local pct=0 total_mb=0 used_mb=0
    if [ "$os" = "Darwin" ]; then
        local total_bytes=$(sysctl -n hw.memsize 2>/dev/null)
        total_mb=$(( total_bytes / 1024 / 1024 ))
        local pages=$(vm_stat 2>/dev/null | awk '/Pages active/{print $3}' | tr -d '.')
        used_mb=$(( pages * 4096 / 1024 / 1024 ))
    elif [ -f /proc/meminfo ]; then
        total_mb=$(awk '/MemTotal/{print int($2/1024)}' /proc/meminfo)
        local avail=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
        used_mb=$((total_mb - avail))
    fi
    if [ "$total_mb" -gt 0 ]; then
        pct=$(( used_mb * 100 / total_mb ))
        local val="${used_mb}MiB / ${total_mb}MiB"
        if [ "${CFG[show_bars]}" = "1" ]; then
            val+=" $(build_bar $pct)"
        fi
        echo "$val"
    fi
}

get_disk() { df -h / 2>/dev/null | awk 'NR==2{print $3" / "$2" ("$5")"}'; }

get_battery() {
    local os=$(uname -s)
    if [ "$os" = "Darwin" ]; then
        local pct=$(pmset -g batt | grep -Eo '[0-9]+%' | head -1)
        local stat=$(pmset -g batt | grep -o 'discharging\|charging\|charged' | head -1)
        if [ -n "$pct" ]; then
            echo "$pct ($stat)"
        fi
    else
        local bat_path
        bat_path=$(ls -d /sys/class/power_supply/BAT* 2>/dev/null | head -1)
        if [ -n "$bat_path" ] && [ -f "$bat_path/capacity" ]; then
            echo "$(cat "$bat_path/capacity")% ($(cat "$bat_path/status" 2>/dev/null))"
        fi
    fi
}

get_localip() {
    local os=$(uname -s)
    if [ "$os" = "Darwin" ]; then
        ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null
    else
        if command -v ip >/dev/null 2>&1; then
            ip route get 1.1.1.1 2>/dev/null | awk '/src/{for(i=1;i<=NF;i++) if($i=="src") print $(i+1); exit}'
        elif command -v hostname >/dev/null 2>&1 && hostname -I >/dev/null 2>&1; then
            hostname -I 2>/dev/null | awk '{print $1}'
        elif command -v ifconfig >/dev/null 2>&1; then
            ifconfig 2>/dev/null | awk '/inet /{if($2!="127.0.0.1") {print $2; exit}}'
        fi
    fi
}

get_publicip() {
    if command -v curl >/dev/null; then
        curl -s --max-time 3 https://api.ipify.org 2>/dev/null
    fi
}

get_weather() {
    if command -v curl >/dev/null; then
        local loc="${CFG[weather_location]}"
        curl -s --max-time 2 "wttr.in/${loc}?format=3" 2>/dev/null
    fi
}

get_git() {
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        local branch=$(git branch --show-current 2>/dev/null)
        local dirty=$(git status --porcelain 2>/dev/null | wc -l)
        if [ "$dirty" -gt 0 ]; then
            echo "$branch * (dirty)"
        else
            echo "$branch ✓ (clean)"
        fi
    fi
}

add_line() {
    local label="$1" value="$2"
    [ -n "$value" ] && lines+=("$(printf "${AC}${BOLD}%-12s${RESET} ${VAL_COLOR}%s${RESET}" "$label" "$value")")
}

[ "${CFG[show_os]}" = "1" ]         && add_line "OS:"         "$(get_os)"
[ "${CFG[show_kernel]}" = "1" ]     && add_line "Kernel:"     "$(get_kernel)"
[ "${CFG[show_uptime]}" = "1" ]     && add_line "Uptime:"     "$(get_uptime)"
[ "${CFG[show_packages]}" = "1" ]   && add_line "Packages:"   "$(get_packages)"
[ "${CFG[show_shell]}" = "1" ]      && add_line "Shell:"      "$(get_shell)"
[ "${CFG[show_terminal]}" = "1" ]   && add_line "Terminal:"   "$(get_terminal)"
[ "${CFG[show_resolution]}" = "1" ] && add_line "Resolution:" "$(get_resolution)"
[ "${CFG[show_cpu]}" = "1" ]        && add_line "CPU:"        "$(get_cpu_display)"
[ "${CFG[show_gpu]}" = "1" ]        && add_line "GPU:"        "$(get_gpu)"
[ "${CFG[show_memory]}" = "1" ]     && add_line "Memory:"     "$(get_memory)"
[ "${CFG[show_disk]}" = "1" ]       && add_line "Disk:"       "$(get_disk)"
[ "${CFG[show_battery]}" = "1" ]    && add_line "Battery:"    "$(get_battery)"
[ "${CFG[show_localip]}" = "1" ]    && add_line "Local IP:"   "$(get_localip)"

if [ "${CFG[show_publicip]}" = "1" ]; then
    pub_ip=$(get_publicip)
    [ -n "$pub_ip" ] && add_line "Public IP:" "$pub_ip"
fi
if [ "${CFG[show_weather]}" = "1" ]; then
    weather=$(get_weather)
    [ -n "$weather" ] && add_line "Weather:" "$weather"
fi
if [ "${CFG[show_git]}" = "1" ]; then
    git_val=$(get_git)
    [ -n "$git_val" ] && add_line "Git:" "$git_val"
fi

if [ "${CFG[show_palette]}" = "1" ]; then
    lines+=("")
    palette1=""
    palette2=""
    
    std_colors=("0;0;0" "170;0;0" "0;170;0" "170;170;0" "0;0;170" "170;0;170" "0;170;170" "170;170;170")
    for c in "${std_colors[@]}"; do
        palette1+="$(printf "\033[48;2;%sm   \033[0m" "$c")"
    done
    
    brt_colors=("85;85;85" "255;85;85" "85;255;85" "255;255;85" "85;85;255" "255;85;255" "85;255;255" "255;255;255")
    for c in "${brt_colors[@]}"; do
        palette2+="$(printf "\033[48;2;%sm   \033[0m" "$c")"
    done
    
    lines+=("$palette1")
    lines+=("$palette2")
fi

DEFAULT_LOGO=(
"    ___    "
"   /   \\   "
"  | O O |  "
"  |  ^  |  "
"   \\___/   "
"  CLIDECOR "
)

text_block=()
text_block+=("$(printf "${AC}${BOLD}%s${RESET}" "$USER_HOST")")
text_block+=("$(printf "${AC}%s${RESET}" "$(printf '%*s' "${#USER_HOST}" '' | tr ' ' '-')")")
for l in "${lines[@]}"; do
    text_block+=("$l")
done

if [ -n "${CFG[custom_text]}" ]; then
    text_block+=("")
    IFS='|' read -ra texts <<< "${CFG[custom_text]}"
    num_texts=${#texts[@]}
    if [ "$num_texts" -gt 0 ]; then
        idx=$(( RANDOM % num_texts ))
        chosen_text="${texts[$idx]}"
        text_block+=("$(printf "${AC}${BOLD}%s${RESET}" "$chosen_text")")
    fi
fi

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

max_lines=${#logo_block[@]}
[ ${#text_block[@]} -gt "$max_lines" ] && max_lines=${#text_block[@]}

pad_logo=$(( (max_lines - ${#logo_block[@]}) / 2 ))
pad_text=$(( (max_lines - ${#text_block[@]}) / 2 ))

new_logo=()
for ((i=0; i<pad_logo; i++)); do new_logo+=("$(printf "%*s" "$img_width" "")"); done
for l in "${logo_block[@]}"; do new_logo+=("$l"); done
logo_block=("${new_logo[@]}")

new_text=()
for ((i=0; i<pad_text; i++)); do new_text+=(""); done
for l in "${text_block[@]}"; do new_text+=("$l"); done
text_block=("${new_text[@]}")

max_lines=${#logo_block[@]}
[ ${#text_block[@]} -gt "$max_lines" ] && max_lines=${#text_block[@]}

for ((i=0; i<max_lines; i++)); do
    logo_line="${logo_block[$i]:-$(printf "%*s" "$img_width" "")}"
    text_line="${text_block[$i]}"
    printf "%s   %s\n" "$logo_line" "$text_line"
done
