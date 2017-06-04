#define SMOL_IMPLEMENTATION
#include "smol.h"
#include <stdio.h>

void addFn(smol_ctx *C, void*p)
{
	int a, b;
	if (!smolArg(C, 0, "ii", &a, &b)) return;
	smolReturn(C, "i", a + b);
}

int main(int argc, char **argv)
{
	int result;
	smol_ctx *C = smolInit();

	smolDefGlobal(C, "add", "n", &addFn, NULL);
	smolDefGlobal(C, "three", "i", 3);

	smolPush(C, "GGi", "add", "three", 5);
	smolCall(C, 2);
	smolPop(C, "i", &result);

	smolDestroy(C);

	printf("5 + 3 = %d\n", result);

	return 0;
}

