#ifndef OBJECT_H
#define OBJECT_H
#include "value.h"
#include "chunk.h"

#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value)  ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE(value)  ((ObjNative*)AS_OBJ(value))

#define OBJ_TYPE(value)  (AS_OBJ(value)->type)

#define IS_STRING(value)   isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)


typedef enum ObjType
{
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_NATIVE,
	OBJ_UPVALUE,
} ObjType;

// anything on the heap.
typedef struct Obj
{
	ObjType type;
	struct Obj* next;
} Obj;

typedef struct ObjString
{
	ObjType type;
	Obj* next;
	uint32_t hash;
	int length;
	char chars[]; // flexible array member
} ObjString;

typedef struct ObjFunction
{
	ObjType type;
	struct Obj* next;
	int arity;
	struct Chunk chunk;
	ObjString* name;
} ObjFunction;

typedef struct ObjUpvalue
{
	ObjType type;
	struct Obj* next;
	Value* value;
	struct ObjUpvalue* nextUpvalue;
	Value closed;
} ObjUpvalue;

typedef struct ObjClosure
{
	ObjType type;
	struct Obj* next;
	ObjFunction* function;
	int upvalueCount;
	ObjUpvalue** upvalues;
} ObjClosure;

typedef Value (*NativeFun)(int argCount, Value* args);

typedef struct ObjNative
{
	ObjType type;
	struct Obj* next;
	int arity;
	ObjString* name;
	NativeFun fun;
} ObjNative;

bool isObjType(Value v, ObjType t)
{
	return IS_OBJ(v) && OBJ_TYPE(v) == t;
}

void printFunction(ObjFunction* function)
{
    printf("function %s", function->name == NULL ? "script" : function->name->chars);
}

void printNative(ObjNative* native)
{
    printf("native function %s", native->name->chars);
}

void printObject(Value v)
{
	switch(OBJ_TYPE(v))
	{
		case OBJ_STRING: printf("%s", AS_CSTRING(v)); break;
        case OBJ_FUNCTION: printFunction(AS_FUNCTION(v)); break;
        case OBJ_NATIVE: printNative(AS_NATIVE(v)); break;
		case OBJ_CLOSURE: printFunction(AS_CLOSURE(v)->function); break;
		case OBJ_UPVALUE: printf("upvalue"); break;
	}
}

uint32_t hashStringObj(ObjString* string)
{
	uint32_t hash = 2166136261; // big prime
	for(int i = 0; i < string->length; i++)
	{
		hash ^= (uint8_t)string->chars[i];
		hash *= 16777619; // other big prime
	}
	return hash;
}

uint32_t hashString(const char* chars, int length)
{
	uint32_t hash = 2166136261; // big prime
	for(int i = 0; i < length; i++)
	{
		hash ^= (uint8_t)chars[i];
		hash *= 16777619; // other big prime
	}
	return hash;
}

struct VM;
void registerObject(struct VM*, Obj*);

Obj* allocateObject(struct VM* vm, size_t size, ObjType type)
{
	Obj* object = (Obj*)malloc(size);
	object->type = type;
	registerObject(vm, object);
	return object;
}

void freeObject(Obj* object)
{
	switch(object->type)
	{ 
		case OBJ_STRING:
		case OBJ_NATIVE:
		case OBJ_UPVALUE:
			free(object);
            break;
		case OBJ_CLOSURE:
			free(((ObjClosure*)object)->upvalues);
			free(object);
            break;
        case OBJ_FUNCTION:
            freeChunk(&((ObjFunction*)object)->chunk);
            free(object);
            break;
	}
}

ObjFunction* newFunction(struct VM* vm)
{
	ObjFunction* function = (ObjFunction*)allocateObject(vm, sizeof(ObjFunction), OBJ_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

ObjClosure* newClosure(ObjFunction* function, int upvalueCount, struct VM* vm)
{
	ObjClosure* closure = (ObjClosure*)allocateObject(vm, sizeof(ObjClosure), OBJ_CLOSURE);
	closure->function = function;
	closure->upvalueCount = upvalueCount;
	closure->upvalues = (ObjUpvalue**)malloc(sizeof(ObjUpvalue*) * upvalueCount);
	return closure;
}

ObjUpvalue* newUpvalue(Value* value, struct VM* vm)
{
	ObjUpvalue* upvalue = (ObjUpvalue*)allocateObject(vm, sizeof(ObjUpvalue), OBJ_UPVALUE);
	upvalue->value = value;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

#endif