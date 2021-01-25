//
// Created by Tomev on 19/01/2021.
//
// Implementation of Linear Wavelet Density Estimator strategy.
//

#include "Compressed_Cumulative_WDE_Wrappers/LinearWDE.h"

#include <cmath>

#include "math_helpers.h"

using std::cout, std::endl;

TranslatedDilatedScalingFunction LinearWDE::translated_dilated_scaling_function_ = TranslatedDilatedScalingFunction(0, 0);

LinearWDE::LinearWDE(const double &threshold)
    :  coefficient_threshold_(threshold) { }

LinearWDE::LinearWDE(vector<EmpiricalCoefficientData> empirical_scaling_coefficients, const double &threshold)
    :  coefficient_threshold_(threshold) , empirical_scaling_coefficients_(empirical_scaling_coefficients) {
  if(!empirical_scaling_coefficients.empty()){
    resolution_index_ = empirical_scaling_coefficients[0].j_;
    k_min_ = empirical_scaling_coefficients[0].k_;
    k_max_ = empirical_scaling_coefficients[empirical_scaling_coefficients.size() - 1].k_;
  }
}

/** Updates linear WDE data.
 * @brief Updates linear WDE data.
 * @param values_block - Block of new values which will be used during update process.
 */
void LinearWDE::UpdateWDEData(const vector<double> &values_block) {
  ComputeOptimalResolutionIndex(values_block);
  ComputeTranslations(values_block);
  ComputeEmpiricalScalingCoefficients(values_block);
}

/** Computes optimal resolition index.
 *
 * @brief Computes optimal resolition index.
 * @param values_block - Values from which resolution (dilation) index is computed.
 * @return Optimal resolution index.
 */
void LinearWDE::ComputeOptimalResolutionIndex(const vector<double> &values_block) {
    resolution_index_ = log2(values_block.size()) / 3.0 - 2.0 - log2(stDev(values_block));
}

/** Computes k_min and k_max (translation indices).
 * @brief Computes k_min and k_max (translation indices).
 * @param values_block - SORTED vector of values.
 */
void LinearWDE::ComputeTranslations(const vector<double> &values_block) {

  auto support = translated_dilated_scaling_function_.GetOriginalScalingFunctionSupport();
  int support_min = support.first;
  int support_max = support.second;

  k_min_ = ceil(pow(2, resolution_index_) * values_block[0] - support_max);
  k_max_ = floor(pow(2, resolution_index_) * values_block[values_block.size() - 1] - support_min);
}

/** Computes most important (according to threshold) empirical scaling function coefficients.
 * @brief Computes most important (according to threshold) empirical scaling function coefficients.
 * @param values - Vector of values in the block.
 */
void LinearWDE::ComputeEmpiricalScalingCoefficients(const vector<double> &values) {

  empirical_scaling_coefficients_ = {};

  if(values.empty()){
    return;
  }

  for(int k = k_min_; k <= k_max_; ++k){

    double coefficient = 0;

    translated_dilated_scaling_function_.UpdateIndices(resolution_index_, k);

    for(auto val : values){
      coefficient += translated_dilated_scaling_function_.GetValue(val);
    }

    coefficient /= values.size();

    //if(fabs(coefficient) > coefficient_threshold_){
    EmpiricalCoefficientData data;
    data.coefficient_ = coefficient;
    data.j_ = resolution_index_;
    data.k_ = k;
    empirical_scaling_coefficients_.push_back(data);
    //}
  }

}

/** Computes value of decomposed function in 1D point.
 * @brief Computes value of decomposed function in 1D point.
 * @param x - 1D point in which the value of function should be computed.
 * @return Value of DWT in x.
 */
double LinearWDE::GetValue(const double &x) const {
  double result = 0;

  for(auto data : empirical_scaling_coefficients_){
    translated_dilated_scaling_function_.UpdateIndices(data.j_, data.k_);
    result += data.coefficient_ * translated_dilated_scaling_function_.GetValue(x);
  }

  return weight_ * result;
}

/** Computes coefficients for the lower resolution.
 * @brief Computes coefficients for the lower resolution.
 */
