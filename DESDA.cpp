#include "DESDA.h"
#include "KDE/pluginsmoothingparametercounter.h"

#include <QTime>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <algorithm>
#include <fstream>
#include <math.h>

# define M_PI           3.14159265358979323846  /* pi */

DESDA::DESDA(std::shared_ptr<kernelDensityEstimator> estimator,
             std::shared_ptr<kernelDensityEstimator> estimatorDerivative,
             std::shared_ptr<kernelDensityEstimator> enchancedKDE,
             double weightModifier,
             reservoirSamplingAlgorithm *samplingAlgorithm,
             std::vector<std::shared_ptr<cluster> > *clusters,
             std::vector<std::shared_ptr<cluster> > *storedMedoids,
             double desiredRarity, double pluginRank) :
    _weightModifier(weightModifier), _samplingAlgorithm(samplingAlgorithm),
    _estimatorDerivative(estimatorDerivative), _estimator(estimator),
    _clusters(clusters), _storedMedoids(storedMedoids), _r(desiredRarity),
    _enhancedKDE(enchancedKDE), _pluginRank(pluginRank) {
  _objects.clear();

  _maxM = _samplingAlgorithm->getReservoidMaxSize(); // _maxM should be like 1000 + 100 for every mode

  _m = _maxM;

  _minM = _maxM / 10; // This works for both 2D and 1D experiments with default settings.
  _kpssM = 600; // This is independent of maxM. Normally 500.

  _sgmKPSS = -1;
  _sgmKPSSPercent = 30;
  _stepNumber = 1;
  _smoothingParameterEnhancer = 0.9;

  for(int i = 0; i < estimator->getDimension(); ++i) {
    stationarityTests.push_back(std::make_shared<KPSSStationarityTest>(_kpssM));
  }

  generator = std::default_random_engine(5625); // Seed should be as in UI, can be passed to constructor
  dist = std::uniform_real_distribution<double>(0.0, 1.0);
}

int sgn(double val) {
  return (val > 0) - (val < 0);
}

double sum(std::vector<double> vals) {
  double valsSum = 0.0;

  for(auto val : vals) valsSum += val;

  return valsSum;
}

double absSum(std::vector<double> vals) {
  double valsSum = 0.0;

  for(auto val : vals) valsSum += fabs(val);

  return valsSum;
}

double stDev(std::vector<double> vals) {
  int m = vals.size();

  if(m < 2) return 1.0;

  double sigma = 0.0;

  sigma = sum(vals);
  sigma *= sigma / m;

  double sqrSum = 0;

  for(auto val : vals) sqrSum += val * val;

  sigma = sqrSum - sigma;

  sigma /= m - 1;

  return std::sqrt(sigma);
}

double sigmoid(double x) {
  return 1.0 / (1.0 + exp(-x));
}

double average(std::vector<double> values) {
  double average = 0;

  for(auto val : values)
    average += val;

  average /= values.size();

  return average;
}

