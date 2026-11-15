//
// Created by Raffaele Montella on 15/11/26.
//

#ifndef WACOMMPLUSPLUS_FORCINGPREFETCH_HPP
#define WACOMMPLUSPLUS_FORCINGPREFETCH_HPP

#include <functional>
#include <future>
#include <stdexcept>
#include <utility>

// Read one forcing window ahead on a separate thread (docs/parallelism.md#forcing-prefetch).
// At most one window is read ahead. The interval loop decides which input index is needed and
// when; the prefetch only changes which thread reads it.
template<typename Window>
class ForcingPrefetch {
public:
    // Start reading the window of an input index on a new thread
    void start(int index, std::function<Window()> read) {
        if (pending()) throw std::logic_error("A forcing window is already being prefetched");
        future=std::async(std::launch::async, std::move(read));
        prefetchedIndex=index;
    }

    // True while a started window has not been taken
    bool pending() const { return prefetchedIndex>=0; }

    // True if the window of this input index is being, or has been, prefetched
    bool holds(int index) const { return pending() && prefetchedIndex==index; }

    // Wait for the prefetched window; a read failure is rethrown here, on the calling thread
    Window take() {
        if (!pending()) throw std::logic_error("No forcing window is being prefetched");
        prefetchedIndex=-1;
        return future.get();
    }

private:
    int prefetchedIndex=-1;
    std::future<Window> future;
};

#endif //WACOMMPLUSPLUS_FORCINGPREFETCH_HPP
