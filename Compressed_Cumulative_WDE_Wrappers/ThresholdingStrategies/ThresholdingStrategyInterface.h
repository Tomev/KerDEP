//
// Created by Tomev on 07/02/2021.
//

#ifndef KERDEP_THRESHOLDINGSTRATEGYINTERFACE_H
#define KERDEP_THRESHOLDINGSTRATEGYINTERFACE_H

#include <vector>
#include <memory>

#include "LinearWDE.h"

class ThresholdingStrategyInterface{
  public:
    virtual void UpdateThresholds(const vector<EmpiricalCoefficientData> &wavelet_coefficients) = 0;
    virtual double ComputeThresholdedCoefficient(const EmpiricalCoefficientData &wavelet_coefficient) = 0;
};

typedef std::shared_ptr<ThresholdingStrategyInterface> ThresholdingStrategyPtr;

#endif //KERDEP_THRESHOLDINGSTRATEGYINTERFACE_H
