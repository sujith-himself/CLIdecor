#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <lmcons.h>
#include <conio.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <termios.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <tuple>
#include <random>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ─── Utility Helpers ─────────────────────────────────────────────────────────

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string exec(const char* cmd) {
    char buffer[256];
    std::string result = "";
#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

// ─── Config Parser ───────────────────────────────────────────────────────────

struct Config {
    std::unordered_map<std::string, std::string> settings;

    bool get_bool(const std::string& key, bool default_val = true) const {
        auto it = settings.find(key);
        if (it == settings.end()) return default_val;
        std::string val = trim(it->second);
        if (val == "1" || val == "true" || val == "yes" || val == "on") return true;
        if (val == "0" || val == "false" || val == "no" || val == "off") return false;
        return default_val;
    }

    int get_int(const std::string& key, int default_val = 0) const {
        auto it = settings.find(key);
        if (it == settings.end()) return default_val;
        try {
            return std::stoi(trim(it->second));
        } catch (...) {
            return default_val;
        }
    }

    std::string get_string(const std::string& key, const std::string& default_val = "") const {
        auto it = settings.find(key);
        if (it == settings.end()) return default_val;
        return trim(it->second);
    }

    static std::string get_default_config_path() {
        std::string home_dir = "";
#ifdef _WIN32
        const char* userprofile = std::getenv("USERPROFILE");
        if (userprofile) home_dir = userprofile;
#else
        const char* home = std::getenv("HOME");
        if (home) {
            home_dir = home;
        } else {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home_dir = pw->pw_dir;
        }
#endif
        if (!home_dir.empty()) {
            std::string cfg = home_dir + "/.config/clidecor/config.conf";
            std::ifstream f(cfg.c_str());
            if (f.good()) return cfg;
        }
        return "config.conf";
    }

    static Config load_from_file(const std::string& filepath) {
        Config cfg;
        std::ifstream file(filepath.c_str());
        if (!file.is_open()) return cfg;

        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = trim(line.substr(0, eq_pos));
                std::string val = trim(line.substr(eq_pos + 1));
                if (!key.empty()) cfg.settings[key] = val;
            }
        }
        return cfg;
    }

    void save_to_file(const std::string& filepath) const {
        std::ofstream file(filepath.c_str());
        if (!file.is_open()) return;
        file << "# CLI DECOR config\n";
        for (const auto& pair : settings) {
            file << pair.first << "=" << pair.second << "\n";
        }
    }
};

// ─── System Metrics Provider ─────────────────────────────────────────────────

namespace SysInfo {

std::string get_user_host() {
    std::string user = "";
    std::string host = "";
#ifdef _WIN32
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    if (GetUserNameA(username, &username_len)) user = username;

    char hostname[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD hostname_len = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(hostname, &hostname_len)) host = hostname;
#else
    const char* u = std::getenv("USER");
    if (u) user = u;
    else {
        struct passwd* pw = getpwuid(getuid());
        if (pw) user = pw->pw_name;
    }
    char h[256];
    if (gethostname(h, sizeof(h)) == 0) host = h;
#endif
    if (user.empty()) user = "user";
    if (host.empty()) host = "localhost";
    return user + "@" + host;
}

#ifdef _WIN32
#include <winreg.h>

static std::string get_win_registry_string(HKEY hKeyRoot, const char* subKey, const char* valueName) {
    HKEY hKey;
    if (RegOpenKeyExA(hKeyRoot, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "";
    }
    char buffer[256];
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;
    LONG result = RegQueryValueExA(hKey, valueName, NULL, &type, (LPBYTE)buffer, &bufferSize);
    RegCloseKey(hKey);
    if (result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        size_t len = bufferSize;
        if (len > 0 && buffer[len - 1] == '\0') len--;
        return trim(std::string(buffer, len));
    }
    return "";
}
#endif

std::string get_os() {
#ifdef _WIN32
    std::string prod = get_win_registry_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
    std::string disp = get_win_registry_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion");
    if (!prod.empty()) {
        if (!disp.empty()) return prod + " " + disp;
        return prod;
    }
    return "Windows";
#elif defined(__APPLE__)
    std::string ver = trim(exec("sw_vers -productVersion"));
    return ver.empty() ? "macOS" : "macOS " + ver;
#else
    std::ifstream file("/etc/os-release");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("PRETTY_NAME=") == 0) {
                std::string val = line.substr(12);
                if (!val.empty() && val.front() == '"') val = val.substr(1);
                if (!val.empty() && val.back() == '"') val.pop_back();
                return trim(val);
            }
        }
    }
    struct utsname info;
    if (uname(&info) == 0) {
        std::string os = trim(info.sysname);
        std::string arch = trim(info.machine);
        return os + " " + arch;
    }
    return "Linux";
#endif
}

