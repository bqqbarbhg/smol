#define SMOL_IMPLEMENTATION
#include "smol.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	int result;

	{
		sm_ctx *C = smInit();
		smCall(C, "__add__", "ii", 1, 2);
		smPop(C, "i", &result);
		smDestroy(C);
	}

	printf("1 + 2 = %d\n", result);

	return 0;
}

