#include <stdio.h>

#include "../common/arraylist.h"

#define ROW_LENGTH 16
#define ROWS 16

ArrayList getRow()
{
    ArrayList output = arraylist_new(double, ROW_LENGTH);
    // Generate a row of <ROW_LENGTH> values
    for (double i = 0; i < ROW_LENGTH; i += 1.0)
    {
        arraylist_push(output, &i);
    }
    return output;
}

void getDataset()
{
    ArrayList dataset = arraylist_new(double *, 0);
    // Generate <ROWS> amounts of rows
    for (size_t i = 0; i < ROWS; i++)
    {
        ArrayList row = getRow();
        arraylist_push(dataset, &row->_array);
        free(row); // Disposing the list container since we only use it's internal array
    }

    double **double2d = (double **)dataset->_array;

    // Printing array content
    for (size_t i = 0; i < dataset->length; i++)
    {
        printf("Row[%zu]: ", i);
        for (size_t j = 0; j < ROW_LENGTH; j++)
        {
            double item = double2d[i][j];
            printf("%lf ", item);
        }
        puts("");
    }
    printf("Rows: %zu\n", dataset->length);

    // Freeing up memory
    for (size_t i = 0; i < dataset->length; i++)
    {
        ArrayList item = *(ArrayList *)arraylist_elementAt(dataset, i);
        arraylist_destroy(item);
    }
    arraylist_destroy(dataset);
}

int main(void)
{
    getDataset();
    puts("Program finished");
}