void DESDA::performStep() {
  // Making place for new cluster
  while(_clustersForWindowed.size() >= _maxM) {
    _clustersForWindowed.pop_back();
    _objects.erase(_objects.begin(), _objects.begin() + 1);
  }

  while(_clusters->size() > _m) {
    _clusters->pop_back();
  }

  // Reservoir movement
  _samplingAlgorithm->performSingleStep(&_objects, _stepNumber);

  std::shared_ptr<cluster> newCluster =
      std::shared_ptr<cluster>(new cluster(_stepNumber, _objects.back()));
  newCluster->setTimestamp(_stepNumber);

  // KPSS count
  /*
  std::vector<double> values = {};


  for(size_t i = 0; i < newCluster->dimension(); ++i){
    values.push_back(stod(newCluster->getObject()->attributesValues["Val" + std::to_string(i)]));
  }

  for(size_t i = 0; i < _clusters->size() && values.size() < _kpssM; ++i) {
    auto c = (*_clusters)[i];
    values.push_back(std::stod(c->getObject()->attributesValues["Val0"]));
  }
   */

  for(int i = 0; i < stationarityTests.size(); ++i) {
    std::string attribute = (*newCluster->getObject()->attirbutesOrder)[i];
    stationarityTests[i]->addNewSample(std::stod(newCluster->getObject()->attributesValues[attribute]));
  }


  _sgmKPSS = sigmoid(_sgmKPSSParameters[_sgmKPSSPercent][0] * getStationarityTestValue()
                     - _sgmKPSSParameters[_sgmKPSSPercent][1]);
  _d = _sgmKPSS;

  // Beta0 update
  _beta0 = 2.0 / 3 * _sgmKPSS; // According to formula from 13 IV 2020

  _clusters->insert(_clusters->begin(), newCluster);
  _clustersForWindowed.insert(_clustersForWindowed.begin(), newCluster);

  // M update
  updateM();
  updateExaminedClustersIndices(); // For labels update

  _v = _m > _clusters->size() ? 1.0 - 1.0 / _clusters->size() : 1.0 - 1.0 / _m;
  cluster::_deactualizationParameter = _v;

  // Calculate smoothing parameterers
  _windowedSmoothingParametersVector = calculateH(*_clusters);
  auto currentClusters = getClustersForEstimator();
  _smoothingParametersVector = calculateH(currentClusters);

  // Update weights
  updateWeights();

  qDebug() << "Reservoir size in step " << _stepNumber
           << " is: " << currentClusters.size() << ".";

  // Update clusters prognosis
  countKDEValuesOnClusters();
  updatePrognosisParameters();
  countDerivativeValuesOnClusters();

  // Update a
  updateMaxAbsAVector();
  updateMaxAbsDerivativeVector();
  updateMaxAbsDerivativeInCurrentStep();

  _examinedClustersDerivatives.clear();
  for(auto index : _examinedClustersIndices) {
    if(index < 0) {
      _examinedClustersDerivatives.push_back(0);
    }
    else {
      _examinedClustersDerivatives.push_back(currentClusters[index]->_currentDerivativeValue);
    }
  }

  // Update uncommon elements
  _r = 0.01 + 0.09 * _sgmKPSS;

  ++_stepNumber;
}

void DESDA::updateWeights() {
  _examinedClustersWStar.clear();
  auto consideredClusters = getClustersForEstimator();
  auto m = consideredClusters.size();

  for(int j = 0; j < std::count(_examinedClustersIndices.begin(), _examinedClustersIndices.end(), -1); ++j) {
    _examinedClustersWStar.push_back(0);
  }

  for(int i = 0; i < consideredClusters.size(); ++i) {
    double newWeight = 2 * (1.0 - i * _sgmKPSS / m);
    consideredClusters[i]->setCWeight(newWeight);
    for(int j = 0; j < std::count(_examinedClustersIndices.begin(), _examinedClustersIndices.end(), i); ++j)
      _examinedClustersWStar.push_back(newWeight);
  }
}

/** DESDA::updateExaminedClustersIndices
 * @brief A function that updates indices of examined clusters. Note that it
 * should be called after m update.
 */
void DESDA::updateExaminedClustersIndices() {
  auto desiredClustersLocations = {0.2, 0.5, 0.8};
  _examinedClustersIndices.clear();

  auto m = getClustersForEstimator().size();

  for(auto val : desiredClustersLocations)
    _examinedClustersIndices.push_back(round(val * m) - 1);
}

std::vector<std::shared_ptr<cluster> > DESDA::getClustersForEstimator() {
  std::vector<std::shared_ptr<cluster> > consideredClusters = {};
  int i = 0;

  while(consideredClusters.size() < _m && i < _clusters->size()) {
    consideredClusters.push_back((*_clusters)[i]);
    ++i;
  }

  return consideredClusters;
}

std::vector<std::shared_ptr<cluster> > DESDA::getClustersForWindowedEstimator() {
  return _clustersForWindowed;

  std::vector<std::shared_ptr<cluster>> consideredClusters = {};

  for(auto c : *_clusters) consideredClusters.push_back(c);

  return consideredClusters;
}

/** DESDA::enhanceWeightsOfUncommonElements
 * @brief Enhance weights of atypical elements.
 *
 * The method is described in Kulczycki, Kus, Rybotycki 2020
 *
 */