std::string get_host() {
#ifdef _WIN32
    std::string host = get_win_registry_string(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName");
    if (!host.empty()) return host;
    return "Unknown Host";
#else
    std::string product_name = trim(exec("cat /sys/devices/virtual/dmi/id/product_name 2>/dev/null"));
    std::string product_version = trim(exec("cat /sys/devices/virtual/dmi/id/product_version 2>/dev/null"));
    if (!product_name.empty()) {
        if (!product_version.empty() && product_version != "None") return product_name + " " + product_version;
        return product_name;
    }
    return "Unknown Host";
#endif
}

std::string get_kernel() {
#ifdef _WIN32
    std::string build = get_win_registry_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuildNumber");
    if (!build.empty()) return "Version 10.0." + build;
    return "Windows NT";
#else
    struct utsname info;
    if (uname(&info) == 0) return std::string(info.sysname) + " " + trim(info.release);
    return "Unknown";
#endif
}

std::string get_uptime() {
#ifdef _WIN32
    DWORD uptime_ms = GetTickCount();
    long long uptime_sec = uptime_ms / 1000;
#else
    long long uptime_sec = 0;
    std::ifstream file("/proc/uptime");
    if (file.is_open()) {
        file >> uptime_sec;
    } else {
        std::string up_str = exec("uptime -p 2>/dev/null");
        if (!up_str.empty()) return up_str;
    }
#endif
    if (uptime_sec <= 0) return "Unknown";
    long long days = uptime_sec / 86400;
    long long hours = (uptime_sec % 86400) / 3600;
    long long mins = (uptime_sec % 3600) / 60;

    std::ostringstream ss;
    if (days > 0) ss << days << (days == 1 ? " day, " : " days, ");
    if (hours > 0) ss << hours << (hours == 1 ? " hour, " : " hours, ");
    ss << mins << (mins == 1 ? " minute" : " minutes");
    return ss.str();
}

std::string get_packages() {
#ifdef _WIN32
    return "";
#else
    std::vector<std::string> pkgs;
    if (access("/usr/bin/dpkg-query", F_OK) == 0) {
        std::string res = exec("dpkg-query -f '.\\n' -W 2>/dev/null | wc -l");
        if (!res.empty() && res != "0") pkgs.push_back(res + " (dpkg)");
    } else if (access("/usr/bin/pacman", F_OK) == 0) {
        std::string res = exec("pacman -Qq 2>/dev/null | wc -l");
        if (!res.empty() && res != "0") pkgs.push_back(res + " (pacman)");
    } else if (access("/usr/bin/rpm", F_OK) == 0) {
        std::string res = exec("rpm -qa 2>/dev/null | wc -l");
        if (!res.empty() && res != "0") pkgs.push_back(res + " (rpm)");
    }
    if (access("/usr/bin/flatpak", F_OK) == 0) {
        std::string res = exec("flatpak list 2>/dev/null | wc -l");
        if (!res.empty() && res != "0") pkgs.push_back(res + " (flatpak)");
    }
    if (access("/usr/bin/snap", F_OK) == 0) {
        std::string res = exec("snap list 2>/dev/null | tail -n +2 | wc -l");
        if (!res.empty() && res != "0") pkgs.push_back(trim(res) + " (snap)");
    }
    if (pkgs.empty()) return "";
    std::string out = "";
    for (size_t i = 0; i < pkgs.size(); ++i) {
        out += pkgs[i] + (i + 1 < pkgs.size() ? ", " : "");
    }
    return out;
#endif
}

std::string get_shell() {
    std::string shell_name = "bash";
    const char* sh = std::getenv("SHELL");
    if (sh) {
        std::string path(sh);
        size_t idx = path.find_last_of("/\\");
        if (idx != std::string::npos) shell_name = path.substr(idx + 1);
        else shell_name = path;
    }
#ifdef _WIN32
    if (!sh) shell_name = "cmd.exe";
#else
    std::string cmd = shell_name + " --version 2>/dev/null | grep -m 1 -o '[0-9]\\+\\.[0-9]\\+\\.[0-9]\\+' | head -n 1";
    std::string ver = exec(cmd.c_str());
    if (!ver.empty()) return shell_name + " " + trim(ver);
#endif
    return shell_name;
}

std::string get_terminal() {
    std::string term = "";
#ifndef _WIN32
    const char* ssh_tty = std::getenv("SSH_TTY");
    const char* ssh_conn = std::getenv("SSH_CONNECTION");
    if (ssh_tty || ssh_conn) {
        std::string tty = exec("tty 2>/dev/null");
        if (!tty.empty() && tty.find("not a tty") == std::string::npos) {
            term = trim(tty);
            std::string ppid = exec("ps -o ppid= -p $$ 2>/dev/null");
            if (!ppid.empty()) {
                std::string parent = exec(("ps -o comm= -p " + trim(ppid) + " 2>/dev/null").c_str());
                if (!parent.empty() && trim(parent) == "sshd") {
                    std::string sshd_ver = exec("sshd -V 2>&1 | head -n 1 | awk '{print $1}' | sed 's/OpenSSH_//'");
                    if (!sshd_ver.empty()) term += " " + trim(sshd_ver);
                }
            }
            return term;
        }
    }
#endif
    const char* term_prog = std::getenv("TERM_PROGRAM");
    if (term_prog) return term_prog;
    const char* env_term = std::getenv("TERM");
    if (env_term) return env_term;
#ifdef _WIN32
    return "Windows Terminal / Console";
#else
    return "Terminal";
#endif
}

std::string get_resolution() {
#ifdef _WIN32
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    if (w > 0 && h > 0) return std::to_string(w) + "x" + std::to_string(h);
    return "";
#else
    std::string res = exec("xdpyinfo 2>/dev/null | grep dimensions | awk '{print $2}'");
    if (!res.empty()) return res;
    return exec("xrandr 2>/dev/null | awk '/\\*/{print $1; exit}'");
#endif
}

std::string get_cpu() {
#ifdef _WIN32
    std::string cpu = get_win_registry_string(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
    if (!cpu.empty()) {
        std::string remove_list[] = {"(R)", "(TM)", " CPU", " Processor", " Core ", " with Radeon Graphics"};
        for (const auto& s : remove_list) {
            size_t pos = 0;
            while ((pos = cpu.find(s, pos)) != std::string::npos) {
                cpu.erase(pos, s.length());
            }
        }
        cpu = trim(cpu);
        
        HKEY hKey;
        double mhz = 0.0;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD data = 0;
            DWORD dataSize = sizeof(data);
            DWORD type = 0;
            if (RegQueryValueExA(hKey, "~MHz", NULL, &type, (LPBYTE)&data, &dataSize) == ERROR_SUCCESS && type == REG_DWORD) {
                mhz = (double)data;
            }
            RegCloseKey(hKey);
        }
        
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        int cores = sysinfo.dwNumberOfProcessors;
        
        if (cores > 0) cpu += " (" + std::to_string(cores) + ")";
        if (mhz > 0.0) {
            char buf[32];
            snprintf(buf, sizeof(buf), " @ %.2f GHz", mhz / 1000.0);
            cpu += buf;
        }
        return cpu;
    }
    return "Generic CPU";
#elif defined(__APPLE__)
    char buf[128];
    size_t len = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", &buf, &len, NULL, 0) == 0) return trim(buf);
    return "Apple Silicon / Intel";
#else
    std::ifstream file("/proc/cpuinfo");
    if (file.is_open()) {
        std::string line;
        std::string cpu_model = "";
        int cores = 0;
        double mhz = 0.0;
        while (std::getline(file, line)) {
            if (line.find("processor\t") == 0 || line.find("processor ") == 0) {
                cores++;
            } else if (cpu_model.empty() && line.find("model name") == 0) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    cpu_model = trim(line.substr(colon + 1));
                }
            } else if (mhz == 0.0 && line.find("cpu MHz") == 0) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    try { mhz = std::stod(trim(line.substr(colon + 1))); } catch(...) {}
                }
            }
        }
        if (!cpu_model.empty()) {
            std::string remove_list[] = {"(R)", "(TM)", " CPU", " Processor", " Core ", " with Radeon Graphics"};
            for (const auto& s : remove_list) {
                size_t pos = 0;
                while ((pos = cpu_model.find(s, pos)) != std::string::npos) {
                    cpu_model.erase(pos, s.length());
                }
            }
            cpu_model = trim(cpu_model);
            if (cores > 0) cpu_model += " (" + std::to_string(cores) + ")";
            if (mhz > 0.0) {
                char buf[32];
                snprintf(buf, sizeof(buf), " @ %.2f GHz", mhz / 1000.0);
                cpu_model += buf;
            }
            return cpu_model;
        }
    }
    return "Generic CPU";
#endif
}

int get_cpu_usage() {
#ifdef _WIN32
    return 0;
#else
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0;
    std::string line;
    std::getline(file, line);
    if (line.rfind("cpu ", 0) != 0) return 0;

    std::istringstream ss(line);
    std::string cpu_label;
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (ss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
        long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        if (total <= 0) return 0;
        return (int)(((total - idle) * 100) / total);
    }
    return 0;
#endif
}

std::string get_gpu() {
#ifdef _WIN32
    DISPLAY_DEVICEA dd;
    std::memset(&dd, 0, sizeof(dd));
    dd.cb = sizeof(dd);
    if (EnumDisplayDevicesA(NULL, 0, &dd, 0)) {
        std::string card(dd.DeviceString);
        if (!card.empty()) return trim(card);
    }
    return get_win_registry_string(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000", "DriverDesc");
#elif defined(__APPLE__)
    return trim(exec("system_profiler SPDisplaysDataType 2>/dev/null | awk -F': ' '/Chipset Model/{print $2; exit}'"));
#else
    return trim(exec("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | grep -i 'nvidia\\|amd\\|radeon\\|intel\\|geforce' | head -1 | cut -d: -f3"));
#endif
}

std::tuple<long long, long long> get_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        long long total_mb = memInfo.ullTotalPhys / (1024 * 1024);
        long long free_mb = memInfo.ullAvailPhys / (1024 * 1024);
        return {total_mb - free_mb, total_mb};
    }
    return {0, 0};
