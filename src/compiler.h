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
        printf("\x1B[31m[at %d:%d] Error at end: %s.\x1B[0m\n", token.line, token.collumn, message);
    else if (token.type == TOKEN_ERROR)
        printf("\x1B[31m[at %d:%d] Error: %.*s.\x1B[0m\n", token.line, token.collumn, token.length, message);
    else
        printf("\x1B[31m[at %d:%d] Error at '%.*s': %s.\x1B[0m\n", token.line, token.collumn, token.length, token.start, message);
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

uint32_t makeConstant(Compiler* comp, Value v)
{
    int constant = addToValueArray(&comp->chunk->values, v);
    if(constant >= 0xffff)
    {
        errorAtCurrent(comp, "Too many constant in one chunk.");
        return 0;
    }
    return constant;
}

void emitConstant(Compiler* comp, Value value)
{
    addConstantInstrution(comp->chunk, makeConstant(comp, value), comp->previous.line);
}

Token nextToken(Compiler* comp)
{
    comp->previous = comp->current;

    while (true)
    {
        comp->current = scanToken(comp->scanner);

        if(comp->current.type != TOKEN_ERROR)
            return comp->previous;

        errorAtCurrent(comp, comp->current.start);
    }
}

void consume(Compiler* comp, TokenType type, const char* message)
{
    if(nextToken(comp).type != type)
        errorAtCurrent(comp, message);
}

typedef enum Precedence
{
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,
} Precedence;

typedef void(*parseFn)(Compiler*);

typedef struct ParseRule
{
    parseFn prefix;
    parseFn infix;
    Precedence precedence;
} ParseRule;

void group (Compiler*);
void binary(Compiler*);
void unary (Compiler*);
void number(Compiler*);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]         = {group , NULL  , PREC_NONE  },
    [TOKEN_RIGHT_PAREN]        = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_LEFT_BRACE]         = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_RIGHT_BRACE]        = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_LEFT_SQUARE_BRACE]  = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_RIGHT_SQUARE_BRACE] = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_COMMA]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_DOT]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_SEMICOLON]          = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_COLON]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_PLUS]               = {NULL  , binary, PREC_TERM  },
    [TOKEN_MINUS]              = {unary , binary, PREC_TERM  },
    [TOKEN_STAR]               = {NULL  , binary, PREC_FACTOR},
    [TOKEN_SLASH]              = {NULL  , binary, PREC_FACTOR},
    [TOKEN_EQUAL]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_EQUAL_EQUAL]        = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_BANG]               = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_BANG_EQUAL]         = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_MORE]               = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_MORE_EQUAL]         = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_LESS]               = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_LESS_EQUAL]         = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_IDENTIFIER]         = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_NUMBER]             = {number, NULL  , PREC_NONE  },
    [TOKEN_STRING]             = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_AND]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_OR]                 = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_NOT]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_VAR]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_CONST]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_FUN]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_CLASS]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_NIL]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_THIS]               = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_SUPER]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_TRUE]               = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_FALSE]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_IF]                 = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_ELSE]               = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_FOR]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_WHILE]              = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_EOF]                = {NULL  , NULL  , PREC_NONE  },
    [TOKEN_ERROR]              = {NULL  , NULL  , PREC_NONE  },
};

ParseRule getRule(TokenType type)
{
    return rules[type];
}

void parsePrecedence(Compiler* comp, Precedence p)
{
    nextToken(comp);
    parseFn prefix = getRule(comp->previous.type).prefix;
    if(prefix == NULL)
    {
        errorAtCurrent(comp, "Expected expression");
        return;
    }

    prefix(comp);

    while(p <= getRule(comp->current.type).precedence)
    {
        nextToken(comp);
        getRule(comp->previous.type).infix(comp);
    }
}

void expression(Compiler* comp)
{
    parsePrecedence(comp, PREC_ASSIGNMENT);
}

void number(Compiler* comp)
{
    double value = strtod(comp->previous.start, NULL);
    emitConstant(comp, value);
}

void group(Compiler* comp)
{
    expression(comp);
    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

void unary(Compiler* comp)
{
    TokenType operator = comp->previous.type;
    parsePrecedence(comp, PREC_UNARY);
    switch(operator)
    {
        case TOKEN_MINUS: emitByte(comp, OP_NEGATE); break;
    }
}


void binary(Compiler* comp)
{
    TokenType operator = comp->previous.type;
    parsePrecedence(comp, (Precedence)(getRule(operator).precedence + 1));
    switch(operator)
    {
        case TOKEN_PLUS : emitByte(comp, OP_ADD     ); break;
        case TOKEN_MINUS: emitByte(comp, OP_SUBTRACT); break;
        case TOKEN_STAR : emitByte(comp, OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(comp, OP_DIVIDE  ); break;
    }
}

Result compile(Compiler* comp, const char* source, Chunk* chunk)
{
    Scanner scanner;
    initScanner(&scanner, source);
    comp->scanner = &scanner;
    comp->chunk = chunk;

    nextToken(comp);
    expression(comp);

    consume(comp, TOKEN_EOF, "Expected end of file");
    freeScanner(&scanner);
    emitByte(comp, OP_RETURN);

    if(comp->error)
        return RESULT_COMPILE_ERROR;

    return RESULT_OK;
}

#endif