#ifndef WACOMMPLUSPLUS_ENVIRONMENTALREGRIDDER_HPP
#define WACOMMPLUSPLUS_ENVIRONMENTALREGRIDDER_HPP

#include "Array.h"

namespace EnvironmentalRegridder {
    Array::Array3<float> bilinearGeographic(const Array::Array2<double>& sourceLon,
                                            const Array::Array2<double>& sourceLat,
                                            const Array::Array3<float>& source,
                                            const Array::Array2<double>& targetLon,
                                            const Array::Array2<double>& targetLat);
}

#endif
