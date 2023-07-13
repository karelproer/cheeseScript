#ifndef VM_H
#define VM_H
#include <stdarg.h>
#include "chunk.h"
#include "disassembler.h"
#include "common.h"
#include "table.h"

//#define DEBUG_TRACE_EXECUTION
//#define DEBUG_TRACE_STACK
// dynamic stack size?
#define STACK_MAX 1024

// todo: chunk naming

typedef struct VM
{
	Chunk* chunk;
	uint8_t* ip;

	Value stack[STACK_MAX];
	Value* stackTop;

	Obj* objects;
	Table strings;
	Table globals;
} VM;

void registerObject(VM* vm, Obj* object)
{
	object->next = vm->objects;
	vm->objects = object;
}

ObjString* copyString(VM* vm, const char* chars, int length)
{
	uint32_t hash = hashString(chars, length);
	ObjString* interned = (ObjString*)TableFindString(&vm->strings, chars, length, hash);
	if(interned != NULL)
		return interned;

	interned = (ObjString*)malloc(sizeof(ObjString) + length + 1);
	interned->length = length;
	registerObject(vm, (Obj*)interned);
	memcpy(interned->chars, chars, length);
	interned->chars[length] = '\0';
	interned->hash = hash;
	tableSet(&vm->strings, interned, NIL_VAL);
	return interned;
}

void initVM(VM* vm)
{
	vm->stackTop = vm->stack;
	vm->objects = NULL;
	initTable(&vm->strings);
	initTable(&vm->globals);
}

void loadChunk(VM* vm, Chunk* chunk)
{
	vm->chunk = chunk;
	vm->ip = chunk->data;
}

void freeObjects(VM* vm)
{
	Obj* object = vm->objects;
	while(object != NULL)
	{
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}
}

void freeVM(VM* vm)
{
	freeObjects(vm);
	freetable(&vm->strings);
	freetable(&vm->globals);
}

void push(VM* vm, Value v)
{
	*vm->stackTop = v;
	vm->stackTop++;
}

Value pop(VM* vm)
{
	vm->stackTop--;
	return *vm->stackTop; 
}

Value top(VM* vm)
{
	return vm->stackTop[-1];
}

Value peekStack(VM* vm, int distance)
{
	return vm->stackTop[-1 - distance];
}

void printStack(VM* vm)
{
	printf("[ ");
	for(Value* v = vm->stack; v < vm->stackTop; v++)
	{
		printf("[ ");
		printValue(*v);
		printf(" ]");
	}
	printf(" ]");
}

