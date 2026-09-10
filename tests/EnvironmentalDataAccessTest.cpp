#include "../EnvironmentalDataAccess.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    using namespace EnvironmentalDataAccess;
    assert(resolve("data","ocean.nc")=="data/ocean.nc");
    assert(resolve("data/","ocean.nc")=="data/ocean.nc");
    assert(resolve("data","data/ocean.nc")=="data/ocean.nc");
    assert(resolve("https://example.test/dap","ocean.nc")=="https://example.test/dap/ocean.nc");
    assert(resolve("ignored","https://example.test/ocean.nc")=="https://example.test/ocean.nc");
    assert(resolve("ignored","/data/ocean.nc")=="/data/ocean.nc");
    assert(isRemote("http://example.test/ocean.nc"));
    assert(!isRemote("data/ocean.nc"));
    bool rejected=false;
    try { resolve("", "ftp://example.test/ocean.nc"); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    rejected=false;
    try { resolve("", "https://user:secret@example.test/ocean.nc"); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
}