#elif defined(__APPLE__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t total_bytes = 0;
    size_t len = sizeof(total_bytes);
    sysctl(mib, 2, &total_bytes, &len, NULL, 0);
    long long total_mb = total_bytes / (1024 * 1024);
    return {total_mb / 2, total_mb};
#else
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return {0, 0};
    long long total_kb = 0, avail_kb = 0;
    std::string key;
    long long val;
    std::string unit;
    while (file >> key >> val >> unit) {
        if (key == "MemTotal:") total_kb = val;
        else if (key == "MemAvailable:") avail_kb = val;
    }
    long long total_mb = total_kb / 1024;
    long long used_mb = (total_kb - avail_kb) / 1024;
    return {used_mb, total_mb};
#endif
}

std::string get_disk() {
#ifdef _WIN32
    ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFreeBytes)) {
        double total_gb = (double)totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        double free_gb = (double)totalFreeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        double used_gb = total_gb - free_gb;
        int pct = total_gb > 0 ? (int)((used_gb * 100) / total_gb) : 0;
        char buf[128];
        snprintf(buf, sizeof(buf), "C:\\: %.2f GiB / %.2f GiB (%d%%) - NTFS", used_gb, total_gb, pct);
        return buf;
    }
    return "";
#else
    std::string disk_info = exec("df -B1 | awk '$1 ~ /^\\/dev\\// && $1 !~ /loop/ && $1 !~ /sr/ {print $2, $3, $5, $6; exit}'");
    if (disk_info.empty()) {
        std::string lsblk = exec("lsblk -b -d -n -o SIZE,NAME,TYPE 2>/dev/null | awk '$3==\"disk\" && $2!~/^loop/ && $2!~/^sr/ {print $1, $2; exit}'");
        if (!trim(lsblk).empty()) {
            std::istringstream iss(trim(lsblk));
            long long total = 0;
            std::string name;
            if (iss >> total >> name) {
                double total_gb = total / (1024.0 * 1024.0 * 1024.0);
                char buf[128];
                snprintf(buf, sizeof(buf), "(/dev/%s): 0.00 GiB / %.2f GiB (0%%) - Unmounted", name.c_str(), total_gb);
                return buf;
            }
        }
        disk_info = exec("df -B1 / 2>/dev/null | awk 'NR==2 {print $2, $3, $5, $6}'");
    }
    
    std::istringstream iss(trim(disk_info));
    long long total = 0, used = 0;
    std::string pcent, mount;
    if (iss >> total >> used >> pcent >> mount) {
        double total_gb = total / (1024.0 * 1024.0 * 1024.0);
        double used_gb = used / (1024.0 * 1024.0 * 1024.0);
        char buf[128];
        snprintf(buf, sizeof(buf), "(%s): %.2f GiB / %.2f GiB (%s)", mount.c_str(), used_gb, total_gb, pcent.c_str());
        return buf;
    }
    return "";
#endif
}

std::string get_swap() {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        long long total_page = memInfo.ullTotalPageFile;
        long long total_phys = memInfo.ullTotalPhys;
        if (total_page > total_phys) {
            double swap_total = (double)(total_page - total_phys) / (1024.0 * 1024.0 * 1024.0);
            double swap_free = (double)(memInfo.ullAvailPageFile - memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
            if (swap_free < 0) swap_free = 0;
            double swap_used = swap_total - swap_free;
            char buf[128];
            snprintf(buf, sizeof(buf), "%.2f GiB / %.2f GiB", swap_used, swap_total);
            return buf;
        }
    }
    return "Disabled";
#else
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return "Disabled";
    long long total_kb = 0, free_kb = 0;
    std::string key;
    long long val;
    std::string unit;
    while (file >> key >> val >> unit) {
        if (key == "SwapTotal:") total_kb = val;
        else if (key == "SwapFree:") free_kb = val;
    }
    if (total_kb == 0) return "Disabled";
    double total_gb = total_kb / (1024.0 * 1024.0);
    double used_gb = (total_kb - free_kb) / (1024.0 * 1024.0);
    char buf[128];
    snprintf(buf, sizeof(buf), "%.2f GiB / %.2f GiB", used_gb, total_gb);
    return buf;
#endif
}

std::string get_locale() {
    const char* lc = std::getenv("LC_ALL");
    if (!lc) lc = std::getenv("LANG");
    if (lc) return lc;
#ifdef _WIN32
    return "English_United States.1252";
#else
    return "C";
#endif
}

std::string get_battery() {
#ifdef _WIN32
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        if (status.BatteryFlag != 128 && status.BatteryLifePercent != 255) {
            std::string state = (status.ACLineStatus == 1) ? "charging" : "discharging";
            return std::to_string(status.BatteryLifePercent) + "% (" + state + ")";
        }
    }
    return "";
#else
    std::ifstream cap_file("/sys/class/power_supply/BAT0/capacity");
    if (cap_file.is_open()) {
        int cap;
        cap_file >> cap;
        std::string status = "discharging";
        std::ifstream stat_file("/sys/class/power_supply/BAT0/status");
        if (stat_file.is_open()) stat_file >> status;
        return std::to_string(cap) + "% (" + status + ")";
    }
    return "";
#endif
}

std::string get_local_ip() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            struct hostent* host = gethostbyname(hostname);
            if (host && host->h_addr_list[0]) {
                struct in_addr addr;
                memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
                std::string ip = inet_ntoa(addr);
                WSACleanup();
                return ip;
            }
        }
        WSACleanup();
    }
    return "";
#else
    std::string out = exec("ip -4 route get 1 2>/dev/null | awk '{print $7, $5; exit}'");
    if (!out.empty()) {
        std::istringstream iss(out);
        std::string ip, iface;
        if (iss >> ip >> iface) {
            std::string mask = exec(("ip -o -f inet addr show " + iface + " 2>/dev/null | awk '{print $4}' | cut -d/ -f2").c_str());
            return ip + "/" + trim(mask) + " (" + iface + ")";
        }
    }
    return "127.0.0.1";
#endif
}

std::string get_public_ip() {
    return exec("curl -s --max-time 2 https://api.ipify.org 2>/dev/null");
}

std::string get_weather(const std::string& location) {
    return exec(("curl -s --max-time 2 \"wttr.in/" + location + "?format=3\" 2>/dev/null").c_str());
}

std::string get_git() {
    std::ifstream head_file(".git/HEAD");
    if (!head_file.is_open()) return "";
    std::string line;
    std::getline(head_file, line);
    line = trim(line);
    if (line.empty()) return "";
    std::string branch = line;
    if (line.rfind("ref: refs/heads/", 0) == 0) {
        branch = line.substr(16);
    }
    return branch + " \xe2\x9c\x93 (clean)";
}

} // namespace SysInfo

// ─── Image Rendering Engine ──────────────────────────────────────────────────

