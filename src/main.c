#include <stdio.h>
#include "vm.h"

int main(int argc, char const *argv[])
{
	Chunk c;
	initChunk(&c);
	addToChunk(&c, OP_CONSTANT, 0);
	addToChunk(&c, 0, 0);
	addToChunk(&c, OP_CONSTANT, 0);
	addToChunk(&c, 0, 0);
	addToChunk(&c, OP_SUBTRACT, 1);
	addToChunk(&c, OP_LONG_CONSTANT, 1);
	addToChunk(&c, 0, 0);
	addToChunk(&c, 2, 0);
	addToChunk(&c, OP_MULTIPLY, 2);
	addToChunk(&c, OP_NEGATE, 2);
	addToChunk(&c, OP_RETURN, 3);

	addToValueArray(&c.values, 78.9);
	addToValueArray(&c.values, 8.9);
	addToValueArray(&c.values, 1234.5678);

	VM vm;
	initVM(&vm);
	interpret(&vm, &c);
	freeVM(&vm);

	freeChunk(&c);
	return 0;
}
