#ifndef COMPILER_H
#define COMPILER_H
#include <stdlib.h>
#include "scanner.h"
// todo: print line of error

typedef struct Compiler
{
    Token current;
    Token previous;
    Scanner* scanner;
    Chunk* chunk;

    bool error;
    bool panic;
} Compiler;

void initCompiler(Compiler* comp)
{
    comp->error = false;
    comp->panic = false;
}

void freeCompiler(Compiler* comp)
{

}

void error(Compiler* comp, Token token, const char* message)
{
    if(comp->panic)
        return;

    if(token.type == TOKEN_EOF)
        printf("\x1B[31m[at %d:%d] Error at end: %s.\x1B[0m", token.line, token.collumn, message);
    else if (token.type == TOKEN_ERROR)
        printf("\x1B[31m[at %d:%d] Error: %.*s.\x1B[0m", token.line, token.collumn, token.length, message);
    else
        printf("\x1B[31m[at %d:%d] Error at '%.*s': %s.\x1B[0m", token.line, token.collumn, token.length, token.start, message);
    comp->error = true;
}

void errorAtCurrent(Compiler* comp, const char* message)
{
    error(comp, comp->previous, message);
}

void emitByte(Compiler* comp, uint8_t byte)
{
    addToChunk(comp->chunk, byte, comp->previous.line);
}

void emitBytes(Compiler* comp, uint8_t byte1, uint8_t byte2)
{
    emitByte(comp, byte1);
    emitByte(comp, byte2);
}

Token nextToken(Compiler* comp)
{
    comp->current = comp->previous;

    while (true)
    {
        comp->current = scanToken(comp->scanner);

        if(comp->current.type != TOKEN_ERROR)
            return comp->current;

        errorAtCurrent(comp, comp->current.start);
    }   
}

void consume(Compiler* comp, TokenType type, const char* message)
{
    if(nextToken(comp).type != type)
        errorAtCurrent(comp, message);
}

void expression(Compiler* comp)
{

}

Result compile(Compiler* comp, const char* source, Chunk* chunk)
{
    Scanner scanner;
    initScanner(&scanner, source);
    comp->scanner = &scanner;
    comp->chunk = chunk;

    expression(comp);

    consume(comp, TOKEN_EOF, "Expected end of file.");
    freeScanner(&scanner);
    emitByte(comp, OP_RETURN);

    if(comp->error)
        return RESULT_COMPILE_ERROR;

    return RESULT_OK;
}

#endif