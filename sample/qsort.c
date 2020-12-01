#include <stdio.h>
#include <stdlib.h>

#define COUNT 100000

void qsort(int data[],  int start, int end)
{
	int i = start, j = end;
	int pivot = data[(start + end) / 2];
	int temp;
	do {
		while (data[i] < pivot) i++;
		while (data[j] > pivot) j--;
		if (i <= j) {
			temp = data[i];
			data[i] = data[j];
			data[j] = temp;
			i++;
			j--;
		}
	} while (i <= j);
	if (start < j) qsort(data, start, j);
	if (i < end) qsort(data, i, end);
}

int main()
{
	int *data = (int *) malloc(sizeof(int) * COUNT);
	srand(1);
	for (int i = 0; i < COUNT; i++) data[i] = rand();
	qsort(data, 0, COUNT - 1);
	int have_error = 0;
	for (int i = 0; i < COUNT - 1;++i)
		if (data[i] > data[i + 1]) {
			have_error = 1;
			break;
		}
	if (0 == have_error) printf("SUCCESS\n");
	else printf("FAIL\n");
}