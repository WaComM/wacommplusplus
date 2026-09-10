#ifndef WACOMMPLUSPLUS_ENVIRONMENTALDATAACCESS_HPP
#define WACOMMPLUSPLUS_ENVIRONMENTALDATAACCESS_HPP

#include <string>

namespace EnvironmentalDataAccess {

bool isRemote(const std::string& location);
std::string resolve(const std::string& base,const std::string& reference);

}

#endif //WACOMMPLUSPLUS_ENVIRONMENTALDATAACCESS_HPP
