#include "../Concentration.hpp"

#include <cassert>

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
}
