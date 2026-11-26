#include "../NetcdfLock.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>

int main() {
    using Clock=std::chrono::steady_clock;
    const int reads=20;
    const auto readTime=std::chrono::milliseconds(50);
    std::mutex netcdf;

    // Without a yielding lock, or without waiting priority threads, yielding does nothing
    NetcdfLock::yieldToPriority();
    {
        NetcdfLock::Yielding lock(netcdf);
        NetcdfLock::yieldToPriority();
        assert(!netcdf.try_lock());
    }
    assert(NetcdfLock::yieldingLock()==nullptr);
    assert(netcdf.try_lock());
    netcdf.unlock();

    // A yielding reader holds the lock for 20 reads of 50 ms; a priority thread gets it between two reads
    std::atomic<int> done{0};
    std::atomic<bool> holding{false};
    std::thread reader([&]() {
        NetcdfLock::Yielding lock(netcdf);
        holding=true;
        for (int read=0; read<reads; read++) {
            NetcdfLock::yieldToPriority();
            std::this_thread::sleep_for(readTime);
            done++;
        }
    });
    while (!holding) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    auto start=Clock::now();
    int doneWhenLocked;
    Clock::duration waited;
    {
        NetcdfLock::Priority lock(netcdf);
        // Only the wait for the lock is timed; sleep overshoot on a loaded host is not
        waited=Clock::now()-start;
        doneWhenLocked=done;
        assert(NetcdfLock::priorityWaiting()==0);
        // The reader cannot read while the priority thread holds the lock
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        assert(done==doneWhenLocked);
    }
    reader.join();
    assert(doneWhenLocked<reads);
    assert(waited<4*readTime);
    assert(done==reads);

    // A plain lock holder is not interrupted: the priority thread waits for its release
    std::atomic<bool> held{false};
    std::thread holder([&]() {
        std::lock_guard<std::mutex> lock(netcdf);
        held=true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });
    while (!held) std::this_thread::yield();
    start=Clock::now();
    {
        NetcdfLock::Priority lock(netcdf);
    }
    assert(Clock::now()-start>=std::chrono::milliseconds(100));
    holder.join();
    assert(NetcdfLock::priorityWaiting()==0);
    return 0;
}
