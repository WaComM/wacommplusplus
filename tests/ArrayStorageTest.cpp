#include "../Array.h"

#include <cassert>
#include <sstream>
#include <string>

int main() {
    float storage[2*3*2*2*2]{};
    Array::Array5<float> view(2,3,2,2,2,storage);

    assert(view()==storage);
    assert(view.Nx()==2);
    assert(view.Ny()==3);
    assert(view.Nz()==2);
    assert(view.N4()==2);
    assert(view.N5()==2);

    for (unsigned int i=0;i<view.Size();i++) view(static_cast<int>(i))=static_cast<float>(i);
    assert(view(1,2,1,1,1)==47.0f);
    view(0,1,0,1,1)=17.0f;
    assert(storage[11]==17.0f);

    std::ostringstream output;
    output << view;
    std::istringstream values(output.str());
    float value=0;
    unsigned int count=0;
    while (values >> value) count++;
    assert(count==view.Size());
}
