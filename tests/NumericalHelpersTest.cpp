#include "../NumericalHelpers.hpp"

#include <cassert>
#include <cmath>

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
}
