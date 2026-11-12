#include "../Concentration.hpp"

#include <cassert>
#include <cstdint>
#include <map>
#include <tuple>

#ifdef USE_OMP
#include <omp.h>
#endif

namespace {
// Reference count of the living particles per rounded rho cell
void referenceCount(const Particles &particles, int t, int s_rho, int eta_rho, int xi_rho, Array4<float> &conc) {
    std::map<std::tuple<int,int,int>, long> counts;
    for (const Particle &particle: particles) {
        if (!particle.isAlive()) continue;
        int k=(int)round(particle.K()), j=(int)round(particle.J()), i=(int)round(particle.I());
        if (k>=-s_rho+1 && k<=0 && j>=0 && j<eta_rho && i>=0 && i<xi_rho) counts[std::make_tuple(k,j,i)]++;
    }
    for (const auto &count: counts) {
        conc(t,std::get<0>(count.first),std::get<1>(count.first),std::get<2>(count.first))+=(float)count.second;
    }
}
}

int main() {
    const int particleCount=100000;
    Particles particles;
    for (int i=0;i<particleCount;i++) {
        particles.push_back(Particle(i,-1,1,1,0));
    }
    particles.push_back(Particle(particleCount,-1,4,1,0));
    particles.push_back(Particle(particleCount+1,-1,1,1,0,0,0));

    Array4<float> concentration(1,3,3,3,0,-2,0,0);
    concentration=0.0f;
    Concentration::evaluate(&particles,0,3,3,3,concentration);

    assert(concentration(0,-1,1,1)==particleCount);
    float total=0;
    for (int k=-2;k<=0;k++) {
        for (int j=0;j<3;j++) {
            for (int i=0;i<3;i++) {
                total+=concentration(0,k,j,i);
            }
        }
    }
    assert(total==particleCount);

    // Clustered and spread particles, dead ones, and ones outside a non-cubic grid: the counts equal
    // the reference exactly for every thread count, and add to the existing values
    const int s_rho=4, eta_rho=7, xi_rho=11;
    Particles mixed;
    std::uint64_t state=12345;
    for (int n=0;n<200003;n++) {
        state=state*6364136223846793005ull+1442695040888963407ull;
        double u=(double)(state>>11)/(double)(1ull<<53);
        double k, j, i;
        if (n%3==0) { k=-1.2; j=3.1; i=5.4; }
        else if (n%3==1) { k=-0.3-3.0*u; j=6.4*u; i=10.4*(1.0-u); }
        else { k=-4.0+5.0*u; j=-1.0+9.0*u; i=-1.0+13.0*u; }
        if (n%97==0) mixed.push_back(Particle(n,k,j,i,0,0,0));
        else mixed.push_back(Particle(n,k,j,i,0));
    }
    // Boundary cells
    mixed.push_back(Particle(300000,-(s_rho-1),0,0,0));
    mixed.push_back(Particle(300001,0,eta_rho-1,xi_rho-1,0));

    Array4<float> expected(2,s_rho,eta_rho,xi_rho,0,-s_rho+1,0,0);
    expected=0.0f;
    expected(1,0,0,0)=5.0f;
    referenceCount(mixed,1,s_rho,eta_rho,xi_rho,expected);
    assert(expected(1,-(s_rho-1),0,0)>=1 && expected(1,0,eta_rho-1,xi_rho-1)>=1);

    int maxThreads=1;
#ifdef USE_OMP
    maxThreads=omp_get_max_threads();
#endif
    for (int threads: {1, 2, 3, 7, 16}) {
#ifdef USE_OMP
        omp_set_num_threads(threads);
#endif
        Array4<float> computed(2,s_rho,eta_rho,xi_rho,0,-s_rho+1,0,0);
        computed=0.0f;
        computed(1,0,0,0)=5.0f;
        Concentration::evaluate(&mixed,1,s_rho,eta_rho,xi_rho,computed);
        for (int k=-s_rho+1;k<=0;k++) {
            for (int j=0;j<eta_rho;j++) {
                for (int i=0;i<xi_rho;i++) {
                    assert(computed(1,k,j,i)==expected(1,k,j,i));
                    assert(computed(0,k,j,i)==0.0f);
                }
            }
        }
    }
#ifdef USE_OMP
    omp_set_num_threads(maxThreads);
#endif
    (void)maxThreads;

    // An empty set leaves the counts unchanged
    Particles none;
    Array4<float> unchanged(1,s_rho,eta_rho,xi_rho,0,-s_rho+1,0,0);
    unchanged=0.0f;
    Concentration::evaluate(&none,0,s_rho,eta_rho,xi_rho,unchanged);
    assert(unchanged(0,0,0,0)==0.0f);
}
