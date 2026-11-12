// Collective output contract (docs/io.md#collective-writes), run on 1, 2, and 3 MPI ranks:
//   - a text history written with MPI-IO equals Particles::saveAsTxt byte for byte;
//   - a NetCDF history written with parallel NetCDF-4 equals Particles::saveAsNetCDF in
//     every attribute, dimension, and variable, including the out-of-domain filtering;
//   - gridded counts split along eta_rho equal the serial array exactly, also when a rank
//     receives no rows;
//   - empty particle sets, MPI-IO hints, and fsync are accepted;
//   - a failure on rank 0 stops every rank and publishes nothing, without a hang;
//   - no ".partial" file is left behind.

#include "../ParallelOutput.hpp"
#include "../OutputWriter.hpp"

#ifndef WACOMM_PARALLEL_NETCDF
#error "parallel_output_test requires USE_MPI and NetCDF-C with parallel HDF5"
#endif

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

namespace fs = std::filesystem;

namespace {

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

std::shared_ptr<OceanModelAdapter> fixture(int nj, int ni) {
    auto adapter=std::make_shared<FixtureAdapter>();
    adapter->OceanTime().Allocate(2); adapter->OceanTime()(0)=1.0e9; adapter->OceanTime()(1)=1.0e9+3600;
    adapter->SRho().Allocate(2,-1); adapter->SRho()(-1)=-.75; adapter->SRho()(0)=-.25;
    adapter->SW().Allocate(3,-2); adapter->SW()(-2)=-1; adapter->SW()(-1)=-.5; adapter->SW()(0)=0;
    adapter->Mask().Allocate(nj,ni); adapter->Mask()=1.0;
    adapter->Lon().Allocate(nj,ni); adapter->Lat().Allocate(nj,ni); adapter->H().Allocate(nj,ni);
    for (int j=0;j<nj;j++) for (int i=0;i<ni;i++) {
        adapter->Lon()(j,i)=14.0+0.01*i; adapter->Lat()(j,i)=40.7+0.01*j; adapter->H()(j,i)=100;
    }
    adapter->Zeta().Allocate(2,nj,ni); adapter->Zeta()=0.0f;
    return adapter;
}

std::shared_ptr<Config> configuration(const std::string &dir) {
    int rank=0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank==0)
        std::ofstream(dir+"/config.json") << R"({"simulation":{"start":"20210701Z00"},"io":{"save_history":"nc"}})" << "\n";
    MPI_Barrier(MPI_COMM_WORLD);
    return std::make_shared<Config>(dir+"/config.json");
}

// Particles with identities above 2^53 and full-precision state; some outside the domain
Particles particles(std::size_t n) {
    Particles result;
    for (std::size_t p=0;p<n;p++) {
        double j=(p%5==3) ? -1.0 : 0.37+0.71*(p%4), i=0.11+0.53*(p%6), k=-0.1-0.13*(p%3);
        Particle particle((1ull<<60)+p*1234567ull, k, j, i, 1.0/(p+3), 17.25*p, 1.0e9+0.1*p);
        result.push_back(particle);
    }
    return result;
}

std::string content(const std::string &fileName) {
    std::ifstream input(fileName, std::ios::binary);
    assert(input);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

// Every global attribute, dimension, and variable of a NetCDF file
std::string dump(const std::string &fileName) {
    netCDF::NcFile file(fileName, netCDF::NcFile::read);
    std::ostringstream out;
    out.precision(17);
    for (auto &att: file.getAtts()) {
        out << "att " << att.first << " " << att.second.getType().getName() << " ";
        if (att.second.getType()==netCDF::ncChar) { std::string v; att.second.getValues(v); out << v; }
        else { std::vector<double> v(att.second.getAttLength()); att.second.getValues(v.data()); for (double x: v) out << x << " "; }
        out << "\n";
    }
    for (auto &dim: file.getDims()) out << "dim " << dim.first << " " << dim.second.getSize() << "\n";
    for (auto &var: file.getVars()) {
        std::size_t size=1;
        for (auto &dim: var.second.getDims()) size*=dim.getSize();
        out << "var " << var.first << " " << var.second.getType().getName() << " " << size << ":";
        if (var.second.getType()==netCDF::ncUint64) {
            std::vector<unsigned long long> v(size); if (size) var.second.getVar(v.data()); for (auto x: v) out << " " << x;
        } else {
            std::vector<double> v(size); if (size) var.second.getVar(v.data()); for (double x: v) out << " " << x;
        }
        for (auto &att: var.second.getAtts()) out << " @" << att.first;
        out << "\n";
    }
    return out.str();
}

bool noPartial(const std::string &dir) {
    for (const auto &entry: fs::directory_iterator(dir))
        if (entry.path().extension()==".partial") return false;
    return true;
}

void histories(const std::string &dir, int rank, std::size_t n, bool fsync, const std::map<std::string,std::string> &hints) {
    auto adapter=fixture(5,6);
    auto config=configuration(dir);
    Particles root=particles(n);
    std::string tag=std::to_string(n)+(fsync ? "-fsync" : "");

    if (rank==0) {
        root.saveAsTxt(dir+"/serial-"+tag+".txt");
        root.saveAsNetCDF(dir+"/serial-"+tag+".nc", 1.0e9+3600, adapter, config);
    }
    ParallelOutput::writeTextHistory(MPI_COMM_WORLD, dir+"/collective-"+tag+".txt", rank==0 ? &root : nullptr, fsync, hints);
    ParallelOutput::writeNetCDFHistory(MPI_COMM_WORLD, dir+"/collective-"+tag+".nc", rank==0 ? &root : nullptr,
                                       1.0e9+3600, adapter, config, fsync, hints);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank==0) {
        assert(content(dir+"/collective-"+tag+".txt")==content(dir+"/serial-"+tag+".txt"));
        assert(dump(dir+"/collective-"+tag+".nc")==dump(dir+"/serial-"+tag+".nc"));
        std::cout << "history n=" << n << " fsync=" << fsync << " equal\n";
    }
}

