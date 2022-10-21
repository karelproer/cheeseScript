#include <stdio.h>
#include "disassembler.h"

int main(int argc, char const *argv[])
{
	Chunk c;
	initChunk(&c);
	addToChunk(&c, OP_CONSTANT, 0);
	addToChunk(&c, 0, 0);
	addToChunk(&c, OP_LONG_CONSTANT, 0);
	addToChunk(&c, 0, 0);
	addToChunk(&c, 0, 0);
	addToChunk(&c, 1, 0);
	addToChunk(&c, OP_RETURN, 2);

	addToValueArray(&c.values, 78.9);
	addToValueArray(&c.values, 1234.5678);

	printf("ja");

	disassembleChunk(&c, "main");

	freeChunk(&c);
	return 0;
}
