//
// Created by Tomev on 23/01/2021.
//

#ifndef MATH_HELPERS_H
#define MATH_HELPERS_H

#include <vector>
using std::vector;

/** Computes standard deviation estimator from given vector.
 * @brief Computes standard deviation estimator from given vector.
 * @param values - Vector of values.
 * @return Standard deviation estimator of values.
 */
double StDev(const vector<double> &values);

double WeightedStDev(const vector<double> &values, const vector<double> &weights);

#endif