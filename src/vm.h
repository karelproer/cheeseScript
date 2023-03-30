#ifndef VM_H
#define VM_H
#include "chunk.h"
#include "disassembler.h"
#include "common.h"

#define DEBUG_TRACE_EXECUTION
#define DEBUG_TRACE_STACK
// dynamic stack size?
#define STACK_MAX 1024

typedef struct VM
{
	Chunk* chunk;
	uint8_t* ip;

	Value stack[STACK_MAX];
	Value* stackTop;
} VM;

void initVM(VM* vm)
{
	vm->stackTop = vm->stack;
}

void loadChunk(VM* vm, Chunk* chunk)
{
	vm->chunk = chunk;
	vm->ip = chunk->data;
}

void freeVM(VM* vm)
{
	
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

#define BINARY_OP(op) { \
		Value b = pop(vm); \
		Value a = pop(vm); \
		push(vm, a op b); \
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
			printValue(pop(vm));
			printf("\n");
			return RESULT_OK;
		case OP_CONSTANT:
			push(vm, vm->chunk->values.data[*vm->ip++]);
			break;
		case OP_LONG_CONSTANT:
			push(vm, vm->chunk->values.data[*vm->ip++ * 0xff + *vm->ip++]);
			break;
		case OP_NEGATE:
			push(vm, -pop(vm));
			break;
		case OP_ADD:
			BINARY_OP(+)
			break;
		case OP_SUBTRACT:
			BINARY_OP(-)
			break;
		case OP_MULTIPLY:
			BINARY_OP(*)
			break;
		case OP_DIVIDE:
			BINARY_OP(/)
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