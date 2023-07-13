#ifndef CHUNK_H
#define CHUNK_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "value.h"
#include "opCode.h"

typedef struct ValueArray
{
	Value* data;
	size_t size;
	size_t capacity;
} ValueArray;

typedef struct LineInfo
{
	uint32_t* data; // [line, amount, line, amount, ...]
	size_t size;
	size_t capacity;
} LineInfo;

typedef struct Chunk
{
	uint8_t* data;
	size_t size;
	size_t capacity;
	ValueArray values;
	LineInfo lines;
} Chunk;

void initValueArray(ValueArray* valueArray)
{
	valueArray->data = NULL;
	valueArray->size = 0;
	valueArray->capacity = 0;
}

void initLineInfo(LineInfo* lineInfo)
{
	lineInfo->data = NULL;
	lineInfo->size = 0;
	lineInfo->capacity = 0;
}

void initChunk(Chunk* chunk)
{
	chunk->data = NULL;
	chunk->size = 0;
	chunk->capacity = 0;
	initValueArray(&chunk->values);
	initLineInfo(&chunk->lines);
}

void addToLineInfo(LineInfo* lineInfo, uint32_t line)
{
	if(lineInfo->data && (lineInfo->data[lineInfo->size - 2] == line))
	{
		lineInfo->data[lineInfo->size - 1]++;
		return;
	}

	if(lineInfo->capacity == lineInfo->size)
	{
		if(lineInfo->capacity < 8)
			lineInfo->capacity = 8;
		else
			lineInfo->capacity = lineInfo->capacity * 2;

		if(!(lineInfo->data = (uint32_t*)realloc(lineInfo->data, lineInfo->capacity * sizeof(uint32_t))))
		{
			fprintf(stderr, "memory allocation failed!\n");
			exit(1);
		}
	}
	lineInfo->data[lineInfo->size++] = line;
	lineInfo->data[lineInfo->size++] = 1;
}

void addToChunk(Chunk* chunk, uint8_t byte, uint32_t line)
{
	if(chunk->capacity <= chunk->size)
	{
		if(chunk->capacity < 8)
			chunk->capacity = 8;
		else
			chunk->capacity = chunk->capacity * 2;

		if(!(chunk->data = (uint8_t*)realloc(chunk->data, chunk->capacity)))
		{
			fprintf(stderr, "memory allocation failed!\n");
			exit(74);
		}

	}
	chunk->data[chunk->size++] = byte;

	addToLineInfo(&chunk->lines, line);
}

void freeValueArray(ValueArray* ValueArray)
{
	free(ValueArray->data);
}

void freeLineInfo(LineInfo* lineInfo)
{
	free(lineInfo->data);
}

void freeChunk(Chunk* chunk)
{
	free(chunk->data);
	freeValueArray(&chunk->values);
	freeLineInfo(&chunk->lines);
}

uint32_t addToValueArray(ValueArray* valueArray, Value v)
{

	if(valueArray->capacity == valueArray->size)
	{
		if(valueArray->capacity < 8)
			valueArray->capacity = 8;
		else
			valueArray->capacity = valueArray->capacity * 2;

		if(!(valueArray->data = (Value*)realloc(valueArray->data, valueArray->capacity * sizeof(Value))))
		{
			fprintf(stderr, "memory allocation failed!\n");
			exit(74);
		}
	}

	valueArray->data[valueArray->size++] = v;
	return valueArray->size - 1;
}

uint32_t getLine(LineInfo* lineInfo, uint32_t index)
{
	int32_t i = -2;
	int32_t distance = index;
	while (distance >= 0)
	{
		i += 2;
		distance -= lineInfo->data[i + 1];
	}

	return lineInfo->data[i];
}

void addConstantInstrution(Chunk* chunk, uint32_t constant, uint32_t line)
{
	if(constant <= 0xff)
	{
		addToChunk(chunk, OP_CONSTANT, line);
		addToChunk(chunk, (uint8_t)constant, line);
	}
	else if(constant <= 0xffff)
	{
		addToChunk(chunk, OP_LONG_CONSTANT, line);
		addToChunk(chunk, (uint8_t)constant, line);
		addToChunk(chunk, (uint8_t)constant >> 8, line);
	}
}

#endif