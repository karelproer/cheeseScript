#ifndef VM_H
#define VM_H
#include "chunk.h"
#include "disassembler.h"

#define DEBUG_TRACE_EXECUTION
#define DEBUG_TRACE_STACK
// dynamic stack size?
#define STACK_MAX 1024

typedef enum result
{
	RESULT_OK,
	RESULT_COMPILE_ERROR,
	RESULT_RUNTIME_ERROR,
} result;

typedef struct VM
{
	Chunk* chunk;
	uint8_t* ip;

	value stack[STACK_MAX];
	value* stackTop;
} VM;

void initVM(VM* vm)
{
	vm->stackTop = vm->stack;
}

void freeVM(VM* vm)
{
	
}

void push(VM* vm, value v)
{
	*vm->stackTop = v;
	vm->stackTop++;
}

value pop(VM* vm)
{
	vm->stackTop--;
	return *vm->stackTop; 
}

void printStack(VM* vm)
{
	printf("[ ");
	for(value* v = vm->stack; v < vm->stackTop; v++)
	{
		printf("[ ");
		printValue(*v);
		printf(" ]");
	}
	printf(" ]");
}

#define BINARY_OP(op) { \
		value b = pop(vm); \
		value a = pop(vm); \
		push(vm, a op b); \
	}


result run(VM* vm)
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
		case OP_ADD:
			BINARY_OP(+)
		case OP_SUBTRACT:
			BINARY_OP(-)
		case OP_MULTIPLY:
			BINARY_OP(*)
		case OP_DIVIDE:
			BINARY_OP(*)
		}
	}	
}

result interpret(VM* vm, Chunk* chunk)
{
	vm->chunk = chunk;
	vm->ip = chunk->data;
	return run(vm);
}

#endif