namespace ImgRender {

struct RGB {
    unsigned char r, g, b;
};

static const std::string ASCII_RAMP = " .,:;+*?%#@";
static const RGB TERM_BG = {0, 0, 0};

static bool is_bg(const RGB& p, int threshold = 20) {
    return std::abs((int)p.r - (int)TERM_BG.r) <= threshold &&
           std::abs((int)p.g - (int)TERM_BG.g) <= threshold &&
           std::abs((int)p.b - (int)TERM_BG.b) <= threshold;
}

static RGB get_sample(unsigned char* data, int orig_w, int orig_h, int channels, float x_ratio, float y_ratio) {
    int px = std::min((int)(x_ratio), orig_w - 1);
    int py = std::min((int)(y_ratio), orig_h - 1);
    int idx = (py * orig_w + px) * channels;
    unsigned char r = data[idx];
    unsigned char g = data[idx + 1];
    unsigned char b = data[idx + 2];
    unsigned char a = (channels == 4) ? data[idx + 3] : 255;
    if (a < 255) {
        float alpha = a / 255.0f;
        r = (unsigned char)(r * alpha + TERM_BG.r * (1.0f - alpha));
        g = (unsigned char)(g * alpha + TERM_BG.g * (1.0f - alpha));
        b = (unsigned char)(b * alpha + TERM_BG.b * (1.0f - alpha));
    }
    return {r, g, b};
}

std::vector<std::string> render_image(
    const std::string& path,
    int base_width,
    float scale_x = 1.0f,
    float scale_y = 1.0f,
    const std::string& style = "color",
    int block_size = 1,
    int blur_radius = 0,
    int max_h = 0
) {
    std::vector<std::string> out_lines;
    int orig_w, orig_h, channels;
    unsigned char* data = stbi_load(path.c_str(), &orig_w, &orig_h, &channels, 0);
    if (!data) return out_lines;

    if (base_width <= 0) base_width = 28;
    if (block_size < 1) block_size = 1;

    float aspect = (float)orig_h / (float)orig_w;
    int rows_mult = (style == "ascii") ? 1 : 2;
    int base_h = (int)(base_width * aspect * 0.55f * rows_mult);
    
    int width_cols = std::max(1, (int)(base_width * scale_x));
    int target_h = std::max(2, (int)(base_h * scale_y));
    if (rows_mult == 2 && target_h % 2 != 0) target_h += 1;

    std::vector<std::vector<RGB>> grid(target_h, std::vector<RGB>(width_cols));
    for (int y = 0; y < target_h; ++y) {
        for (int x = 0; x < width_cols; ++x) {
            int effective_x = (x / block_size) * block_size;
            int effective_y = (y / block_size) * block_size;
            float rx = ((float)effective_x / (float)width_cols) * orig_w;
            float ry = ((float)effective_y / (float)target_h) * orig_h;
            grid[y][x] = get_sample(data, orig_w, orig_h, channels, rx, ry);
        }
    }
    stbi_image_free(data);

    if (blur_radius > 0) {
        std::vector<std::vector<RGB>> blurred = grid;
        for (int y = 0; y < target_h; ++y) {
            for (int x = 0; x < width_cols; ++x) {
                long long r_tot = 0, g_tot = 0, b_tot = 0;
                int count = 0;
                for (int dy = -blur_radius; dy <= blur_radius; ++dy) {
                    for (int dx = -blur_radius; dx <= blur_radius; ++dx) {
                        int ny = std::clamp(y + dy, 0, target_h - 1);
                        int nx = std::clamp(x + dx, 0, width_cols - 1);
                        r_tot += grid[ny][nx].r;
                        g_tot += grid[ny][nx].g;
                        b_tot += grid[ny][nx].b;
                        count++;
                    }
                }
                blurred[y][x] = {(unsigned char)(r_tot / count), (unsigned char)(g_tot / count), (unsigned char)(b_tot / count)};
            }
        }
        grid = blurred;
    }

    if (style == "ascii") {
        for (int y = 0; y < target_h; ++y) {
            std::string line = "";
            for (int x = 0; x < width_cols; ++x) {
                const RGB& p = grid[y][x];
                float bright = (p.r * 0.299f + p.g * 0.587f + p.b * 0.114f) / 255.0f;
                int idx = (int)(bright * (ASCII_RAMP.length() - 1));
                idx = std::clamp(idx, 0, (int)ASCII_RAMP.length() - 1);
                line += ASCII_RAMP[idx];
            }
            out_lines.push_back(line);
        }
    } else {
        for (int y = 0; y < target_h; y += 2) {
            std::ostringstream ss;
            for (int x = 0; x < width_cols; ++x) {
                RGB top = grid[y][x];
                RGB bot = (y + 1 < target_h) ? grid[y + 1][x] : top;

                bool top_bg = is_bg(top);
                bool bot_bg = is_bg(bot);

                if (top_bg && bot_bg) {
                    ss << " ";
                } else if (top_bg) {
                    ss << "\033[38;2;" << (int)bot.r << ";" << (int)bot.g << ";" << (int)bot.b << "m\xe2\x96\x84\033[0m"; // ▄
                } else if (bot_bg) {
                    ss << "\033[38;2;" << (int)top.r << ";" << (int)top.g << ";" << (int)top.b << "m\xe2\x96\x80\033[0m"; // ▀
                } else {
                    ss << "\033[38;2;" << (int)top.r << ";" << (int)top.g << ";" << (int)top.b << "m"
                       << "\033[48;2;" << (int)bot.r << ";" << (int)bot.g << ";" << (int)bot.b << "m\xe2\x96\x80\033[0m";
                }
            }
            out_lines.push_back(ss.str());
        }
    }
    return out_lines;
}

} // namespace ImgRender

// ─── Main Entry Point ────────────────────────────────────────────────────────

static std::string build_bar(int pct, const std::string& accent_code, const std::string& val_code) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int filled = (pct * 16) / 100;
    int empty = 16 - filled;

    std::string bar = "[" + accent_code;
    for (int i = 0; i < filled; ++i) bar += "\xe2\x96\x88"; // █
    bar += "\033[0m" + val_code;
    for (int i = 0; i < empty; ++i) bar += "\xe2\x96\x91"; // ░
    bar += "] " + std::to_string(pct) + "%";
    return bar;
}

static std::string get_accent_code(const std::string& theme, const std::string& accent_color) {
    std::string color = accent_color;
    if (theme == "hacker") color = "green";
    else if (theme == "dracula") color = "magenta";
    else if (theme == "nord") color = "blue";
    else if (theme == "fire") color = "red";
    else if (theme == "gold") color = "yellow";

    if (color == "red") return "\033[31m";
    if (color == "green") return "\033[32m";
    if (color == "yellow") return "\033[33m";
    if (color == "blue") return "\033[34m";
    if (color == "magenta") return "\033[35m";
    if (color == "cyan") return "\033[36m";
    if (color == "white") return "\033[37m";
    return "\033[36m";
}

std::vector<std::string> get_os_logo(const std::string& os, int& width) {
    std::string os_lower = os;
    for (char& c : os_lower) c = std::tolower(c);
    
    width = 17;
    if (os_lower.find("windows") != std::string::npos) {
        return {
            "     ____  ____  ",
            "    |    ||    | ",
            "    |____||____| ",
            "     ____  ____  ",
            "    |    ||    | ",
            "    |____||____| "
        };
    } else if (os_lower.find("ubuntu") != std::string::npos) {
        return {
            "       _         ",
            "   ---(_)---     ",
            " _/         \\_   ",
            "(_)         (_)  ",
            "  \\         /    ",
            "   ---(_)---     "
        };
    } else if (os_lower.find("arch") != std::string::npos) {
        return {
            "        /\\       ",
            "       /  \\      ",
            "      /    \\     ",
            "     /      \\    ",
            "    /   ,,   \\   ",
            "   /   |  |   \\  ",
            "  /_-''    ''-_\\"
        };
    } else if (os_lower.find("mac") != std::string::npos || os_lower.find("darwin") != std::string::npos) {
        return {
            "       __        ",
            "      /  |       ",
            "    .'   |       ",
            "   /  ___|       ",
            "  |  |           ",
            "  |  |__         ",
            "   \\    `        ",
            "    `.___.'      "
        };
    }
    return {
        "       .--.      ",
        "      |o_o |     ",
        "      |:_/ |     ",
        "     //   \\ \\    ",
        "    (|     | )   ",
        "   /'\\_   _/`\\   ",
        "   \\___)=(___/   "
    };
}

