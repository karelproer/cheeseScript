#ifndef SCANNER_H
#define SCANNER_H
// todo: more keywords
// todo: string interpolation

typedef struct Scanner 
{
    const char* start;
    const char* current;
    int line;
    int collumn;
} Scanner;

typedef enum TokenType
{
    // parentheses and braces.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, // parentheses.
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE, // curly braces.
    TOKEN_LEFT_SQUARE_BRACE, TOKEN_RIGHT_SQUARE_BRACE, // square braces.
    // punctuation.
    TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON, TOKEN_COLON,
    // arithmetics.
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    // comparison and logic.
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, // equality.
    TOKEN_BANG, TOKEN_BANG_EQUAL, // not.
    TOKEN_MORE, TOKEN_MORE_EQUAL, // more.
    TOKEN_LESS, TOKEN_LESS_EQUAL, // less.
    // literals.
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
    // keywords.
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, // logic.
    TOKEN_VAR, TOKEN_CONST, TOKEN_FUN, TOKEN_CLASS, // declarations.
    TOKEN_NIL, TOKEN_THIS, TOKEN_SUPER, TOKEN_TRUE, TOKEN_FALSE, // special values.
    TOKEN_IF, TOKEN_ELSE, TOKEN_FOR, TOKEN_WHILE, // control flow.

    // special tokens.
    TOKEN_EOF, TOKEN_ERROR,
} TokenType;

typedef struct Token
{
    TokenType type;
    const char* start;
    unsigned int length;
    unsigned int line;
    unsigned int collumn;
} Token;

void initScanner(Scanner* sc, const char* source)
{
    sc->start = source;
    sc->current = source;
    sc->line = 1;
    sc->collumn = 1;
} 

void freeScanner(Scanner* sc)
{
}

bool atEnd(Scanner* sc)
{
    return *sc->current == '\0';
}

Token makeToken(Scanner* sc, TokenType type)
{
    Token t;
    t.type = type;
    t.start = sc->start;
    t.length = (unsigned int)(sc->current - sc->start);
    t.line = sc->line;
    t.collumn = sc->collumn - t.length;
    return t;
}

Token errorToken(Scanner* sc, const char* message)
{
    Token t;
    t.type = TOKEN_ERROR;
    t.start = message;
    t.length = (unsigned int)strlen(message);
    t.line = sc->line;
    t.collumn = sc->collumn;
    return t;
}

char advance(Scanner* sc)
{
    sc->current++;
    sc->collumn++;
    return sc->current[-1];
}

bool match(Scanner* sc, char expected)
{
    if(atEnd(sc))
        return false;

    if(*sc->current != expected)
        return false;

    sc->current++;
    sc->collumn++;
    return true;
}

char peek(Scanner* sc)
{
    return *sc->current;
}

char doublePeek(Scanner* sc)
{
    if(atEnd(sc))
        return '\0';
    return sc->current[1];
}

char skip(Scanner* sc)
{
    char c = advance(sc);
    if(c == '\n')
    {
        sc->line++;
        sc->collumn = 1;
    }
    else if(c == '\t')
    {
        sc->line += 3;
    }
}

void skipWhiteSpace(Scanner* sc)
{
    while(true)
    {
        char c = peek(sc);
        switch(c)
        {
            case '\t':
                sc->collumn += 3;
                advance(sc);
                break;
            case '\n':
                advance(sc);
                sc->line++;
                sc->collumn = 1;
                break;
            case ' ':
            case '\r':
                advance(sc);
                break;
            case '/':
                if(doublePeek(sc) == '/')
                    while(!atEnd(sc) && peek(sc) != '\n')
                        advance(sc);
                else if(doublePeek(sc) == '*')
                {
                    while(!atEnd(sc) && (peek(sc) != '*' || doublePeek(sc) != '/'))
                        skip(sc);
                    advance(sc);
                    advance(sc);
                }
                else
                    return;
                break;
            default:
                return;
        }
    }
}