void DESDA::enhanceWeightsOfUncommonElements() {
  auto uncommonElements = getAtypicalElements();
  std::vector<double> examinedClustersEnhancedWeights = {};
  std::vector<double> weightsEnhancers = {};

  for(int i = 0; i < uncommonElements.size(); ++i) {
    auto ue = uncommonElements[i];
    double weightEnhancer = ue->_currentDerivativeValue /
                            _maxAbsDerivativeValueInCurrentStep;
    weightEnhancer *= _sgmKPSS;
    weightEnhancer += 1;
    weightsEnhancers.push_back(weightEnhancer);
    ue->setCWeight(ue->getCWeight() * weightEnhancer);
  }

  // For labels
  _examinedClustersWStar3.clear();
  auto clusters = getClustersForEstimator();
  std::vector<std::string> examinedClustersIds = {};

  for(auto idx : _examinedClustersIndices) {
    if(idx < 0) examinedClustersIds.push_back("");
    else examinedClustersIds.push_back(clusters[idx]->getClustersId());
  }

  bool wasExaminedClusterUncommon = false;

  for(int i = 0; i < _examinedClustersIndices.size(); ++i) {

    wasExaminedClusterUncommon = false;

    for(int j = 0; j < uncommonElements.size(); ++j) {
      if(examinedClustersIds[i].compare(uncommonElements[j]->getClustersId()))
        continue;
      wasExaminedClusterUncommon = true;
      _examinedClustersWStar3.push_back(weightsEnhancers[j]);
      break;
    }
    if(!wasExaminedClusterUncommon)
      _examinedClustersWStar3.push_back(1);
  }
}

void DESDA::countKDEValuesOnClusters() {
  std::vector<double> x;
  auto consideredClusters = getClustersForEstimator();
  _estimator->setClusters(consideredClusters);

  _estimator->setSmoothingParameters(_smoothingParametersVector);

  for(std::shared_ptr<cluster> c : *_clusters) {
    x.clear();
    for(auto attribute: *c->getRepresentative()->attirbutesOrder) {
      x.push_back(std::stod(c->getRepresentative()->attributesValues[attribute]));
    }
    double estimatorValueOnCluster = _estimator->getValue(&x);
    c->_currentKDEValue = estimatorValueOnCluster;
  }
}

void DESDA::updatePrognosisParameters() {
  for(std::shared_ptr<cluster> c : *_clusters)
    c->updatePrediction();
}

/** @brief DESDA::countDerivativeValuesOnClusters
 *  Calculates derivative values on points represented by current clusters and
 *  assings them to clusters.
 */
void DESDA::countDerivativeValuesOnClusters() {
  // Get the domain. Formally only m would be needed, but it will not hurt
  // to count on whole domain.
  QVector<double> domain = {};

  for(auto c : *_clusters)
    domain.push_back(std::stod(c->getObject()->attributesValues["Val0"]));

  auto derivativeValues = getKernelPrognosisDerivativeValues(&domain);

  for(int i = 0; i < _clusters->size(); ++i)
    (*_clusters)[i]->_currentDerivativeValue = derivativeValues[i];
}

void DESDA::updateM() {
  if(_sgmKPSS < 0) return;

  //_m = round(1.1 * _maxM - (1.1 * _maxM - 0.9 * _minM) * _sgmKPSS);
  _m = round(1.1 * _maxM * (1.0 - _sgmKPSS));
  _m = _m < _minM ? _minM : _m;
  _m = _clusters->size() < _m ? _clusters->size() : _m;
  _m = _m > _maxM ? _maxM : _m;
}

/** DESDA::updateMaxAbsAVector
 * @brief Updates vector of absolute values of a. This vector should store _maxM
 * values at most and it's values should be counted on all stored clusters (as
 * all of them have their predictions updated.
 */
void DESDA::updateMaxAbsAVector() {
  // Add new value
  _maxAbsAs.insert(_maxAbsAs.begin(), getCurrentMaxAbsA());

  // Ensure size is as expected
  while(_maxAbsAs.size() > _maxM) _maxAbsAs.pop_back();
}

/** DESDA::getCurrentMaxAbsA
* @brief Finds and returns current maximal value of abs(a) of all clusters.
* @return Current maximal values of abs(a) of all clusters.
*/
double DESDA::getCurrentMaxAbsA() {
  if(_clusters->size() < 0) return -1; // Should not happen.
  double maxA = fabs((*_clusters)[0]->predictionParameters[1]);
  for(auto c : *_clusters) {
    double currentA = fabs(c->predictionParameters[1]);
    maxA = currentA > maxA ? currentA : maxA;
  }

  return maxA;
}

