/* file: host_cancel_compute.cpp */
/*******************************************************************************
* Copyright 2014-2020 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/*
!  Content:
!    C++ example demonstrates how to cancel algorithm's compute() call on a
!    host application request by means of user-defined callback.
!
!    The program trains the decision forest model on a training
!    datasetFileName and cancels computation on application request.
!******************************************************************************/
/**
 * <a name="DAAL-EXAMPLE-CPP-HOST_CANCEL_COMPUTE"></a>
 * \example host_cancel_compute.cpp
 */

#define DAAL_NOTHROW_EXCEPTIONS //it is required to get cancellation status as compute() return code rather than exception
#include "daal.h"
#include "service.h"
#include <time.h>

using namespace std;
using namespace daal;
using namespace daal::algorithms;
using namespace daal::data_management;
using namespace daal::algorithms::decision_forest::classification;
using namespace daal::services;

/* Input data set parameters */
const string trainDatasetFileName         = "../data/batch/df_classification_train.csv";
const size_t categoricalFeaturesIndices[] = { 2 };
const size_t nFeatures                    = 3; /* Number of features in training and testing data sets */

/* Decision forest parameters */
const size_t nTrees                    = 10;
const size_t minObservationsInLeafNode = 8;

const size_t nClasses = 5; /* Number of classes */

training::ResultPtr trainModel();
void loadData(const std::string & fileName, NumericTablePtr & pData, NumericTablePtr & pDependentVar);

int main(int argc, char * argv[])
{
    checkArguments(argc, argv, 1, &trainDatasetFileName);
    training::ResultPtr trainingResult = trainModel();
    return 0;
}

/** User-defined callback that implements HostAppIface */
class ExampleApp : public daal::services::HostAppIface
{
public:
    ExampleApp(double timeLimitSec) : _timeLimitSec(timeLimitSec), _bCancelled(false), _startTime(0) {}
    void start()
    {
        time(&_startTime);
        _bCancelled = false;
    }

    virtual bool isCancelled() DAAL_C11_OVERRIDE
    {
        if (_bCancelled) return true;
        time_t now;
        time(&now);
        const double sec = difftime(now, _startTime);
        if ((sec >= _timeLimitSec) && !_bCancelled)
        {
            if (!_bCancelled)
            {
                _bCancelled = true;
                std::cout << "Cancelled after " << sec << " seconds" << std::endl;
                return true;
            }
        }
        return false;
    }

private:
    time_t _startTime;
    const double _timeLimitSec;
    volatile bool _bCancelled;
};

training::ResultPtr trainModel()
{
    /* Create Numeric Tables for training data and dependent variables */
    NumericTablePtr trainData;
    NumericTablePtr trainDependentVariable;

    loadData(trainDatasetFileName, trainData, trainDependentVariable);

    /* Create an algorithm object to train the decision forest classification model */
    training::Batch<> algorithm(nClasses);

    /* Pass a training data set and dependent values to the algorithm */
    algorithm.input.set(classifier::training::data, trainData);
    algorithm.input.set(classifier::training::labels, trainDependentVariable);

    algorithm.parameter.nTrees                    = nTrees;
    algorithm.parameter.featuresPerNode           = nFeatures;
    algorithm.parameter.minObservationsInLeafNode = minObservationsInLeafNode;
    algorithm.parameter.varImportance             = algorithms::decision_forest::training::MDI;
    algorithm.parameter.resultsToCompute          = algorithms::decision_forest::training::computeOutOfBagError;

    ExampleApp host(10); /* set the time limit to work before cancelling equal to 10 sec */
    algorithm.setHostApp(HostAppIfacePtr(&host, EmptyDeleter()));
    host.start();

    Status s;
    do
    {
        /* Build the decision forest classification model */
        std::cout << "compute()" << std::endl;
        s = algorithm.compute();
    } while (s.ok());

    if (!s)
    {
        std::cout << s.getDescription() << std::endl;
        return training::ResultPtr();
    }

    /* Retrieve the algorithm results */
    return algorithm.getResult();
}

void loadData(const std::string & fileName, NumericTablePtr & pData, NumericTablePtr & pDependentVar)
{
    /* Initialize FileDataSource<CSVFeatureManager> to retrieve the input data from a .csv file */
    FileDataSource<CSVFeatureManager> trainDataSource(fileName, DataSource::notAllocateNumericTable, DataSource::doDictionaryFromContext);

    /* Create Numeric Tables for training data and dependent variables */
    pData.reset(new HomogenNumericTable<>(nFeatures, 0, NumericTable::notAllocate));
    pDependentVar.reset(new HomogenNumericTable<>(1, 0, NumericTable::notAllocate));
    NumericTablePtr mergedData(new MergedNumericTable(pData, pDependentVar));

    /* Retrieve the data from input file */
    trainDataSource.loadDataBlock(mergedData.get());

    NumericTableDictionaryPtr pDictionary = pData->getDictionarySharedPtr();
    for (size_t i = 0, n = sizeof(categoricalFeaturesIndices) / sizeof(categoricalFeaturesIndices[0]); i < n; ++i)
        (*pDictionary)[categoricalFeaturesIndices[i]].featureType = data_feature_utils::DAAL_CATEGORICAL;
}
