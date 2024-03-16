#ifndef VM_H
#define VM_H
#include <stdarg.h>
#include <time.h>
#include "chunk.h"
#include "disassembler.h"
#include "common.h"
#include "table.h"
#include "object.h"

//#define DEBUG_TRACE_EXECUTION
//#define DEBUG_TRACE_STACK
// dynamic stack size??
#define STACK_MAX 65536
#define FRAME_MAX 256
// todo: chunk naming

Value clockNative(int argCount, Value* args)
{
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

typedef struct CallFrame
{
	ObjFunction* function;
	uint8_t* ip;
	Value* slots;
	ObjUpvalue** upvalues;
} CallFrame;

typedef struct VM
{
	CallFrame frames[FRAME_MAX];
	int frameCount;

	Value stack[STACK_MAX];
	Value* stackTop;

	ObjUpvalue* openUpvalues;

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
	interned->type = OBJ_STRING;
	interned->length = length;
	registerObject(vm, (Obj*)interned);
	memcpy(interned->chars, chars, length);
	interned->chars[length] = '\0';
	interned->hash = hash;
	tableSet(&vm->strings, interned, NIL_VAL);
	return interned;
}

ObjNative* newNative(struct VM* vm, NativeFun fun, int arity, const char* name)
{
	ObjNative* native = (ObjNative*)allocateObject(vm, sizeof(ObjNative), OBJ_NATIVE);
	native->arity = arity;
	native->fun = fun;
	native->name = copyString(vm, name, strlen(name));
	return native;
}

void defineNative(VM* vm, const char* name, NativeFun function, int arity)
{
	ObjNative* native = newNative(vm, function, arity, name);
	tableSet(&vm->globals, native->name, OBJ_VAL(native));
}

void initVM(VM* vm)
{
	vm->stackTop = vm->stack;
	vm->objects = NULL;
	vm->frameCount = 0;
	vm->openUpvalues = NULL;
	initTable(&vm->strings);
	initTable(&vm->globals);

	defineNative(vm, "clock", clockNative, 0);
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

	for(int i = vm->frameCount - 1; i >= 0; i--)
	{
		CallFrame* frame = &vm->frames[i];
		ObjFunction* function = frame->function;
		size_t instruction = frame->ip - function->chunk.data - 1;
		fprintf(stderr, "[line %d] in ", getLine(&function->chunk.lines, function->chunk.data[instruction]));
		if(function->name == NULL)
			fprintf(stderr, "script\n");
		else
			fprintf(stderr, "%s()\n", function->name->chars);
	}
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

bool callFunction(VM* vm, ObjFunction* function, int argCount, ObjUpvalue** upvalues)
{
	if(argCount != function->arity)
	{
		runtimeError(vm, "Expected %d arguments, but got %d", function->arity, argCount);
		return false;
	}
	CallFrame* frame = &vm->frames[vm->frameCount++];
	frame->function = function;
	frame->ip = function->chunk.data;
	frame->slots = vm->stackTop - argCount - 1;
	frame->upvalues = upvalues;
	return true;
}

bool callNative(VM* vm, ObjNative* native, int argCount)
{
	if(argCount != native->arity && native->arity != -1)
	{
		runtimeError(vm, "Expected %d arguments, but got %d", native->arity, argCount);
		return false;
	}

	Value result = native->fun(argCount, vm->stackTop - argCount);
	vm->stackTop -= argCount + 1;
	push(vm, result);
	return true;
}

bool callValue(VM* vm, Value callee, int argCount)
{
	if(IS_OBJ(callee))
	{
		switch(OBJ_TYPE(callee))
		{
			case OBJ_FUNCTION:
				return callFunction(vm, AS_FUNCTION(callee), argCount, NULL);
			case OBJ_NATIVE:
				return callNative(vm, AS_NATIVE(callee), argCount);
			case OBJ_CLOSURE:
				return callFunction(vm, AS_CLOSURE(callee)->function, argCount, AS_CLOSURE(callee)->upvalues);
			default:
				break;
		}
	}

	runtimeError(vm, "Object is not callable");
	return false;
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

ObjUpvalue* captureUpvalue(VM* vm, Value* local)
{
	ObjUpvalue** prevUpvalue = &vm->openUpvalues;
	ObjUpvalue* upvalue = vm->openUpvalues;
	while(upvalue != NULL && upvalue->value > local)
	{
		prevUpvalue = &upvalue->nextUpvalue;
		upvalue = upvalue->nextUpvalue;
	}
	if(upvalue != NULL && upvalue->value == local)
		return upvalue;


	ObjUpvalue* new = newUpvalue(local, vm);
	new->nextUpvalue = upvalue;
	
	*prevUpvalue = new;
	return new;
}

void closeUpvalues(VM* vm, Value* last)
{
	while(vm->openUpvalues != NULL && vm->openUpvalues->value >= last)
	{
		ObjUpvalue* upvalue = vm->openUpvalues;
		upvalue->closed = *upvalue->value;
		upvalue->value = &upvalue->closed;
		vm->openUpvalues = upvalue->nextUpvalue;
	}
}

Result run(VM* vm)
{
	CallFrame* frame = &vm->frames[vm->frameCount - 1];
	while (true)
	{
	#ifdef DEBUG_TRACE_STACK
		printf("stack: ");
		printStack(vm);
		printf("\n");
	#endif
	#ifdef DEBUG_TRACE_EXECUTION
		disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.data));
	#endif
		switch (*frame->ip++)
		{
			case OP_RETURN:
			{
				Value value = pop(vm);
				closeUpvalues(vm, frame->slots);
				vm->frameCount--;
				if(vm->frameCount == 0)
				{
					pop(vm);
					return RESULT_OK;
				}

				vm->stackTop = frame->slots;
				push(vm, value);
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}
			case OP_CONSTANT:
				push(vm, frame->function->chunk.values.data[*frame->ip++]);
				break;
			case OP_LONG_CONSTANT:
				push(vm, frame->function->chunk.values.data[*frame->ip++ * 0xff + *frame->ip++]);
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
				ObjString* name = AS_STRING(frame->function->chunk.values.data[*frame->ip++]);
				tableSet(&vm->globals, name, pop(vm));
				break;
			}
			case OP_DEFINE_LONG_GLOBAL:
			{
				ObjString* name = AS_STRING(frame->function->chunk.values.data[*frame->ip++ * 0xff + *frame->ip++]);
				tableSet(&vm->globals, name, pop(vm));
				break;
			}
			case OP_GET_GLOBAL:
			{
				ObjString* name = AS_STRING(frame->function->chunk.values.data[*frame->ip++]);
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
				ObjString* name = AS_STRING(frame->function->chunk.values.data[*frame->ip++ * 0xff + *frame->ip++]);
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
				ObjString* name = AS_STRING(frame->function->chunk.values.data[*frame->ip++]);
				if(tableSet(&vm->globals, name, peekStack(vm, 0)))
				{
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return RESULT_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_LONG_GLOBAL:
			{
				ObjString* name = AS_STRING(frame->function->chunk.values.data[*frame->ip++ * 0xff + *frame->ip++]);
				if(tableSet(&vm->globals, name, peekStack(vm, 0)))
				{
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return RESULT_RUNTIME_ERROR;
				}
				break;
			}
			case OP_GET_LOCAL:
				push(vm, frame->slots[*frame->ip++]);
				break;
			case OP_SET_LOCAL:
				frame->slots[*frame->ip++] = peekStack(vm, 0);
				break;
			case OP_JUMP:
				frame->ip += (*frame->ip++ * 0xff + *frame->ip++);
				break;
			case OP_LOOP:
				frame->ip -= (*frame->ip++ * 0xff + *frame->ip++);
				break;
			case OP_JUMP_IF_FALSE:
				frame->ip += (int)isFalse(peekStack(vm, 0)) * (*frame->ip++ * 0xff + *frame->ip++);
				break;
			case OP_JUMP_IF_TRUE:
				frame->ip += (int)!isFalse(peekStack(vm, 0)) * (*frame->ip++ * 0xff + *frame->ip++);
				break;
			case OP_CALL:
			{
				int argCount = *frame->ip++;
				if(!callValue(vm, peekStack(vm, argCount), argCount))
					return RESULT_RUNTIME_ERROR;
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}
			case OP_PRINT:
				printValue(pop(vm));
				printf("\n");
				break;
			case OP_CLOSURE:
			{
				ObjFunction* function = AS_FUNCTION(pop(vm));
				uint8_t upvalueCount = *frame->ip++;
				ObjClosure* closure = newClosure(function, upvalueCount, vm);
				for(uint8_t i = 0; i < upvalueCount; i++)
				{
					bool isLocal = *frame->ip++;
					uint8_t index = *frame->ip++;
					if(isLocal)
					{
						closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
					}
					else
					{
						closure->upvalues[i] = frame->upvalues[index];
					}
				}
				push(vm, OBJ_VAL(closure));
				break;
			}
			case OP_GET_UPVALUE:
			{
				uint8_t index = *frame->ip++;
				push(vm, *frame->upvalues[index]->value);
				break;
			}
			case OP_SET_UPVALUE:
			{
				uint8_t index = *frame->ip++;
				*frame->upvalues[index]->value = peekStack(vm, 0);
				break;
			}
		}
	}	
}

Result execute(VM* vm, Chunk* chunk)
{
	return run(vm);
}

#endif