void DESDA::updateMaxAbsDerivativeVector() {
  // Add new value.
  _maxAbsDerivatives.insert(_maxAbsDerivatives.begin(),
                            getCurrentMaxAbsDerivativeValue());
  // Ensure size is proper.
  while(_maxAbsDerivatives.size() > _maxM) _maxAbsDerivatives.pop_back();
}

double DESDA::getCurrentMaxAbsDerivativeValue() {
  if(_clusters->size() < 0) return -1; // Should not happen.
  double maxAbsDerivative = fabs((*_clusters)[0]->_currentDerivativeValue);
  for(auto c : *_clusters) {
    double currentDerivative = fabs(c->_currentDerivativeValue);
    maxAbsDerivative =
        currentDerivative > maxAbsDerivative ? currentDerivative : maxAbsDerivative;
  }

  return maxAbsDerivative;
}

void DESDA::updateMaxAbsDerivativeInCurrentStep() {
  auto consideredClusters = getClustersForEstimator();
  _maxAbsDerivativeValueInCurrentStep = 0;
  for(auto c : consideredClusters) {
    auto cAbsDerivativeValue = fabs(c->_currentDerivativeValue);
    _maxAbsDerivativeValueInCurrentStep =
        _maxAbsDerivativeValueInCurrentStep > cAbsDerivativeValue ?
        _maxAbsDerivativeValueInCurrentStep
                                                                  : cAbsDerivativeValue;
  }
}

std::vector<double> DESDA::getAttributesValuesFromClusters(std::vector<std::shared_ptr<cluster> > clusters,
                                                           int dimension) {
  std::string attribute = (*_samplingAlgorithm->getAttributesList())[dimension];
  std::vector<double> values = {};

  for(auto c: clusters) {
    values.push_back(
        std::stod(c->getRepresentative()->attributesValues[attribute])
                    );
  }

  return values;
}

double DESDA::getDomainMinValue(const std::vector<double> &values, double h) {
  if(values.size() == 0) return -5 * h;

  double domainMin = values[0];

  for(auto val : values) {
    domainMin = val < domainMin ? val : domainMin;
  }

  return domainMin - 5 * h;
}

double DESDA::getDomainMaxValue(const std::vector<double> &values, double h) {
  if(values.size() == 0) return 5 * h;

  double domainMax = values[0];

  for(auto value : values) {
    domainMax = value > domainMax ? value : domainMax;
  }

  return domainMax + 5 * h;
}

QVector<double> DESDA::getErrorDomain(int dimension) {
  std::vector<std::shared_ptr<cluster>> currentClusters =
      getClustersForEstimator();
  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue = getDomainMinValue(attributesValues, _smoothingParametersVector[dimension]);
  double domainMaxValue = getDomainMaxValue(attributesValues, _smoothingParametersVector[dimension]);
  QVector<double> domain = {};
  double stepSize = (domainMaxValue - domainMinValue) / (1000);

  for(auto val = domainMinValue; val <= domainMaxValue; val += stepSize) {
    domain.push_back(val);
  }

  return domain;
}

QVector<double> DESDA::getWindowedErrorDomain(int dimension) {
  std::vector<std::shared_ptr<cluster>> currentClusters =
      getClustersForWindowedEstimator();
  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue =
      getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[dimension]);
  double domainMaxValue =
      getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[dimension]);
  QVector<double> domain = {};
  double stepSize = (domainMaxValue - domainMinValue) / (1000);

  for(auto val = domainMinValue; val <= domainMaxValue; val += stepSize) {
    domain.push_back(val);
  }

  return domain;
}

std::vector<double> DESDA::calculateH(const std::vector<clusterPtr> &clusters) {
  int dimensionsNumber = _estimator->getDimension();
  std::vector<double> smoothingParameters = {};

  if(clusters.size() == 1) {
    for(int i = 0; i < dimensionsNumber; ++i) {
      smoothingParameters.push_back(1);
    }
    return smoothingParameters;
  }

  // Plugin method.
  QVector<qreal> samples = {};

  for(auto attribute: *_samplingAlgorithm->getAttributesList()) {
    samples.clear();
    for(auto c: clusters) {
      samples.append(std::stod(c->getRepresentative()->attributesValues[attribute]));
    }
    pluginSmoothingParameterCounter counter(&samples, _pluginRank);
    smoothingParameters.push_back(counter.countSmoothingParameterValue()
                                  * _smoothingParameterEnhancer);
  }

  return smoothingParameters;
}