Token string(Scanner* sc, char end)
{
    while(peek(sc) != end)
    {
        if(atEnd(sc))
            return errorToken(sc, "Unterminated string.");
        skip(sc);
    }

    advance(sc);
    return makeToken(sc, TOKEN_STRING);
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool isAlpha(char c)
{
    return (c >= 'a' && c >= 'z') || (c >= 'A' && c >= 'Z') || c == '_';
}

Token scanNumber(Scanner* sc)
{
    while(isDigit(peek(sc)))
        advance(sc);
    
    if(peek(sc) == '.' && isDigit(doublePeek(sc)))
    {
        advance(sc);
        
        while(isDigit(peek(sc)))
            advance(sc);
    }

    return makeToken(sc, TOKEN_NUMBER);
}

TokenType checkKeyword(Scanner* sc, int start, const char* rest, TokenType type)
{
    if(sc->current - sc->start == start + strlen(rest) && !memcmp(sc->start + start, rest, strlen(rest)))
        return type;

    return TOKEN_IDENTIFIER;
}

TokenType identifierType(Scanner* sc)
{
    switch (*sc->start)
    {
        case 'a': return checkKeyword(sc, 1, "nd"  , TOKEN_AND  );
        case 'e': return checkKeyword(sc, 1, "lse" , TOKEN_ELSE );
        case 'i': return checkKeyword(sc, 1, "f"   , TOKEN_IF   );
        case 'o': return checkKeyword(sc, 1, "r"   , TOKEN_OR   );
        case 's': return checkKeyword(sc, 1, "uper", TOKEN_SUPER);
        case 'v': return checkKeyword(sc, 1, "ar"  , TOKEN_VAR  );
        case 'w': return checkKeyword(sc, 1, "hile", TOKEN_WHILE);
        case 'c': 
            if(sc->current - sc->start > 1)
            {
                switch(sc->start[1])
                {
                    case 'l': return checkKeyword(sc, 2, "ass", TOKEN_CLASS);
                    case 'o': return checkKeyword(sc, 2, "nst", TOKEN_CONST);
                }
            }
        case 'f':
           if(sc->current - sc->start > 1)
            {
                switch(sc->start[1])
                {
                    case 'a': return checkKeyword(sc, 2, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(sc, 2, "r"  ,   TOKEN_FOR);
                    case 'u': return checkKeyword(sc, 2, "n"  ,   TOKEN_FUN);

                }
            }
        case 'n':
            if(sc->current - sc->start > 1)
            {
                switch(sc->start[1])
                {
                    case 'i': return checkKeyword(sc, 2, "l", TOKEN_NIL);
                    case 'o': return checkKeyword(sc, 2, "t", TOKEN_NOT);
                }
            }
        case 't': 
            if(sc->current - sc->start > 1)
            {
                switch(sc->start[1])
                {
                    case 'h': return checkKeyword(sc, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(sc, 2, "ue", TOKEN_TRUE);
                }
            }
        default: return TOKEN_IDENTIFIER;
    }
}

Token identifier(Scanner* sc)
{
    while (isAlpha(peek(sc)) || isDigit(peek(sc)))
        advance(sc);
    return makeToken(sc, identifierType(sc));    
}

Token scanToken(Scanner* sc)
{
    skipWhiteSpace(sc);

    sc->start = sc->current;
    if(atEnd(sc))
        return makeToken(sc, TOKEN_EOF);
    
    char c = advance(sc);
    
    if(isAlpha(c)) return identifier(sc);
    if(isDigit(c)) return scanNumber(sc);
    switch (c)
    {
        // single character.
        case '(': return makeToken(sc, TOKEN_LEFT_PAREN        ); break;
        case ')': return makeToken(sc, TOKEN_RIGHT_PAREN       ); break;
        case '{': return makeToken(sc, TOKEN_LEFT_BRACE        ); break;
        case '}': return makeToken(sc, TOKEN_RIGHT_BRACE       ); break;
        case '[': return makeToken(sc, TOKEN_LEFT_SQUARE_BRACE ); break;
        case ']': return makeToken(sc, TOKEN_RIGHT_SQUARE_BRACE); break;
        case ',': return makeToken(sc, TOKEN_COMMA             ); break;
        case '.': return makeToken(sc, TOKEN_DOT               ); break;
        case ':': return makeToken(sc, TOKEN_COLON             ); break;
        case ';': return makeToken(sc, TOKEN_SEMICOLON         ); break;
        case '-': return makeToken(sc, TOKEN_MINUS             ); break;
        case '+': return makeToken(sc, TOKEN_PLUS              ); break;
        case '*': return makeToken(sc, TOKEN_STAR              ); break;
        case '/': return makeToken(sc, TOKEN_SLASH             ); break;
        // one or two character.
        case '=': return match(sc, '=') ? makeToken(sc, TOKEN_EQUAL_EQUAL) : makeToken(sc, TOKEN_EQUAL); break;
        case '!': return match(sc, '=') ? makeToken(sc, TOKEN_BANG_EQUAL ) : makeToken(sc, TOKEN_EQUAL); break;
        case '<': return match(sc, '=') ? makeToken(sc, TOKEN_LESS_EQUAL ) : makeToken(sc, TOKEN_EQUAL); break;
        case '>': return match(sc, '=') ? makeToken(sc, TOKEN_MORE_EQUAL ) : makeToken(sc, TOKEN_EQUAL); break;
        // literals
        case '"': return string(sc, '"');
        case '\'': return string(sc, '\'');
    }

    return errorToken(sc, "Unexpected character.");
}

#endif