#ifdef _WIN32
int get_keypress() {
    int c = _getch();
    if (c == 0 || c == 224) {
        c = _getch();
        if (c == 72) return 1001; // UP
        if (c == 80) return 1002; // DOWN
        if (c == 75) return 1003; // LEFT
        if (c == 77) return 1004; // RIGHT
    }
    return c;
}
#else
int get_keypress() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    if (ch == 27) { // ESC
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        struct termios t_nb = newt;
        t_nb.c_cc[VMIN] = 0;
        t_nb.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &t_nb);
        int c2 = getchar();
        int c3 = -1;
        if (c2 != EOF) c3 = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        if (c2 == 91 || c2 == 79) {
            if (c3 == 65) return 1001; // UP
            if (c3 == 66) return 1002; // DOWN
            if (c3 == 68) return 1003; // LEFT
            if (c3 == 67) return 1004; // RIGHT
        }
        return 27;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

struct MenuItem {
    std::string label;
    std::string key;
    std::vector<std::string> options;
};

std::vector<std::string> generate_preview(Config& cfg) {
    std::vector<std::string> out;
    std::string theme = cfg.get_string("theme", "default");
    std::string accent_color = cfg.get_string("accent_color", "cyan");
    std::string AC = get_accent_code(theme, accent_color);
    std::string BOLD = "\033[1m";
    std::string RESET = "\033[0m";
    std::string VAL_COLOR = "\033[37m";

    static std::string user_host = SysInfo::get_user_host();
    std::vector<std::pair<std::string, std::string>> info_items;

    if (cfg.get_bool("show_os", true)) {
        static std::string os = SysInfo::get_os();
        if (!os.empty()) info_items.push_back({"OS:", os});
    }
    if (cfg.get_bool("show_host", true)) {
        static std::string host = SysInfo::get_host();
        if (!host.empty()) info_items.push_back({"Host:", host});
    }
    if (cfg.get_bool("show_kernel", true)) {
        static std::string kernel = SysInfo::get_kernel();
        if (!kernel.empty()) info_items.push_back({"Kernel:", kernel});
    }
    if (cfg.get_bool("show_uptime", true)) {
        static std::string uptime = SysInfo::get_uptime();
        if (!uptime.empty()) info_items.push_back({"Uptime:", uptime});
    }
    if (cfg.get_bool("show_packages", true)) {
        static std::string pkgs = SysInfo::get_packages();
        if (!pkgs.empty()) info_items.push_back({"Packages:", pkgs});
    }
    if (cfg.get_bool("show_shell", true)) {
        static std::string sh = SysInfo::get_shell();
        if (!sh.empty()) info_items.push_back({"Shell:", sh});
    }
    if (cfg.get_bool("show_terminal", true)) {
        static std::string term = SysInfo::get_terminal();
        if (!term.empty()) info_items.push_back({"Terminal:", term});
    }
    if (cfg.get_bool("show_resolution", true)) {
        static std::string res = SysInfo::get_resolution();
        if (!res.empty()) info_items.push_back({"Resolution:", res});
    }
    if (cfg.get_bool("show_cpu", true)) {
        static std::string cpu = SysInfo::get_cpu();
        static int pct = SysInfo::get_cpu_usage();
        if (!cpu.empty()) {
            std::string cpu_str = cpu;
            if (cfg.get_bool("show_bars", true) && cfg.get_bool("show_cpu_bar", true)) {
                cpu_str += " " + build_bar(pct, AC, VAL_COLOR);
            }
            info_items.push_back({"CPU:", cpu_str});
        }
    }
    if (cfg.get_bool("show_gpu", true)) {
        static std::string gpu = SysInfo::get_gpu();
        if (!gpu.empty()) info_items.push_back({"GPU:", gpu});
    }
    if (cfg.get_bool("show_memory", true)) {
        static auto mem = SysInfo::get_memory();
        auto [used_mb, total_mb] = mem;
        if (total_mb > 0) {
            int pct = (int)((used_mb * 100) / total_mb);
            std::string mem_str;
            if (total_mb >= 1024) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f GiB / %.2f GiB", used_mb / 1024.0, total_mb / 1024.0);
                mem_str = buf;
            } else {
                mem_str = std::to_string(used_mb) + " MiB / " + std::to_string(total_mb) + " MiB";
            }
            if (cfg.get_bool("show_bars", true)) {
                mem_str += " " + build_bar(pct, AC, VAL_COLOR);
            }
            info_items.push_back({"Memory:", mem_str});
        }
    }
    if (cfg.get_bool("show_swap", true)) {
        static std::string swap_str = SysInfo::get_swap();
        if (!swap_str.empty()) info_items.push_back({"Swap:", swap_str});
    }
    if (cfg.get_bool("show_disk", true)) {
        static std::string disk = SysInfo::get_disk();
        if (!disk.empty()) info_items.push_back({"Disk:", disk});
    }
    if (cfg.get_bool("show_battery", false)) {
        static std::string batt = SysInfo::get_battery();
        if (!batt.empty()) info_items.push_back({"Battery:", batt});
    }
    if (cfg.get_bool("show_localip", true)) {
        static std::string lip = SysInfo::get_local_ip();
        if (!lip.empty()) info_items.push_back({"Local IP:", lip});
    }
    if (cfg.get_bool("show_publicip", false)) {
        static std::string pip = SysInfo::get_public_ip();
        if (!pip.empty()) info_items.push_back({"Public IP:", pip});
    }
    if (cfg.get_bool("show_locale", true)) {
        static std::string loc = SysInfo::get_locale();
        if (!loc.empty()) info_items.push_back({"Locale:", loc});
    }
    if (cfg.get_bool("show_weather", false)) {
        std::string loc = cfg.get_string("weather_location", "");
        std::string wtr = SysInfo::get_weather(loc);
        if (!wtr.empty()) info_items.push_back({"Weather:", wtr});
    }

    std::vector<std::string> text_block;
    text_block.push_back(AC + BOLD + user_host + RESET);
    text_block.push_back(AC + std::string(user_host.length(), '-') + RESET);

    for (const auto& item : info_items) {
        std::ostringstream ss;
        ss << AC << BOLD << std::left << std::setw(12) << item.first << RESET << " " << VAL_COLOR << item.second << RESET;
        text_block.push_back(ss.str());
    }

    std::string reminder = cfg.get_string("reminder_text", "");
    if (!reminder.empty()) {
        text_block.push_back("");
        int max_width = 46;
        std::vector<std::string> lines;
        std::string current_line = "";
        std::istringstream words(reminder);
        std::string word;
        while (words >> word) {
            if (current_line.length() + word.length() + 1 > (size_t)max_width) {
                if (current_line.empty()) {
                    lines.push_back(word);
                } else {
                    lines.push_back(current_line);
                    current_line = word;
                }
            } else {
                if (!current_line.empty()) current_line += " ";
                current_line += word;
            }
        }
        if (!current_line.empty()) lines.push_back(current_line);

        int longest = 0;
        for (const auto& l : lines) {
            if ((int)l.length() > longest) longest = l.length();
        }

        std::string bg_color = AC;
        size_t pos = bg_color.find("38;2;");
        if (pos != std::string::npos) {
            bg_color.replace(pos, 5, "48;2;");
        } else {
            pos = bg_color.find("[3");
            if (pos != std::string::npos) {
                bg_color[pos + 1] = '4';
            }
        }
        std::string fg_color = "\033[1;37m"; 

        for (const auto& l : lines) {
            std::string pad(longest - l.length(), ' ');
            text_block.push_back(bg_color + fg_color + " " + l + pad + " " + RESET);
        }
    }

    std::vector<std::string> logo_block;
    std::string img_path = cfg.get_string("image_path", "");
    int img_width = cfg.get_int("image_width", 28);
    std::string img_style = cfg.get_string("image_style", "color");
    int pixel_size = cfg.get_int("pixel_size", 1);
    int blur_radius = cfg.get_int("image_blur", 0);
    
    float scale_x = 1.0f;
    try { scale_x = std::stod(cfg.get_string("image_scale_x", "1.0")); } catch(...) {}
    
    float scale_y = 1.0f;
    try { scale_y = std::stod(cfg.get_string("image_scale_y", "1.0")); } catch(...) {}
    
    int pad_x = cfg.get_int("image_pad_x", 0);
    int pad_y = cfg.get_int("image_pad_y", 0);

    int max_h = text_block.size();

    if (!img_path.empty()) {
        logo_block = ImgRender::render_image(img_path, img_width, scale_x, scale_y, img_style, pixel_size, blur_radius, max_h);
    }

    if (logo_block.empty()) {
        std::string os = "";
        for (const auto& item : info_items) {
            if (item.first == "OS:") os = item.second;
        }
        std::vector<std::string> default_logo = get_os_logo(os, img_width);
        for (const auto& l : default_logo) {
            logo_block.push_back(AC + l + RESET);
        }
    }

    size_t max_lines = text_block.size();
    size_t pad_text = 0;

    auto get_visual_len = [](const std::string& str) {
        size_t len = 0;
        bool in_ansi = false;
        for (char c : str) {
            if (c == '\033') in_ansi = true;
            else if (in_ansi && c == 'm') in_ansi = false;
            else if (!in_ansi) {
                if ((c & 0xC0) != 0x80) len++;
            }
        }
        return len;
    };

    size_t actual_logo_width = 0;
    for (const auto& l : logo_block) {
        size_t vlen = get_visual_len(l);
        if (vlen > actual_logo_width) actual_logo_width = vlen;
    }
    if (actual_logo_width == 0) actual_logo_width = img_width;

    std::vector<std::string> new_logo(max_lines, std::string(actual_logo_width, ' '));
    int start_y = (int)(max_lines - logo_block.size()) / 2 + pad_y;
    
    for (size_t i = 0; i < logo_block.size(); ++i) {
        int y = start_y + (int)i;
        if (y >= 0 && y < (int)max_lines) {
            new_logo[y] = logo_block[i];
        }
    }

    std::vector<std::string> new_text(pad_text, "");
    new_text.insert(new_text.end(), text_block.begin(), text_block.end());
    while (new_text.size() < max_lines) new_text.push_back("");

    std::string x_pad_str = (pad_x > 0) ? std::string(pad_x, ' ') : "";

    for (size_t i = 0; i < max_lines; ++i) {
        std::string l_line = new_logo[i];
        size_t vlen = get_visual_len(l_line);
        if (vlen < actual_logo_width) {
            l_line += std::string(actual_logo_width - vlen, ' ');
        }
        
        if (pad_x < 0) {
            int to_remove = -pad_x;
            int count = 0;
            std::string clipped = "";
            bool in_ansi = false;
            for (char c : l_line) {
                if (c == '\033') in_ansi = true;
                if (in_ansi) {
                    clipped += c;
                    if (c == 'm') in_ansi = false;
                    continue;
                }
                if (count >= to_remove) clipped += c;
                else count++;
            }
            l_line = clipped;
        } else {
            l_line = x_pad_str + l_line;
        }
        
        std::string t_line = (i < new_text.size()) ? new_text[i] : "";
        out.push_back(l_line + "   " + t_line);
    }
    return out;
}

