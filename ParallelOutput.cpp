//
// Created by Raffaele Montella on 12/11/26.
//

#include "ParallelOutput.hpp"

#ifdef WACOMM_PARALLEL_NETCDF

#include "MpiHelpers.hpp"
#include "OutputWriter.hpp"

#include <netcdf.h>
#include <netcdf_par.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

namespace fs = std::filesystem;

namespace {

    log4cplus::Logger logger() {
        return log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
    }

    // MPI_Info from io.mpiio_hints; the caller frees it
    MPI_Info info(const std::map<std::string,std::string> &hints) {
        MPI_Info result;
        MPI_Info_create(&result);
        for (const auto &hint: hints) MPI_Info_set(result, hint.first.c_str(), hint.second.c_str());
        return result;
    }

    // All ranks agree on success; a failure on any rank fails every rank
    void agree(MPI_Comm comm, const std::string &fileName, const std::string &stage, std::string &error) {
        int failed=error.empty() ? 0 : 1, anyFailed=0;
        MPI_Allreduce(&failed, &anyFailed, 1, MPI_INT, MPI_MAX, comm);
        if (anyFailed) {
            throw std::runtime_error("Unable to write " + fileName + " collectively (" + stage + "): " +
                                     (error.empty() ? std::string("failed on another rank") : error));
        }
    }

    void check(int status, const char *what) {
        if (status!=NC_NOERR) throw std::runtime_error(std::string(what) + ": " + nc_strerror(status));
    }

    void checkMpi(int status, const char *what) {
        if (status!=MPI_SUCCESS) {
            char message[MPI_MAX_ERROR_STRING];
            int length=0;
            MPI_Error_string(status, message, &length);
            throw std::runtime_error(std::string(what) + ": " + std::string(message, length));
        }
    }

    /*
     * Atomic collective publication:
     * rank 0 creates <name>.partial with rootCreate (may be empty), every rank writes
     * with collectiveWrite, then rank 0 flushes and renames. The ranks agree after
     * each stage, so a failure on any rank stops every rank and publishes nothing.
     */
    void publish(MPI_Comm comm, const std::string &fileName, bool fsync,
                 const std::function<void(const std::string&)> &rootCreate,
                 const std::function<void(const std::string&)> &collectiveWrite) {
        int rank=0;
        MPI_Comm_rank(comm, &rank);
        std::string partial=OutputWriter::partialName(fileName);
        auto start=MPI_Wtime();

        std::string error;
        if (rank==0) {
            try {
                std::error_code ec;
                fs::remove(partial, ec);
                if (rootCreate) {
                    std::lock_guard<std::mutex> lock(OutputWriter::netcdfMutex());
                    rootCreate(partial);
                }
            } catch (const std::exception &e) { error=e.what(); }
        }
        agree(comm, fileName, "create", error);

        try {
            collectiveWrite(partial);
        } catch (const std::exception &e) { error=e.what(); }
        try {
            agree(comm, fileName, "write", error);
        } catch (...) {
            if (rank==0) { std::error_code ec; fs::remove(partial, ec); }
            throw;
        }

        if (rank==0) {
            try {
                OutputWriter::commit(partial, fileName, fsync);
            } catch (const std::exception &e) {
                error=e.what();
                std::error_code ec;
                fs::remove(partial, ec);
            }
        }
        agree(comm, fileName, "publish", error);

        if (rank==0) {
            LOG4CPLUS_INFO(logger(), "Published " << fileName << " collectively in " << (MPI_Wtime()-start) << " seconds");
        }
    }

    // Redistribute the rank-0 particle set in balanced contiguous blocks
    Particles scatterParticles(MPI_Comm comm, Particles *rootParticles) {
        int rank=0, size=1;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);

        std::uint64_t n=rank==0 ? rootParticles->size() : 0;
        MPI_Bcast(&n, 1, MPI_UINT64_T, 0, comm);

        std::vector<int> counts, displs;
        ParallelOutput::blocks(n, size, counts, displs);

        std::vector<particle_data> sendbuf;
        if (rank==0) {
            sendbuf.resize(n);
            for (std::size_t idx=0; idx<n; idx++) sendbuf[idx]=rootParticles->at(idx).data();
        }
        std::vector<particle_data> recvbuf(counts[rank]);

        MPI_Datatype mpiParticleData=MpiHelpers::particleDataType();
        MPI_Scatterv(sendbuf.data(), counts.data(), displs.data(), mpiParticleData,
                     recvbuf.data(), counts[rank], mpiParticleData, 0, comm);
        MPI_Type_free(&mpiParticleData);

