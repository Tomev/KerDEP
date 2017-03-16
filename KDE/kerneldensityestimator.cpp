#include "kerneldensityestimator.h"

#include "QDebug"

kernelDensityEstimator::kernelDensityEstimator(int sampleSize, QVector<qreal> *smoothingParameters, int kernelType, QVector<int> *kernelsIDs, distribution* targetDistribution)
    : kernelType(kernelType), smoothingParameters(smoothingParameters)
{
    if(kernelsIDs->size() != smoothingParameters->size())
    {
        qDebug() << "Smoothing parameters and kernels number aren't equal.";
        return;
    }

    if(sampleSize < 1)
    {
        qDebug() << "Sample size < 1.";
        return;
    }

    for(int sampleNumber = 0; sampleNumber < sampleSize; ++sampleNumber)
        samples.append(targetDistribution->getValue());

    fillKernelsList(kernelsIDs);
}

qreal kernelDensityEstimator::getValue(QVector<qreal>* x)
{
    if(x == NULL)
    {
        qDebug() << "Argument is null pointer.";
        return -1.0;
    }

    if(x->size() == 0)
    {
        qDebug() << "Argument is empty.";
        return -1.0;
    }

    switch(kernelType)
    {
        case PRODUCT:
            return getProductKernelValue(x);
        break;
        case RADIAL:
            return getRadialKernelValue(x);
        break;
        default:
            qDebug() << "Kernel type not precised.";
            return -1.0;
        break;
    }
}

qreal kernelDensityEstimator::getProductKernelValue(QVector<qreal> *x)
{
    // Check if values vector dimension is same size as kernels dimension
    if(x->size() != kernels.size())
    {
        qDebug() << "Kernel size and argument dimension doesn't match.";
        return -1.0;
    }

    qreal result = 0.0, addend;

    QVector<qreal>* tempValueHolder = new QVector<qreal>();

    foreach(qreal sample, samples)
    {
        addend = 1.0;

        for(int i = 0; i < kernels.size(); ++i)
        {
            tempValueHolder->clear();
            tempValueHolder->append((x->at(i)-sample)/smoothingParameters->at(i));
            addend *= kernels.at(i)->getValue(tempValueHolder);
        }

        result += addend;
    }

    foreach (qreal smoothingParameter, *smoothingParameters)
    {
        result /= smoothingParameter;
    }

    result /= samples.size();

    return result;

}

qreal kernelDensityEstimator::getRadialKernelValue(QVector<qreal> *x)
{
    // TODO TR: Code it when radial kernel is implemented

    return -1.0;
}

void kernelDensityEstimator::fillKernelsList(QVector<int> *kernelsIDs)
{
    switch(kernelType)
    {
        case PRODUCT:
            addProductKernelsToTheList(kernelsIDs);
        break;
        case RADIAL:
            addRadialKernelsToTheList(kernelsIDs);
        break;
        default:
        break;
    }
}

void kernelDensityEstimator::addProductKernelsToTheList(QVector<int> *kernelsIDs)
{
    foreach(int kernelID, *kernelsIDs)
    {
        switch (kernelID)
        {
            case NORMAL:
                kernels.append(kernelPtr(new normalKernel()));
            break;
            case TRIANGLE:
                kernels.append(kernelPtr(new triangleKernel()));
            break;
            case EPANECZNIKOW:
                kernels.append(kernelPtr(new epanecznikowKernel()));
            break;
            case DULL:
            default:
                kernels.append(kernelPtr(new dullKernel()));
            break;
        }
    }
}

void kernelDensityEstimator::addRadialKernelsToTheList(QVector<int> *kernelsIDs)
{
    // TR TODO: Fill when radial kernel will be used
}
