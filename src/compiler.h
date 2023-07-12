#ifndef COMPILER_H
#define COMPILER_H
#include <stdlib.h>
#include "scanner.h"
// todo: print line of error

struct vm;

typedef struct Compiler
{
    Token current;
    Token previous;
    Scanner* scanner;
    Chunk* chunk;

    bool error;
    bool panic;

    struct VM* vm;
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

void synchronize(Compiler* comp)
{
    comp->panic = false;
    
    while(comp->current.type != TOKEN_EOF)
    {
        if(comp->previous.type == TOKEN_SEMICOLON)
            return;
        switch(comp->current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_CONST:
            case TOKEN_PRINT:
                return;
            default:;
        }
        nextToken(comp);
    }
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
    for(int i = 0; i < comp->chunk->values.size; i++)
        if(valuesEqual(comp->chunk->values.data[i], v))
            return i;

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

void consume(Compiler* comp, TokenType type, const char* message)
{
    if(nextToken(comp).type != type)
        errorAtCurrent(comp, message);
}

bool checkToken(Compiler* comp, TokenType type)
{
    return comp->current.type == type;
}

bool matchToken(Compiler* comp, TokenType type)
{
    if(!checkToken(comp, type))
        return false;
    nextToken(comp);
    return true;
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

typedef void(*parseFn)(Compiler*, bool);

typedef struct ParseRule
{
    parseFn prefix;
    parseFn infix;
    Precedence precedence;
} ParseRule;

void group       (Compiler*, bool);
void binary      (Compiler*, bool);
void unary       (Compiler*, bool);
void number      (Compiler*, bool);
void string      (Compiler*, bool);
void literalTrue (Compiler*, bool);
void literalFalse(Compiler*, bool);
void literalNil  (Compiler*, bool);
void variable    (Compiler*, bool);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]         = {group        , NULL  , PREC_NONE      },
    [TOKEN_RIGHT_PAREN]        = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_LEFT_BRACE]         = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_RIGHT_BRACE]        = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_LEFT_SQUARE_BRACE]  = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_RIGHT_SQUARE_BRACE] = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_COMMA]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_DOT]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_SEMICOLON]          = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_COLON]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_PLUS]               = {NULL         , binary, PREC_TERM      },
    [TOKEN_MINUS]              = {unary        , binary, PREC_TERM      },
    [TOKEN_STAR]               = {NULL         , binary, PREC_FACTOR    },
    [TOKEN_SLASH]              = {NULL         , binary, PREC_FACTOR    },
    [TOKEN_EQUAL]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_EQUAL_EQUAL]        = {NULL         , binary, PREC_EQUALITY  },
    [TOKEN_BANG]               = {unary        , NULL  , PREC_NONE      },
    [TOKEN_BANG_EQUAL]         = {NULL         , binary, PREC_EQUALITY  },
    [TOKEN_MORE]               = {NULL         , binary, PREC_COMPARISON},
    [TOKEN_MORE_EQUAL]         = {NULL         , binary, PREC_COMPARISON},
    [TOKEN_LESS]               = {NULL         , binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]         = {NULL         , binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]         = {variable     , NULL  , PREC_NONE      },
    [TOKEN_NUMBER]             = {number       , NULL  , PREC_NONE      },
    [TOKEN_STRING]             = {string       , NULL  , PREC_NONE      },
    [TOKEN_AND]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_OR]                 = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_NOT]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_VAR]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_CONST]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_FUN]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_CLASS]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_NIL]                = {literalNil   , NULL  , PREC_NONE      },
    [TOKEN_THIS]               = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_SUPER]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_TRUE]               = {literalTrue  , NULL  , PREC_NONE      },
    [TOKEN_FALSE]              = {literalFalse , NULL  , PREC_NONE      },
    [TOKEN_IF]                 = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_ELSE]               = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_FOR]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_WHILE]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_EOF]                = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_ERROR]              = {NULL         , NULL  , PREC_NONE      },
    [TOKEN_PRINT]              = {NULL         , NULL  , PREC_NONE      },
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

    bool canAssign = p <= PREC_ASSIGNMENT;
    prefix(comp, canAssign);

    while(p <= getRule(comp->current.type).precedence)
    {
        nextToken(comp);
        getRule(comp->previous.type).infix(comp, canAssign);
    }

    if(canAssign && matchToken(comp, TOKEN_EQUAL))
    {
        errorAtCurrent(comp, "Invalid assignment target");
    }
}

void expression(Compiler* comp)
{
    parsePrecedence(comp, PREC_ASSIGNMENT);
}

void number(Compiler* comp, bool canAssign)
{
    emitConstant(comp, NUMBER_VAL(strtod(comp->previous.start, NULL)));
}

void string(Compiler* comp, bool canAssign)
{
    emitConstant(comp, OBJ_VAL(copyString(comp->vm, comp->previous.start + 1, comp->previous.length - 2)));
}