void runtimeError(VM* vm, const char* format, ...)
{
	va_list args;
	fprintf(stderr, "Runtime error: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	// todo: call stack trace

	fprintf(stderr, "at line %d.\n", vm->chunk->lines.data[vm->ip - vm->chunk->data - 1]);
}

bool isFalse(Value v)
{
	return IS_NIL(v) || (IS_BOOL(v) && AS_BOOL(v) == false) || (IS_NUMBER(v) && AS_NUMBER(v) == 0.0);
}

bool valuesEqual(Value a, Value b)
{
	if(a.type != b.type)
		return false;
	switch(a.type)
	{
		case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
		case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NIL: return true;
		case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
		default: return true;
	}
}

void concatenate(VM* vm)
{
	ObjString* b = AS_STRING(pop(vm));
	ObjString* a = AS_STRING(pop(vm));
	
	int length = a->length + b->length;
	char* chars = (char*)malloc(length);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	ObjString* string = copyString(vm, chars, length);	
	push(vm, OBJ_VAL(string));
}

#define BINARY_OP(type, op) { \
		if(!IS_NUMBER(top(vm)) || !IS_NUMBER(peekStack(vm, 1))) { \
			runtimeError(vm, "Operands to '%s' must be numbers.", #op); \
			return RESULT_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(pop(vm)); \
		double a = AS_NUMBER(pop(vm)); \
		push(vm, type(a op b)); \
	}

Result run(VM* vm)
{
	while (true)
	{
	#ifdef DEBUG_TRACE_STACK
		printf("stack: ");
		printStack(vm);
		printf("\n");
	#endif
	#ifdef DEBUG_TRACE_EXECUTION
		disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->data));
	#endif
		switch (*vm->ip++)
		{
			case OP_RETURN:
				//printValue(pop(vm));
				//printf("\n");
				return RESULT_OK;
			case OP_CONSTANT:
				push(vm, vm->chunk->values.data[*vm->ip++]);
				break;
			case OP_LONG_CONSTANT:
				push(vm, vm->chunk->values.data[*vm->ip++ * 0xff + *vm->ip++]);
				break;
			case OP_NEGATE:
				if(!IS_NUMBER(top(vm)))
				{
					runtimeError(vm, "Operand to '-' must be a number.");
					return RESULT_RUNTIME_ERROR;
				}
				push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
				break;
			case OP_ADD:
			{
				if(IS_STRING(top(vm)) && IS_STRING(peekStack(vm, 1)))
					concatenate(vm);
				else if(IS_NUMBER(top(vm)) && IS_NUMBER(peekStack(vm, 1)))
				{
					double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
					push(vm, NUMBER_VAL(a + b));
				}
				else
				{
					runtimeError(vm, "Operands to '+' must be two numbers or two strings.");
					return RESULT_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT:
				BINARY_OP(NUMBER_VAL, -)
				break;
			case OP_MULTIPLY:
				BINARY_OP(NUMBER_VAL, *)
				break;
			case OP_DIVIDE:
				BINARY_OP(NUMBER_VAL, /)
				break;
			case OP_TRUE:
				push(vm, BOOL_VAL(true));
				break;
			case OP_FALSE:
				push(vm, BOOL_VAL(false));
				break;
			case OP_NIL:
				push(vm, NIL_VAL);
				break;
			case OP_NOT:
				push(vm, BOOL_VAL(isFalse(pop(vm))));
				break;
			case OP_EQUAL:
			{
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(valuesEqual(b, a)));
				break;
			}
			case OP_NOT_EQUAL:
			{
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(!valuesEqual(b, a)));
				break;
			}
			case OP_LESS:
				BINARY_OP(BOOL_VAL, <)
				break;
			case OP_LESS_EQUAL:
				BINARY_OP(BOOL_VAL, <=)
				break;
			case OP_MORE:
				BINARY_OP(BOOL_VAL, >)
				break;
			case OP_MORE_EQUAL:
				BINARY_OP(BOOL_VAL, >=)
				break;
			case OP_POP:
				pop(vm);
				break;
			case OP_DEFINE_GLOBAL:
			{
				ObjString* name = AS_STRING(vm->chunk->values.data[*vm->ip++]);
				tableSet(&vm->globals, name, pop(vm));
				break;
			}
			case OP_DEFINE_LONG_GLOBAL:
			{
				ObjString* name = AS_STRING(vm->chunk->values.data[*vm->ip++ * 0xff + *vm->ip++]);
				tableSet(&vm->globals, name, pop(vm));
				break;
			}
			case OP_GET_GLOBAL:
			{
				ObjString* name = AS_STRING(vm->chunk->values.data[*vm->ip++]);
				Value value;
				if(!tableGet(&vm->globals, name, &value))
				{
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return RESULT_RUNTIME_ERROR;
				}
				push(vm, value);
				break;
			}
			case OP_GET_LONG_GLOBAL:
			{
				ObjString* name = AS_STRING(vm->chunk->values.data[*vm->ip++ * 0xff + *vm->ip++]);
				Value value;
				if(!tableGet(&vm->globals, name, &value))
				{
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return RESULT_RUNTIME_ERROR;
				}
				push(vm, value);
				break;
			}
			case OP_SET_GLOBAL:
			{
				ObjString* name = AS_STRING(vm->chunk->values.data[*vm->ip++]);
				if(tableSet(&vm->globals, name, peekStack(vm, 0)))
				{
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return RESULT_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_LONG_GLOBAL:
			{
				ObjString* name = AS_STRING(vm->chunk->values.data[*vm->ip++ * 0xff + *vm->ip++]);
				if(tableSet(&vm->globals, name, peekStack(vm, 0)))
				{
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return RESULT_RUNTIME_ERROR;
				}
				break;
			}
			case OP_GET_LOCAL:
				push(vm, vm->stack[*vm->ip++]);
				break;
			case OP_SET_LOCAL:
				vm->stack[*vm->ip++] = peekStack(vm, 0);
				break;
			case OP_JUMP:
				vm->ip += (*vm->ip++ * 0xff + *vm->ip++);
				break;
			case OP_LOOP:
				vm->ip -= (*vm->ip++ * 0xff + *vm->ip++);
				break;
			case OP_JUMP_IF_FALSE:
				vm->ip += (int)isFalse(peekStack(vm, 0)) * (*vm->ip++ * 0xff + *vm->ip++);
				break;
			case OP_JUMP_IF_TRUE:
				vm->ip += (int)!isFalse(peekStack(vm, 0)) * (*vm->ip++ * 0xff + *vm->ip++);
				break;
			case OP_PRINT:
				printValue(pop(vm));
				printf("\n");
				break;
		}
	}	
}

Result execute(VM* vm, Chunk* chunk)
{
	vm->chunk = chunk;
	vm->ip = chunk->data;
	return run(vm);
}

#endif