QVector<double> DESDA::getKernelPrognosisDerivativeValues(const QVector<qreal> *X, int dimension) {
  std::vector<std::shared_ptr<cluster>> currentClusters
      = getClustersForEstimator();
  std::vector<double> prognosisCoefficients = {};
  QVector<double> kernelPrognosisDerivativeValues = {};

  for(auto c : currentClusters)
    prognosisCoefficients.push_back(c->predictionParameters[1]);

  if(prognosisCoefficients.size() == currentClusters.size()) {
    _estimatorDerivative->setAdditionalMultipliers(prognosisCoefficients);
    _estimatorDerivative->setSmoothingParameters({_smoothingParametersVector});
    _estimatorDerivative->setClusters(currentClusters);

    std::string attributeKey =
        (*_samplingAlgorithm->getAttributesList())[dimension];
    std::vector<double> attributesValues =
        getAttributesValuesFromClusters(currentClusters, dimension);
    double domainMinValue =
        getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[0]);
    double domainMaxValue =
        getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[0]);

    for(qreal x: *X) {
      if(x > domainMinValue && x < domainMaxValue) {
        std::vector<double> pt = {x};
        kernelPrognosisDerivativeValues.push_back(
            _estimatorDerivative->getValue(&pt) * 10000 // For visibility
                                                 );
      }
      else {
        kernelPrognosisDerivativeValues.push_back(0);
      }
    }
  }

  return kernelPrognosisDerivativeValues;
}

std::vector<double> DESDA::getEnhancedKDEValues(const std::vector<std::vector<double>> *X, int dimension) {
  auto currentClusters = getClustersForEstimator();
  auto standardWeights = getClustersWeights(currentClusters);
  sigmoidallyEnhanceClustersWeights(&currentClusters);

  std::vector<double> enhancedKDEValues = {};

  _enhancedKDE->setClusters(currentClusters);
  _enhancedKDE->setSmoothingParameters({_smoothingParametersVector});

  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue =
      getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[0]);
  double domainMaxValue =
      getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[0]);

  for(auto x: *X) {
    if(x[dimension] > domainMinValue && x[dimension] < domainMaxValue) {
      enhancedKDEValues.push_back(_enhancedKDE->getValue(&x));
    }
    else {
      enhancedKDEValues.push_back(0);
    }
  }

  // Restore weights
  for(unsigned int i = 0; i < currentClusters.size(); ++i)
    currentClusters[i]->setCWeight(standardWeights[i]);

  return enhancedKDEValues;
}

/** DESDA::getClustersWeights
 * @brief Gets CWeights from given clusters.
 * @param clusters -- clusters to get weights from
 * @return std::vector<double> of weights
 */
std::vector<double> DESDA::getClustersWeights(const std::vector<std::shared_ptr<cluster> > &clusters) {
  std::vector<double> weights = {};

  for(auto c : clusters)
    weights.push_back(c->getCWeight());

  return weights;
}

/** DESDA::sigmoidallyEnhanceClustersWeights
 * @brief Enhancing weights of considered cluters based on prognosis.
 * @param clusters - clusters to enhance wieghts
 */
void DESDA::sigmoidallyEnhanceClustersWeights(std::vector<std::shared_ptr<cluster> > *clusters) {
  _examinedClustersWStar2.clear();

  for(auto index : _examinedClustersIndices)
    if(index < 0) _examinedClustersWStar2.push_back(0);

  for(int i = 0; i < clusters->size(); ++i) {
    auto c = (*clusters)[i];
    double beta = _beta0 * c->_currentDerivativeValue /
                  _maxAbsDerivativeValueInCurrentStep;
    if(_maxAbsDerivativeValueInCurrentStep == 0)
      beta = 0;

    double weightEnhancement = 1 + _sgmKPSS * beta;
    c->setCWeight(c->getCWeight() * weightEnhancement);

    for(int j = 0; j < std::count(_examinedClustersIndices.begin(), _examinedClustersIndices.end(), i); ++j)
      _examinedClustersWStar2.push_back(weightEnhancement);
  }
}