        Particles local;
        for (const auto &data: recvbuf) local.push_back(Particle(data));
        return local;
    }

    // Exclusive prefix sum and total of a per-rank count
    void offsets(MPI_Comm comm, std::uint64_t local, std::uint64_t &offset, std::uint64_t &total) {
        int rank=0;
        MPI_Comm_rank(comm, &rank);
        offset=0;
        MPI_Exscan(&local, &offset, 1, MPI_UINT64_T, MPI_SUM, comm);
        if (rank==0) offset=0;
        MPI_Allreduce(&local, &total, 1, MPI_UINT64_T, MPI_SUM, comm);
    }

    // Open the rank-0 file for collective parallel NetCDF-4 writes
    int openParallel(MPI_Comm comm, const std::string &path, const std::map<std::string,std::string> &hints) {
        MPI_Info mpiInfo=info(hints);
        int ncid=-1;
        int status=nc_open_par(path.c_str(), NC_WRITE, comm, mpiInfo, &ncid);
        MPI_Info_free(&mpiInfo);
        check(status, "nc_open_par (requires NetCDF-C with parallel HDF5)");
        return ncid;
    }

    int collectiveVar(int ncid, const char *name) {
        int varid=-1;
        check(nc_inq_varid(ncid, name, &varid), name);
        check(nc_var_par_access(ncid, varid, NC_COLLECTIVE), name);
        return varid;
    }

    void closeParallel(int ncid, bool fsync) {
        // nc_sync flushes the HDF5 file (MPI_File_sync) on every rank
        if (fsync) check(nc_sync(ncid), "nc_sync");
        check(nc_close(ncid), "nc_close");
    }
}

void ParallelOutput::blocks(std::size_t n, int size, std::vector<int> &counts, std::vector<int> &displs) {
    counts.assign(size, 0);
    displs.assign(size, 0);
    std::size_t base=n/size, extra=n%size, offset=0;
    for (int r=0; r<size; r++) {
        std::size_t count=base+((std::size_t)r<extra ? 1 : 0);
        if (count>(std::size_t)std::numeric_limits<int>::max() || offset>(std::size_t)std::numeric_limits<int>::max())
            throw std::runtime_error("collective output: particle block exceeds the MPI count range");
        counts[r]=(int)count;
        displs[r]=(int)offset;
        offset+=count;
    }
}

void ParallelOutput::broadcast(MPI_Comm comm, std::string &value) {
    int rank=0;
    MPI_Comm_rank(comm, &rank);
    std::uint64_t length=rank==0 ? value.size() : 0;
    MPI_Bcast(&length, 1, MPI_UINT64_T, 0, comm);
    if (rank!=0) value.assign(length, '\0');
    if (length>0) MPI_Bcast(&value[0], (int)length, MPI_CHAR, 0, comm);
}

void ParallelOutput::writeTextHistory(MPI_Comm comm, const std::string &fileName, Particles *rootParticles,
                                      bool fsync, const std::map<std::string,std::string> &hints) {
    int rank=0;
    MPI_Comm_rank(comm, &rank);

    // Format the local block with the serial text layout; rank 0 adds the header
    Particles local=scatterParticles(comm, rootParticles);
    std::uint64_t total=0, ignored=0;
    offsets(comm, local.size(), ignored, total);
    std::ostringstream text;
    if (rank==0) Particles::writeTxtHeader(text, total);
    for (std::size_t idx=0; idx<local.size(); idx++) Particles::writeTxtRecord(text, local.at(idx));
    std::string bytes=text.str();

    std::uint64_t offset=0, size=0;
    offsets(comm, bytes.size(), offset, size);

    publish(comm, fileName, fsync, nullptr, [&](const std::string &path) {
        MPI_Info mpiInfo=info(hints);
        MPI_File file;
        int status=MPI_File_open(comm, path.c_str(), MPI_MODE_CREATE | MPI_MODE_WRONLY, mpiInfo, &file);
        MPI_Info_free(&mpiInfo);
        checkMpi(status, "MPI_File_open");

        std::string error;
        try {
            checkMpi(MPI_File_set_size(file, 0), "MPI_File_set_size");

            // Collective writes in chunks within the MPI int count range
            const std::uint64_t chunk=1ull<<30;
            std::uint64_t chunks=(bytes.size()+chunk-1)/chunk, maxChunks=0;
            MPI_Allreduce(&chunks, &maxChunks, 1, MPI_UINT64_T, MPI_MAX, comm);
            for (std::uint64_t c=0; c<maxChunks; c++) {
                std::uint64_t first=std::min<std::uint64_t>(c*chunk, bytes.size());
                std::uint64_t length=std::min<std::uint64_t>(chunk, bytes.size()-first);
                MPI_Status mpiStatus;
                checkMpi(MPI_File_write_at_all(file, (MPI_Offset)(offset+first), bytes.data()+first,
                                               (int)length, MPI_CHAR, &mpiStatus), "MPI_File_write_at_all");
            }
            if (fsync) checkMpi(MPI_File_sync(file), "MPI_File_sync");
        } catch (const std::exception &e) { error=e.what(); }
        MPI_File_close(&file);
        if (!error.empty()) throw std::runtime_error(error);
    });
}

