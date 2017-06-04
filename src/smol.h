#if !defined(SMOL_HEADER_INCLUDED)
#define SMOL_HEADER_INCLUDED

#include <stdarg.h>

typedef struct smol_ctx smol_ctx;
typedef void (*smol_native_fn)(smol_ctx *, void*);

smol_ctx *smolInit();
void smolDestroy(smol_ctx *C);

void smolPush(smol_ctx *C, const char *fmt, ...);
int smolPop(smol_ctx *C, const char *fmt, ...);
int smolDrop(smol_ctx *C, int n);
int smolArg(smol_ctx *C, int index, const char *fmt, ...);
int smolReturn(smol_ctx *C, const char *fmt, ...);

void smolCall(smol_ctx *C, int numArgs);

#endif

#if (defined(SMOL_IMPLEMENTATION) || defined(SMOL_INTERNAL)) && !defined(SMOL_IMPLEMENTATION_INCLUDED)
#define SMOL_IMPLEMENTATION_INCLUDED

#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct smol_type smol_type;
typedef struct smol_val smol_val;
typedef struct smol_native smol_native;
typedef struct smol_val_ptr smol_val_ptr;
typedef struct smol_val_pair smol_val_pair;

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
	char *DataP;
	unsigned BP, SP;

	char **GlobalNames;
	smol_val_pair **GlobalValues;
	int GlobalCount;

	smol_type *Int, *Native;
};

smol_ctx *smolInit()
{
	smol_ctx *C;
	C = (smol_ctx*)calloc(1, sizeof(smol_ctx));

	C->StackData = (char*)calloc(16, 1024);
	C->Stack = (smol_val_ptr*)calloc(1, sizeof(smol_val_ptr));
	C->DataP = C->StackData;
	C->BP = C->SP = 0;

	C->GlobalNames = (char**)calloc(128, sizeof(char*));
	C->GlobalValues = (smol_val_pair**)calloc(128, sizeof(smol_val_pair*));
	C->GlobalCount = 0;

	C->Int = (smol_type*)calloc(1, sizeof(smol_type));
	C->Int->Size = sizeof(int);
	C->Native = (smol_type*)calloc(1, sizeof(smol_type));
	C->Native->Size = sizeof(smol_native);
}

void smolDestroy(smol_ctx *C)
{
	free(C);
}

int smol_StackWrite(smol_ctx *C, const char *fmt, va_list va)
{
	int start = C->SP, pos = start;
	char *datap = C->DataP;
	smol_val_ptr *val;

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

		case 'G': {
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
		val->Type = type;
		val->Value = datap;

		pos++;
		datap += type->Size;
		datap += (8 - (uintptr_t)datap % 8) % 8;
	}

	C->DataP = datap;
	C->SP = pos;
	return pos - start;
}

int smol_StackRead(smol_ctx *C, int start, int dir, int end, const char *fmt, va_list va)
{
	int pos = start;
	char *datap = (char*)C->Stack[pos].Value;
	smol_val_ptr *val;

	for (const char *c = fmt; *c; c++) {
		smol_type *type = NULL;

		if (pos == end)
			return -1;

		val = &C->Stack[pos];

		switch (*c) {

		case 'n': {
			smol_native *n = (smol_native*)val->Value;
			*va_arg(va, smol_native_fn*) = n->Func;
			*va_arg(va, void**) = n->Arg;
			} break;

		case 'i': {
			*va_arg(va, int*) = *(int*)val->Value;
			} break;

		case 'G': {
			int i;
			const char *name = va_arg(va, const char*);
			int inserted = 0, num = C->GlobalCount;

			for (i = 0; i < num; i++) {
				if (!strcmp(C->GlobalNames[i], name)) {
					smol_val_pair *p = C->GlobalValues[i];
					memcpy(p->Value, val->Value, val->Type->Size);
					inserted = 1;
					break;
				}
			}

			if (!inserted) {
				smol_type *t = val->Type;
				smol_val_pair *p = (smol_val_pair*)malloc(sizeof(smol_type*) + t->Size);
				p->Type = val->Type;
				memcpy(p->Value, val->Value, t->Size);
				C->GlobalNames[num] = strdup(name);
				C->GlobalValues[num] = p;
				C->GlobalCount = num + 1;
			}

			}

		}

		pos += dir;
	}

	return dir > 0 ? pos - start : start - pos;
}

void smolPush(smol_ctx *C, const char *fmt, ...)
{
	int num;
	va_list va;
	va_start(va, fmt);
	num = smol_StackWrite(C, fmt, va);
	va_end(va);
}

int smolPop(smol_ctx *C, const char *fmt, ...)
{
	int num;
	va_list va;
	va_start(va, fmt);
	num = smol_StackRead(C, C->SP - 1, -1, C->BP - 1, fmt, va);
	va_end(va);

	smolDrop(C, num);

	return num >= 0;
}

int smolDrop(smol_ctx *C, int n)
{
	C->SP -= n;
	C->DataP = (char*)C->Stack[C->SP].Value;
}

int smolArg(smol_ctx *C, int index, const char *fmt, ...)
{
	int num;
	va_list va;
	va_start(va, fmt);
	num = smol_StackRead(C, C->BP, 1, C->SP, fmt, va);
	va_end(va);

	return num >= 0;
}

int smolReturn(smol_ctx *C, const char *fmt, ...)
{
	int num;
	va_list va;

	C->SP = C->BP;

	va_start(va, fmt);
	num = smol_StackWrite(C, fmt, va);
	va_end(va);
}

void smolCall(smol_ctx *C, int numArgs)
{
	smol_val_ptr *val = &C->Stack[C->SP - numArgs - 1];
	smol_native *n = (smol_native*)val->Value;
	int bp = C->BP;
	C->BP = C->SP - numArgs;
	n->Func(C, n->Arg);
	C->BP = bp;
}

#endif

