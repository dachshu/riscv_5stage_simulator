#include <stdio.h>
#include <stdlib.h>

#define SIZE 64

int get_rand()
{
	int num = rand() % 100;
	if (0 == (rand() % 2)) num = -num;
	return num;
}

int main()
{
	int first[SIZE][SIZE], second[SIZE][SIZE], multiply[SIZE][SIZE];

	srand(1);

	for (int i = 0; i < SIZE; ++i)
		for (int j = 0; j < SIZE; ++j) {
			first[i][j] = get_rand();
			second[i][j] = get_rand();
		}

	for (int i = 0; i < SIZE; ++i) {
		for (int j = 0; j < SIZE; ++j) {
			int sum = 0;
			for (int k = 0; k < SIZE; ++k) {
				sum = sum + first[i][k] * second[k][j];
			}
			multiply[i][j] = sum;
		}
	}

	printf("Multiply Finished\n");

	int sum = 0;
	for (int i = 0; i < SIZE; ++i)
		for (int j = 0; j < SIZE; ++j)
			sum = sum + multiply[i][j];

	if (1131377 == sum) printf("Value is Correct.\n");
	else printf("Calculation Error.\n");
}