#include "../DriftModel.hpp"

#include <cassert>
#include <cmath>
#include <string>

int main() {
    static_assert(static_cast<std::uint16_t>(DriftObjectType::PASSIVE)==0);
    static_assert(static_cast<std::uint16_t>(DriftObjectType::SHIPPING_CONTAINER)==5);
    static_assert(static_cast<std::uint16_t>(DriftObjectType::KAYAK_WITH_PERSON)==11);
    LeewayCoefficients downwind{.01,0,0,0,0,0,0,0,0};
    DriftVelocity calm=computeLeeway(downwind,0,0,DriftSide::RIGHT);
    assert(calm.u==0 && calm.v==0);

    DriftVelocity east=computeLeeway(downwind,10,0,DriftSide::RIGHT);
    assert(std::abs(east.u-.1)<1.e-12 && east.v==0);

    LeewayCoefficients crosswind{0,0,0,.01,0,0,-.01,0,0};
    DriftVelocity left=computeLeeway(crosswind,10,0,DriftSide::LEFT);
    DriftVelocity right=computeLeeway(crosswind,10,0,DriftSide::RIGHT);
    assert(left.u==0 && right.u==0);
    assert(std::abs(left.v+right.v)<1.e-12);
    assert(std::abs(left.v)==std::abs(right.v));

    LeewayCoefficients uncertain{.01,.02,.03,.04,.05,.06,-.04,-.05,.06};
    DriftVelocity member=computeLeeway(uncertain,10,0,DriftSide::RIGHT,2,-1);
    assert(std::abs(member.u-(.1+.02+2*.03))<1.e-12);
    assert(std::abs(member.v-(.4+.05-.06))<1.e-12);
    assert(correlatedNormal(2,-1,0)==-1);
    assert(correlatedNormal(2,-1,1)==2);
    assert(correlatedNormal(2,-1,-1)==-2);

    assert(driftSideFromUniform(.2,.3)==DriftSide::RIGHT);
    assert(driftSideFromUniform(.3,.3)==DriftSide::LEFT);
    DriftSide sampled=sampleDriftSide(5489,42,.5);
    assert(sampled==sampleDriftSide(5489,42,.5));
    assert(sampleDriftSide(5489,42,0)==DriftSide::LEFT);
    assert(sampleDriftSide(5489,42,1)==DriftSide::RIGHT);
    assert(NumericalHelpers::uniform(5489,42,LEEWAY_SIDE_RANDOM_INTERVAL,0,0)!=
           NumericalHelpers::uniform(5489,42,LEEWAY_ENSEMBLE_RANDOM_INTERVAL,0,0));
    assert(NumericalHelpers::normal(5489,42,0,0,LEEWAY_WIND_ERROR_U_COMPONENT)!=
           NumericalHelpers::normal(5489,42,0,0,LEEWAY_WIND_ERROR_V_COMPONENT));
    double shared0=forcingErrorNormal(5489,42,0,0,LEEWAY_WIND_ERROR_U_COMPONENT,
                                      .1,.5,1200,10000,3600);
    double shared1=forcingErrorNormal(5489,99,9,7,LEEWAY_WIND_ERROR_U_COMPONENT,
                                      .100001,.500001,1250,10000,3600);
    assert(shared0==shared1);
    double later=forcingErrorNormal(5489,99,9,7,LEEWAY_WIND_ERROR_U_COMPONENT,
                                    .100001,.500001,4800,10000,3600);
    assert(shared0!=later);
    struct Region { double west,south,east,north; } regions[]={{10,40,12,42},{13,40,15,42}};
    constexpr double degreesToRadians=.017453292519943295769;
    assert(observationalCalibrationRegion(regions,2,11*degreesToRadians,41*degreesToRadians)==0);
    assert(observationalCalibrationRegion(regions,2,14*degreesToRadians,41*degreesToRadians)==1);
    assert(observationalCalibrationRegion(regions,2,0,0)==-1);
    assert(jibeStepProbability(0,3600)==0);
    assert(jibeStepProbability(1,1)==1);
    assert(std::abs(jibeStepProbability(.04,3600)-.04)<1.e-12);
    double halfHour=jibeStepProbability(.04,1800);
    assert(std::abs((1-halfHour)*(1-halfHour)-.96)<1.e-12);

    const auto& person=DriftObjectCatalog::definition(DriftObjectType::PERSON_IN_WATER);
    assert(person.leeway.downwindSlope==.0096);
    assert(DriftObjectCatalog::type("SHIPPING_CONTAINER")==DriftObjectType::SHIPPING_CONTAINER);
    const auto& suit=DriftObjectCatalog::definition(DriftObjectType::PERSON_IN_WATER_SURVIVAL_SUIT);
    assert(std::string(suit.sourceKey)=="PIW-4");
    DriftVelocity suitRight=computeLeeway(suit.leeway,10,0,DriftSide::RIGHT);
    DriftVelocity suitLeft=computeLeeway(suit.leeway,10,0,DriftSide::LEFT);
    assert(std::abs(suitRight.v-(.0136*10-.033))<1.e-12);
    assert(std::abs(suitLeft.v-(-.0013*10-.0265))<1.e-12);
    DriftVelocity legacyLeftMember=computeLeeway(person.leeway,10,0,DriftSide::LEFT,0,1);
    assert(std::abs(legacyLeftMember.v-(-.0054*10-.094))<1.e-12);
    assert(DriftObjectCatalog::type("KAYAK_WITH_PERSON")==DriftObjectType::KAYAK_WITH_PERSON);
    assert(DriftObjectCatalog::definition(DriftObjectType::OIL_DRUM).leeway.downwindIntercept==.0266);

    const auto& current=DriftProcessCatalog::definition(DriftProcessType::OCEAN_CURRENT);
    assert(std::string(current.units)=="m s-1" && !current.stochastic);
    assert(DriftProcessCatalog::type("jibing")==DriftProcessType::JIBING);
    assert(DriftProcessCatalog::definition(DriftProcessType::JIBING).stateful);
}