void LinearWDE::LowerCoefficientsResolution() {
  --resolution_index_;

  auto filter_coefficients = translated_dilated_scaling_function_.GetFilterCoefficients();

  vector<EmpiricalCoefficientData> LowerResolutionEmpiricalCoefficients = {};

  ComputeLowerResolutionTranslations(filter_coefficients.size() - 1);

  for(int k = k_min_; k <= k_max_; ++k){

    double lower_res_coefficient = 0;

    for(auto val : empirical_scaling_coefficients_){
      int l = val.k_;
      int filter_coefficient_index = l - 2 * k;

      if(filter_coefficient_index < 0 || filter_coefficient_index >= filter_coefficients.size()){
        continue;
      }

      lower_res_coefficient += val.coefficient_ * filter_coefficients[filter_coefficient_index];
    }

    //if(fabs(lower_res_coefficient) > coefficient_threshold_){
    auto data = EmpiricalCoefficientData();
    data.coefficient_ = lower_res_coefficient;
    data.j_ = resolution_index_;
    data.k_ = k;
    LowerResolutionEmpiricalCoefficients.push_back(data);
    //}

  }

  empirical_scaling_coefficients_ = LowerResolutionEmpiricalCoefficients;
}

void LinearWDE::ComputeLowerResolutionTranslations(const int &number_of_filter_coefficients) {
  int min_coefficient_number = 0;
  int max_coefficient_number = number_of_filter_coefficients - 1;

  int k_min = ceil((empirical_scaling_coefficients_[0].k_ - max_coefficient_number) / 2);
  int k_max = empirical_scaling_coefficients_[empirical_scaling_coefficients_.size() - 1].k_;
  k_max = floor((k_max - min_coefficient_number) / 2);

  k_min_ = k_min;
  k_max_ = k_max;
}

int LinearWDE::GetResolutionIndex() const {
  return resolution_index_;
}

vector<EmpiricalCoefficientData> LinearWDE::GetEmpiricalCoefficients() const {
  return empirical_scaling_coefficients_;
}

double LinearWDE::GetWeight() const {
  return weight_;
}

void LinearWDE::SetWeight(const double &new_weight){
  weight_ = new_weight;
}

void LinearWDE::MultiplyWeight(const double &multiplier) {
  weight_ *= multiplier;
}

unsigned int LinearWDE::GetEmpiricalCoefficientsNumber() const {
  return empirical_scaling_coefficients_.size();
}

WaveletDensityEstimator *LinearWDE::Merge(WaveletDensityEstimator *other_wde) const {

  //cout << "Entering merge function. Creating empty vector.\n";

  vector<EmpiricalCoefficientData> merged_coefficients = {};

  //cout << "Created empty vector. Filling it with coefficients from this.\n";

  for(auto coefficient_data : empirical_scaling_coefficients_){
    merged_coefficients.push_back(coefficient_data);
  }

  //cout << "Filled empty vector initially filled. Multiplying coefficients by weights.\n";

  //for(auto coefficient_data : merged_coefficients){
  for(int i = 0; i < merged_coefficients.size(); ++i) {
    auto weighted_coefficient = merged_coefficients[i].coefficient_ * weight_;
    merged_coefficients[i].coefficient_ = weighted_coefficient;
  }

  //cout << "Coefficients weighted. Getting weight and coeffs from other.\n";

  auto other_coefficients = other_wde->GetEmpiricalCoefficients();
  auto other_weight = other_wde->GetWeight();

  //cout << "Got weight and coeffs from other. Summing weighted coeffs to merged coeffs.\n";

  unsigned int i = 0;

  /*
  cout  << "Merged size: " << merged_coefficients.size() << "\n"
        << "Others size: " << other_coefficients.size() << "\n"
        << "\n";
  */

  for(auto coefficient_data : other_coefficients){
    while(merged_coefficients[i].k_ < coefficient_data.k_){
      ++i;
      //cout << "Incremented i to " << i << endl;
    }
    if(merged_coefficients[i].k_ == coefficient_data.k_){
      //cout << "Adding coefficients at " << i << ".\n";
      merged_coefficients[i].coefficient_ += coefficient_data.coefficient_ * other_weight;
      //cout << "Coefficients at " << i << " added.\n";
    } else {
      //cout << "Inserting coefficient at " << i << ".\n";
      //cout << "Merged coefficient size is: " << merged_coefficients.size() << endl;
      if(merged_coefficients.size() <= i){
        //cout << "Pushing back!\n";
        merged_coefficients.push_back(coefficient_data);
      } else {
        //cout << "Inserting!\n";
        merged_coefficients.insert(merged_coefficients.begin() + i, coefficient_data);
      }
      //cout << "Inserted. Weighting coefficient at " << i << ".\n";
      merged_coefficients[i].coefficient_ *= other_weight;
    }
  }

  //cout << "Returning from merge function.\n";
  return new LinearWDE(merged_coefficients);
}