vector<double> DESDA::getWindowKDEValues(const vector<vector<qreal>> *X, int dimension) {
  vector<double> windowKDEValues = {};
  auto currentClusters = getClustersForWindowedEstimator();
  _estimator->setClusters(currentClusters);
  _estimator->setSmoothingParameters({_windowedSmoothingParametersVector});
  _estimator->_shouldConsiderWeights = false;

  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue =
      getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[0]);
  double domainMaxValue =
      getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[0]);

  for(auto x: *X) {
    if(x[dimension] > domainMinValue && x[dimension] < domainMaxValue) {
      windowKDEValues.push_back(_estimator->getValue(&x));
    }
    else {
      windowKDEValues.push_back(0);
    }
  }

  _estimator->_shouldConsiderWeights = true;
  _estimator->setClusters(getClustersForEstimator());

  return windowKDEValues;
}

std::vector<double> DESDA::getKDEValues(const vector<vector<double>> *X, int dimension) {
  std::vector<double> KDEValues = {};
  auto currentClusters = getClustersForEstimator();
  _estimator->setClusters(currentClusters);
  _estimator->setSmoothingParameters({_smoothingParametersVector});
  _estimator->_shouldConsiderWeights = false;

  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue =
      getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[0]);
  double domainMaxValue =
      getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[0]);

  for(auto x: *X) {
    if(x[dimension] > domainMinValue && x[dimension] < domainMaxValue) {
      KDEValues.push_back(_estimator->getValue(&x));
    }
    else {
      KDEValues.push_back(0);
    }
  }

  return KDEValues;
}

std::vector<double> DESDA::getWeightedKDEValues(const vector<vector<double>> *X, int dimension) {
  std::vector<double> weightedKDEValues = {};
  auto currentClusters = getClustersForEstimator();
  _estimator->setClusters(currentClusters);
  _estimator->setSmoothingParameters({_smoothingParametersVector});
  _estimator->_shouldConsiderWeights = true;

  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue =
      getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[0]);
  double domainMaxValue =
      getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[0]);

  for(auto x: *X) {
    if(x[dimension] > domainMinValue && x[dimension] < domainMaxValue) {
      weightedKDEValues.push_back(_estimator->getValue(&x));
    }
    else {
      weightedKDEValues.push_back(0);
    }
  }

  return weightedKDEValues;
}

double DESDA::getAverageOfFirstMSampleValues(int M) {
  double avg = 0;
  int m0 = std::min(M, (int) _clusters->size());

  for(int i = 0; i < m0; ++i)
    avg += std::stod(_clusters->at(i)->getObject()->attributesValues["Val0"]);

  return avg / m0;
}

double DESDA::getStdDevOfFirstMSampleValues(int M) {
  int m0 = std::min(M, (int) _clusters->size());

  if(m0 == 1) return 1;

  double avgME = 0;

  // Counting average
  for(int i = 0; i < m0; ++i) {
    avgME += std::stod(_clusters->at(i)->getObject()->attributesValues["Val0"]);
  }

  avgME /= m0;

  // Counting var
  double val, var = 0;

  for(int i = 0; i < m0; ++i) {
    val = std::stod(_clusters->at(i)->getObject()->attributesValues["Val0"]);
    var += pow(val - avgME, 2);
  }

  var /= m0 - 1;

  _stDev = pow(var, 0.5);
  return _stDev;
}

double DESDA::getStationarityTestValue() {
  std::vector<double> stationarityTestsValues = {};
  for(auto test: stationarityTests) {
    stationarityTestsValues.push_back(test->getTestsValue());
  }
  // Max
  return *std::max_element(stationarityTestsValues.begin(), stationarityTestsValues.end());
  // Euclidean
  double sum = 0;
  for(auto test_value : stationarityTestsValues){
    sum += pow(test_value, 2);
  }
  return sqrt(sum);
}

void DESDA::prepareEstimatorForContourPlotDrawing() {
  auto currentClusters = getClustersForEstimator();
  _unmodifiedCWeightsOfClusters = getClustersWeights(*_clusters);

  sigmoidallyEnhanceClustersWeights(&currentClusters);
  enhanceWeightsOfUncommonElements();

  _estimator->setClusters(currentClusters);
  _estimator->setSmoothingParameters({_smoothingParametersVector});
}

void DESDA::restoreClustersCWeights() {
  for(int i = 0; i < _clusters->size(); ++i) {
    (*_clusters)[i]->setCWeight(_unmodifiedCWeightsOfClusters[i]);
  }
}

