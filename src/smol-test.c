#define SMOL_IMPLEMENTATION
#include "smol.h"
#include <stdio.h>

void squareFn(smol_ctx *C, void*p)
{
	int x;
	if (!smolArg(C, 0, "i", &x)) return;
	smolReturn(C, "i", x * x);
}

int main(int argc, char **argv)
{
	int result;
	smol_ctx *C = smolInit();

	smolPush(C, "n", &squareFn, NULL);
	smolPop(C, "G", "square");

	smolPush(C, "Gi", "square", 5);
	smolCall(C, 1);
	smolPop(C, "i", &result);

	smolDestroy(C);

	printf("square(5) = %d\n", result);

	return 0;
}

