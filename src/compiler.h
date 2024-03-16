#ifndef COMPILER_H
#define COMPILER_H
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
// todo: print line of error
// todo: global constants

struct vm;

typedef struct Local
{
    Token name;
    int depth;
    bool constant;
    bool isCaptured;
} Local;

typedef enum FunctionType
{
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

struct Compiler;

typedef struct Upvalue 
{
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef struct Scope
{
    Local locals[UINT8_MAX + 1];
    int localCount;
    int scopeDepth;

    ObjFunction* function;
    FunctionType type;

    struct Scope* enclosing;
    struct Compiler* comp;

    uint8_t upvalueCount;
    Upvalue upvalues[UINT8_MAX + 1];
} Scope;

typedef struct Compiler
{
    Token current;
    Token previous;
    Scanner* scanner;

    bool error;
    bool panic;

    bool disassemble;

    Scope* scope;

    struct VM* vm;
} Compiler;

void initScope(Scope* scope, Compiler* comp, FunctionType type)
{
    scope->localCount = 0;
    scope->scopeDepth = 0;
    scope->type = type;
    scope->function = newFunction(comp->vm);
    scope->comp = comp;
    scope->upvalueCount = 0;

    Local* local = &scope->locals[scope->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
    local->constant = false;
    local->isCaptured = false;
    scope->enclosing = comp->scope;
    comp->scope = scope;
}

void initCompiler(Compiler* comp, Scanner* sc, VM* vm)
{
    comp->error = false;
    comp->panic = false;
    comp->scanner = sc;
    comp->vm = vm;
}

Chunk* currentChunk(Compiler* comp)
{
    return &comp->scope->function->chunk;
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
    addToChunk(currentChunk(comp), byte, comp->previous.line);
}

void emitBytes(Compiler* comp, uint8_t byte1, uint8_t byte2)
{
    emitByte(comp, byte1);
    emitByte(comp, byte2);
}

ObjFunction* freeScope(Scope* scope)
{
    emitByte(scope->comp, OP_RETURN);
    scope->comp->scope = scope->enclosing;
    
    if(scope->comp->disassemble)
		disassembleChunk(&scope->function->chunk, scope->function->name == NULL ? "script" : scope->function->name->chars);
    
    return scope->function;
}

ObjFunction* freeCompiler(Compiler* comp)
{
    return freeScope(comp->scope);
}

uint32_t makeConstant(Compiler* comp, Value v)
{
    for(int i = 0; i < currentChunk(comp)->values.size; i++)
        if(valuesEqual(currentChunk(comp)->values.data[i], v))
            return i;

    int constant = addToValueArray(&currentChunk(comp)->values, v);
    if(constant >= 0xffff)
    {
        errorAtCurrent(comp, "Too many constant in one chunk.");
        return 0;
    }
    return constant;
}

void emitConstant(Compiler* comp, Value value)
{
    addConstantInstrution(currentChunk(comp), makeConstant(comp, value), comp->previous.line);
}

int emitJump(Compiler* comp, OpCode instruction)
{
    emitByte(comp, instruction);
    emitBytes(comp, 0xff, 0xff);
    return currentChunk(comp)->size - 2;
}

void patchJump(Compiler* comp, int offset)
{
    int jump = currentChunk(comp)->size - offset - 2;

    if(jump > UINT16_MAX)
    {
        errorAtCurrent(comp, "Jump body is too big");
    }

    currentChunk(comp)->data[offset] = (jump >> 8) & 0xff;
    currentChunk(comp)->data[offset + 1] = jump & 0xff;
}

void emitLoop(Compiler* comp, int loopStart)
{
    emitByte(comp, OP_LOOP);

    int offset = currentChunk(comp)->size - loopStart + 2;
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
    bool canBeStatement;
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
void blockExpr   (Compiler*, bool);
void ifExpr      (Compiler*, bool);
void funExpr     (Compiler*, bool);
void call        (Compiler*, bool);


ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]         = {group        , call  , PREC_CALL      , false},
    [TOKEN_RIGHT_PAREN]        = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_LEFT_BRACE]         = {blockExpr    , NULL  , PREC_NONE      ,  true},
    [TOKEN_RIGHT_BRACE]        = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_LEFT_SQUARE_BRACE]  = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_RIGHT_SQUARE_BRACE] = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_COMMA]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_DOT]                = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_SEMICOLON]          = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_COLON]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_PLUS]               = {NULL         , binary, PREC_TERM      , false},
    [TOKEN_MINUS]              = {unary        , binary, PREC_TERM      , false},
    [TOKEN_STAR]               = {NULL         , binary, PREC_FACTOR    , false},
    [TOKEN_SLASH]              = {NULL         , binary, PREC_FACTOR    , false},
    [TOKEN_EQUAL]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_EQUAL_EQUAL]        = {NULL         , binary, PREC_EQUALITY  , false},
    [TOKEN_BANG]               = {unary        , NULL  , PREC_NONE      , false},
    [TOKEN_BANG_EQUAL]         = {NULL         , binary, PREC_EQUALITY  , false},
    [TOKEN_MORE]               = {NULL         , binary, PREC_COMPARISON, false},
    [TOKEN_MORE_EQUAL]         = {NULL         , binary, PREC_COMPARISON, false},
    [TOKEN_LESS]               = {NULL         , binary, PREC_COMPARISON, false},
    [TOKEN_LESS_EQUAL]         = {NULL         , binary, PREC_COMPARISON, false},
    [TOKEN_IDENTIFIER]         = {variable     , NULL  , PREC_NONE      , false},
    [TOKEN_NUMBER]             = {number       , NULL  , PREC_NONE      , false},
    [TOKEN_STRING]             = {string       , NULL  , PREC_NONE      , false},
    [TOKEN_AND]                = {NULL         , and   , PREC_AND       , false},
    [TOKEN_OR]                 = {NULL         , or    , PREC_OR        , false},
    [TOKEN_NOT]                = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_VAR]                = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_CONST]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_FUN]                = {funExpr      , NULL  , PREC_NONE      , false},
    [TOKEN_CLASS]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_NIL]                = {literalNil   , NULL  , PREC_NONE      , false},
    [TOKEN_THIS]               = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_SUPER]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_TRUE]               = {literalTrue  , NULL  , PREC_NONE      , false},
    [TOKEN_FALSE]              = {literalFalse , NULL  , PREC_NONE      , false},
    [TOKEN_IF]                 = {ifExpr       , NULL  , PREC_NONE      ,  true},
    [TOKEN_ELSE]               = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_FOR]                = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_WHILE]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_EOF]                = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_ERROR]              = {NULL         , NULL  , PREC_NONE      , false},
    [TOKEN_PRINT]              = {NULL         , NULL  , PREC_NONE      , false},
};

