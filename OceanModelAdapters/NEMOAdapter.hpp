#ifndef WACOMMPLUSPLUS_NEMOADAPTER_HPP
#define WACOMMPLUSPLUS_NEMOADAPTER_HPP

#include "StructuredGridAdapter.hpp"

class NEMOAdapter: public StructuredGridAdapter {
public:
    explicit NEMOAdapter(string &fileName): StructuredGridAdapter(fileName,"NEMO") {}
};

#endif