void group(Compiler* comp, bool canAssign)
{
    expression(comp);
    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

void unary(Compiler* comp, bool canAssign)
{
    TokenType operator = comp->previous.type;
    parsePrecedence(comp, PREC_UNARY);
    switch(operator)
    {
        case TOKEN_MINUS: emitByte(comp, OP_NEGATE); break;
        case TOKEN_BANG:  emitByte(comp, OP_NOT   ); break;
    }
}

void binary(Compiler* comp, bool canAssign)
{
    TokenType operator = comp->previous.type;
    parsePrecedence(comp, (Precedence)(getRule(operator).precedence + 1));
    switch(operator)
    {
        case TOKEN_PLUS       : emitByte(comp, OP_ADD       ); break;
        case TOKEN_MINUS      : emitByte(comp, OP_SUBTRACT  ); break;
        case TOKEN_STAR       : emitByte(comp, OP_MULTIPLY  ); break;
        case TOKEN_SLASH      : emitByte(comp, OP_DIVIDE    ); break;
        case TOKEN_EQUAL_EQUAL: emitByte(comp, OP_EQUAL     ); break;
        case TOKEN_BANG_EQUAL : emitByte(comp, OP_NOT_EQUAL ); break;
        case TOKEN_MORE       : emitByte(comp, OP_MORE      ); break;
        case TOKEN_MORE_EQUAL : emitByte(comp, OP_MORE_EQUAL); break;
        case TOKEN_LESS       : emitByte(comp, OP_LESS      ); break;
        case TOKEN_LESS_EQUAL : emitByte(comp, OP_LESS_EQUAL); break;
    }
}

void literalTrue(Compiler* comp, bool canAssign)
{
    emitByte(comp, OP_TRUE);
}

void literalFalse(Compiler* comp, bool canAssign)
{
    emitByte(comp, OP_FALSE);
}

void literalNil(Compiler* comp, bool canAssign)
{
    emitByte(comp, OP_NIL);
}

uint16_t identifierConstant(Compiler* comp, Token* name)
{
    return makeConstant(comp, OBJ_VAL(copyString(comp->vm, name->start, name->length)));
}

void namedVariable(Compiler* comp, Token name, bool canAssign)
{
    OpCode op = OP_GET_GLOBAL;

    if(canAssign && matchToken(comp, TOKEN_EQUAL))
    {
        expression(comp);
        op = OP_SET_GLOBAL;
    }

    uint16_t arg = identifierConstant(comp, &name);
    if(arg < 256)
        emitBytes(comp, op, (uint8_t)arg);
    else
    {
        emitBytes(comp, (OpCode)((int)op + 1), (uint8_t)arg);
        emitByte(comp, (uint8_t)(arg >> 8));
    }
}

void variable(Compiler* comp, bool canAssign)
{
    namedVariable(comp, comp->previous, canAssign);
}

void printStatement(Compiler* comp)
{
    expression(comp);
    consume(comp, TOKEN_SEMICOLON, "Expected ';' after value");
    emitByte(comp, OP_PRINT);
}

void expressionStatement(Compiler* comp)
{
    expression(comp);
    consume(comp, TOKEN_SEMICOLON, "Expected ';' after expression");
    emitByte(comp, OP_POP);
}

void statement(Compiler* comp)
{
    if(matchToken(comp, TOKEN_PRINT))
        printStatement(comp);
    else
        expressionStatement(comp);
}

uint16_t parseVariable(Compiler* comp, const char* error)
{
    consume(comp, TOKEN_IDENTIFIER, error);
    return identifierConstant(comp, &comp->previous);
}

void defineVariable(Compiler* comp, uint16_t global)
{
    if(global < 256)
        emitBytes(comp, OP_DEFINE_GLOBAL, (uint8_t)global);
    else
    {
        emitBytes(comp, OP_DEFINE_LONG_GLOBAL, (uint8_t)global);
        emitByte(comp, (uint8_t)(global >> 8));
    }
}

void varDeclaration(Compiler* comp)
{
    uint16_t global = parseVariable(comp, "Expected variable name");

    if(matchToken(comp, TOKEN_EQUAL))
        expression(comp);
    else
        emitByte(comp, OP_NIL);

    consume(comp, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    defineVariable(comp, global);
}

void declaration(Compiler* comp)
{
    if(matchToken(comp, TOKEN_VAR))
        varDeclaration(comp);
    else
        statement(comp);
    
    if(comp->panic)
        synchronize(comp);
}

Result compile(Compiler* comp, const char* source, Chunk* chunk, struct VM* vm)
{
    Scanner scanner;
    initScanner(&scanner, source);
    comp->scanner = &scanner;
    comp->chunk = chunk;
    comp->vm = vm;

    nextToken(comp);
    while(!matchToken(comp, TOKEN_EOF))
    {
        declaration(comp);
    }

    consume(comp, TOKEN_EOF, "Expected end of file");
    freeScanner(&scanner);
    emitByte(comp, OP_RETURN);

    if(comp->error)
        return RESULT_COMPILE_ERROR;

    return RESULT_OK;
}

#endif