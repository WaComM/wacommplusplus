#ifndef WACOMMPLUSPLUS_RESOURCELIMITS_HPP
#define WACOMMPLUSPLUS_RESOURCELIMITS_HPP

// Resource limits checked before a run commits to them (docs/parallelism.md#resource-limits)

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ResourceLimits {

// Particle counts, displacements, and loop indices of the solver interval are int (MPI-3 counts)
inline int particleCount(std::size_t value, const std::string &what) {
    if (value>(std::size_t)INT_MAX) {
        throw std::runtime_error(what + " is " + std::to_string(value) +
                                 ", above the INT_MAX=" + std::to_string(INT_MAX) +
                                 " particle limit of the solver interval and its MPI scatter/gather counts");
    }
    return (int)value;
}

// Dimensions of one normalized ocean forcing window, read from the file header only
struct ForcingDimensions {
    std::size_t oceanTime=0, sRho=0, sW=0, etaRho=0, xiRho=0;
    bool known() const { return oceanTime>0 && sRho>0 && sW>0 && etaRho>0 && xiRho>0; }
};

// Bytes of the oceanmodel_data arrays of one window with the given number of time records
inline std::uint64_t oceanWindowBytes(const ForcingDimensions &d, std::size_t records) {
    std::uint64_t cells=(std::uint64_t)d.etaRho*d.xiRho;
    // u, v (s_rho), w, akt (s_w), and zeta as float
    std::uint64_t fields=4ull*records*cells*(2ull*d.sRho+2ull*d.sW+1ull);
    // h, mask, lon, lat, lonRad, latRad as double
    std::uint64_t grid=8ull*6ull*cells;
    // ocean_time, s_rho, s_w, and depth intervals as double
    std::uint64_t axes=8ull*((std::uint64_t)records+d.sRho+2ull*d.sW);
    return fields+grid+axes;
}

// Resident ocean forcing of one rank: the active window with its appended boundary record,
// plus the adjacent window held for that record
inline std::uint64_t residentForcingBytes(const ForcingDimensions &d, bool adjacent) {
    if (!d.known()) return 0;
    std::uint64_t bytes=oceanWindowBytes(d, d.oceanTime+(adjacent ? 1 : 0));
    if (adjacent) bytes+=oceanWindowBytes(d, d.oceanTime);
    return bytes;
}

// A memory limit and where it came from; bytes==0 means unknown
struct MemoryLimit {
    std::uint64_t bytes=0;
    std::string source;
    bool known() const { return bytes>0; }
};

inline MemoryLimit tighter(const MemoryLimit &a, const MemoryLimit &b) {
    if (!a.known()) return b;
    if (!b.known()) return a;
    return b.bytes<a.bytes ? b : a;
}

namespace detail {
    inline bool readText(const std::string &fileName, std::string &text) {
        std::ifstream in(fileName);
        if (!in) return false;
        std::ostringstream content;
        content << in.rdbuf();
        text=content.str();
        return true;
    }

    // A cgroup v1 limit of 2^60 bytes or more is the kernel's "unlimited" page-rounded maximum
    inline bool parseLimit(const std::string &text, std::uint64_t &bytes) {
        std::istringstream in(text);
        std::string token;
        if (!(in >> token) || token=="max") return false;
        if (token.find_first_not_of("0123456789")!=std::string::npos) return false;
        bytes=std::stoull(token);
        return bytes>0 && bytes<(1ull<<60);
    }
}

// Tightest memory limit of the cgroup path and its ancestors. procSelfCgroup is the content
// of /proc/self/cgroup and cgroupRoot the mount point of the unified (v2) hierarchy;
// the v1 memory controller is read from cgroupRoot/memory.
inline MemoryLimit cgroupLimit(const std::string &procSelfCgroup, const std::string &cgroupRoot) {
    MemoryLimit limit;
    std::istringstream lines(procSelfCgroup);
    std::string line;
    while (std::getline(lines, line)) {
        std::size_t first=line.find(':');
        std::size_t second=first==std::string::npos ? first : line.find(':', first+1);
        if (second==std::string::npos) continue;
        std::string controllers=line.substr(first+1, second-first-1);
        std::string path=line.substr(second+1);
        std::string base, file;
        if (controllers.empty()) {
            base=cgroupRoot; file="memory.max";
        } else if (("," + controllers + ",").find(",memory,")!=std::string::npos) {
            base=cgroupRoot + "/memory"; file="memory.limit_in_bytes";
        } else continue;
        while (true) {
            std::string dir=base + (path=="/" ? "" : path);
            std::string text;
            std::uint64_t bytes=0;
            if (detail::readText(dir + "/" + file, text) && detail::parseLimit(text, bytes)) {
                limit=tighter(limit, MemoryLimit{bytes, dir + "/" + file});
            }
            if (path.empty() || path=="/") break;
            std::size_t slash=path.find_last_of('/');
            path=slash==0 || slash==std::string::npos ? "/" : path.substr(0, slash);
        }
    }
    return limit;
}

// MemAvailable of /proc/meminfo content, in bytes
inline MemoryLimit memAvailable(const std::string &meminfo) {
    std::istringstream lines(meminfo);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("MemAvailable:", 0)!=0) continue;
        std::istringstream in(line.substr(13));
        std::uint64_t kib=0;
        std::string unit;
        if (in >> kib >> unit && unit=="kB" && kib>0) return MemoryLimit{kib*1024ull, "/proc/meminfo MemAvailable"};
    }
    return MemoryLimit();
}

// Tightest limit visible to this process; unknown where neither file exists (macOS, Windows)
inline MemoryLimit processMemoryLimit(const std::string &procRoot="/proc", const std::string &cgroupRoot="/sys/fs/cgroup") {
    std::string cgroup, meminfo;
    MemoryLimit limit;
    if (detail::readText(procRoot + "/self/cgroup", cgroup)) limit=cgroupLimit(cgroup, cgroupRoot);
    if (detail::readText(procRoot + "/meminfo", meminfo)) limit=tighter(limit, memAvailable(meminfo));
    return limit;
}

enum class MemoryVerdict { Unknown=0, Fits=1, Warn=2, Exceeds=3 };

// Warn above 80% of the limit, exceed above 100%
inline MemoryVerdict assess(std::uint64_t demand, const MemoryLimit &limit) {
    if (!limit.known() || demand==0) return MemoryVerdict::Unknown;
    if (demand>limit.bytes) return MemoryVerdict::Exceeds;
    if ((long double)demand>0.8L*(long double)limit.bytes) return MemoryVerdict::Warn;
    return MemoryVerdict::Fits;
}

// Binary units with two decimals: GiB from 1 GiB, MiB below
inline std::string gib(std::uint64_t bytes) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    if (bytes>=(1ull<<30)) out << (double)bytes/(1024.0*1024.0*1024.0) << " GiB";
    else out << (double)bytes/(1024.0*1024.0) << " MiB";
    return out.str();
}

}

#endif //WACOMMPLUSPLUS_RESOURCELIMITS_HPP
