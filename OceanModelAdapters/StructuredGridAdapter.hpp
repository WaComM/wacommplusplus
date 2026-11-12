#ifndef WACOMMPLUSPLUS_STRUCTUREDGRIDADAPTER_HPP
#define WACOMMPLUSPLUS_STRUCTUREDGRIDADAPTER_HPP

#include "../OceanModelAdapter.hpp"

class StructuredGridAdapter: public OceanModelAdapter {
public:
    StructuredGridAdapter(string &fileName, string model);
    ~StructuredGridAdapter();
    void process() override;

private:
    log4cplus::Logger logger;
    string &fileName;
    string model;

    NcVar variable(NcFile &dataFile, const vector<string> &names, bool optional=false);
};

#endif
