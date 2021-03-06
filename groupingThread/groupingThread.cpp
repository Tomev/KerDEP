#include "groupingThread.h"

#include <QDebug>
#include <memory>

#include "kMedoidsAlgorithm/kMedoidsAlgorithm.h"
#include "kMedoidsAlgorithm/customObjectsDistanceMeasure.h"

#include "kMedoidsAlgorithm/attributesDistanceMeasures/numerical/gowersNumericalAttributesDistanceMeasure.h"
#include "kMedoidsAlgorithm/attributesDistanceMeasures/categorical/smdCategoricalAttributesDistanceMeasure.h"
#include "kMedoidsAlgorithm/clusterDistanceMeasures/completeLinkClusterDistanceMeasure.h"
#include "kMedoidsAlgorithm/clusterDistanceMeasures/centroidLinkClusterDistanceMeasure.h"



groupingThread::groupingThread(std::vector<std::shared_ptr<cluster>> *medoidsStorage, std::shared_ptr<dataParser> parser)
{
  this->medoidsStorage = medoidsStorage;
  this->parser = parser;
}

int groupingThread::initialize(int medoidsNumber, int bufferSize)
{
  int NUMBER_OF_MEDOIDS = medoidsNumber;
  int MEDOIDS_FINDING_STRATEGY = kMeansAlgorithm::RANDOM_ACCORDING_TO_DISTANCE; // k-means++

  attributesDistanceMeasure* CADM = new smdCategoricalAttributesDistanceMeasure();
  attributesDistanceMeasure* NADM = new gowersNumericalAttributesDistanceMeasure(attributesData);
  objectsDistanceMeasure* ODM = new customObjectsDistanceMeasure(CADM, NADM, attributesData);
  std::shared_ptr<clustersDistanceMeasure> CDM(new centroidLinkClusterDistanceMeasure(ODM));

  std::shared_ptr<groupingAlgorithm> algorithm(new kMeansAlgorithm
                                               (
                                                 NUMBER_OF_MEDOIDS,
                                                 CDM,
                                                 MEDOIDS_FINDING_STRATEGY,
                                                 parser
                                               )
                                              );

  storingAlgorithm.reset(new medoidStoringAlgorithm(algorithm, bufferSize));

  return 0;
}

void groupingThread::run()
{
  storingAlgorithm->findAndStoreMedoidsFromClusters(&clusters, medoidsStorage);

  qDebug() << "Grouping finished and medoids stored.";
}

int groupingThread::getObjectsForGrouping(std::vector<std::shared_ptr<sample>> samples)
{
  objects.clear();

  for(auto object : samples)
    objects.push_back(std::shared_ptr<sample>(object));

  return objects.size();
}

int groupingThread::getClustersForGrouping(std::vector<std::shared_ptr<cluster> > clusters)
{
  this->clusters.clear();

  this->clusters.insert(this->clusters.end(), clusters.begin(), clusters.end());

  return this->clusters.size();
}

int groupingThread::setAttributesData(std::unordered_map<std::string, attributeData *> *attributesData)
{
  this->attributesData = attributesData;

  return attributesData->size();
}
