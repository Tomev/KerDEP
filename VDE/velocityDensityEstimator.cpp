#include "velocityDensityEstimator.h"

#include <QDebug>

velocityDensityEstimator::velocityDensityEstimator(kernelDensityEstimator *kde, std::map<long, std::map<point, double> > *temporalVelocityDensityProfile)
{
  KDE.reset(kde);
  this->temporalVelocityDensityProfile = temporalVelocityDensityProfile;
}

double velocityDensityEstimator::countTemporalVelocityDensityProfileFromClusters(
    std::vector<std::shared_ptr<cluster> > clusters)
{
  KDE->setClusters(clusters);
  countTemporalWindowFromClusters(clusters);

  double ptVelocityDensity;
  std::vector<double> forwardTimeSliceDensity, reverseTimeSliceDensity;

  for(std::shared_ptr<point> pt : domain)
  {
    forwardTimeSliceDensity.push_back(countForwardTimeSliceDensityInPoint(clusters, pt));
    reverseTimeSliceDensity.push_back(countReverseTimeSliceDensityInPoint(clusters, pt));
  }

  normalizeTimeSliceDensity(&forwardTimeSliceDensity);
  normalizeTimeSliceDensity(&reverseTimeSliceDensity);

  for(unsigned int i = 0; i < forwardTimeSliceDensity.size(); ++i)
  {
    ptVelocityDensity = forwardTimeSliceDensity[i] - reverseTimeSliceDensity[i];
    ptVelocityDensity /= temporalWindow;
    (*temporalVelocityDensityProfile)[time][*(domain[i].get())] = ptVelocityDensity;
  }

  return 0.0;
}

long velocityDensityEstimator::setTime(long time)
{
  this->time = time;

  return this->time;
}

QVector<std::shared_ptr<point> > *velocityDensityEstimator::getDomainPtr()
{
  return &domain;
}

std::map<long, std::map<point, double> >* velocityDensityEstimator::getTemporalVelocityDensityProfilePtr()
{
  return temporalVelocityDensityProfile;
}

long velocityDensityEstimator::countTemporalWindowFromClusters
  (std::vector<std::shared_ptr<cluster> > clusters)
{
  long largestTimestamp, smallestTimestamp = largestTimestamp = clusters[0]->getTimestamp();
  long clustersTimestamp;

  for(std::shared_ptr<cluster> c : clusters)
  {
    clustersTimestamp = c->getTimestamp();

    if(clustersTimestamp > largestTimestamp)
      largestTimestamp = clustersTimestamp;

    if(clustersTimestamp < smallestTimestamp)
      smallestTimestamp = clustersTimestamp;
  }

  temporalWindow = largestTimestamp - smallestTimestamp;

  return this->temporalWindow;
}

double velocityDensityEstimator::countForwardTimeSliceDensityInPoint
  (std::vector<std::shared_ptr<cluster> > clusters, std::shared_ptr<point> pt)
{
  double result = 0.0;

  point spatialLocation;
  long moment;

  for(std::shared_ptr<cluster> c: clusters)
  {
    moment = time - c->getTimestamp();
    spatialLocation = countSpatialLocationForTimeSliceComputation(pt, c);

    result += countSpatiotemporalKernelValue(pt, moment);
  }

  return result;
}

QVector<qreal> velocityDensityEstimator::countSpatialLocationForTimeSliceComputation
  (std::shared_ptr<point> pt, std::shared_ptr<cluster> c)
{
  QVector<qreal> location;

  std::string dimensionAccessor;
  std::unordered_map<std::string, std::string> clustersData = c->getObject()->attributesValues;

  for(unsigned int i = 0; i < clustersData.size(); ++i)
  {
    dimensionAccessor = "Val" + std::to_string(i);

    location.push_back(pt->at(i) - std::stod(clustersData[dimensionAccessor]));
  }

  return location;
}

double velocityDensityEstimator::countSpatiotemporalKernelValue(
    std::shared_ptr<point> pt, long moment)
{
  double result = 0.0;

  result = KDE->getValue(pt.get());

  result *= (1.0 - (double) moment / (double) temporalWindow);

  return result;
}

double velocityDensityEstimator::countReverseTimeSliceDensityInPoint
  (std::vector<std::shared_ptr<cluster> > clusters, std::shared_ptr<point> pt)
{
  double result = 0.0;

  point spatialLocation;
  long moment;

  for(std::shared_ptr<cluster> c: clusters)
  {
    moment = c->getTimestamp() - (time - temporalWindow);
    spatialLocation = countSpatialLocationForTimeSliceComputation(pt, c);

    result += countSpatiotemporalKernelValue(pt, moment);
  }

  return result;
}

int velocityDensityEstimator::normalizeTimeSliceDensity(std::vector<double> *timeSliceDensity)
{
  double sum = 0.0;

  for(double value : *timeSliceDensity)
    sum += value;

  for(unsigned int i = 0; i < timeSliceDensity->size(); ++i)
    (*timeSliceDensity)[i] /= sum;

  return timeSliceDensity->size();
}