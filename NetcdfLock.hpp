//
// Created by Raffaele Montella on 15/11/26.
//

#ifndef WACOMMPLUSPLUS_NETCDFLOCK_HPP
#define WACOMMPLUSPLUS_NETCDFLOCK_HPP

#include <atomic>
#include <mutex>
#include <thread>

// Hand the process-wide NetCDF lock over between threads (docs/parallelism.md#forcing-prefetch).
// NetCDF-C and HDF5 are not thread safe, so one lock serializes every NetCDF call of a process.
// The forcing prefetch thread holds it with a Yielding lock and gives it up between variable reads
// while a Priority lock waits: the solver thread and the output writer, which the solver waits for.
namespace NetcdfLock {

    // Number of threads waiting for the lock with priority
    inline std::atomic<int> &priorityWaiting() {
        static std::atomic<int> waiting{0};
        return waiting;
    }

    // The lock this thread holds as a yielding reader, if any
    inline std::unique_lock<std::mutex> *&yieldingLock() {
        thread_local std::unique_lock<std::mutex> *lock=nullptr;
        return lock;
    }

    // Let the waiting priority threads take the lock, then take it back.
    // It does nothing outside a Yielding lock, or when no priority thread waits.
    inline void yieldToPriority() {
        std::unique_lock<std::mutex> *lock=yieldingLock();
        if (lock==nullptr || !lock->owns_lock() || priorityWaiting().load()==0) return;
        lock->unlock();
        // A priority thread stops waiting once it owns the lock
        while (priorityWaiting().load()>0) std::this_thread::yield();
        lock->lock();
    }

    // The lock of a thread the solver waits for
    class Priority {
    public:
        explicit Priority(std::mutex &mutex): lock(mutex, std::defer_lock) {
            priorityWaiting()++;
            try {
                lock.lock();
            } catch (...) {
                priorityWaiting()--;
                throw;
            }
            priorityWaiting()--;
        }
        Priority(const Priority&)=delete;
        Priority &operator=(const Priority&)=delete;

    private:
        std::unique_lock<std::mutex> lock;
    };

    // The lock of a background reader, given up at each yieldToPriority()
    class Yielding {
    public:
        explicit Yielding(std::mutex &mutex): lock(mutex) { yieldingLock()=&lock; }
        ~Yielding() { yieldingLock()=nullptr; }
        Yielding(const Yielding&)=delete;
        Yielding &operator=(const Yielding&)=delete;

    private:
        std::unique_lock<std::mutex> lock;
    };
}

#endif //WACOMMPLUSPLUS_NETCDFLOCK_HPP
