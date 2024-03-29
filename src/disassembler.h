#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H
#include <stdbool.h>
#include "chunk.h"

int disassembleSimpleInstruction(const char* name, int offset)
{
	printf("%s\n", name);
	return 1;
}

int disassembleConstantInstruction(const char* name, int offset, Chunk* chunk, bool big)
{
	printf("%s %i : ", name, chunk->data[offset + 1]);

	if(!big)
	{
		printValue(chunk->values.data[chunk->data[offset + 1]]);
		printf("\n");
		return 2;
	}

	printValue(chunk->values.data[chunk->data[offset + 1] * 0xff + chunk->data[offset + 2]]);
	printf("\n");
	return 3;
}

int disassembleInstruction(Chunk* chunk, int offset)
{
	printf("%04d ", offset);

	if(offset > 0 && getLine(&chunk->lines, offset) == getLine(&chunk->lines, offset - 1))
		printf("   | ");
	else
		printf("%4d ", getLine(&chunk->lines, offset));

	switch (chunk->data[offset])
	{
	case OP_RETURN:
		return disassembleSimpleInstruction("RETURN", offset);
	case OP_CONSTANT:
		return disassembleConstantInstruction("CONSTANT", offset, chunk, false);
	case OP_LONG_CONSTANT:
		return disassembleConstantInstruction("LONG_CONSTANT", offset, chunk, true);
	case OP_NEGATE:
		return disassembleSimpleInstruction("NEGATE", offset);
	case OP_ADD:
		return disassembleSimpleInstruction("ADD", offset);
	case OP_SUBTRACT:
		return disassembleSimpleInstruction("SUBTRACT", offset);
	case OP_MULTIPLY:
		return disassembleSimpleInstruction("MULTIPLY", offset);
	case OP_DIVIDE:
		return disassembleSimpleInstruction("DIVIDE", offset);
	default:
		printf("unknown opCode: %i\n", chunk->data[offset]);
		return 1;
	}
}

void disassembleChunk(Chunk* chunk, const char* name)
{
	printf("==== disassembly: %s | instructions: %i ====\n", name, chunk->size);

	for(int i = 0; i < chunk->size; i += disassembleInstruction(chunk, i));
	printf("============ end of dissasembly ============\n\n");
}

#endif