#ifndef WACOMMPLUSPLUS_WRFADAPTER_HPP
#define WACOMMPLUSPLUS_WRFADAPTER_HPP

#include "../WeatherModelAdapter.hpp"

class WRFAdapter: public WeatherModelAdapter {
public:
    explicit WRFAdapter(std::string &fileName);
    void process() override;
private:
    std::string &fileName;
};

#endif
