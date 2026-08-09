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
#include <thread>
#include <chrono>
#include <functional>
#include <sys/stat.h>
#include <csignal>

void handle_signal(int sig) {
    std::cout << "\033[?1049l";
    std::cout << "\033[?25h\033[?1000l\033[?1007l\033[0m\n";
    std::cout.flush();
#ifndef _WIN32
    int res = system("stty sane 2>/dev/null");
    (void)res;
#endif
    std::exit(sig);
}

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

struct ModuleDef {
    std::string config_key;
    std::string label;
    bool default_enabled = true;
    std::function<std::string(const Config&)> fetcher;
    bool has_bar = false;
    std::function<int()> bar_fetcher = nullptr;
    std::string cached_value = "";
    int cached_bar = 0;
    bool is_cached = false;
};

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
            double swap_free = 0.0;
            if (memInfo.ullAvailPageFile > memInfo.ullAvailPhys) {
                swap_free = (double)(memInfo.ullAvailPageFile - memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
            }
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

static std::string get_ping() {
#ifdef _WIN32
    std::string out = exec("ping -n 1 8.8.8.8");
    size_t pos = out.find("time=");
    if (pos != std::string::npos) {
        size_t end = out.find("ms", pos);
        if (end != std::string::npos) {
            return out.substr(pos + 5, end - pos - 5) + " ms";
        }
    }
#else
    std::string out = exec("ping -c 1 8.8.8.8 2>/dev/null");
    size_t pos = out.find("time=");
    if (pos != std::string::npos) {
        size_t end = out.find(" ms", pos);
        if (end != std::string::npos) {
            return out.substr(pos + 5, end - pos - 5) + " ms";
        }
    }
#endif
    return "Timeout";
}

std::vector<ModuleDef> get_registry() {
    return {
        {"show_os", "OS:", true, [](const Config&) { return get_os(); }, false, nullptr},
        {"show_host", "Host:", true, [](const Config&) { return get_host(); }, false, nullptr},
        {"show_kernel", "Kernel:", true, [](const Config&) { return get_kernel(); }, false, nullptr},
        {"show_uptime", "Uptime:", true, [](const Config&) { return get_uptime(); }, false, nullptr},
        {"show_packages", "Packages:", true, [](const Config&) { return get_packages(); }, false, nullptr},
        {"show_shell", "Shell:", true, [](const Config&) { return get_shell(); }, false, nullptr},
        {"show_terminal", "Terminal:", true, [](const Config&) { return get_terminal(); }, false, nullptr},
        {"show_resolution", "Resolution:", true, [](const Config&) { return get_resolution(); }, false, nullptr},
        {"show_cpu", "CPU:", true, [](const Config&) { return get_cpu(); }, true, get_cpu_usage},
        {"show_gpu", "GPU:", true, [](const Config&) { return get_gpu(); }, false, nullptr},
        {"show_memory", "Memory:", true, [](const Config&) {
             auto mem = get_memory();
             auto [used_mb, total_mb] = mem;
             if (total_mb > 0) {
                 if (total_mb >= 1024) {
                     char buf[64]; snprintf(buf, sizeof(buf), "%.2f GiB / %.2f GiB", used_mb / 1024.0, total_mb / 1024.0);
                     return std::string(buf);
                 }
                 return std::to_string(used_mb) + " MiB / " + std::to_string(total_mb) + " MiB";
             }
             return std::string("");
         }, true, []() {
             auto mem = get_memory();
             if (std::get<1>(mem) > 0) return (int)((std::get<0>(mem) * 100) / std::get<1>(mem));
             return 0;
         }},
        {"show_swap", "Swap:", true, [](const Config&) { return get_swap(); }, false, nullptr},
        {"show_disk", "Disk:", true, [](const Config&) { return get_disk(); }, false, nullptr},
        {"show_battery", "Battery:", false, [](const Config&) { return get_battery(); }, false, nullptr},
        {"show_localip", "Local IP:", true, [](const Config&) { return get_local_ip(); }, false, nullptr},
        {"show_publicip", "Public IP:", false, [](const Config&) { return get_public_ip(); }, false, nullptr},
        {"show_ping", "Ping (8.8.8.8):", false, [](const Config&) { return get_ping(); }, false, nullptr},
        {"show_locale", "Locale:", true, [](const Config&) { return get_locale(); }, false, nullptr},
        {"show_weather", "Weather:", false, [](const Config& cfg) { return get_weather(cfg.get_string("weather_location", "")); }, false, nullptr},
        {"show_git", "Git:", false, [](const Config&) { return get_git(); }, false, nullptr}
    };
}

} // namespace SysInfo

// ─── Image Rendering Engine ──────────────────────────────────────────────────

