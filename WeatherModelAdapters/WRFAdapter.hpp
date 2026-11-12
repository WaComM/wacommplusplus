#ifndef WACOMMPLUSPLUS_WRFADAPTER_HPP
#define WACOMMPLUSPLUS_WRFADAPTER_HPP

#include "../WeatherModelAdapter.hpp"

class WRFAdapter: public WeatherModelAdapter {
public:
    explicit WRFAdapter(std::string &fileName,const std::string& sourceCrs="");
    void process() override;
private:
    std::string &fileName;
    std::string sourceCrs;
};

#endif