/** DESDA::getAtypicalElements
 * @brief Finds and returns vector of atypical elements.
 *
 * Atypical elements are found using Kulczycki-Kruszewski method.
 *
 * @return Vector of atypical/rare/uncommon elements in _clusters.
 */
std::vector<clusterPtr> DESDA::getAtypicalElements() {
  auto AKDEValues = getVectorOfAcceleratedKDEValuesOnClusters();
  auto sortedIndicesValues = getSortedAcceleratedKDEValues(AKDEValues);
  recountQuantileEstimatorValue(sortedIndicesValues);
  std::vector<clusterPtr> atypicalElements = {};

  _trendsNumber = 0;

  for(int i = 0; i < sortedIndicesValues.size(); ++i) {
    if(_quantileEstimator > sortedIndicesValues[i].second) {
      atypicalElements.push_back((*_clusters)[sortedIndicesValues[i].first]);
      if((*_clusters)[sortedIndicesValues[i].first]->_currentDerivativeValue > 0){
        ++_trendsNumber;
      }
    }
  }

  _rareElementsNumber = atypicalElements.size();

  return atypicalElements;
}

std::vector<double> DESDA::getVectorOfAcceleratedKDEValuesOnClusters() {
  std::vector<double> x;
  auto consideredClusters = getClustersForEstimator();
  auto standardWeights = getClustersWeights(consideredClusters);

  if(consideredClusters.size() == 1)
    return {consideredClusters[0]->_currentKDEValue};

  sigmoidallyEnhanceClustersWeights(&consideredClusters);

  std::vector<double> AKDEValues = {};
  auto m = consideredClusters.size();

  for(auto i = 0; i < m; ++i) {
    auto c = consideredClusters[0];
    consideredClusters.erase(consideredClusters.begin(), consideredClusters.begin() + 1);
    _enhancedKDE->setClusters(consideredClusters);
    _enhancedKDE->setSmoothingParameters({_smoothingParametersVector});
    _enhancedKDE->_shouldConsiderWeights = true;

    for(auto attribute : *c->getRepresentative()->attirbutesOrder) {
      x.push_back(std::stod(c->getRepresentative()->attributesValues[attribute]));
    }
    AKDEValues.push_back(_enhancedKDE->getValue(&x));

    x.clear();
    consideredClusters.push_back(c);
  }

  // Restore weights
  for(unsigned int i = 0; i < consideredClusters.size(); ++i)
    consideredClusters[i]->setCWeight(standardWeights[i]);

  return AKDEValues;
}

std::vector<std::pair<int, double> > DESDA::getSortedAcceleratedKDEValues(const std::vector<double> &AKDEValues) {
  // Create pairs containing indexes and values of AKDE
  std::vector<std::pair<int, double>> indexesValues = {};

  for(int i = 0; i < AKDEValues.size(); ++i) {
    indexesValues.push_back(std::pair<int, double>(i, AKDEValues[i]));
  }

  // Sort them
  std::sort(
      indexesValues.begin(), indexesValues.end(),
      [](std::pair<int, double> a, std::pair<int, double> b) {
        return a.second > b.second;
      }
           );

  std::reverse(indexesValues.begin(), indexesValues.end());

  return indexesValues;
}

