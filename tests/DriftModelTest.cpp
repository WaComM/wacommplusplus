#include "../DriftModel.hpp"

#include <cassert>
#include <cmath>

int main() {
    LeewayCoefficients downwind{.01,0,0,0,0,0};
    DriftVelocity calm=computeLeeway(downwind,0,0,DriftSide::RIGHT);
    assert(calm.u==0 && calm.v==0);

    DriftVelocity east=computeLeeway(downwind,10,0,DriftSide::RIGHT);
    assert(std::abs(east.u-.1)<1.e-12 && east.v==0);

    LeewayCoefficients crosswind{0,0,0,.01,0,0};
    DriftVelocity left=computeLeeway(crosswind,10,0,DriftSide::LEFT);
    DriftVelocity right=computeLeeway(crosswind,10,0,DriftSide::RIGHT);
    assert(left.u==0 && right.u==0);
    assert(std::abs(left.v+right.v)<1.e-12);
    assert(std::abs(left.v)==std::abs(right.v));

    LeewayCoefficients uncertain{.01,.02,.03,.04,.05,.06};
    DriftVelocity member=computeLeeway(uncertain,10,0,DriftSide::RIGHT,2,-1);
    assert(std::abs(member.u-(.1+.02+2*.03))<1.e-12);
    assert(std::abs(member.v-(.4+.05-.06))<1.e-12);

    const auto& person=DriftObjectCatalog::definition(DriftObjectType::PERSON_IN_WATER);
    assert(person.leeway.downwindSlope==.0096);
    assert(DriftObjectCatalog::type("SHIPPING_CONTAINER")==DriftObjectType::SHIPPING_CONTAINER);
}
