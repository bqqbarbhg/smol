
typedef struct sm_ctx sm_ctx;

sm_ctx *smInit();
void smDestroy(sm_ctx *C);
void smPush(sm_ctx *C, const char *fmt, ...);
void smPop(sm_ctx *C, const char *fmt, ...);
void smCall(sm_ctx *C, const char *method);

typedef void (*sm_native)(sm_ctx *C, void *);

#if defined(SMOL_IMPLEMENTATION) || defined(SMOL_INTERNAL)

typedef unsigned char sm_byte;
typedef struct sm_list sm_list;
typedef struct sm_type sm_type;
typedef struct sm_val sm_val;
typedef struct sm_bind sm_bind;

struct sm_list
{
	sm_type *Type;
	sm_byte *Data;
	int Count, Capacity;
};

struct sm_val
{
	sm_type *Type;
	void *Value;
};

struct sm_bind
{
	sm_native *Native;
	void *Arg;
};

struct sm_type
{
	sm_type *Parent;
	unsigned Size;

	void *ArgData;
	sm_val *Args;
	int NumArgs;

	const char *MethodName;
	sm_bind *MethodBind;
	int MethodCount, MethodCap;
};

#endif

#if defined(SMOL_IMPLEMENTATION)

#include <stdarg.h>

struct sm_ctx
{
	sm_type *Int;
	sm_type *Type;

	sm_type **Types;
	int NumTypes;

	char *StackData;
	sm_val *Stack;
	unsigned BP, SP;

	const char *GlobalNames;
	sm_val *GlobalValues;
};

int sm_TypeEqual(const sm_type *a, const sm_type *b)
{
	return a == b;
}

int smReturn(sm_ctx *C, const char *fmt, ...)
{
	int pos = C->BP;
	char *dpos = (char*)C->Stack[pos].Value;

	sm_val *val;
	sm_type *type;

	va_args va;
	va_start(va, fmt);

	for (const char *c = fmt; *c; c++) {
		val = &C->Stack[pos];
		val->Value = dpos;

		switch (*c) {
		case '%':
			type = va_arg(va, sm_type*);
			memcpy(dpos, va_arg(va, void*), type->Size);
			dpos += type->Size;
			break;
		case 'i':
			type = C->Int;
			memcpy(dpos, &va_arg(va, int), sizeof(int));
			dpos += sizeof(int);
			break;
		}

		val->Type = type;
		pos++;
	}

	va_end(va);

	C->SP = pos;
}

int smArg(sm_ctx *C, int start, const char *fmt, ...)
{
	unsigned pos = SP - 1 - start;
	sm_val *val;
	sm_type *expect = NULL;

	va_args va;
	va_start(va, fmt);

	for (const char *c = fmt; *c; c++) {
		val = &C->Stack[pos];

		switch (*c) {
		case '&':
			expect = va_arg(va, sm_type*);
			if (sm_TypeEqual(val->Type, expect)) goto fail;
			*va_arg(va, void**) = val->Value;
			break;
		case '?':
			expect = va_arg(va, sm_type*);
			if (sm_TypeEqual(val->Type, expect)) goto fail;
			memcpy(va_arg(va, void*), val->Value, val->Type->Size);
			break;
		case 'i':
			expect = C->Int;
			if (sm_TypeEqual(val->Type, expect)) goto fail;
			*va_arg(va, int*) = *(int*)val->Value;
			break;
		case 'V':
			expect = NULL;
			*va_arg(va, sm_val*) = val;
			break;
		}

		pos--;
	}

	va_end(va);
	return 1;

fail:
	va_end(va);

	{
		int arg = (int)(SP - 1 - start) - (int)pos;
		smThrowFmt(C, "Argument {} type mismatch: got {} , expected {}", "itt",
				arg, val->Type, expected);
	}

	return 0;
}

void sm_TypeAddNativeMethod(sm_ctx *C, void*)
{
	sm_type *self;
	sm_native *native;
	void *arg;
	if (!smArg(C, 0, "tpp", self))
		return;
}

void sm_TypeInstantiate(sm_ctx *C, void*)
{
	sm_type *self;
	sm_val args[32];
	unsigned totalSize = 0;
	int i, j, numArgs = smNumArgs();

	if (!smArg(C, 0, "t", self))
		return;

	for (i = 1; i < numArgs; i++) {
		if (!smArg(C, i, "V", &args[i]))
			return;
		totalSize += args[i].Type->Size;
	}

	for (i = 0; i < C->NumTypes; i++) {
		int match = 1;
		sm_type *t = &C->Types[i];
		if (sm_TypeEqual(self, t->Parent) && t->NumArgs == numArgs) {
			if (!sm_ValEqual(&t->Args[i], &args[i])) {
				match = 0;
				break;
			}
		}
		if (match) {
			smReturn("t", t);
			return;
		}
	}

	{
		sm_type *t = &C->Types[C->NumTypes++];
		char *buf = (char*)malloc(totalSize + sizeof(sm_val) * numArgs);
		t->Parent = self;
		t->ArgData = buf;
		t->Args = buf + totalSize;
		t->NumArgs = numArgs;

		for (i = 0; i < numArgs; i++) {
			t->Args[i].Value = buf;
			t->Args[i].Type = args[i].Type;
			memcpy(t->Args[i].Value, args[i].Value, args[i].Size);
			buf += args[i].Size;
		}
	}
}

void sm_ListPush(sm_ctx *C, void*)
{
	sm_type *st; sm_list *self; void *data;
	if (!smArg(C, 0, "?&", &st, &self, self->Type, &data))
		return;

	if (ls->Count >= ls->Capacity) {
		unsigned cap = ls->Capacity ? ls->Capacity * 2 : 1;
		ls->Data = (sm_byte*)realloc(ls->Data, cap * sz);
	}
	memcpy(ls->Data + ls->Count * sz, data, sz);
	ls->Count++;

	smReturn(C, "%", st, &self);
}

void sm_ListGet(sm_ctx *C, void*)
{
	sm_type *st; sm_list *self; int index;
	if (!smArg(C, 0, "?i", &st, &self, &index))
		return;

	if (index < 0 || index >= self->Count) {
		smThrowFmt("Index {} out of bounds [0, {}]", "II", index, self->Count);
	}

	smReturn(C, "%", self->Type, self->Data + index * self->Type->Size);
}

sm_ctx *smInit()
{
	sm_ctx *C = (sm_ctx*)calloc(1, sizeof(sm_ctx));
	smPush(C, "Lt", C->Builtins);
	smCall(C, "Push");
}

void smDestroy(sm_ctx *C)
{
}

void smPush(sm_ctx *C, const char *fmt, ...)
{
}

void smPop(sm_ctx *C, const char *fmt, ...)
{
}

void smCall(sm_ctx *C, const char *method)
{
}

#endif