void DESDA::recountQuantileEstimatorValue(const std::vector<std::pair<int, double> > &sortedIndicesValues) {
  int m = sortedIndicesValues.size();
  double mr = _r * m;
  bool originalShouldConsiderWeights = _estimator->_shouldConsiderWeights;
  _estimator->_shouldConsiderWeights = false;

  if(mr < 0.5) {
    _quantileEstimator = sortedIndicesValues[0].second;

    if(_quantileEstimator < 1e-6) {
      qDebug() << "mr = " << mr;
      qDebug() << "Sorted indices values (using 0):";
      for(auto pair: sortedIndicesValues) {
        auto attrVals = _clusters->at(pair.first)->getRepresentative()->attributesValues;
        //std::vector<double> pt = {std::stod(attrVals["Val0"]), std::stod(attrVals["Val1"])};
        std::vector<double> pt = {std::stod(attrVals["Val0"])};
        qDebug() << "\ti: " << pair.first << ", x: " << pt[0] << ", y: " << pt[1]
                 << ", remembered value: " << pair.second
                 << ", estimator value: " << _estimator->getValue(&pt);
      }
    }

    _estimator->_shouldConsiderWeights = originalShouldConsiderWeights;
    return;
  }

  int i = mr + 0.5;

  _quantileEstimator =
      (0.5 + i - mr) * sortedIndicesValues[i - 1].second; // Remember that indices in the formulas start from 1.
  _quantileEstimator +=
      (0.5 - i + mr) * sortedIndicesValues[i].second; // Remember that indices in the formulas start from 1.

  if(_quantileEstimator < 1e-6) {
    qDebug() << "mr = " << mr;
    qDebug() << "Sorted indices values (using " << i - 1 << "and" << i << "):";
    for(auto pair: sortedIndicesValues) {
      auto attrVals = _clusters->at(pair.first)->getRepresentative()->attributesValues;
      //std::vector<double> pt = {std::stod(attrVals["Val0"]), std::stod(attrVals["Val1"])};
      std::vector<double> pt = {std::stod(attrVals["Val0"])};
      qDebug() << "\ti: " << pair.first << ", x: " << pt[0] << ", y: " << pt[1]
               << ", remembered value: " << pair.second
               << ", estimator value: " << _estimator->getValue(&pt);
    }
  }
  _estimator->_shouldConsiderWeights = originalShouldConsiderWeights;
}

std::vector<double> DESDA::getRareElementsEnhancedKDEValues(const std::vector<std::vector<double>> *X, int dimension) {
  std::vector<double> enhancedKDEValues = {};
  auto currentClusters = getClustersForEstimator();
  auto standardWeights = getClustersWeights(currentClusters);

  enhanceWeightsOfUncommonElements();
  sigmoidallyEnhanceClustersWeights(&currentClusters);

  _enhancedKDE->setClusters(currentClusters);
  _enhancedKDE->setSmoothingParameters({_smoothingParametersVector});

  _examinedClustersW.clear();

  for(auto index : _examinedClustersIndices) {
    if(index < 0)
      _examinedClustersW.push_back(0);
    else
      _examinedClustersW.push_back(currentClusters[index]->getCWeight());
  }

  std::vector<double> attributesValues =
      getAttributesValuesFromClusters(currentClusters, dimension);
  double domainMinValue =
      getDomainMinValue(attributesValues, _windowedSmoothingParametersVector[0]);
  double domainMaxValue =
      getDomainMaxValue(attributesValues, _windowedSmoothingParametersVector[0]);

  for(auto x: *X) {
    if(x[dimension] > domainMinValue && x[dimension] < domainMaxValue) {
      enhancedKDEValues.push_back(_enhancedKDE->getValue(&x));
    }
    else {
      enhancedKDEValues.push_back(0);
    }
  }

  // Restore weights
  for(unsigned int i = 0; i < currentClusters.size(); ++i)
    currentClusters[i]->setCWeight(standardWeights[i]);

  //qDebug() << "Max a param: " << maxAParam;
  return enhancedKDEValues;
}

/** DESDA::getAtypicalElementsValuesAndDerivatives
 * @brief Returns vector of current atypical elements values and their derivatives.
 *
 * This method assumes 1 dimensional, specifically constructed objects (for now).
 *
 * @return Vector of pairs of atypical elements values and their derivatives.
 */
QVector<std::pair<double, double>> DESDA::getAtypicalElementsValuesAndDerivatives() {
  QVector<std::pair<double, double>> atypicalElementsValuesAndDerivatives = {};
  auto atypicalElements = getAtypicalElements();

  for(auto a : atypicalElements) {
    std::pair<double, double> valueDerivative = std::pair<double, double>(0, 0);
    valueDerivative.first = std::stod(a->getObject()->attributesValues["Val0"]);
    valueDerivative.second = a->_currentDerivativeValue;
    atypicalElementsValuesAndDerivatives.push_back(valueDerivative);
  }

  return atypicalElementsValuesAndDerivatives;
}

double DESDA::getMaxAbsAOnLastKPSSMSteps() {
  if(_maxAbsAs.size() < _kpssM)
    return *(std::max_element(_maxAbsAs.begin(), _maxAbsAs.end()));

  return *(std::max_element(_maxAbsAs.begin() + _maxAbsAs.size() - _kpssM, _maxAbsAs.end()));
}

