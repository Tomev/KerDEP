#ifndef DESDA_H
#define DESDA_H

#include <QDebug>
#include <memory>

#include "KDE/kerneldensityestimator.h"
#include "KDE/weightedSilvermanSmoothingParameterCounter.h"
#include "Distributions/distributions.h"
#include "Reservoir_sampling/reservoirSamplingAlgorithm.h"
#include "groupingThread/groupingThread.h"
#include "StationarityTests/kpssstationaritytest.h"

class DESDA
{
  public:
    DESDA(std::shared_ptr<kernelDensityEstimator> estimator,
          std::shared_ptr<kernelDensityEstimator> estimatorDerivative,
          std::shared_ptr<kernelDensityEstimator> enchancedKDE,
          double weightModifier,
          reservoirSamplingAlgorithm *samplingAlgorithm,
          std::vector<std::shared_ptr<cluster>> *clusters,
          std::vector<std::shared_ptr<cluster>> *storedMedoids,
          double desiredRarity, groupingThread *gt, double newWeightB, int mE, int kpssX, int lambda);

    void performStep();
    QVector<double> getKernelPrognosisDerivativeValues(const QVector<qreal> *X);
    QVector<double> getEnhancedKDEValues(const QVector<qreal> *X);
    std::vector<double> getClustersWeights(const std::vector<std::shared_ptr<cluster>> &clusters);
    void sigmoidallyEnhanceClustersWeights(std::vector<std::shared_ptr<cluster>> *clusters);
    QVector<double> getWindowKDEValues(const QVector<qreal> *X);
    QVector<double> getKDEValues(const QVector<qreal> *X);
    QVector<double> getWeightedKDEValues(const QVector<qreal> *X);
    double getAverageOfFirstMSampleValues(int M);
    double getStdDevOfFirstMSampleValues(int M);
    cluster getEmECluster();
    double getStationarityTestValue();

    stationarityTestPtr stationarityTest;

    double _u_i = 0.0;
    std::vector<double> _selectedVValues = {};

    double delta = 0, gamma = 50000.0;
    double w_E = 0.98; // w_EmE
    double avg = 0;
    double _stDev = 1;

    int _maxM = 1000;
    int _minM = 200;
    int _mE = 0; // Cardinality of observed new objects
    int _m = 0; // Cardinality of objects to build KDE
    int _lambda = 100;

    double _newWeightB = 0;
    double _alpha = 2 * 5.1;
    double _beta = 1 * 5.5;
    int _kpssM = 0; // m_Eta

    // From 31 XI 2019 mail
    // Delta update parameters
    double _s = -4.0;
    double _mu = 1000;

    // Rare elements
    double _r = 0.05;
    double _quantileEstimator = 0;
    QVector<double> getRareElementsEnhancedKDEValues(const QVector<qreal> *X);
    std::vector<std::shared_ptr<cluster>> getAtypicalElements();
    QVector<std::pair<double, double> > getAtypicalElementsValuesAndDerivatives();

    // General purpose
    double _sgmKPSS = 0;

    // Analysis
    double getMaxAbsAOnLastKPSSMSteps();

    /** According to 2020 first article **/
    std::vector<unsigned int> _examinedClustersIndices = {};
    std::vector<unsigned int> _clustersExaminedForAsIndices = {};
    std::vector<double> _maxAbsAs = {};
    std::vector<double> _examinedClustersAs = {};
    std::vector<double> _examinedClustersWStar = {};
    std::vector<double> _examinedClustersWStar2 = {};
    std::vector<double> _examinedClustersWStar3 = {};
    std::vector<int> _examinedClustersIndicesInUncommonClustersVector = {};
    std::vector<double> _examinedClustersW = {};

  protected:

    const double _MAX_A = 1.5;
    const double _MIN_A = 0.01;

    std::vector<double> _EmEWeights = {};
    double _EmEWeightsSum = 0.0;

    int _stepNumber = 0;
    int _numberOfClustersForGrouping = 100;
    int _medoidsNumber = 50;

    double _weightModifier = 0.0;
    double _smoothingParameterMultiplier = 1.0;
    double _positionalSecondGradeEstimator = 0.0;
    double _maxEstimatorValueOnDomain = 0.0;
    double _a = 0.0;
    double _previousUncommonClustersWeight = 0.0;
    double _averageMaxPredictionAInLastKPSSMSteps = 0;
    double _averageMaxPredictionAInLastMinMSteps = 0;

    cluster emE;
    bool _shouldCluster = false;

    std::shared_ptr<kernelDensityEstimator> _estimator;
    std::shared_ptr<kernelDensityEstimator> _estimatorDerivative;
    std::shared_ptr<kernelDensityEstimator> _enhancedKDE;
    std::shared_ptr<distribution> _targetDistribution;
    weightedSilvermanSmoothingParameterCounter *_smoothingParamCounter;
    reservoirSamplingAlgorithm *_samplingAlgorithm;
    std::vector<std::shared_ptr<cluster>> *_clusters;
    std::vector<std::shared_ptr<sample>> _objects;

    std::vector<std::shared_ptr<cluster>> *_storedMedoids;
    std::vector<std::shared_ptr<cluster>> _uncommonClusters;

    groupingThread *_grpThread;

    void updateWeights();
    void updateExaminedClustersIndices();
    std::vector<std::shared_ptr<cluster>> getClustersForEstimator();
    std::vector<std::shared_ptr<cluster>> getClustersForWindowedEstimator();
    void countKDEValuesOnClusters();
    void updatePrognosisParameters();
    void updateM();
    void updateDelta();
    double getNewEmEValue();
    void updateMaxAbsAVector();
    double getCurrentMaxAbsA();
    void updateAverageMaxAbsAsInLastKPSSMSteps();
    void updateAverageMaxAbsAsInLastMinMSteps();
    void updateExaminedClustersAsVector();

    // Domain reduction
    double getDomainMinValue(const std::vector<clusterPtr> &clusters, double h);
    double getDomainMaxValue(const std::vector<clusterPtr> &clusters, double h);
    QVector<double> getErrorDomain();
    QVector<double> getWindowedErrorDomain();
    double _h;
    double _hWindowed;
    double calculateH(const std::vector<clusterPtr> &clusters);


    // Rare elements
    double _psi = 20.4;

    void enhanceWeightsOfUncommonElements();
    std::vector<double> getVectorOfAcceleratedKDEValuesOnClusters();
    std::vector<std::pair<int, double> > getSortedAcceleratedKDEValues(const std::vector<double> &AKDEValues);
    void recountQuantileEstimatorValue(const std::vector<std::pair<int, double> > &sortedIndicesValues);
};

#endif // DESDA_H