void ParallelOutput::writeNetCDFHistory(MPI_Comm comm, const std::string &fileName, Particles *rootParticles,
                                        double particleTime, std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
                                        std::shared_ptr<Config> config, bool fsync,
                                        const std::map<std::string,std::string> &hints) {
    // Each rank selects the stored records of its block; the order of the blocks is the particle order
    Particles local=scatterParticles(comm, rootParticles);
    Particles::NetCDFHistory records=local.netcdfRecords(oceanModelAdapter);
    std::uint64_t offset=0, total=0;
    offsets(comm, records.id.size(), offset, total);

    publish(comm, fileName, fsync, [&](const std::string &path) {
        netCDF::NcFile dataFile(path, netCDF::NcFile::replace, netCDF::NcFile::nc4);
        Particles::defineNetCDF(dataFile, total, particleTime, config);
    }, [&](const std::string &path) {
        int ncid=openParallel(comm, path, hints);
        std::string error;
        try {
            std::size_t n=records.id.size();
            std::size_t start1[1]={(std::size_t)offset}, count1[1]={n};
            std::size_t start2[2]={(std::size_t)offset,0}, count2[2]={n,1};
            check(nc_put_vara_ulonglong(ncid, collectiveVar(ncid,"id"), start1, count1,
                                        reinterpret_cast<const unsigned long long*>(records.id.data())), "id");
            check(nc_put_vara_uint(ncid, collectiveVar(ncid,"object_type"), start1, count1, records.driftObjectType.data()), "object_type");
            check(nc_put_vara_int(ncid, collectiveVar(ncid,"drift_side"), start1, count1, records.driftSide.data()), "drift_side");
            const std::pair<const char*, const std::vector<double>*> doubles[]={
                    {"lat",&records.lat},{"lon",&records.lon},{"depth",&records.dep},{"k",&records.k},
                    {"j",&records.j},{"i",&records.i},{"health",&records.health},{"age",&records.age},
                    {"time",&records.time}};
            for (const auto &variable: doubles)
                check(nc_put_vara_double(ncid, collectiveVar(ncid,variable.first), start2, count2,
                                         variable.second->data()), variable.first);
            closeParallel(ncid, fsync);
        } catch (const std::exception &e) {
            error=e.what();
            nc_close(ncid);
        }
        if (!error.empty()) throw std::runtime_error(error);
    });
}

void ParallelOutput::writeConcentration(MPI_Comm comm, const std::string &fileName, Array::Array4<float> *rootConc,
                                        std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
                                        std::function<void(const std::string&)> rootDefine, bool fsync,
                                        const std::map<std::string,std::string> &hints) {
    int rank=0, size=1;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // conc(ocean_time, s_rho, eta_rho, xi_rho) is split in blocks of eta_rho rows
    int nt=(int)oceanModelAdapter->OceanTime().Nx();
    int ns=(int)oceanModelAdapter->SRho().Nx();
    int neta=(int)oceanModelAdapter->Mask().Nx();
    int nxi=(int)oceanModelAdapter->Mask().Ny();
    std::vector<int> rows, firstRow;
    blocks(neta, size, rows, firstRow);

    if ((std::size_t)nt*ns*rows[0]*nxi>(std::size_t)std::numeric_limits<int>::max())
        throw std::runtime_error("collective output: concentration block exceeds the MPI count range");
    std::vector<float> slab((std::size_t)nt*ns*rows[rank]*nxi);
    int sizes[4]={nt,ns,neta,nxi};
    if (rank==0) {
        const float *base=(*rootConc)();
        std::vector<MPI_Request> requests;
        std::vector<MPI_Datatype> types;
        for (int r=1; r<size; r++) {
            if (rows[r]==0) continue;
            int subsizes[4]={nt,ns,rows[r],nxi}, starts[4]={0,0,firstRow[r],0};
            MPI_Datatype type;
            MPI_Type_create_subarray(4, sizes, subsizes, starts, MPI_ORDER_C, MPI_FLOAT, &type);
            MPI_Type_commit(&type);
            requests.emplace_back();
            MPI_Isend(base, 1, type, r, 0, comm, &requests.back());
            types.push_back(type);
        }
        // Rank 0 block
        std::size_t idx=0;
        for (int t=0; t<nt; t++) for (int s=0; s<ns; s++) for (int j=0; j<rows[0]; j++) {
            const float *row=base+(((std::size_t)t*ns+s)*neta+firstRow[0]+j)*nxi;
            std::memcpy(slab.data()+idx, row, sizeof(float)*nxi);
            idx+=nxi;
        }
        MPI_Waitall((int)requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        for (auto &type: types) MPI_Type_free(&type);
    } else if (rows[rank]>0) {
        MPI_Recv(slab.data(), (int)slab.size(), MPI_FLOAT, 0, 0, comm, MPI_STATUS_IGNORE);
    }

    publish(comm, fileName, fsync, rootDefine, [&](const std::string &path) {
        int ncid=openParallel(comm, path, hints);
        std::string error;
        try {
            std::size_t start[4]={0,0,(std::size_t)firstRow[rank],0};
            std::size_t count[4]={(std::size_t)nt,(std::size_t)ns,(std::size_t)rows[rank],(std::size_t)nxi};
            check(nc_put_vara_float(ncid, collectiveVar(ncid,"conc"), start, count, slab.data()), "conc");
            closeParallel(ncid, fsync);
        } catch (const std::exception &e) {
            error=e.what();
            nc_close(ncid);
        }
        if (!error.empty()) throw std::runtime_error(error);
    });
}

#endif
