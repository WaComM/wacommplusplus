#ifndef WACOMMPLUSPLUS_WW3ADAPTER_HPP
#define WACOMMPLUSPLUS_WW3ADAPTER_HPP

#include "../WaveModelAdapter.hpp"

class WW3Adapter: public WaveModelAdapter {
public:
    explicit WW3Adapter(std::string &fileName);
    void process() override;
private:
    std::string &fileName;
    NcVar variable(NcFile &file, const std::vector<std::string>& names);
};

#endif
