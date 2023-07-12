#ifndef VALUE_H
#define VALUE_H
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
//todo: string encoding.
#define    NIL_VAL        ((Value){VAL_NIL   , {.number  = 0          }})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number  =       value}})
#define   BOOL_VAL(value) ((Value){VAL_BOOL  , {.boolean =       value}})
#define    OBJ_VAL(value) ((Value){VAL_OBJ   , {.obj     = (Obj*)value}})

#define IS_NIL(value)    ((value.type == VAL_NIL   ))
#define IS_NUMBER(value) ((value.type == VAL_NUMBER))
#define IS_BOOL(value)   ((value.type == VAL_BOOL  ))
#define IS_OBJ(value)    ((value.type == VAL_OBJ   ))
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_NUMBER(value) ((value).as.number )
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_OBJ(value)    ((value).as.obj    )
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

#define OBJ_TYPE(value)  (AS_OBJ(value)->type)

typedef enum ValueType
{
	VAL_NIL,
	VAL_NUMBER,
	VAL_BOOL,
	VAL_OBJ,
} ValueType;

typedef enum ObjType
{
	OBJ_STRING,
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

typedef struct Value
{
	ValueType type;
	 
	union
	{
		bool boolean;
		double number;
		Obj* obj;
	} as;
} Value;

bool isObjType(Value v, ObjType t)
{
	return IS_OBJ(v) && OBJ_TYPE(v) == t;
}

void printObject(Value v)
{
	switch(OBJ_TYPE(v))
	{
		case OBJ_STRING: printf("%s", AS_CSTRING(v)); break;
	}
}

void printValue(Value v)
{
	switch(v.type)
	{
		case VAL_NUMBER: printf("%g", AS_NUMBER(v));            break;
		case VAL_BOOL  : printf(AS_BOOL(v) ? "true" : "false"); break;
		case VAL_NIL   : printf("nil");                         break;
		case VAL_OBJ   : printObject(v);                        break;
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
			free(object);
	}
}

#endif