#ifndef COMPILER_H
#define COMPILER_H
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
// todo: print line of error

struct vm;

typedef struct Local
{
    Token name;
    int depth;
} Local;

typedef struct Scope
{
    Local locals[UINT8_MAX + 1];
    int localCount;
    int scopeDepth;
} Scope;

void initScope(Scope* scope)
{
    scope->localCount = 0;
    scope->scopeDepth = 0;
}

typedef struct Compiler
{
    Token current;
    Token previous;
    Scanner* scanner;
    Chunk* chunk;

    bool error;
    bool panic;

    Scope scope;

    struct VM* vm;
} Compiler;

void initCompiler(Compiler* comp)
{
    comp->error = false;
    comp->panic = false;
    initScope(&comp->scope);
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

int emitJump(Compiler* comp, OpCode instruction)
{
    emitByte(comp, instruction);
    emitBytes(comp, 0xff, 0xff);
    return comp->chunk->size - 2;
}

void patchJump(Compiler* comp, int offset)
{
    int jump = comp->chunk->size - offset - 2;

    if(jump > UINT16_MAX)
    {
        errorAtCurrent(comp, "Jump body is too big");
    }

    comp->chunk->data[offset] = (jump >> 8) & 0xff;
    comp->chunk->data[offset + 1] = jump & 0xff;
}

void emitLoop(Compiler* comp, int loopStart)
{
    emitByte(comp, OP_LOOP);

    int offset = comp->chunk->size - loopStart + 2;
    if(offset > UINT16_MAX)
    {
        errorAtCurrent(comp, "Loop body is too big");
    }
    emitByte(comp, (uint8_t)offset>>8);
    emitByte(comp, (uint8_t)offset);
}

void consume(Compiler* comp, TokenType type, const char* message)
{
    if(comp->current.type != type)
        errorAtCurrent(comp, message);
    else
        nextToken(comp);
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

bool identifiersEqual(Token* a, Token* b)
{
    if(a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
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
void and         (Compiler*, bool);
void or          (Compiler*, bool);

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
    [TOKEN_AND]                = {NULL         , and   , PREC_AND       },
    [TOKEN_OR]                 = {NULL         , or    , PREC_OR        },
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
        ParseRule rule = getRule(comp->previous.type);
        rule.infix(comp, canAssign);
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

void and(Compiler* comp, bool canAssign)
{
    int endJump = emitJump(comp, OP_JUMP_IF_FALSE);
    emitByte(comp, OP_POP);
    parsePrecedence(comp, PREC_AND);
    patchJump(comp, endJump);
}

void or(Compiler* comp, bool canAssign)
{
    int endJump = emitJump(comp, OP_JUMP_IF_TRUE);
    emitByte(comp, OP_POP);
    parsePrecedence(comp, PREC_OR);
    patchJump(comp, endJump);
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
    int16_t arg = -1;
    for(int i = comp->scope.localCount - 1; i >= 0; i--)
    {
        Local* local = &comp->scope.locals[i];
        if(identifiersEqual(&name, &local->name))
        {
            if(local->depth == -1)
            {
                errorAtCurrent(comp, "Reading a local in its own initializer is not allowed");
            }
            arg = i;
        }
    }
    
    OpCode getOp = OP_GET_LOCAL;
    OpCode setOp = OP_SET_LOCAL;
    
    if(arg == -1)
    {
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        arg = identifierConstant(comp, &name);
    }
    OpCode op = getOp;

    if(canAssign && matchToken(comp, TOKEN_EQUAL))
    {
        expression(comp);
        op = setOp;
    }

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

void declaration(Compiler*);

void block(Compiler* comp)
{
    while(!checkToken(comp, TOKEN_RIGHT_BRACE) && !checkToken(comp, TOKEN_EOF))
        declaration(comp);

    consume(comp, TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

void beginScope(Compiler* comp)
{
    comp->scope.scopeDepth++;
}

void endScope(Compiler* comp)
{
    comp->scope.scopeDepth--;
    while(comp->scope.localCount > 0  && comp->scope.locals[comp->scope.localCount - 1].depth > comp->scope.scopeDepth)
    {
        emitByte(comp, OP_POP);
        comp->scope.localCount--;
    }
}

void expressionStatement(Compiler* comp)
{
    expression(comp);
    consume(comp, TOKEN_SEMICOLON, "Expected ';' after expression");
    emitByte(comp, OP_POP);
}

void statement(Compiler* comp);

void ifStatement(Compiler* comp)
{
    consume(comp, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    expression(comp);
    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    int thenJump = emitJump(comp, OP_JUMP_IF_FALSE);
    emitByte(comp, OP_POP);
    statement(comp);
    int elseJump = emitJump(comp, OP_JUMP);

    patchJump(comp, thenJump);
    emitByte(comp, OP_POP);
    if(matchToken(comp, TOKEN_ELSE))
        statement(comp);
    patchJump(comp, elseJump);

}

void whileStatement(Compiler* comp)
{
    int loopStart = comp->chunk->size;
    consume(comp, TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
    expression(comp);
    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    int exitJump = emitJump(comp, OP_JUMP_IF_FALSE);
    emitByte(comp, OP_POP);
    statement(comp);

    emitLoop(comp, loopStart);
    patchJump(comp, exitJump);
    emitByte(comp, OP_POP);
}

void statement(Compiler* comp)
{
    if(matchToken(comp, TOKEN_PRINT))
        printStatement(comp);
    else if(matchToken(comp, TOKEN_IF))
        ifStatement(comp);
    else if(matchToken(comp, TOKEN_WHILE))
        whileStatement(comp);
    else if(matchToken(comp, TOKEN_LEFT_BRACE))
    {
        beginScope(comp);
        block(comp);
        endScope(comp);
    }
    else
        expressionStatement(comp);
}

void addLocal(Compiler* comp, Token name)
{
    if (comp->scope.localCount == UINT8_MAX + 1)
    {
        errorAtCurrent(comp, "Too many local variables in scope");
        return;
    }
    
    Local* local = &comp->scope.locals[comp->scope.localCount++];
    local->name = name;
    local->depth = -1;
}

void markInitialized(Compiler* comp)
{
    comp->scope.locals[comp->scope.localCount - 1].depth = comp->scope.scopeDepth;
}

void declareVariable(Compiler* comp)
{
    Token* name = &comp->previous;
    for(int i = comp->scope.localCount - 1; i >= 0; i--)
    {
        Local* local = &comp->scope.locals[i];
        if(local->depth != -1 && local->depth < comp->scope.scopeDepth)
            break;
        
        if(identifiersEqual(name, &local->name))
        {
            errorAtCurrent(comp, "A variable with this name already exists");
        }
    }

    addLocal(comp, *name);
}

uint16_t parseVariable(Compiler* comp, const char* error)
{
    consume(comp, TOKEN_IDENTIFIER, error);

    if(comp->scope.scopeDepth > 0)
    {
        declareVariable(comp);
        return 0;
    }

    return identifierConstant(comp, &comp->previous);
}

void defineVariable(Compiler* comp, uint16_t global)
{
    if(comp->scope.scopeDepth > 0)
    {
        markInitialized(comp);
        return;
    }

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