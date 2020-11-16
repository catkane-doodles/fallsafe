#include "combined_classifier.h"

#define combine_classifier "dataFiles/combinedClassifier.ann"
#define outputSize 4

Classifier classifier_new()
{
    Classifier c = (Classifier)malloc(sizeof(struct classifier));
    c->cC = fopen(combine_classifier, "r");
    c->cClassifier = genann_read(c->cC);

    fclose(c->cC);

    return c;
}

void classifier_destroy(Classifier c)
{
    genann_free(c->cClassifier);
}

int prediction(const double *output)
{
    int maxIndex = 0;
    double biggest = 0;
    for (int i = 0; i < outputSize; i++)
        if (output[i] > biggest)
        {
            maxIndex = i;
            biggest = output[i];
        }
    return maxIndex;
}

void classifier_reinforce(Classifier c, double *sample, int value)
{
    int *label = {0, 0, 0, 0};
    label[i] = 1;

    genann_train(c, sample, label, 1);
}

int classifier_predict(Classifier c, double *sample)
{
    /**
    * Prediction:
    * 0 -> Fall
    * 1 -> Walk
    * 2 -> Run
    * 3 -> Stationary
   */

    const double *cOutput = genann_run(c->cClassifier, sample);

    return prediction(cOutput);
}
