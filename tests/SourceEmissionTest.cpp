#include "../SourceEmission.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

int main() {
    auto hourly=sourceEmissionTimes(EmissionMode::UNIFORM_RATE,1000,0,-1,0,3600);
    assert(hourly.size()==1000);
    assert(std::abs(hourly.front()-1.8)<1.e-12);
    assert(std::abs(hourly.back()-3598.2)<1.e-9);

    auto gap=sourceEmissionTimes(EmissionMode::UNIFORM_RATE,1000,0,-1,3600,10800);
    assert(gap.size()==2000);
    assert(std::abs(gap.front()-3601.8)<1.e-9);
    assert(std::abs(gap.back()-10798.2)<1.e-9);

    auto partialA=sourceEmissionTimes(EmissionMode::UNIFORM_RATE,7,0,-1,0,450);
    auto partialB=sourceEmissionTimes(EmissionMode::UNIFORM_RATE,7,0,-1,450,3600);
    assert(partialA.size()+partialB.size()==7);

    auto continuous=sourceEmissionTimes(EmissionMode::UNIFORM_RATE,7,0,-1,0,3600);
    partialA.insert(partialA.end(),partialB.begin(),partialB.end());
    assert(partialA==continuous);

    auto bounded=sourceEmissionTimes(EmissionMode::UNIFORM_RATE,10,900,2700,0,3600);
    assert(bounded.size()==5);
    for (double time: bounded) assert(time>=900 && time<2700);

    auto legacy=sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,1000,0,-1,0,7200);
    assert(legacy.size()==1000);
    for (double time: legacy) assert(time==0);

    // Legacy batch: closed source window over forcing-record times, as in the original
    // WaComM record loop, so start==end releases exactly one batch.
    auto instant=sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,3600,3600,3600,7200);
    assert(instant.size()==4);
    for (double time: instant) assert(time==3600);
    assert(sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,3600,3600,7200,10800).empty());
    assert(sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,3600,3600,0,3600).empty());
    // The end record is included and a window inside one interval emits nothing after its start.
    assert(sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,0,7200,7200,10800).size()==4);
    assert(sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,0,1800,3600,7200).empty());
    // Modified-Julian round-off of order 1e-5 s does not decide membership.
    assert(sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,3600.00001,3600.00001,3600,7200).size()==4);
    assert(sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,4,3599.99999,3599.99999,3600,7200).size()==4);

    auto pulse=sourceEmissionTimes(EmissionMode::SINGLE_PULSE,25,1800,-1,0,3600);
    assert(pulse.size()==25);
    for (double time: pulse) assert(time==1800);
    assert(sourceEmissionTimes(EmissionMode::SINGLE_PULSE,25,1800,-1,1800,3600).size()==25);
    assert(sourceEmissionTimes(EmissionMode::SINGLE_PULSE,25,1800,-1,0,1800).empty());

    bool rejected=false;
    try { sourceEmissionTimes(EmissionMode::FORCING_INTERVAL_BATCH,1.5,0,-1,0,3600); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);

    double offset=sourcePositionOffset(5489,42,0);
    assert(offset==sourcePositionOffset(5489,42,0));
    assert(offset!=sourcePositionOffset(5489,43,0));
    assert(offset!=sourcePositionOffset(5489,42,1));
    assert(offset!=sourcePositionOffset(5490,42,0));
}
