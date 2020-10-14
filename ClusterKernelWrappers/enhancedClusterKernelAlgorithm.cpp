#include "enhancedClusterKernelAlgorithm.h"

EnhancedClusterKernelAlgorithm::EnhancedClusterKernelAlgorithm(const int &m,
                                                               ClusterKernel *(*clusterKernelFactoryMethod)(ClusterKernelStreamElement *stream_element))
 : UnivariateListBasedClusterKernelAlgorithm(m, clusterKernelFactoryMethod)
{}

std::vector<Point> EnhancedClusterKernelAlgorithm::GetErrorDomain(const int &dimension) {
  double minimal_value = FindMinimalValueOnDimension(dimension);
  double maximal_value = FindMaximalValueOnDimension(dimension);
  // Similarly to 5 sigma.
  minimal_value -= 5 * bandwidth_[dimension];
  maximal_value += 5 * bandwidth_[dimension];
  // We want hard 1000 points, so:
  double step_size = (maximal_value - minimal_value) / 1000;
  auto domain = std::vector<Point>();

  for(auto current_value = minimal_value; current_value < maximal_value; current_value += step_size){
    domain.push_back({current_value});
  }

  return domain;
}

double EnhancedClusterKernelAlgorithm::FindMinimalValueOnDimension(const int &dimension) {
  if(cluster_kernels_.empty()){
    return 0;
  }

  double minimal_value_on_dimension = cluster_kernels_[0]->GetMean()[dimension];

  for(auto i = 1; i < cluster_kernels_.size(); ++i){
    if(cluster_kernels_[i]->GetMean()[dimension] < minimal_value_on_dimension){
      minimal_value_on_dimension = cluster_kernels_[i]->GetMean()[dimension];
    }
  }

  return minimal_value_on_dimension;
}

double EnhancedClusterKernelAlgorithm::FindMaximalValueOnDimension(const int &dimension) {
  if(cluster_kernels_.empty()){
    return 0;
  }

  double maximal_value_on_dimension = cluster_kernels_[0]->GetMean()[dimension];

  for(auto i = 1; i < cluster_kernels_.size(); ++i){
    if(cluster_kernels_[i]->GetMean()[dimension] > maximal_value_on_dimension){
      maximal_value_on_dimension = cluster_kernels_[i]->GetMean()[dimension];
    }
  }

  return maximal_value_on_dimension;
}

Point EnhancedClusterKernelAlgorithm::GetKDEValuesOnDomain(std::vector<Point> domain) {
  Point kde_values_on_domain = {};

  for(const Point& pt : domain){
    // Note that KDE returned values should be one dimensional
    double value_on_point = GetValue(pt)[0];
    kde_values_on_domain.push_back(value_on_point);
  }

  return kde_values_on_domain;
}

void EnhancedClusterKernelAlgorithm::PerformStep(ClusterKernelStreamElement *stream_element) {
  ClusterKernelsAlgorithm::PerformStep(stream_element);
}