void run_settings_menu(Config& cfg, const std::string& config_path) {
    int state = 0;
    int selected = 0;
    
    std::vector<MenuItem> info_menu = {
        {"Show OS", "show_os", {}},
        {"Show Host", "show_host", {}},
        {"Show Kernel", "show_kernel", {}},
        {"Show Uptime", "show_uptime", {}},
        {"Show Packages", "show_packages", {}},
        {"Show Shell", "show_shell", {}},
        {"Show Terminal", "show_terminal", {}},
        {"Show Resolution", "show_resolution", {}},
        {"Show CPU", "show_cpu", {}},
        {"Show GPU", "show_gpu", {}},
        {"Show Memory", "show_memory", {}},
        {"Show Swap", "show_swap", {}},
        {"Show Disk", "show_disk", {}},
        {"Show Battery", "show_battery", {}},
        {"Show Local IP", "show_localip", {}},
        {"Show Public IP", "show_publicip", {}},
        {"Show Locale", "show_locale", {}},
        {"Show Weather", "show_weather", {}}
    };
    
    std::vector<MenuItem> app_menu = {
        {"Theme", "theme", {"default", "hacker", "dracula", "nord", "fire", "gold"}},
        {"Show Bars", "show_bars", {}}
    };

    auto get_visual_len = [](const std::string& str) {
        size_t len = 0;
        bool in_ansi = false;
        for (char c : str) {
            if (c == '\033') in_ansi = true;
            else if (in_ansi && c == 'm') in_ansi = false;
            else if (!in_ansi) {
                if ((c & 0xC0) != 0x80) len++;
            }
        }
        return len;
    };

    std::cout << "\033[?1049h\033[?25l\033[?1000l\033[?1007l"; // Enter alternate screen buffer, hide cursor, disable mouse scroll

    while (true) {
        std::vector<std::string> left_lines;
        left_lines.push_back("\033[1;36m  ██████╗ ██╗      ██╗    ██████╗  ███████╗ ██████╗  ██████╗  ██████╗ \033[0m");
        left_lines.push_back("\033[1;36m ██╔════╝ ██║      ██║    ██╔══██╗ ██╔════╝ ██╔════╝ ██╔═══██╗ ██╔══██╗\033[0m");
        left_lines.push_back("\033[1;36m ██║      ██║      ██║    ██║  ██║ █████╗   ██║      ██║   ██║ ██████╔╝\033[0m");
        left_lines.push_back("\033[1;36m ██║      ██║      ██║    ██║  ██║ ██╔══╝   ██║      ██║   ██║ ██╔══██╗\033[0m");
        left_lines.push_back("\033[1;36m ╚██████╗ ███████╗ ██║    ██████╔╝ ███████╗ ╚██████╗ ╚██████╔╝ ██║  ██║\033[0m");
        left_lines.push_back("\033[1;36m  ╚═════╝ ╚══════╝ ╚═╝    ╚═════╝  ╚══════╝  ╚═════╝  ╚═════╝  ╚═╝  ╚═╝\033[0m");
        left_lines.push_back("");
        left_lines.push_back("Press [\033[1;33mq\033[0m] to quit/back, [\033[1;33mUP/DOWN\033[0m] to navigate, [\033[1;32mENTER\033[0m] to confirm/edit.");
        left_lines.push_back("");
        left_lines.push_back("\033[1;37m=== SETTINGS ===\033[0m");
        left_lines.push_back("");
        
        std::vector<std::string> main_opts = {
            "1. Information Customization",
            "2. Side Art",
            "3. Appearance & Themes",
            "4. Reminder Box",
            "5. Save & Exit"
        };
        std::vector<std::string> side_art_opts = {
            "1. ASCII Art (Default)",
            "2. Custom Image Art"
        };
        std::vector<std::string> custom_img_opts = {
            "1. Set Image Path Manually",
            "2. Choose Image (File Manager)",
            "3. Live Resize & Align Image",
            "4. Adjust Image Blur"
        };
        std::vector<std::string> rem_opts = {
            "1. Set New Reminder",
            "2. Remove Reminder"
        };

        if (state == 0) { // Main Menu
            for (size_t i = 0; i < main_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? "\033[1;32m> " : "  ");
                left_lines.push_back(prefix + main_opts[i] + "\033[0m");
            }
        } 
        else if (state == 1 || state == 2) { // Info or Appearance
            std::vector<MenuItem>& menu = (state == 1) ? info_menu : app_menu;
            for (size_t i = 0; i < menu.size(); ++i) {
                std::ostringstream ss;
                ss << (i == (size_t)selected ? "\033[1;32m> " : "  ");
                ss << std::left << std::setw(20) << menu[i].label << "\033[0m";
                std::string current_val = cfg.get_string(menu[i].key, "");
                if (menu[i].options.empty()) {
                    if (current_val.empty()) current_val = "1";
                    bool is_on = (current_val == "1" || current_val == "true" || current_val == "yes" || current_val == "on");
                    ss << (is_on ? "\033[1;32m[ ON  ]" : "\033[1;31m[ OFF ]") << "\033[0m";
                } else {
                    if (current_val.empty()) current_val = menu[i].options[0];
                    ss << "\033[1;36m< " << current_val << " >\033[0m";
                }
                left_lines.push_back(ss.str());
            }
        }
        else if (state == 3) { // Side Art
            for (size_t i = 0; i < side_art_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? "\033[1;32m> " : "  ");
                left_lines.push_back(prefix + side_art_opts[i] + "\033[0m");
            }
        }
        else if (state == 6) { // Custom Image Art
            for (size_t i = 0; i < custom_img_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? "\033[1;32m> " : "  ");
                if ((i == 2 || i == 3) && cfg.get_string("image_path", "").empty()) {
                    left_lines.push_back(prefix + "\033[1;30m" + custom_img_opts[i] + " (Requires Custom Image)\033[0m");
                } else {
                    left_lines.push_back(prefix + custom_img_opts[i] + "\033[0m");
                }
            }
        }
        else if (state == 4) { // Live Resize & Align
            left_lines.push_back("");
            left_lines.push_back("\033[1;33m=== Live Resize & Align ===\033[0m");
            left_lines.push_back("[\033[1;32mUP/DOWN\033[0m] Stretch Height");
            left_lines.push_back("[\033[1;32mLEFT/RIGHT\033[0m] Stretch Width");
            left_lines.push_back("[\033[1;32mW/A/S/D\033[0m] Move Image");
            left_lines.push_back("[\033[1;32mENTER or ESC\033[0m] Finish");
            left_lines.push_back("");
            left_lines.push_back("\033[1;30mX Offset: " + std::to_string(cfg.get_int("image_pad_x", 0)) + " | Y Offset: " + std::to_string(cfg.get_int("image_pad_y", 0)) + "\033[0m");
            left_lines.push_back("\033[1;30mX Scale: " + cfg.get_string("image_scale_x", "1.0") + " | Y Scale: " + cfg.get_string("image_scale_y", "1.0") + "\033[0m");
        }
        else if (state == 5) { // Reminder Box
            for (size_t i = 0; i < rem_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? "\033[1;32m> " : "  ");
                left_lines.push_back(prefix + rem_opts[i] + "\033[0m");
            }
        }
        std::vector<std::string> right_lines = generate_preview(cfg);
        right_lines.insert(right_lines.begin(), "\033[1;37m=== LIVE PREVIEW ===\033[0m");
        right_lines.insert(right_lines.begin() + 1, "");
        
        std::cout << "\033[2J"; // Clear screen once per frame
        size_t mlines = std::max(left_lines.size(), right_lines.size());
        size_t left_width = 75;

        for (size_t i = 0; i < mlines; ++i) {
            std::string l = (i < left_lines.size()) ? left_lines[i] : "";
            size_t vlen = get_visual_len(l);
            if (vlen < left_width) {
                l += std::string(left_width - vlen, ' ');
            }
            std::string r = (i < right_lines.size()) ? right_lines[i] : "";
            std::cout << "\033[" << (i + 1) << ";1H\033[K" << l << " | " << r;
        }
        std::cout << std::flush;
        
        int key = get_keypress();
        if (key == 113 || key == 27) { // q or ESC
            if (state == 0) break; // exit settings
            else { state = 0; selected = 0; continue; }
        }
        if (key == 1001) { // UP
            int s_max = (state == 0) ? main_opts.size() : (state == 3 ? side_art_opts.size() : (state == 6 ? custom_img_opts.size() : (state == 5 ? rem_opts.size() : ((state == 1) ? info_menu.size() : app_menu.size()))));
            selected = (selected > 0) ? selected - 1 : s_max - 1;
        }
        if (key == 1002) { // DOWN
            int s_max = (state == 0) ? main_opts.size() : (state == 3 ? side_art_opts.size() : (state == 6 ? custom_img_opts.size() : (state == 5 ? rem_opts.size() : ((state == 1) ? info_menu.size() : app_menu.size()))));
            selected = (selected + 1) % s_max;
        }
        
        if (key == 1003 || key == 1004 || key == 13 || key == 10) { // LEFT, RIGHT, ENTER
            if (state == 0) { // Main Menu
                if (key == 13 || key == 10) {
                    if (selected == 0) { state = 1; selected = 0; }
                    else if (selected == 1) { state = 3; selected = 0; }
                    else if (selected == 2) { state = 2; selected = 0; }
                    else if (selected == 3) { state = 5; selected = 0; }
                    else if (selected == 4) break; // Save & Exit
                }
            } 
            else if (state == 1 || state == 2) {
                std::vector<MenuItem>& menu = (state == 1) ? info_menu : app_menu;
                MenuItem& m = menu[selected];
                if (m.options.empty()) {
                    if (key == 13 || key == 10 || key == 1003 || key == 1004) {
                        std::string val = cfg.get_string(m.key, "1");
                        cfg.settings[m.key] = (val == "1" || val == "true" || val == "yes" || val == "on") ? "0" : "1";
                    }
                } else {
                    std::string val = cfg.get_string(m.key, m.options[0]);
                    int idx = 0;
                    for (size_t i = 0; i < m.options.size(); ++i) {
                        if (m.options[i] == val) idx = i;
                    }
                    if (key == 13 || key == 10) {
                        state = 0; selected = 0; continue;
                    } else if (key == 1003) {
                        idx = (idx > 0) ? idx - 1 : (int)m.options.size() - 1;
                    } else {
                        idx = (idx + 1) % m.options.size();
                    }
                    cfg.settings[m.key] = m.options[idx];
                }
            }
            else if (state == 3) { // Side Art
                if (key == 13 || key == 10) {
                    if (selected == 0) {
                        cfg.settings["image_path"] = "";
                        state = 0; selected = 0;
                    } else if (selected == 1) {
                        state = 6; selected = 0;
                    }
                }
            }
            else if (state == 6) { // Custom Image Art
                if (key == 13 || key == 10) {
                    if (selected == 0) {
                        std::cout << "\033[" << (left_lines.size() + 2) << ";1H";
                        std::cout << "\033[1;36mEnter custom image path: \033[0m";
                        std::cout << "\033[?25h";
                        std::string path;
                        std::getline(std::cin, path);
                        std::cout << "\033[?25l";
                        if (!path.empty()) cfg.settings["image_path"] = path;
                    } else if (selected == 1) {
                        std::string path;
#ifdef _WIN32
                        std::cout << "\033[" << (left_lines.size() + 2) << ";1H\033[1;36mOpening Windows File Picker...\033[0m\n";
                        std::string ps = "powershell -c \"Add-Type -AssemblyName System.Windows.Forms; $f = New-Object System.Windows.Forms.OpenFileDialog; $f.Filter = 'Image Files|*.jpg;*.jpeg;*.png;*.bmp;*.webp'; $f.ShowHelp = $true; if($f.ShowDialog() -eq 'OK'){ $f.FileName }\"";
                        path = trim(exec(ps.c_str()));
#else
                        if (access("/usr/bin/zenity", F_OK) == 0) {
                            std::cout << "\033[" << (left_lines.size() + 2) << ";1H\033[1;36mOpening Zenity File Picker...\033[0m\n";
                            path = trim(exec("zenity --file-selection --title=\"Select Image for CLI DECOR\" 2>/dev/null"));
                        } else if (access("/usr/bin/kdialog", F_OK) == 0) {
                            std::cout << "\033[" << (left_lines.size() + 2) << ";1H\033[1;36mOpening KDE File Picker...\033[0m\n";
                            path = trim(exec("kdialog --getopenfilename . \"Image Files (*.jpg *.jpeg *.png *.bmp *.webp)\" 2>/dev/null"));
                        } else {
                            std::cout << "\033[" << (left_lines.size() + 2) << ";1H\033[1;31mNo GUI picker (zenity/kdialog). Enter path manually: \033[0m";
                            std::cout << "\033[?25h";
                            std::getline(std::cin, path);
                            std::cout << "\033[?25l";
                        }
#endif
                        if (!path.empty()) cfg.settings["image_path"] = path;
                    } else if (selected == 2) {
                        if (!cfg.get_string("image_path", "").empty()) {
                            state = 4; selected = 0; continue;
                        }
                    } else if (selected == 3) {
                        if (!cfg.get_string("image_path", "").empty()) {
                            std::cout << "\033[" << (left_lines.size() + 2) << ";1H";
                            std::cout << "\033[1;36mEnter Blur Radius (0 to 10): \033[0m";
                            std::cout << "\033[?25h";
                            std::string b;
                            std::getline(std::cin, b);
                            std::cout << "\033[?25l";
                            try {
                                int br = std::stoi(b);
                                cfg.settings["image_blur"] = std::to_string(std::clamp(br, 0, 10));
                            } catch(...) {}
                        }
                    }
                    if (state == 6) { state = 3; selected = 1; }
                }
            }
            else if (state == 5) { // Reminder Box Submenu
                if (key == 13 || key == 10) {
                    if (selected == 0) {
                        std::cout << "\033[" << (left_lines.size() + 2) << ";1H";
                        std::cout << "\033[1;36mEnter Reminder Text: \033[0m";
                        std::cout << "\033[?25h";
                        std::string txt;
                        std::getline(std::cin, txt);
                        std::cout << "\033[?25l";
                        if (!txt.empty()) cfg.settings["reminder_text"] = txt;
                    } else if (selected == 1) {
                        cfg.settings["reminder_text"] = "";
                    }
                    state = 0; selected = 0;
                }
            }
        }
        else if (state == 4) { // Live Resize & Align (Process ALL keys)
            float sx = 1.0f; try { sx = std::stod(cfg.get_string("image_scale_x", "1.0")); } catch(...) {}
            float sy = 1.0f; try { sy = std::stod(cfg.get_string("image_scale_y", "1.0")); } catch(...) {}
            
            if (key == 119 || key == 87) { // W
                cfg.settings["image_pad_y"] = std::to_string(cfg.get_int("image_pad_y", 0) - 1);
            } else if (key == 115 || key == 83) { // S
                cfg.settings["image_pad_y"] = std::to_string(cfg.get_int("image_pad_y", 0) + 1);
            } else if (key == 97 || key == 65) { // A
                cfg.settings["image_pad_x"] = std::to_string(cfg.get_int("image_pad_x", 0) - 1);
            } else if (key == 100 || key == 68) { // D
                cfg.settings["image_pad_x"] = std::to_string(cfg.get_int("image_pad_x", 0) + 1);
            } else if (key == 1001) { // UP
                cfg.settings["image_scale_y"] = std::to_string(sy + 0.10f);
            } else if (key == 1002) { // DOWN
                if (sy > 0.1f) cfg.settings["image_scale_y"] = std::to_string(sy - 0.10f);
            } else if (key == 1003) { // LEFT
                if (sx > 0.1f) cfg.settings["image_scale_x"] = std::to_string(sx - 0.10f);
            } else if (key == 1004) { // RIGHT
                cfg.settings["image_scale_x"] = std::to_string(sx + 0.10f);
            } else if (key == 13 || key == 10 || key == 27 || key == 113) {
                state = 6; selected = 2; // Return to Custom Image Art
            }
        }
    }
    
    std::cout << "\033[?1049l\033[?25h"; // Exit alternate screen buffer & show cursor
    cfg.save_to_file(config_path);
    std::cout << "\033[1;32mSettings saved to " << config_path << "!\033[0m\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            std::cout << "CLI DECOR — neofetch replacement (C++ engine)\n\n"
                      << "Usage:\n"
                      << "  clidecor              run normally\n"
                      << "  clidecor -s           open interactive settings menu\n"
                      << "  clidecor --refresh    clear cache and re-render\n"
                      << "  clidecor --help       show this message\n\n"
                      << "Config: ~/.config/clidecor/config.conf\n";
            return 0;
        } else if (arg == "--refresh") {
            std::cout << "Cache cleared.\n";
            return 0;
        } else if (arg == "--settings" || arg == "-s") {
            std::string config_path = Config::get_default_config_path();
            Config cfg = Config::load_from_file(config_path);
            run_settings_menu(cfg, config_path);
            return 0;
        }
    }

    std::string config_path = Config::get_default_config_path();
    Config cfg = Config::load_from_file(config_path);

    std::vector<std::string> output = generate_preview(cfg);
    for (const auto& line : output) {
        std::cout << line << "\n";
    }

    return 0;
}