ParseRule getRule(TokenType type)
{
    return rules[type];
}

bool parsePrecedence(Compiler* comp, Precedence p)
{
    nextToken(comp);
    ParseRule rule = getRule(comp->previous.type);
    parseFn prefix = rule.prefix;
    if(prefix == NULL)
    {
        errorAtCurrent(comp, "Expected expression");
        return false;
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
    return rule.canBeStatement;
}

bool expression(Compiler* comp)
{
    return parsePrecedence(comp, PREC_ASSIGNMENT);
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

bool declaration(Compiler*, bool);

void beginScope(Compiler* comp)
{
    comp->scope->scopeDepth++;
}

void endScope(Compiler* comp)
{
    comp->scope->scopeDepth--;
    while(comp->scope->localCount > 0  && comp->scope->locals[comp->scope->localCount - 1].depth > comp->scope->scopeDepth)
    {
        if(comp->scope->locals[comp->scope->localCount - 1].isCaptured)
        {
            emitByte(comp, OP_CLOSE_UPVALUE);
        }
        else
            emitByte(comp, OP_POP);
        comp->scope->localCount--;
    }
}

void blockExpr(Compiler* comp, bool canAssign)
{
    beginScope(comp);

    bool expr = false;

    while(!checkToken(comp, TOKEN_RIGHT_BRACE))
    {
        if(expr) emitByte(comp, OP_POP);
        expr = declaration(comp, true);
        if(!expr && matchToken(comp, TOKEN_RIGHT_BRACE))
        {
            emitByte(comp, OP_NIL);
            goto noexpr;
        }
    }
    consume(comp, TOKEN_RIGHT_BRACE, "Expected '}' after block");

noexpr:
    endScope(comp);
}

void ifExpr(Compiler* comp, bool canAssign)
{
    consume(comp, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    expression(comp);
    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    int thenJump = emitJump(comp, OP_JUMP_IF_FALSE);
    emitByte(comp, OP_POP);
    expression(comp);
    int elseJump = emitJump(comp, OP_JUMP);

    patchJump(comp, thenJump);
    emitByte(comp, OP_POP);
    if(matchToken(comp, TOKEN_ELSE))
        expression(comp);
    else
        emitByte(comp, OP_NIL);
    
    patchJump(comp, elseJump);
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

int16_t resolveLocal(Compiler* comp, Scope* scope, Token name)
{
    int16_t arg = -1;
    for(int i = scope->localCount - 1; i >= 0; i--)
    {
        Local* local = &scope->locals[i];
        if(identifiersEqual(&name, &local->name))
        {
            if(local->depth == -1)
            {
                errorAtCurrent(comp, "Reading a local in its own initializer is not allowed");
            }
            arg = i;
        }
    }

    return arg;
}

uint8_t addUpvalue(Compiler* comp, Scope* scope, uint16_t index, bool isLocal)
{
    for(int i = 0; i < scope->upvalueCount; i++)
    {
        if(scope->upvalues[i].index == index && scope->upvalues[i].isLocal == isLocal)
        {
            return i;
        }
    }
    if(scope->upvalueCount == UINT8_MAX)
    {
        errorAtCurrent(comp, "Too many closure variables in one function");
        return 0;
    }
    scope->upvalues[scope->upvalueCount].index = index;
    scope->upvalues[scope->upvalueCount].isLocal = isLocal;
    if(isLocal)
    {
        comp->scope->enclosing->locals[index].isCaptured = true;
    }
    return scope->upvalueCount++;
}

int8_t resolveUpvalue(Compiler* comp, Scope* scope, Token name)
{
    if(scope->enclosing == NULL)
        return -1;

    int local = resolveLocal(comp, scope->enclosing, name);
    if(local != -1)
        return addUpvalue(comp, scope, local, true);
    
    int upvalue = resolveUpvalue(comp, scope->enclosing, name);
    if(upvalue != -1)
        return addUpvalue(comp, scope, upvalue, false);

    return -1;
}

void namedVariable(Compiler* comp, Token name, bool canAssign)
{
    int16_t arg = resolveLocal(comp, comp->scope, name);

    OpCode getOp = OP_GET_LOCAL;
    OpCode setOp = OP_SET_LOCAL;
    
    if(arg == -1)
    {
        if((arg = resolveUpvalue(comp, comp->scope, name)) != -1)
        {
            getOp = OP_GET_UPVALUE;
            setOp = OP_SET_UPVALUE;
        }
        else
        {
            getOp = OP_GET_GLOBAL;
            setOp = OP_SET_GLOBAL;
            arg = identifierConstant(comp, &name);
        }
    }
    OpCode op = getOp;

    Local* l = arg == -1 ? NULL : &comp->scope->locals[arg];
    if(canAssign && matchToken(comp, TOKEN_EQUAL))
    {
        expression(comp);
        op = setOp;
        if(l && l->constant)
            errorAtCurrent(comp, "Assigning to a constant is not allowed");
    }

    if(arg < 256)
        emitBytes(comp, op, (uint8_t)arg);
    else
    {
        emitBytes(comp, (OpCode)((int)op + 1), (uint8_t)arg);
        emitByte(comp, (uint8_t)(arg >> 8));
    }
}

void addLocal(Compiler* comp, Token name, bool constant)
{
    if (comp->scope->localCount == UINT8_MAX + 1)
    {
        errorAtCurrent(comp, "Too many local variables in scope");
        return;
    }
    
    Local* local = &comp->scope->locals[comp->scope->localCount++];
    local->name = name;
    local->depth = -1;
    local->constant = constant;
    local->isCaptured = false;
}

void markInitialized(Compiler* comp)
{
    if(comp->scope->scopeDepth == 0) return;
    comp->scope->locals[comp->scope->localCount - 1].depth = comp->scope->scopeDepth;
}

void variable(Compiler* comp, bool canAssign)
{
    namedVariable(comp, comp->previous, canAssign);
}

void declareVariable(Compiler* comp, bool constant)
{
    Token* name = &comp->previous;
    for(int i = comp->scope->localCount - 1; i >= 0; i--)
    {
        Local* local = &comp->scope->locals[i];
        if(local->depth != -1 && local->depth < comp->scope->scopeDepth)
            break;
        
        if(identifiersEqual(name, &local->name))
        {
            errorAtCurrent(comp, "A variable with this name already exists");
        }
    }

    addLocal(comp, *name, constant);
}

uint16_t parseVariable(Compiler* comp, const char* error, bool constant)
{
    consume(comp, TOKEN_IDENTIFIER, error);

    if(comp->scope->scopeDepth > 0)
    {
        declareVariable(comp, constant);
        return 0;
    }

    return identifierConstant(comp, &comp->previous);
}

void defineVariable(Compiler* comp, uint16_t global)
{
    if(comp->scope->scopeDepth > 0)
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

void function(Compiler* comp, FunctionType type)
{
    Scope scope;
    initScope(&scope, comp, type);
    beginScope(comp);

    consume(comp, TOKEN_LEFT_PAREN, "Expected '(' after function");
    if(!checkToken(comp, TOKEN_RIGHT_PAREN))
        do {
            comp->scope->function->arity++;
            if(comp->scope->function->arity > 255)
                errorAtCurrent(comp, "A function with more than 255 parameters is not allowed");
            uint16_t constant = parseVariable(comp, "Expected parameter name", false);
            defineVariable(comp, constant);
        } while (matchToken(comp, TOKEN_COMMA));

    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list");
    consume(comp, TOKEN_LEFT_BRACE, "Expected '{' before function body");

    blockExpr(comp, true);

    scope.function->name = copyString(comp->vm, "anonymous function", 19);
    ObjFunction* function = freeScope(&scope);
    emitConstant(comp, OBJ_VAL(scope.function));
    uint8_t upvalueCount = scope.upvalueCount;
    Upvalue* upvalues = scope.upvalues;    
    if(upvalueCount != 0)
    {
        emitByte(comp, OP_CLOSURE);
        emitByte(comp, upvalueCount);
        for(int i = 0; i < upvalueCount; i++)
        {
            emitByte(comp, upvalues[i].isLocal);
            emitByte(comp, upvalues[i].index);
        }
    }
}

void funExpr(Compiler* comp, bool canAssign)
{
    function(comp, TYPE_FUNCTION);
}

uint8_t argumentList(Compiler* comp)
{
    uint8_t argCount = 0;
    if(!checkToken(comp, TOKEN_RIGHT_PAREN))
        do
        {
            expression(comp);
            if(argCount == 255)
            {
                errorAtCurrent(comp, "Calling with more than 255 arguments is not allowed");
            }
            argCount++;
        } while(matchToken(comp, TOKEN_COMMA));

    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return argCount;
}

void call(Compiler* comp, bool canAssign)
{
    uint8_t argCount = argumentList(comp);
    emitBytes(comp, OP_CALL, argCount);
}

void printStatement(Compiler* comp)
{
    expression(comp);
    consume(comp, TOKEN_SEMICOLON, "Expected ';' after value");
    emitByte(comp, OP_PRINT);
}

bool expressionStatement(Compiler* comp, bool requireSemicolon)
{
    bool noSemicolon = expression(comp);
    if(requireSemicolon && !noSemicolon)
    {
        consume(comp, TOKEN_SEMICOLON, "Expected ';' after expression");
    
        emitByte(comp, OP_POP);
        return false;
    }

    if(!matchToken(comp, TOKEN_SEMICOLON))
        return true;
    emitByte(comp, OP_POP);
}

bool statement(Compiler* comp, bool);

void whileStatement(Compiler* comp)
{
    int loopStart = currentChunk(comp)->size;
    consume(comp, TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
    expression(comp);
    consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    int exitJump = emitJump(comp, OP_JUMP_IF_FALSE);
    emitByte(comp, OP_POP);
    statement(comp, false);

    emitLoop(comp, loopStart);
    patchJump(comp, exitJump);
    emitByte(comp, OP_POP);
}

void varDeclaration(Compiler*, bool);

void forStatement(Compiler* comp)
{
    consume(comp, TOKEN_LEFT_PAREN, "Expected '(' after 'for'");
    beginScope(comp);
    Token name;
    name.length = 0;
    if(matchToken(comp, TOKEN_SEMICOLON))
    {}
    else if(matchToken(comp, TOKEN_VAR))
        varDeclaration(comp, false);
    else
        expressionStatement(comp, true);

    int loopStart = currentChunk(comp)->size;
    int exitJump = -1;
    if(!matchToken(comp, TOKEN_SEMICOLON))
    {
        expression(comp);
        consume(comp, TOKEN_SEMICOLON, "Expected ';' after condition");

        exitJump = emitJump(comp, OP_JUMP_IF_FALSE);
        emitByte(comp, OP_POP);
    }
    if(!matchToken(comp, TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(comp, OP_JUMP);
        int incrementStart = currentChunk(comp)->size;
        expression(comp);
        emitByte(comp, OP_POP);

        consume(comp, TOKEN_RIGHT_PAREN, "Expected ')' after for clauses");
        emitLoop(comp, loopStart);
        loopStart = incrementStart;
        patchJump(comp, bodyJump);
    }

    statement(comp, false);
    emitLoop(comp, loopStart);

    if(exitJump != -1)
    {
        patchJump(comp, exitJump);
        emitByte(comp, OP_POP);
    }
    endScope(comp);
}

void returnStatement(Compiler* comp)
{
    if(matchToken(comp, TOKEN_SEMICOLON))
    {
        emitByte(comp, OP_NIL);
        emitByte(comp, OP_RETURN);
    }
    else
    {
        expression(comp);
        consume(comp, TOKEN_SEMICOLON, "Expected ';' after return value");
        emitByte(comp, OP_RETURN);
    }
}

bool statement(Compiler* comp, bool canExpr)
{
    if(matchToken(comp, TOKEN_PRINT))
        printStatement(comp);
    else if(matchToken(comp, TOKEN_FOR))
        forStatement(comp);
    else if(matchToken(comp, TOKEN_WHILE))
        whileStatement(comp);
    else if(matchToken(comp, TOKEN_RETURN))
        returnStatement(comp);
    else
        return expressionStatement(comp, !canExpr);
    return false;
}

void varDeclaration(Compiler* comp, bool constant)
{
    uint16_t global = parseVariable(comp, "Expected variable name", constant);

    if(matchToken(comp, TOKEN_EQUAL))
        expression(comp);
    else
        emitByte(comp, OP_NIL);

    consume(comp, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    defineVariable(comp, global);
}

void funDeclaration(Compiler* comp)
{
    uint16_t global = parseVariable(comp, "Expected function name", true);
    markInitialized(comp);
    function(comp, TYPE_FUNCTION);
    defineVariable(comp, global);
}

bool declaration(Compiler* comp, bool canExpr)
{
    bool result = false;
    if(matchToken(comp, TOKEN_VAR))
        varDeclaration(comp, false);
    else if(matchToken(comp, TOKEN_CONST))
        varDeclaration(comp, true);
    else if(matchToken(comp, TOKEN_FUN))
        funDeclaration(comp);
    else
        result = statement(comp, canExpr);
    
    if(comp->panic)
        synchronize(comp);
    return result;
}

ObjFunction* compile(const char* source, struct VM* vm, bool disassemble)
{
    Scanner scanner;
    initScanner(&scanner, source);
    Compiler comp;
	initCompiler(&comp, &scanner, vm);
    Scope scope;
    initScope(&scope, &comp, TYPE_SCRIPT);
    comp.disassemble = disassemble;

    nextToken(&comp);
    while(!matchToken(&comp, TOKEN_EOF))
    {
        declaration(&comp, false);
    }

    consume(&comp, TOKEN_EOF, "Expected end of file");
	ObjFunction* function = freeCompiler(&comp);
    freeScanner(&scanner);

    if(comp.error)
        return NULL;

    return function;
}

#endif