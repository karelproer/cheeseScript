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

#define IS_NIL(value)    ((value).type == VAL_NIL   )
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_BOOL(value)   ((value).type == VAL_BOOL  )
#define IS_OBJ(value)    ((value).type == VAL_OBJ   )

#define AS_NUMBER(value) ((value).as.number )
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_OBJ(value)    ((value).as.obj    )

typedef enum ValueType
{
	VAL_NIL,
	VAL_NUMBER,
	VAL_BOOL,
	VAL_OBJ,
} ValueType;

struct Obj;

typedef struct Value
{
	ValueType type;
	 
	union
	{
		bool boolean;
		double number;
		struct Obj* obj;
	} as;
} Value;

void printObject(Value v);

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

#endif