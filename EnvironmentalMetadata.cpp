#include "EnvironmentalMetadata.hpp"
#include "JulianDate.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <stdexcept>
#include <vector>

using namespace netCDF;

namespace {
std::string normalized(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){ return std::tolower(c); });
    value=std::regex_replace(value,std::regex("\\s+")," ");
    if (!value.empty() && value.front()==' ') value.erase(value.begin());
    if (!value.empty() && value.back()==' ') value.pop_back();
    return value;
}

void requireOneOf(NcVar& variable,const std::string& label,const std::vector<std::string>& accepted) {
    std::string units=normalized(EnvironmentalMetadata::attribute(variable,"units"));
    if (std::find(accepted.begin(),accepted.end(),units)==accepted.end())
        throw std::runtime_error(label + " has missing or unsupported units '" + units + "'");
}

int daysInMonth(int year,int month) {
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if (month!=2) return days[month-1];
    bool leap=(year%4==0 && year%100!=0) || year%400==0;
    return leap ? 29 : 28;
}
}

std::string EnvironmentalMetadata::attribute(NcVar& variable,const std::string& name) {
    try {
        NcVarAtt value=variable.getAtt(name);
        if (value.isNull()) return "";
        std::string result; value.getValues(result); return result;
    } catch (const netCDF::exceptions::NcException&) { return ""; }
}

void EnvironmentalMetadata::requireVelocity(NcVar& variable,const std::string& label) {
    requireOneOf(variable,label,{"m s-1","m s^-1","m/s","meter second-1","metre second-1",
                                 "meters per second","metres per second"});
}

void EnvironmentalMetadata::requireLongitude(NcVar& variable,const std::string& label) {
    requireOneOf(variable,label,{"degree_east","degrees_east","degree east","degrees east"});
}

void EnvironmentalMetadata::requireLatitude(NcVar& variable,const std::string& label) {
    requireOneOf(variable,label,{"degree_north","degrees_north","degree north","degrees north"});
}

void EnvironmentalMetadata::readCfTime(NcVar& variable,Array::Array1<double>& destination,const std::string& label) {
    std::string calendar=normalized(attribute(variable,"calendar"));
    if (!calendar.empty() && calendar!="standard" && calendar!="gregorian" && calendar!="proleptic_gregorian")
        throw std::runtime_error(label + " uses unsupported calendar '" + calendar + "'");
    std::string units=normalized(attribute(variable,"units"));
    std::regex pattern("(seconds?|secs?|s|minutes?|mins?|hours?|hrs?|h|days?|d) since "
                       "([0-9]+)-([0-9]{1,2})-([0-9]{1,2})(?:[ t]([0-9]{1,2}):([0-9]{1,2})(?::([0-9]+(?:\\.[0-9]*)?))?)?(?: (?:z|utc|gmt))?");
    std::smatch match;
    if (!std::regex_match(units,match,pattern))
        throw std::runtime_error(label + " requires CF time units '<seconds|minutes|hours|days> since YYYY-MM-DD [HH:MM:SS]' in UTC");
    double scale=1; std::string unit=match[1];
    if (unit[0]=='m') scale=60; else if (unit[0]=='h') scale=3600; else if (unit[0]=='d') scale=86400;
    int year=std::stoi(match[2]),month=std::stoi(match[3]),day=std::stoi(match[4]);
    int hour=match[5].matched ? std::stoi(match[5]) : 0;
    int minute=match[6].matched ? std::stoi(match[6]) : 0;
    double second=match[7].matched ? std::stod(match[7]) : 0;
    if (year<1583 || month<1 || month>12 || day<1 || day>daysInMonth(year,month) ||
        hour>23 || minute>59 || second>=60)
        throw std::runtime_error(label + " has an invalid CF reference datetime");
    Calendar reference(year,month,day,0,0,0);
    double origin=JulianDate::toModJulian(reference)*86400.0+hour*3600.0+minute*60.0+second;
    std::vector<double> values(destination.Nx()); variable.getVar(values.data());
    for (int t=0;t<destination.Nx();t++) {
        destination(t)=origin+values[t]*scale;
        if (!std::isfinite(destination(t))) throw std::runtime_error(label + " contains a non-finite time coordinate");
    }
}
