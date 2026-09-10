#ifndef WACOMMPLUSPLUS_ENVIRONMENTALREGRIDDER_HPP
#define WACOMMPLUSPLUS_ENVIRONMENTALREGRIDDER_HPP

#include "Array.h"
#include <string>

namespace EnvironmentalRegridder {
    Array::Array3<float> bilinearGeographic(const Array::Array2<double>& sourceLon,
                                            const Array::Array2<double>& sourceLat,
                                            const Array::Array3<float>& source,
                                            const Array::Array2<double>& targetLon,
                                            const Array::Array2<double>& targetLat);
    Array::Array3<float> bilinearCurvilinearGeographic(const Array::Array2<double>& sourceLon,
                                                       const Array::Array2<double>& sourceLat,
                                                       const Array::Array3<float>& source,
                                                       const Array::Array2<double>& targetLon,
                                                       const Array::Array2<double>& targetLat);
    Array::Array3<float> bilinearProjected(const Array::Array2<double>& sourceX,
                                           const Array::Array2<double>& sourceY,
                                           const Array::Array3<float>& source,
                                           const Array::Array2<double>& targetLon,
                                           const Array::Array2<double>& targetLat,
                                           const std::string& sourceCrs);
    Array::Array3<float> bilinearCurvilinearCartesian(const Array::Array2<double>& sourceX,
                                                      const Array::Array2<double>& sourceY,
                                                      const Array::Array3<float>& source,
                                                      const Array::Array2<double>& targetX,
                                                      const Array::Array2<double>& targetY);
}

#endif
