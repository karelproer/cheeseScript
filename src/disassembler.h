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

int disassembleByteInstruction(const char* name, int offset, Chunk* chunk)
{
	uint8_t slot = chunk->data[offset+1];
	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

int disassembleJumpInstruction(const char* name, int offset, int sign, Chunk* chunk)
{
	uint16_t jump = chunk->data[offset+1] * 0xff + chunk->data[offset+2];
	printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
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
	case OP_TRUE:
		return disassembleSimpleInstruction("TRUE", offset);
	case OP_FALSE:
		return disassembleSimpleInstruction("FALSE", offset);
	case OP_NIL:
		return disassembleSimpleInstruction("NIL", offset);
	case OP_NOT:
		return disassembleSimpleInstruction("NOT", offset);
	case OP_NOT_EQUAL:
		return disassembleSimpleInstruction("NOT_EQUAL", offset);
	case OP_EQUAL:
		return disassembleSimpleInstruction("EQUAL", offset);	
	case OP_LESS:
		return disassembleSimpleInstruction("LESS", offset);
	case OP_LESS_EQUAL:
		return disassembleSimpleInstruction("LESS_EQUAL", offset);
	case OP_MORE:
		return disassembleSimpleInstruction("MORE", offset);
	case OP_MORE_EQUAL:
		return disassembleSimpleInstruction("MORE_EQUAL", offset);
	case OP_POP:
		return disassembleSimpleInstruction("POP", offset);
	case OP_DEFINE_GLOBAL:
		return disassembleConstantInstruction("DEFINE_GLOBAL", offset, chunk, false);
	case OP_DEFINE_LONG_GLOBAL:
		return disassembleConstantInstruction("LONG_DEFINE_GLOBAL", offset, chunk, true);
	case OP_GET_GLOBAL:
		return disassembleConstantInstruction("GET_GLOBAL", offset, chunk, false);
	case OP_GET_LONG_GLOBAL:
		return disassembleConstantInstruction("LONG_GET_GLOBAL", offset, chunk, true);	
	case OP_SET_GLOBAL:
		return disassembleConstantInstruction("SET_GLOBAL", offset, chunk, false);
	case OP_SET_LONG_GLOBAL:
		return disassembleConstantInstruction("LONG_SET_GLOBAL", offset, chunk, true);
	case OP_GET_LOCAL:
		return disassembleByteInstruction("GET_LOCAL", offset, chunk);
	case OP_SET_LOCAL:
		return disassembleByteInstruction("SET_LOCAL", offset, chunk);
	case OP_JUMP:
		return disassembleJumpInstruction("JUMP", offset, 1, chunk);
	case OP_LOOP:
		return disassembleJumpInstruction("LOOP", offset, -1, chunk);
	case OP_JUMP_IF_FALSE:
		return disassembleJumpInstruction("JUMP_IF_FALSE", offset, 1, chunk);
	case OP_JUMP_IF_TRUE:
		return disassembleJumpInstruction("JUMP_IF_TRUE", offset, 1, chunk);
	case OP_PRINT:
		return disassembleSimpleInstruction("PRINT", offset);
	default:
		printf("unknown opCode: %i\n", chunk->data[offset]);
		return 1;
	}
}

void disassembleChunk(Chunk* chunk, const char* name)
{
	printf("==== disassembly: %s | instructions: %i ====\n", name, (int)chunk->size);

	for(int i = 0; i < chunk->size; i += disassembleInstruction(chunk, i));
	printf("============ end of dissasembly ============\n\n");
}

#endif