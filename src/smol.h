#if !defined(SMOL_HEADER_INCLUDED)
#define SMOL_HEADER_INCLUDED

#include <stdarg.h>

typedef struct smol_ctx smol_ctx;
typedef void (*smol_native_fn)(smol_ctx *, void*);

smol_ctx *smolInit();
void smolDestroy(smol_ctx *C);

void smolPush(smol_ctx *C, const char *fmt, ...);
int smolPop(smol_ctx *C, const char *fmt, ...);
int smolArg(smol_ctx *C, const char *fmt, ...);

void smolCall(smol_ctx *C, int numArgs);

#endif

#if (defined(SMOL_IMPLEMENTATION) || defined(SMOL_INTERNAL)) && !defined(SMOL_IMPLEMENTATION_INCLUDED)
#define SMOL_IMPLEMENTATION_INCLUDED

typedef struct smol_type smol_type;
typedef struct smol_val smol_val;
typedef struct smol_native smol_native;

struct smol_native
{
	smol_native_fn Func;
	void *Arg;
};

struct smol_type
{
	unsigned Size;
};

struct smol_val_ptr
{
	smol_type *Type;
	void *Value;
};

struct smol_val_pair
{
	smol_type *Type;
	char Value[1];
};

struct smol_ctx
{
	char *StackData;
	smol_val_ptr *Stack;
	unsigned BP, SP;

	char **GlobalNames;
	smol_val_pair **GlobalValues;
	int GlobalCount;

	smol_type *Int, *Native;
};

smol_ctx *smolInit()
{
	C = (smol_ctx*)calloc(1, sizeof(smol_ctx));
	C->Int = (smol_type*)calloc(1, sizeof(smol_type));
	C->Int.Size = sizeof(int);
	C->Native = (smol_type*)calloc(1, sizeof(smol_type));
	C->Native.Size = sizeof(smol_native);
}

void smolDestroy(smol_ctx *C)
{
	free(C);
}

void smol_StackWrite(smol_ctx *C, int start, const char *fmt, va_list va)
{
	int pos = start;
	char *datap = (char*)C->Stack[pos].Value;

	for (const char *c = fmt; *c; c++) {
		smol_type *type = NULL;
		val = &C->Stack[pos];

		switch (*c) {

		case 'n': {
			smol_native *n = (smol_native*)datap;
			n->Func = va_arg(va, smol_native_fn);
			n->Arg = va_arg(va, void*);
			type = C->Native;
			} break;

		case 'i': {
			*(int*)datap = va_arg(va, int);
			type = C->Int;
			} break;

		case 'g': {
			int i;
			const char *name = va_arg(va, const char*);
			for (i = 0; i < C->GlobalCount; i++) {
				if (!strcmp(C->GlobalNames[i], name)) {
					smol_val_pair *p = C->GlobalValues[i];
					type = p->Type;
					memcpy(datap, p->Value, p->Type->Size);
					break;
				}
			}
			}

		}

		assert(type != NULL);

		datap += type->Size;
		datap += (8 - (uintptr_t)datap % 8) % 8;
	}
}

void smol_StackRead(smol_ctx *C, int start, int dir, int end, const char *fmt, va_list va)
{
}

void smolPush(smol_ctx *C, const char *fmt, ...)
{
}

int smolPop(smol_ctx *C, const char *fmt, ...)
{
}

int smolArg(smol_ctx *C, const char *fmt, ...)
{
}

void smolCall(smol_ctx *C, int numArgs);

#endif

