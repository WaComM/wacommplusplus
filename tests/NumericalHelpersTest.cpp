#include "../NumericalHelpers.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>

int main() {
    assert(NumericalHelpers::validInterpolationCell(1, 2, 3, 4));
    assert(!NumericalHelpers::validInterpolationCell(2, 2, 3, 4));
    assert(!NumericalHelpers::validInterpolationCell(1, 3, 3, 4));
    assert(NumericalHelpers::storageLevel(-4, 5) == 0);
    assert(NumericalHelpers::storageLevel(0, 5) == 4);
    assert(NumericalHelpers::horizontalCell(-.2)==-1);
    assert(NumericalHelpers::horizontalCell(1.8)==1);
    assert(NumericalHelpers::upperVerticalLevel(-.5)==0);
    assert(NumericalHelpers::upperVerticalLevel(-1.5)==-1);
    assert(NumericalHelpers::lowerVerticalWeight(-.5)==.5);
    assert(NumericalHelpers::timeWeight(15, 10, 20) == .5);
    assert(NumericalHelpers::timeWeight(30, 10, 20) == 1);
    assert(NumericalHelpers::stepSize(0, 65, 30) == 30);
    assert(NumericalHelpers::stepSize(60, 65, 30) == 5);
    assert(NumericalHelpers::stepSize(65, 0, 30) == -30);
    assert(std::isnan(NumericalHelpers::restartElapsed(0,900,900)));
    assert(NumericalHelpers::restartElapsed(0,900,450)==450);
    assert(std::isnan(NumericalHelpers::restartElapsed(900,0,0)));
    assert(NumericalHelpers::restartElapsed(900,0,450)==450);
    assert(!NumericalHelpers::activeInterval(0,900,900));
    assert(NumericalHelpers::activeInterval(0,900,450));
    assert(!NumericalHelpers::activeInterval(900,0,0));
    assert(NumericalHelpers::activeInterval(900,0,450));
    const double irregularTimes[5]={0,900,2700,5400,7200};
    int forwardIntervals=0,backwardIntervals=0;
    for (int i=0;i<4;i++) {
        if (NumericalHelpers::activeInterval(irregularTimes[i],irregularTimes[i+1],2700))
            forwardIntervals++;
    }
    for (int i=4;i>0;i--) {
        if (NumericalHelpers::activeInterval(irregularTimes[i],irregularTimes[i-1],2700))
            backwardIntervals++;
    }
    assert(forwardIntervals==2);
    assert(backwardIntervals==2);
    assert(NumericalHelpers::restartElapsed(2700,5400,2700)==0);
    assert(std::isnan(NumericalHelpers::restartElapsed(5400,2700,2700)));
    assert(!NumericalHelpers::emitAtIntervalStart(0,450));
    assert(NumericalHelpers::emitAtIntervalStart(450,450));
    double left=.7; NumericalHelpers::reflectCell(1.2, 1, left, 0); assert(std::abs(left-1.5)<1e-12);
    double right=2.3; NumericalHelpers::reflectCell(1.8, 1, right, 2); assert(std::abs(right-1.7)<1e-12);
    double same=1.4; NumericalHelpers::reflectCell(1.2, 1, same, 1); assert(same==1.4);
    assert(std::abs(NumericalHelpers::reflectDomain(-.2,2)-.2)<1e-12);
    assert(std::abs(NumericalHelpers::reflectDomain(2.3,2)-1.7)<1e-12);
    assert(std::abs(NumericalHelpers::reflectDomain(6.3,2)-1.7)<1e-12);
    assert(NumericalHelpers::reflectDomain(2,2)<2);
    std::size_t partitionTotal=0;
    for (std::size_t worker=0;worker<4;worker++) {
        std::size_t count=NumericalHelpers::partitionCount(11,4,worker);
        assert(NumericalHelpers::partitionOffset(11,4,worker)==partitionTotal);
        partitionTotal+=count;
    }
    assert(partitionTotal==11);
    double random0=NumericalHelpers::normal(5489, 7, 3600, 2, 0);
    assert(random0 == NumericalHelpers::normal(5489, 7, 3600, 2, 0));
    assert(random0 != NumericalHelpers::normal(5490, 7, 3600, 2, 0));
    assert(random0 != NumericalHelpers::normal(5489, 7, 3600, 3, 0));
    assert(random0 != NumericalHelpers::normal(5489, 7, 3600, 2, 1));

    // Ordered keys: swapping coordinate values must not reproduce a draw, and a
    // repeated value (particle==substep, seed==particle) must not cancel the key.
    assert(NumericalHelpers::uniform(5489, 3, 3600, 7, 0) != NumericalHelpers::uniform(5489, 7, 3600, 3, 0));
    assert(NumericalHelpers::uniform(5489, 5, 3600, 5, 0) != NumericalHelpers::uniform(5489, 9, 3600, 9, 0));
    assert(NumericalHelpers::uniform(5489, 42, 3600, 2, 0) != NumericalHelpers::uniform(42, 5489, 3600, 2, 0));
    assert(NumericalHelpers::uniform(5489, 42, 3600, 2, 0) != NumericalHelpers::uniform(5489, 42, 3600, 0, 2));
    assert(NumericalHelpers::uniform(5489, 42, 3600, 5489, 0) != NumericalHelpers::uniform(5489, 43, 3600, 5489, 0));

    // No duplicated draw over the particle x substep x component lattice of one
    // forcing interval (the XOR-combined key duplicated about 6% of these values).
    std::set<double> draws;
    for (std::uint64_t particle=0;particle<1000;particle++)
        for (std::uint64_t substep=0;substep<120;substep++)
            for (std::uint64_t component=0;component<6;component++)
                assert(draws.insert(NumericalHelpers::uniform(5489,particle,1675900800,substep,component)).second);

    // Pinned values of scheme ordered-splitmix64-v2: a change here changes every
    // seeded realization and requires a new NumericalHelpers::RANDOM_KEY_SCHEME.
    assert(std::string(NumericalHelpers::RANDOM_KEY_SCHEME)=="ordered-splitmix64-v2");
    assert(NumericalHelpers::uniform(5489, 7, 3600, 2, 0) == 0x1.bd90c6590112ep-1);
    assert(NumericalHelpers::uniform(0, 0, 0, 0, 0) == 0x1.e2b96a69ad7f4p-2);
}
