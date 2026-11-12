#include "../Array.h"
#include "../ResourceLimits.hpp"

#include <cassert>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {
void writeText(const fs::path &file, const std::string &text) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file);
    out << text;
}

// The oceanmodel_data arrays as the native and structured-grid adapters allocate them
std::uint64_t allocatedWindowBytes(const ResourceLimits::ForcingDimensions &d, unsigned int records) {
    unsigned int t=records, s=d.sRho, w=d.sW, eta=d.etaRho, xi=d.xiRho;
    Array::Array1<double> oceanTime(t), sRho(s,-(int)s+1), sW(w,-(int)w+1), depthIntervals(w,-(int)w+2);
    Array::Array2<double> mask(eta,xi), lon(eta,xi), lat(eta,xi), lonRad(eta,xi), latRad(eta,xi), h(eta,xi);
    Array::Array3<float> zeta(t,eta,xi);
    Array::Array4<float> u(t,s,eta,xi,0,-(int)s+1,0,0), v(t,s,eta,xi,0,-(int)s+1,0,0);
    Array::Array4<float> wv(t,w,eta,xi,0,-(int)w+1,0,0), akt(t,w,eta,xi,0,-(int)w+1,0,0);
    std::uint64_t doubles=(std::uint64_t)oceanTime.Size()+sRho.Size()+sW.Size()+depthIntervals.Size()+
                          mask.Size()+lon.Size()+lat.Size()+lonRad.Size()+latRad.Size()+h.Size();
    std::uint64_t floats=(std::uint64_t)zeta.Size()+u.Size()+v.Size()+wv.Size()+akt.Size();
    return doubles*sizeof(double)+floats*sizeof(float);
}
}

int main() {
    // Count guard: INT_MAX fits, one more stops with the limit named
    assert(ResourceLimits::particleCount(0, "count")==0);
    assert(ResourceLimits::particleCount((std::size_t)INT_MAX, "count")==INT_MAX);
    for (std::size_t mocked: {(std::size_t)INT_MAX+1, (std::size_t)2160000000ull}) {
        bool rejected=false;
        try { ResourceLimits::particleCount(mocked, "The particle count"); }
        catch (const std::runtime_error &e) {
            rejected=std::string(e.what()).find("INT_MAX=2147483647")!=std::string::npos &&
                     std::string(e.what()).find(std::to_string(mocked))!=std::string::npos;
        }
        assert(rejected);
    }

    // Forcing estimate equals the bytes of the arrays the adapters allocate
    ResourceLimits::ForcingDimensions dims;
    assert(!dims.known() && ResourceLimits::residentForcingBytes(dims, true)==0);
    dims.oceanTime=2; dims.sRho=3; dims.sW=4; dims.etaRho=5; dims.xiRho=6;
    assert(ResourceLimits::oceanWindowBytes(dims, 2)==5144);
    for (unsigned int records: {1u, 2u, 3u}) {
        assert(ResourceLimits::oceanWindowBytes(dims, records)==allocatedWindowBytes(dims, records));
    }
    assert(ResourceLimits::residentForcingBytes(dims, false)==allocatedWindowBytes(dims, 2));
    assert(ResourceLimits::residentForcingBytes(dims, true)==allocatedWindowBytes(dims, 3)+allocatedWindowBytes(dims, 2));

    // Sarno native window (2 x 30 x 1135 x 1528): 4.13 GiB per rank, measured 3.8-5.1 GiB
    ResourceLimits::ForcingDimensions sarno;
    sarno.oceanTime=2; sarno.sRho=30; sarno.sW=31; sarno.etaRho=1135; sarno.xiRho=1528;
    std::uint64_t sarnoBytes=ResourceLimits::residentForcingBytes(sarno, true);
    assert(sarnoBytes==4432821192ull);
    assert(ResourceLimits::gib(sarnoBytes)=="4.13 GiB");
    assert(ResourceLimits::gib(3ull<<19)=="1.50 MiB");

    // Verdicts: unknown without a limit, warn above 80%, exceed above 100%
    ResourceLimits::MemoryLimit limit{1000, "test"};
    assert(ResourceLimits::assess(500, ResourceLimits::MemoryLimit())==ResourceLimits::MemoryVerdict::Unknown);
    assert(ResourceLimits::assess(800, limit)==ResourceLimits::MemoryVerdict::Fits);
    assert(ResourceLimits::assess(801, limit)==ResourceLimits::MemoryVerdict::Warn);
    assert(ResourceLimits::assess(1000, limit)==ResourceLimits::MemoryVerdict::Warn);
    assert(ResourceLimits::assess(1001, limit)==ResourceLimits::MemoryVerdict::Exceeds);
    // 28 Sarno ranks exceed the 56 GiB hhcpu default grant; 4 ranks fit
    ResourceLimits::MemoryLimit grant{56ull<<30, "grant"};
    assert(ResourceLimits::assess(28*sarnoBytes, grant)==ResourceLimits::MemoryVerdict::Exceeds);
    assert(ResourceLimits::assess(4*sarnoBytes, grant)==ResourceLimits::MemoryVerdict::Fits);

    fs::path root=fs::temp_directory_path()/"wacomm_resource_limits_test";
    fs::remove_all(root);

    // cgroup v2: the job limit applies, an unlimited ancestor does not
    writeText(root/"v2/cgroup/slurm/memory.max", "max\n");
    writeText(root/"v2/cgroup/slurm/job_1/memory.max", "60129542144\n");
    writeText(root/"v2/cgroup/slurm/job_1/step_0/memory.max", "max\n");
    writeText(root/"v2/proc/self/cgroup", "0::/slurm/job_1/step_0\n");
    writeText(root/"v2/proc/meminfo", "MemTotal:       250000000 kB\nMemAvailable:   240000000 kB\n");
    ResourceLimits::MemoryLimit v2=ResourceLimits::processMemoryLimit((root/"v2/proc").string(), (root/"v2/cgroup").string());
    assert(v2.bytes==60129542144ull && v2.source.find("job_1/memory.max")!=std::string::npos);

    // cgroup v1: the tightest ancestor applies, the page-rounded maximum means unlimited
    writeText(root/"v1/cgroup/memory/slurm/uid_1/job_2/memory.limit_in_bytes", "4294967296\n");
    writeText(root/"v1/cgroup/memory/slurm/uid_1/job_2/step_0/memory.limit_in_bytes", "9223372036854771712\n");
    writeText(root/"v1/proc/self/cgroup", "11:cpuset:/slurm/uid_1/job_2/step_0\n5:memory,hugetlb:/slurm/uid_1/job_2/step_0\n");
    ResourceLimits::MemoryLimit v1=ResourceLimits::processMemoryLimit((root/"v1/proc").string(), (root/"v1/cgroup").string());
    assert(v1.bytes==4294967296ull && v1.source.find("job_2/memory.limit_in_bytes")!=std::string::npos);

    // MemAvailable is used when it is tighter than the cgroup limit
    writeText(root/"v1/proc/meminfo", "MemAvailable:    1048576 kB\n");
    v1=ResourceLimits::processMemoryLimit((root/"v1/proc").string(), (root/"v1/cgroup").string());
    assert(v1.bytes==1073741824ull && v1.source=="/proc/meminfo MemAvailable");

    // No /proc (macOS, Windows): the limit is unknown
    ResourceLimits::MemoryLimit none=ResourceLimits::processMemoryLimit((root/"missing").string(), (root/"missing").string());
    assert(!none.known());

    fs::remove_all(root);
}
