#include "../ForcingPrefetch.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

int main() {
    using Clock=std::chrono::steady_clock;

    // A slow reader: taking the window waits until it has been read, on another thread
    ForcingPrefetch<std::string> prefetch;
    assert(!prefetch.pending() && !prefetch.holds(2));
    std::thread::id caller=std::this_thread::get_id(), reader;
    auto start=Clock::now();
    prefetch.start(2, [&reader]() {
        reader=std::this_thread::get_id();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return std::string("window-2");
    });
    assert(prefetch.pending() && prefetch.holds(2) && !prefetch.holds(3) && !prefetch.holds(-1));

    // Only one window is read ahead
    bool refused=false;
    try { prefetch.start(3, []() { return std::string("window-3"); }); }
    catch (const std::logic_error &) { refused=true; }
    assert(refused && prefetch.holds(2));

    assert(prefetch.take()=="window-2");
    assert(Clock::now()-start>=std::chrono::milliseconds(300));
    assert(reader!=caller);
    assert(!prefetch.pending() && !prefetch.holds(2));

    // Nothing to take
    bool empty=false;
    try { prefetch.take(); }
    catch (const std::logic_error &) { empty=true; }
    assert(empty);

    // A failed read is rethrown when the window is taken, and the prefetch can start again
    prefetch.start(4, []() -> std::string { throw std::runtime_error("unreadable forcing"); });
    bool rethrown=false;
    try { prefetch.take(); }
    catch (const std::runtime_error &e) { rethrown=std::string(e.what())=="unreadable forcing"; }
    assert(rethrown && !prefetch.pending());
    prefetch.start(5, []() { return std::string("window-5"); });
    assert(prefetch.take()=="window-5");

    // A window that is never taken is waited for when the prefetch is destroyed
    bool finished=false;
    {
        ForcingPrefetch<int> pending;
        pending.start(6, [&finished]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            finished=true;
            return 6;
        });
    }
    assert(finished);
}
