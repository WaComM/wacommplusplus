//
// Created by Raffaele Montella on 26/11/26.
//

#ifndef WACOMMPLUSPLUS_LOGINITIALIZER_HPP
#define WACOMMPLUSPLUS_LOGINITIALIZER_HPP

// Shut log4cplus down before main returns (docs/build.md, Windows).
// With MSVC, log4cplus 2.x deadlocks at process exit while destroying its thread pool
// unless a log4cplus::Initializer created at the start of main is destroyed first.
// log4cplus 1.x (Rocky Linux 8 EPEL) has neither the thread pool nor the Initializer.
#if __has_include(<log4cplus/initializer.h>)
#include <log4cplus/initializer.h>
using LogInitializer = log4cplus::Initializer;
#else
struct LogInitializer {};
#endif

#endif //WACOMMPLUSPLUS_LOGINITIALIZER_HPP
