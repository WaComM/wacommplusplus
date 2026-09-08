#ifndef WACOMMPLUSPLUS_HYCOMADAPTER_HPP
#define WACOMMPLUSPLUS_HYCOMADAPTER_HPP

#include "StructuredGridAdapter.hpp"

class HYCOMAdapter: public StructuredGridAdapter {
public:
    explicit HYCOMAdapter(string &fileName): StructuredGridAdapter(fileName,"HYCOM") {}
};

#endif
