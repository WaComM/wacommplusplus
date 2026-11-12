#include "EnvironmentalDataAccess.hpp"

#include <cctype>
#include <stdexcept>

namespace {

bool hasScheme(const std::string& value) {
    size_t separator=value.find("://");
    if (separator==std::string::npos || separator==0) return false;
    for (size_t i=0;i<separator;i++)
        if (!std::isalpha(static_cast<unsigned char>(value[i])) &&
            !std::isdigit(static_cast<unsigned char>(value[i])) && value[i]!='+' && value[i]!='-' && value[i]!='.')
            return false;
    return true;
}

std::string scheme(const std::string& value) {
    size_t separator=value.find("://");
    std::string result=value.substr(0,separator);
    for (char& character:result) character=static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return result;
}

void validateRemote(const std::string& value) {
    std::string protocol=scheme(value);
    if (protocol!="http" && protocol!="https" && protocol!="dap4")
        throw std::runtime_error("Unsupported environmental data URI scheme: " + protocol);
    size_t authority=value.find("://")+3;
    size_t end=value.find_first_of("/?#",authority);
    if (end==authority) throw std::runtime_error("Environmental data URI requires a host");
    if (value.find('@',authority)<end)
        throw std::runtime_error("Environmental data URI must not contain embedded credentials");
}

}

bool EnvironmentalDataAccess::isRemote(const std::string& location) {
    if (!hasScheme(location)) return false;
    validateRemote(location);
    return true;
}

std::string EnvironmentalDataAccess::resolve(const std::string& base,const std::string& reference) {
    if (reference.empty()) throw std::runtime_error("Environmental data location must not be empty");
    if (hasScheme(reference)) { validateRemote(reference); return reference; }
    if (!reference.empty() && reference[0]=='/') return reference;
    if (base.empty()) return reference;
    if (hasScheme(base)) validateRemote(base);
    std::string prefix=base.back()=='/' ? base : base+"/";
    if (reference.compare(0,prefix.size(),prefix)==0) return reference;
    return prefix+reference;
}