namespace ImgRender {

struct RGB {
    unsigned char r, g, b;
};

static const std::string ASCII_RAMP = " .,:;+*?%#@";
static const RGB TERM_BG = {0, 0, 0};
static std::pair<std::string, std::string> chameleon_theme = {"#00FFFF", "#0055FF"};

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

// Bilinear interpolation — smooth downsampling instead of blocky nearest-neighbor
static RGB get_bilinear(const unsigned char* data, int w, int h, int ch, float rx, float ry) {
    int x0 = std::max(0, std::min((int)rx, w - 1));
    int y0 = std::max(0, std::min((int)ry, h - 1));
    int x1 = std::min(x0 + 1, w - 1);
    int y1 = std::min(y0 + 1, h - 1);
    float fx = rx - x0, fy = ry - y0;
    auto s = [&](int x, int y, int c) { return (float)data[(y * w + x) * ch + c]; };
    RGB r;
    r.r = (unsigned char)((1-fy)*((1-fx)*s(x0,y0,0)+fx*s(x1,y0,0))+fy*((1-fx)*s(x0,y1,0)+fx*s(x1,y1,0)));
    r.g = (unsigned char)((1-fy)*((1-fx)*s(x0,y0,1)+fx*s(x1,y0,1))+fy*((1-fx)*s(x0,y1,1)+fx*s(x1,y1,1)));
    r.b = (unsigned char)((1-fy)*((1-fx)*s(x0,y0,2)+fx*s(x1,y0,2))+fy*((1-fx)*s(x0,y1,2)+fx*s(x1,y1,2)));
    return r;
}

static std::string get_cache_path(const std::string& path, int width, float scale_x, float scale_y) {
    std::string tmp = "/tmp";
#ifdef _WIN32
    if (const char* env_p = std::getenv("TEMP")) tmp = env_p;
#else
    if (const char* env_p = std::getenv("TMPDIR")) tmp = env_p;
#endif
    struct stat attr;
    long long mtime = 0;
    if (stat(path.c_str(), &attr) == 0) mtime = attr.st_mtime;
    std::hash<std::string> hasher;
    std::string key = path + "_" + std::to_string(mtime) + "_" + std::to_string(width) + "_" + std::to_string(scale_x) + "_" + std::to_string(scale_y);
    return tmp + "/clidecor_img_" + std::to_string(hasher(key)) + ".cache";
}

void clear_image_cache() {
    std::string tmp = "/tmp";
#ifdef _WIN32
    if (const char* env_p = std::getenv("TEMP")) tmp = env_p;
    std::string cmd = "del /Q /F \"" + tmp + "\\clidecor_img_*.cache\" 2>nul";
#else
    if (const char* env_p = std::getenv("TMPDIR")) tmp = env_p;
    std::string cmd = "rm -f \"" + tmp + "\"/clidecor_img_*.cache 2>/dev/null";
#endif
    int res = system(cmd.c_str());
    (void)res;
}

std::vector<std::string> render_image(
    const std::string& path,
    int base_width,
    float scale_x = 1.0f,
    float scale_y = 1.0f,
    const std::string& /*style*/ = "color",
    int /*block_size*/ = 1,
    int /*blur_radius*/ = 0,
    int max_h = 0
) {
    std::vector<std::string> out_lines;
    
    std::string cache_file = get_cache_path(path, base_width, scale_x, scale_y);
    std::ifstream c_in(cache_file);
    if (c_in.is_open()) {
        std::string line;
        std::string h1, h2;
        if (std::getline(c_in, h1) && std::getline(c_in, h2)) {
            chameleon_theme = {h1, h2};
        }
        while (std::getline(c_in, line)) {
            out_lines.push_back(line);
        }
        if (!out_lines.empty()) return out_lines;
    }

    int orig_w, orig_h, orig_channels;
    unsigned char* data = stbi_load(path.c_str(), &orig_w, &orig_h, &orig_channels, 4);
    int channels = 4;
    if (!data) {
        out_lines.push_back("\033[1;31m[!] Failed to load image:\033[0m");
        out_lines.push_back("\033[1;31m    " + path + "\033[0m");
        out_lines.push_back("");
        out_lines.push_back("\033[1;33mPlease check that the file exists\033[0m");
        out_lines.push_back("\033[1;33mand is a valid PNG, JPG, or BMP.\033[0m");
        return out_lines;
    }
    if (base_width <= 0) base_width = 28;

    float aspect = (float)orig_h / (float)orig_w;
    int width_cols = std::max(1, (int)(base_width * scale_x));
    int target_h   = std::max(2, (int)(base_width * aspect * 0.55f * 2.0f * scale_y));
    if (target_h % 2 != 0) target_h++;

    std::vector<std::vector<RGB>> grid(target_h, std::vector<RGB>(width_cols));
    for (int y = 0; y < target_h; ++y)
        for (int x = 0; x < width_cols; ++x) {
            float rx = ((float)x / width_cols) * orig_w;
            float ry = ((float)y / target_h)   * orig_h;
            grid[y][x] = get_sample(data, orig_w, orig_h, channels, rx, ry);
        }
    stbi_image_free(data);

    // Chameleon color
    long long tr = 0, tg = 0, tb = 0; int pc = 0;
    for (int y = 0; y < target_h; ++y)
        for (int x = 0; x < width_cols; ++x)
            if (!is_bg(grid[y][x])) { tr += grid[y][x].r; tg += grid[y][x].g; tb += grid[y][x].b; pc++; }
    if (pc > 0) {
        int ar = tr/pc, ag = tg/pc, ab = tb/pc;
        char hex[8];     snprintf(hex,     sizeof(hex),     "#%02X%02X%02X", ar, ag, ab);
        char end_hex[8]; snprintf(end_hex, sizeof(end_hex), "#%02X%02X%02X",
            std::min(255,(int)(ar*1.5)), std::min(255,(int)(ag*1.5)), std::min(255,(int)(ab*1.5)));
        chameleon_theme = {hex, end_hex};
    }

    // Half-block rendering: 2 pixel rows → 1 terminal line (▀ top / ▄ bottom)
    for (int y = 0; y < target_h; y += 2) {
        std::ostringstream ss;
        for (int x = 0; x < width_cols; ++x) {
            RGB top = grid[y][x];
            RGB bot = (y + 1 < target_h) ? grid[y + 1][x] : top;
            bool top_bg = is_bg(top), bot_bg = is_bg(bot);
            if (top_bg && bot_bg) {
                ss << " ";
            } else if (top_bg) {
                ss << "\033[38;2;" << (int)bot.r << ";" << (int)bot.g << ";" << (int)bot.b << "m\xe2\x96\x84\033[0m";
            } else if (bot_bg) {
                ss << "\033[38;2;" << (int)top.r << ";" << (int)top.g << ";" << (int)top.b << "m\xe2\x96\x80\033[0m";
            } else {
                ss << "\033[38;2;" << (int)top.r << ";" << (int)top.g << ";" << (int)top.b << "m"
                   << "\033[48;2;" << (int)bot.r << ";" << (int)bot.g << ";" << (int)bot.b << "m\xe2\x96\x80\033[0m";
            }
        }
        out_lines.push_back(ss.str());
    }

    std::ofstream c_out(cache_file);
    if (c_out.is_open()) {
        c_out << chameleon_theme.first << "\n" << chameleon_theme.second << "\n";
        for (const auto& line : out_lines) {
            c_out << line << "\n";
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

struct ThemeColors {
    std::string start_hex;
    std::string end_hex;
};

static ThemeColors get_theme_colors(const std::string& theme) {
    if (theme == "hacker") return {"#00FF00", "#005500"};
    if (theme == "dracula") return {"#FF79C6", "#BD93F9"};
    if (theme == "nord") return {"#88C0D0", "#5E81AC"};
    if (theme == "fire") return {"#FF5555", "#FFB86C"};
    if (theme == "gold") return {"#F1FA8C", "#FFB86C"};
    if (theme == "chameleon") return {ImgRender::chameleon_theme.first, ImgRender::chameleon_theme.second};
    return {"#00FFFF", "#0055FF"}; // default cyan to blue gradient
}

static std::string hex_to_ansi(const std::string& hexStr) {
    if (hexStr.length() == 7 && hexStr[0] == '#') {
        int r = std::stoi(hexStr.substr(1, 2), nullptr, 16);
        int g = std::stoi(hexStr.substr(3, 2), nullptr, 16);
        int b = std::stoi(hexStr.substr(5, 2), nullptr, 16);
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }
    return "\033[36m";
}

static std::string get_gradient_color(const ThemeColors& colors, int total_steps, int current_step) {
    if (colors.start_hex.length() != 7 || colors.end_hex.length() != 7) return hex_to_ansi(colors.start_hex);
    
    int r1 = std::stoi(colors.start_hex.substr(1, 2), nullptr, 16);
    int g1 = std::stoi(colors.start_hex.substr(3, 2), nullptr, 16);
    int b1 = std::stoi(colors.start_hex.substr(5, 2), nullptr, 16);
    
    int r2 = std::stoi(colors.end_hex.substr(1, 2), nullptr, 16);
    int g2 = std::stoi(colors.end_hex.substr(3, 2), nullptr, 16);
    int b2 = std::stoi(colors.end_hex.substr(5, 2), nullptr, 16);
    
    float t = (total_steps > 1) ? (float)current_step / (total_steps - 1) : 0.0f;
    int r = r1 + t * (r2 - r1);
    int g = g1 + t * (g2 - g1);
    int b = b1 + t * (b2 - b1);
    
    return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
}
static std::unordered_map<char, std::vector<std::string>> figlet_font = {
    {'A', {R"( . )", R"(/ \)", R"(---)", R"(| |)", R"(| |)"}},
    {'B', {R"(|-.)", R"(|-|)", R"(|-|)", R"(|-|)", R"(|-')"}},
    {'C', {R"( --)", R"(/  )", R"(|  )", R"(\  )", R"( --)"}},
    {'D', {R"(|\ )", R"(| \)", R"(| |)", R"(| /)", R"(|/ )"}},
    {'E', {R"(---)", R"(|  )", R"(|- )", R"(|  )", R"(---)"}},
    {'F', {R"(---)", R"(|  )", R"(|- )", R"(|  )", R"(|  )"}},
    {'G', {R"( --)", R"(/  )", R"(| -)", R"(\ |)", R"( --)"}},
    {'H', {R"(| |)", R"(| |)", R"(---)", R"(| |)", R"(| |)"}},
    {'I', {R"(---)", R"( | )", R"( | )", R"( | )", R"(---)"}},
    {'J', {R"( --)", R"(  |)", R"(  |)", R"(\ |)", R"( --)"}},
    {'K', {R"(| /)", R"(|/ )", R"(|\ )", R"(| \)", R"(|  )"}},
    {'L', {R"(|  )", R"(|  )", R"(|  )", R"(|  )", R"(---)"}},
    {'M', {R"(| |)", R"(|\|)", R"(| |)", R"(| |)", R"(| |)"}},
    {'N', {R"(| |)", R"(|\|)", R"(| |)", R"(| |)", R"(| |)"}},
    {'O', {R"( - )", R"(| |)", R"(| |)", R"(| |)", R"( - )"}},
    {'P', {R"(|-.)", R"(| |)", R"(|-')", R"(|  )", R"(|  )"}},
    {'Q', {R"( - )", R"(| |)", R"(| |)", R"(|\ )", R"( -\)"}},
    {'R', {R"(|-.)", R"(| |)", R"(|-')", R"(|\ )", R"(| \)"}},
    {'S', {R"( --)", R"(\  )", R"( - )", R"(  /)", R"(-- )"}},
    {'T', {R"(---)", R"( | )", R"( | )", R"( | )", R"( | )"}},
    {'U', {R"(| |)", R"(| |)", R"(| |)", R"(| |)", R"( - )"}},
    {'V', {R"(| |)", R"(| |)", R"(\ /)", R"(\ /)", R"( v )"}},
    {'W', {R"(| |)", R"(| |)", R"(| |)", R"(|/|)", R"(| |)"}},
    {'X', {R"(\ /)", R"( x )", R"(/ \)", R"(\ /)", R"( x )"}},
    {'Y', {R"(\ /)", R"( v )", R"( | )", R"( | )", R"( | )"}},
    {'Z', {R"(---)", R"(  /)", R"( / )", R"(/  )", R"(---)"}}
};

static std::vector<std::string> generate_figlet(const std::string& text) {
    std::vector<std::string> result(5, "");
    for (char c : text) {
        char upper_c = std::toupper((unsigned char)c);
        if (figlet_font.count(upper_c)) {
            for (int i = 0; i < 5; ++i) {
                result[i] += figlet_font[upper_c][i] + " ";
            }
        } else if (c == ' ') {
            for (int i = 0; i < 5; ++i) {
                result[i] += "   ";
            }
        } else {
            for (int i = 0; i < 5; ++i) {
                result[i] += " ? ";
            }
        }
    }
    return result;
}
std::vector<std::string> get_os_logo(const std::string& os, int& width) {
    std::string os_lower = os;
    for (char& c : os_lower) c = std::tolower((unsigned char)c);
    
    width = 17;
    if (os_lower.find("windows") != std::string::npos) {
        return {
            "     ____  ____  ",
            "    |    ||    | ",
            "    |____||____| ",
            "     ____  ____  ",
            "    |    ||    | ",
            "    |____||____| ",
            "                 "
        };
    } else if (os_lower.find("ubuntu") != std::string::npos) {
        return {
            "       _         ",
            "   ---(_)---     ",
            " _/         \\_   ",
            "(_)         (_)  ",
            "  \\         /    ",
            "   ---(_)---     ",
            "                 "
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
    } else if (os_lower.find("fedora") != std::string::npos) {
        return {
            "      _____      ",
            "     /   __)\\    ",
            "     |  /  \\ \\   ",
            "     |  \\__/ |   ",
            "      \\_____/    ",
            "                 ",
            "                 "
        };
    } else if (os_lower.find("debian") != std::string::npos) {
        return {
            "       _,-=._    ",
            "      /  ,-'_`\\  ",
            "     |  (  (.) ) ",
            "      \\  `=_,_,' ",
            "       `----'    ",
            "                 ",
            "                 "
        };
    } else if (os_lower.find("kali") != std::string::npos) {
        return {
            "      ..         ",
            "    '    '       ",
            "   |  ()  |      ",
            "   |      |      ",
            "    '    '       ",
            "      ''         ",
            "                 "
        };
    } else if (os_lower.find("mint") != std::string::npos) {
        return {
            "    _______      ",
            "   |       |     ",
            "   | \\  /\\ |     ",
            "   |  \\/  \\|     ",
            "   |_______|     ",
            "                 ",
            "                 "
        };
    } else if (os_lower.find("pop") != std::string::npos) {
        return {
            "    ______       ",
            "   /     /\\      ",
            "  /     /  \\     ",
            " /_____/____\\    ",
            "                 ",
            "                 ",
            "                 "
        };
    } else if (os_lower.find("manjaro") != std::string::npos) {
        return {
            "   ||||||||| ||| ",
            "   ||||||||| ||| ",
            "   |||       ||| ",
            "   ||| ||||| ||| ",
            "   ||| ||||| ||| ",
            "   ||| ||||| ||| ",
            "                 "
        };
    } else if (os_lower.find("opensuse") != std::string::npos || os_lower.find("suse") != std::string::npos) {
        return {
            "     ____        ",
            "   /'    `\\      ",
            "  |   __   |     ",
            "  |  /  \\  |     ",
            "   \\_\\__/_/      ",
            "                 ",
            "                 "
        };
    } else if (os_lower.find("endeavour") != std::string::npos) {
        return {
            "      /\\         ",
            "    //  \\\\       ",
            "   //    \\\\      ",
            "  //======\\\\     ",
            " //        \\\\    ",
            "                 ",
            "                 "
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
    
    // Default Tux
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
    ThemeColors theme_colors = get_theme_colors(theme);
    std::string AC = get_gradient_color(theme_colors, 1, 0); // Base color
    
    std::string BOLD = "\033[1m";
    std::string RESET = "\033[0m";
    std::string VAL_COLOR = "\033[38;2;220;220;220m"; // Soft white for values

    static std::string user_host = SysInfo::get_user_host();
    std::vector<std::pair<std::string, std::string>> info_items;

    static std::vector<SysInfo::ModuleDef> registry = SysInfo::get_registry();
    for (auto& mod : registry) {
        if (cfg.get_bool(mod.config_key, mod.default_enabled)) {
            if (!mod.is_cached) {
                mod.cached_value = mod.fetcher(cfg);
                if (mod.has_bar && mod.bar_fetcher) {
                    mod.cached_bar = mod.bar_fetcher();
                }
                mod.is_cached = true;
            }
            if (!mod.cached_value.empty()) {
                std::string val_str = mod.cached_value;
                if (mod.has_bar && cfg.get_bool("show_bars", true) && cfg.get_bool(mod.config_key + "_bar", true)) {
                    std::string bar_color = get_gradient_color(theme_colors, 100, mod.cached_bar);
                    val_str += " " + build_bar(mod.cached_bar, bar_color, "\033[38;2;80;80;80m"); // Dark gray for empty
                }
                info_items.push_back({mod.label, val_str});
            }
        }
    }

    std::vector<std::string> text_block;
    
    std::string title_AC = get_gradient_color(theme_colors, 1, 0);
    text_block.push_back(title_AC + BOLD + user_host + RESET);
    text_block.push_back(title_AC + std::string(user_host.length(), '-') + RESET);

    for (size_t i = 0; i < info_items.size(); ++i) {
        std::ostringstream ss;
        std::string current_AC = get_gradient_color(theme_colors, info_items.size(), i);
        ss << current_AC << BOLD << std::left << std::setw(12) << info_items[i].first << RESET << " " << VAL_COLOR << info_items[i].second << RESET;
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

        std::string bg_color = "";
        if (theme_colors.end_hex.size() >= 7 && theme_colors.end_hex[0] == '#') {
            int r = 0, g = 0, b = 0;
            if (sscanf(theme_colors.end_hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                r = (int)(r * 0.3);
                g = (int)(g * 0.3);
                b = (int)(b * 0.3);
                char bg_buf[64];
                snprintf(bg_buf, sizeof(bg_buf), "\033[48;2;%d;%d;%dm", r, g, b);
                bg_color = bg_buf;
            }
        }
        if (bg_color.empty()) {
            bg_color = "\033[48;2;30;30;30m"; // Dark grey fallback
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
    int pixel_size = 1;
    int blur_radius = 0;
    
    float scale_x = 1.0f;
    try { scale_x = std::stod(cfg.get_string("image_scale_x", "1.0")); } catch(...) {}
    
    float scale_y = 1.0f;
    try { scale_y = std::stod(cfg.get_string("image_scale_y", "1.0")); } catch(...) {}
    
    int pad_x = cfg.get_int("image_pad_x", 0);
    int pad_y = cfg.get_int("image_pad_y", 0);

    int max_h = text_block.size();

    std::string ascii_path = cfg.get_string("ascii_path", "");

    if (!img_path.empty()) {
        logo_block = ImgRender::render_image(img_path, img_width, scale_x, scale_y, img_style, pixel_size, blur_radius, max_h);
    } else if (!ascii_path.empty()) {
        std::ifstream af(ascii_path);
        if (af.is_open()) {
            std::string line;
            std::vector<std::string> raw_ascii;
            while (std::getline(af, line)) {
                raw_ascii.push_back(line);
            }
            for (size_t i = 0; i < raw_ascii.size(); ++i) {
                std::string logo_AC = get_gradient_color(theme_colors, raw_ascii.size(), i);
                logo_block.push_back(logo_AC + raw_ascii[i] + RESET);
            }
        }
    }

    if (logo_block.empty()) {
        std::string header_text = cfg.get_string("header_text", "");
        if (cfg.get_string("header_type", "os") == "figlet" && !header_text.empty()) {
            std::vector<std::string> fig_logo = generate_figlet(header_text);
            for (size_t i = 0; i < fig_logo.size(); ++i) {
                std::string logo_AC = get_gradient_color(theme_colors, fig_logo.size(), i);
                logo_block.push_back(logo_AC + fig_logo[i] + RESET);
            }
        } else {
            std::string os = "";
            for (const auto& item : info_items) {
                if (item.first == "OS:") os = item.second;
            }
            std::vector<std::string> default_logo = get_os_logo(os, img_width);
            for (size_t i = 0; i < default_logo.size(); ++i) {
                std::string logo_AC = get_gradient_color(theme_colors, default_logo.size(), i);
                logo_block.push_back(logo_AC + default_logo[i] + RESET);
            }
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
        out.push_back(l_line + std::string(3, ' ') + t_line);
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
        {"Show Ping", "show_ping", {}},
        {"Show Locale", "show_locale", {}},
        {"Show Weather", "show_weather", {}},
        {"Show Git", "show_git", {}}
    };
    
    std::vector<MenuItem> app_menu = {
        {"Theme", "theme", {"default", "hacker", "dracula", "nord", "fire", "gold", "chameleon"}},
        {"Header Type", "header_type", {"os", "figlet"}},
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
        left_lines.push_back("");
        left_lines.push_back("");
        
        ThemeColors current_tc = get_theme_colors(cfg.get_string("theme", "default"));
        std::string AC = "\033[1m" + get_gradient_color(current_tc, 1, 0);
        std::vector<std::string> header = {
            "██████ ██      ██ ██████  ███████ ██████  ██████  ██████ ",
            "██     ██      ██ ██   ██ ██      ██      ██  ██  ██   ██",
            "██     ██      ██ ██   ██ █████   ██      ██  ██  ██████ ",
            "██     ██      ██ ██   ██ ██      ██      ██  ██  ██   ██",
            "██████ ███████ ██ ██████  ███████ ██████  ██████  ██   ██"
        };
        for (size_t i = 0; i < header.size(); ++i) {
            left_lines.push_back(get_gradient_color(current_tc, header.size(), i) + header[i] + "\033[0m");
        }
        left_lines.push_back("");
        left_lines.push_back("");
        left_lines.push_back("Press [" + AC + "q\033[0m] to quit/back, [" + AC + "UP/DOWN\033[0m] to navigate, [" + AC + "ENTER\033[0m] to confirm.");
        left_lines.push_back("");
        left_lines.push_back(AC + "=== SETTINGS ===\033[0m");
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
            "2. Custom Image Art",
            "3. Custom ASCII Text File"
        };
        std::vector<std::string> custom_img_opts = {
            "1. Set Image Path Manually",
            "2. Choose Image (File Manager)",
            "3. Live Resize & Align Image"
        };
        std::vector<std::string> custom_txt_opts = {
            "1. Set ASCII Text File Path",
            "2. Generator URL (asciiart.website)",
            "3. Paste/Edit ASCII Art (Opens Editor)"
        };
        std::vector<std::string> rem_opts = {
            "1. Set New Reminder",
            "2. Remove Reminder"
        };

        if (state == 0) { // Main Menu
            for (size_t i = 0; i < main_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? AC + "> " : "  ");
                left_lines.push_back(prefix + main_opts[i] + "\033[0m");
            }
        } 
        else if (state == 1 || state == 2) { // Info or Appearance
            std::vector<MenuItem>& menu = (state == 1) ? info_menu : app_menu;
            for (size_t i = 0; i < menu.size(); ++i) {
                std::ostringstream ss;
                ss << (i == (size_t)selected ? AC + "> " : "  ");
                ss << std::left << std::setw(20) << menu[i].label << "\033[0m";
                std::string default_val = "0";
                if (menu[i].options.empty()) {
                    for (auto& mod : SysInfo::get_registry()) {
                        if (mod.config_key == menu[i].key) {
                            default_val = mod.default_enabled ? "1" : "0";
                            break;
                        }
                    }
                } else if (!menu[i].options.empty()) {
                    default_val = menu[i].options[0];
                }
                
                std::string current_val = cfg.get_string(menu[i].key, default_val);
                if (menu[i].options.empty()) {
                    if (current_val.empty()) current_val = "1";
                    bool is_on = (current_val == "1" || current_val == "true" || current_val == "yes" || current_val == "on");
                    ss << (is_on ? AC + "[ ON  ]" : "\033[1;31m[ OFF ]") << "\033[0m";
                } else {
                    if (current_val.empty()) current_val = menu[i].options[0];
                    ss << AC << "< " << current_val << " >\033[0m";
                }
                left_lines.push_back(ss.str());
            }
        }
        else if (state == 3) { // Side Art
            for (size_t i = 0; i < side_art_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? AC + "> " : "  ");
                left_lines.push_back(prefix + side_art_opts[i] + "\033[0m");
            }
        }
        else if (state == 6) { // Custom Image Art
            for (size_t i = 0; i < custom_img_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? AC + "> " : "  ");
                if ((i == 2 || i == 3) && cfg.get_string("image_path", "").empty()) {
                    left_lines.push_back(prefix + "\033[1;30m" + custom_img_opts[i] + " (Requires Custom Image)\033[0m");
                } else {
                    left_lines.push_back(prefix + custom_img_opts[i] + "\033[0m");
                }
            }
        }
        else if (state == 7) { // Custom ASCII Text File
            for (size_t i = 0; i < custom_txt_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? AC + "> " : "  ");
                left_lines.push_back(prefix + custom_txt_opts[i] + "\033[0m");
            }
        }
        else if (state == 4) { // Live Resize & Align
            left_lines.push_back("");
            left_lines.push_back(AC + "=== Live Resize & Align ===\033[0m");
            left_lines.push_back("[" + AC + "UP/DOWN\033[0m] Stretch Height");
            left_lines.push_back("[" + AC + "LEFT/RIGHT\033[0m] Stretch Width");
            left_lines.push_back("[" + AC + "W/A/S/D\033[0m] Move Image");
            left_lines.push_back("[" + AC + "ENTER or ESC\033[0m] Finish");
            left_lines.push_back("");
            left_lines.push_back("\033[1;30mX Offset: " + std::to_string(cfg.get_int("image_pad_x", 0)) + " | Y Offset: " + std::to_string(cfg.get_int("image_pad_y", 0)) + "\033[0m");
            left_lines.push_back("\033[1;30mX Scale: " + cfg.get_string("image_scale_x", "1.0") + " | Y Scale: " + cfg.get_string("image_scale_y", "1.0") + "\033[0m");
        }
        else if (state == 5) { // Reminder Box
            for (size_t i = 0; i < rem_opts.size(); ++i) {
                std::string prefix = (i == (size_t)selected ? AC + "> " : "  ");
                left_lines.push_back(prefix + rem_opts[i] + "\033[0m");
            }
        }
        std::vector<std::string> right_lines = generate_preview(cfg);
        right_lines.insert(right_lines.begin(), AC + "=== LIVE PREVIEW ===\033[0m");
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
        
        if (key == 27) { // ESC
            break;
        }
        if (key == 113) { // q
            if (state == 0) break;
            else if (state == 4) { state = 6; selected = 2; continue; }
            else { state = 0; selected = 0; continue; }
        }

        if (state == 4) { // Live Resize & Align (Process ALL keys)
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
            } else if (key == 13 || key == 10) {
                state = 6; selected = 2; // Return to Custom Image Art
            }
            continue;
        }
        if (key == 1001) { // UP
            int s_max = (state == 0) ? main_opts.size() : (state == 3 ? side_art_opts.size() : (state == 6 ? custom_img_opts.size() : (state == 7 ? custom_txt_opts.size() : (state == 5 ? rem_opts.size() : ((state == 1) ? info_menu.size() : app_menu.size())))));
            selected = (selected > 0) ? selected - 1 : s_max - 1;
        }
        if (key == 1002) { // DOWN
            int s_max = (state == 0) ? main_opts.size() : (state == 3 ? side_art_opts.size() : (state == 6 ? custom_img_opts.size() : (state == 7 ? custom_txt_opts.size() : (state == 5 ? rem_opts.size() : ((state == 1) ? info_menu.size() : app_menu.size())))));
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
                        cfg.settings[m.key] = m.options[idx];
                        if (m.key == "header_type" && m.options[idx] == "figlet") {
                            std::cout << "\033[" << (left_lines.size() + 2) << ";1H\033[1;36mEnter Figlet Text: \033[0m\033[?25h";
                            std::string txt;
                            std::getline(std::cin, txt);
                            std::cout << "\033[?25l";
                            if (!txt.empty()) cfg.settings["header_text"] = txt;
                        }
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
                        cfg.settings["ascii_path"] = "";
                        state = 0; selected = 0;
                    } else if (selected == 1) {
                        cfg.settings["ascii_path"] = "";
                        state = 6; selected = 0;
                    } else if (selected == 2) {
                        cfg.settings["image_path"] = "";
                        state = 7; selected = 0;
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
                    }
                    if (state == 6) { state = 3; selected = 1; }
                }
            }
            else if (state == 7) { // Custom ASCII Text File
                if (key == 13 || key == 10) {
                    if (selected == 0) {
                        std::cout << "\033[" << (left_lines.size() + 2) << ";1H";
                        std::cout << "\033[1;36mEnter path to ASCII text file: \033[0m";
                        std::cout << "\033[?25h";
                        std::string path;
                        std::getline(std::cin, path);
                        std::cout << "\033[?25l";
                        if (!path.empty()) cfg.settings["ascii_path"] = path;
                    } else if (selected == 1) {
                        std::cout << "\033[" << (left_lines.size() + 2) << ";1H\033[1;36mVisit: https://asciiart.website/convert.php\033[0m\n";
#ifdef _WIN32
                        system("start https://asciiart.website/convert.php");
#else
                        system("xdg-open https://asciiart.website/convert.php 2>/dev/null");
#endif
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    } else if (selected == 2) {
                        std::string ascii_file = "custom_ascii.txt";
                        size_t slash = config_path.find_last_of("/\\");
                        if (slash != std::string::npos) {
                            ascii_file = config_path.substr(0, slash) + "/custom_ascii.txt";
                        }
                        
                        std::cout << "\033[?1049l\033[?25h\033[?1000l\033[?1007l";
                        std::cout.flush();
                        
#ifdef _WIN32
                        std::string cmd = "notepad \"" + ascii_file + "\"";
#else
                        std::string editor = "nano";
                        if (std::getenv("EDITOR")) editor = std::getenv("EDITOR");
                        std::string cmd = editor + " \"" + ascii_file + "\"";
#endif
                        int res = system(cmd.c_str());
                        (void)res;
                        
                        std::cout << "\033[?1049h\033[?25l\033[?1000l\033[?1007l";
                        std::cout.flush();
                        
                        cfg.settings["ascii_path"] = ascii_file;
                    }
                    if (state == 7) { state = 3; selected = 2; }
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
    }
    
    std::cout << "\033[?1049l"; // Exit alternate screen buffer
    std::cout.flush();
    std::cout << "\033[?25h\033[?1000l\033[?1007l\033[0m"; // Show cursor, disable mouse, reset formatting
    std::cout.flush();
#ifndef _WIN32
    int res = system("stty sane 2>/dev/null");
    (void)res;
#endif
    cfg.save_to_file(config_path);
    std::cout << "\033[1;32mSettings saved to " << config_path << "!\033[0m\n";
    std::cout.flush();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    
    try {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::cout << "CLI DECOR v1.0.0\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "CLI DECOR — neofetch replacement (C++ engine)\n\n"
                      << "Usage:\n"
                      << "  clidecor              run normally (print preview)\n"
                      << "  clidecor -p, --preview force print preview\n"
                      << "  clidecor -s, --settings open interactive settings menu\n"
                      << "  clidecor -t, --theme <name> change theme (e.g. dracula)\n"
                      << "  clidecor --remind \"text\"  update reminder box text\n"
                      << "  clidecor --refresh    clear cache and re-render\n"
                      << "  clidecor --update     fetch and compile latest version\n"
                      << "  clidecor -v, --version show version number\n"
                      << "  clidecor -h, --help   show this message\n\n"
                      << "Config: ~/.config/clidecor/config.conf\n";
            return 0;
        } else if (arg == "--refresh") {
            ImgRender::clear_image_cache();
            std::cout << "\033[1;32mImage cache cleared.\033[0m\n";
            return 0;
        } else if (arg == "--settings" || arg == "-s") {
            std::string config_path = Config::get_default_config_path();
            Config cfg = Config::load_from_file(config_path);
            run_settings_menu(cfg, config_path);
            return 0;
        } else if (arg == "-t" || arg == "--theme") {
            if (argc > 2) {
                std::string theme_name = argv[2];
                std::string config_path = Config::get_default_config_path();
                Config cfg = Config::load_from_file(config_path);
                cfg.settings["theme"] = theme_name;
                cfg.save_to_file(config_path);
                std::cout << "\033[1;32mTheme updated to: " << theme_name << "\033[0m\n";
            } else {
                std::cout << "Usage: clidecor -t <theme_name>\n";
            }
            return 0;
        } else if (arg == "--remind") {
            if (argc > 2) {
                std::string text = argv[2];
                std::string config_path = Config::get_default_config_path();
                Config cfg = Config::load_from_file(config_path);
                cfg.settings["reminder_text"] = text;
                cfg.save_to_file(config_path);
                std::cout << "\033[1;32mReminder updated.\033[0m\n";
            } else {
                std::cout << "Usage: clidecor --remind \"your text\"\n";
            }
            return 0;
        } else if (arg == "-p" || arg == "--preview") {
            // Fall through to the preview generation at the bottom of main
        } else if (arg == "--update" || arg == "update") {
            std::cout << "\033[1;33mAre you sure you want to update CLIdecor to the latest version?\033[0m\n";
            std::cout << "Repository: https://github.com/sujith-himself/CLIdecor\n";
            std::cout << "[y/N]: ";
            std::string ans;
            std::getline(std::cin, ans);
            if (ans != "y" && ans != "Y") {
                std::cout << "Update cancelled.\n";
                return 0;
            }
            std::cout << "\033[1;36m[\xe2\x86\x93] Fetching latest CLIdecor from GitHub...\033[0m\n";
            #ifdef _WIN32
            int res = system("git clone https://github.com/sujith-himself/CLIdecor.git %TEMP%\\clidecor_update 2>nul");
            if (res == 0) {
                std::cout << "\033[1;32m[\xe2\x9c\x93] Downloaded successfully. Compiling...\033[0m\n";
                system("cd %TEMP%\\clidecor_update && powershell -ExecutionPolicy Bypass -File install.ps1");
                system("rmdir /s /q %TEMP%\\clidecor_update");
                std::cout << "\033[1;32m[\xe2\x9c\x93] Update Complete!\033[0m\n";
            } else {
                std::cout << "\033[1;31m[\xe2\x9c\x97] Failed to clone repository. Ensure git is installed.\033[0m\n";
            }
            #else
            int res = system("git clone https://github.com/sujith-himself/CLIdecor.git /tmp/clidecor_update 2>/dev/null");
            if (res == 0) {
                std::cout << "\033[1;32m[\xe2\x9c\x93] Downloaded successfully. Compiling...\033[0m\n";
                system("cd /tmp/clidecor_update && bash install.sh");
                system("rm -rf /tmp/clidecor_update");
                std::cout << "\033[1;32m[\xe2\x9c\x93] Update Complete!\033[0m\n";
            } else {
                std::cout << "\033[1;31m[\xe2\x9c\x97] Failed to clone repository. Ensure git is installed.\033[0m\n";
            }
            #endif
            return 0;
        } else if (arg == "export") {
            std::string config_path = Config::get_default_config_path();
            std::ifstream file(config_path);
            std::ostringstream ss;
            ss << file.rdbuf();
            std::string raw = ss.str();
            
            // basic base64 encode
            std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            int val = 0, valb = -6;
            for (char c : raw) {
                val = (val << 8) + (unsigned char)c;
                valb += 8;
                while (valb >= 0) {
                    out.push_back(b64[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
            while (out.size() % 4) out.push_back('=');
            
            std::cout << "\033[1;36mShareable Config String:\033[0m\n" << out << "\n";
            return 0;
        } else if (arg == "import" && argc > 2) {
            std::string in = argv[2];
            std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string raw;
            std::vector<int> T(256, -1);
            for (int i=0; i<64; i++) T[b64[i]] = i;
            int val=0, valb=-8;
            for (char c : in) {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    raw.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            std::string config_path = Config::get_default_config_path();
            std::ofstream file(config_path);
            file << raw;
            std::cout << "\033[1;32mConfig Imported Successfully!\033[0m\n";
            return 0;
        } else if (arg == "-live") {
            std::string config_path = Config::get_default_config_path();
            Config cfg = Config::load_from_file(config_path);
            std::cout << "\033[?25l"; // Hide cursor
            while (true) {
                std::cout << "\033[2J\033[H"; // Clear screen and move to home
                std::vector<std::string> output = generate_preview(cfg);
                for (const auto& line : output) {
                    std::cout << line << "\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(1)); // 1 second updates
            }
            return 0;
        } else if (arg == "--desktop") {
            std::string config_path = Config::get_default_config_path();
            Config cfg = Config::load_from_file(config_path);
            
            #ifdef _WIN32
            HWND hwnd = GetConsoleWindow();
            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_VSCROLL | WS_HSCROLL);
            SetWindowLong(hwnd, GWL_STYLE, style);
            
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
            SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
            
            HWND progman = FindWindow("Progman", NULL);
            SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
            
            HWND workerw = NULL;
            EnumWindows([](HWND tophandle, LPARAM topparamhandle) -> BOOL {
                HWND p = FindWindowEx(tophandle, NULL, "SHELLDLL_DefView", NULL);
                if (p != NULL) {
                    *(HWND*)topparamhandle = FindWindowEx(NULL, tophandle, "WorkerW", NULL);
                }
                return TRUE;
            }, (LPARAM)&workerw);
            
            if (workerw != NULL) {
                SetParent(hwnd, workerw);
            } else {
                SetParent(hwnd, progman);
            }
            SetWindowPos(hwnd, HWND_BOTTOM, 100, 100, 800, 800, SWP_SHOWWINDOW);
            #endif

            std::cout << "\033[?25l"; // Hide cursor
            while (true) {
                std::cout << "\033[2J\033[H"; // Clear screen and move to home
                std::vector<std::string> output = generate_preview(cfg);
                for (const auto& line : output) {
                    std::cout << line << "\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep to prevent massive CPU usage
            }
            return 0;
        }
    }

    std::string config_path = Config::get_default_config_path();
    Config cfg = Config::load_from_file(config_path);

    std::vector<std::string> output = generate_preview(cfg);
    
    for (const auto& line : output) {
        std::cout << line << "\n";
    }
    } catch (const std::exception& e) {
        std::cerr << "\n\033[1;31mCLI DECOR CRASHED:\033[0m " << e.what() << "\n";
        std::cerr << "Please check your config.conf or report this issue on GitHub.\n";
        return 1;
    } catch (...) {
        std::cerr << "\n\033[1;31mCLI DECOR CRASHED with an unknown error.\033[0m\n";
        return 1;
    }

    return 0;
}