void concentration(const std::string &dir, int rank, int nj, int ni) {
    auto adapter=fixture(nj,ni);
    int nt=2, ns=2;
    Array::Array4<float> conc(nt,ns,nj,ni,0,-ns+1,0,0);
    for (int t=0;t<nt;t++) for (int k=-ns+1;k<=0;k++) for (int j=0;j<nj;j++) for (int i=0;i<ni;i++)
        conc(t,k,j,i)=(float)(1000*t+100*(k+ns)+10*j+i)+0.25f;

    // Reference: the same schema, written serially
    auto define=[&](const std::string &path, bool data) {
        netCDF::NcFile file(path, netCDF::NcFile::replace, netCDF::NcFile::nc4);
        std::vector<netCDF::NcDim> dims={file.addDim("ocean_time",nt), file.addDim("s_rho",ns),
                                         file.addDim("eta_rho",nj), file.addDim("eta_xi",ni)};
        netCDF::NcVar var=file.addVar("conc", netCDF::ncFloat, dims);
        var.putAtt("_FillValue", netCDF::ncFloat, 9.99999993e+36);
        if (data) var.putVar(conc());
    };
    std::string tag=std::to_string(nj)+"x"+std::to_string(ni);
    if (rank==0) define(dir+"/serial-conc-"+tag+".nc", true);
    ParallelOutput::writeConcentration(MPI_COMM_WORLD, dir+"/collective-conc-"+tag+".nc", rank==0 ? &conc : nullptr,
                                       adapter, [&](const std::string &path) { define(path, false); }, false, {});
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank==0) {
        assert(dump(dir+"/collective-conc-"+tag+".nc")==dump(dir+"/serial-conc-"+tag+".nc"));
        std::cout << "concentration " << tag << " equal\n";
    }
}

void failure(const std::string &dir, int rank) {
    auto adapter=fixture(5,6);
    auto config=configuration(dir);
    Particles root=particles(4);
    std::string missing=dir+"/missing-directory/history.nc";
    bool failed=false;
    try {
        ParallelOutput::writeNetCDFHistory(MPI_COMM_WORLD, missing, rank==0 ? &root : nullptr,
                                           1.0e9, adapter, config, false, {});
    } catch (const std::runtime_error &e) {
        failed=std::string(e.what()).find("Unable to write "+missing+" collectively")!=std::string::npos;
    }
    assert(failed);
    assert(!fs::exists(missing));
}

}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank=0, size=1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    assert(argc==2);
    std::string dir=argv[1];
    if (rank==0) { fs::remove_all(dir); fs::create_directories(dir); }
    MPI_Barrier(MPI_COMM_WORLD);

    std::vector<int> counts, displs;
    ParallelOutput::blocks(7, 3, counts, displs);
    assert((counts==std::vector<int>{3,2,2}) && (displs==std::vector<int>{0,3,5}));

    for (std::size_t n: {0, 1, 11}) histories(dir, rank, n, false, {});
    histories(dir, rank, 23, true, {{"romio_cb_write","enable"},{"cb_buffer_size","1048576"}});
    concentration(dir, rank, 5, 4);
    concentration(dir, rank, 1, 3);
    failure(dir, rank);

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank==0) {
        assert(noPartial(dir));
        std::cout << "collective output contract passed on " << size << " rank(s)\n";
    }
    MPI_Finalize();
    return 0;
}
