#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "compiler.h"
#include "common.h"
// todo: exe name (in usage)
// todo: multi line input in repl

char* readFile(const char* path)
{
	FILE* f = fopen(path, "rb");

	if(f == NULL)
	{
		fprintf(stderr, "could not open file: \"%s\".\n", path);
		exit(74);
	}

	fseek(f, 0L, SEEK_END);
	size_t size = ftell(f);
	rewind(f);

	char* buffer = (char*)malloc(size + 1);
	if(!buffer)
	{
		fprintf(stderr, "memory allocation failed!\n");
		exit(74);
	}
	if(fread(buffer, sizeof(char), size, f) < size)
	{
		fprintf(stderr, "could not read file: \"%s\".\n", path);
		exit(74);
	}

	buffer[size] = '\0';
	return buffer;
}

Result interpret(const char* source, bool disassemble)
{
	VM vm;
	initVM(&vm);
	
	ObjFunction* function = compile(source, &vm, disassemble);
		
	if(!function)
		return RESULT_COMPILE_ERROR;

	if(function->chunk.size == 0)
		return RESULT_OK;

	push(&vm, OBJ_VAL(function));
	callFunction(&vm, function, 0, NULL);

	Result r = run(&vm);
	
	freeVM(&vm);

	return r;
}

// TODO: multi line input
void repl()
{
	char line[1024];
	while(true)
	{
		printf("> ");
		
		if(fgets(line, sizeof(line), stdin))
			interpret(line, false);
		else
			printf(" \n");
	}
}

void runFile(const char* path, bool bytecode)
{
	char* source = readFile(path);
	Result r = interpret(source, bytecode);
	free(source);

	if(r)
		exit((int)r);
}

int main(int argc, char const *argv[])
{
	if(argc == 1)
		repl();
	
	bool bytecode = false;
	const char* file;
	bool fileSet = false;

	for(int i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "--bytecode"))
			bytecode = true;
		else
		{
			if(!fileSet)
			{
				file = argv[i];
				fileSet = true;
			}
			else
				printf("Usage: name [filename]\n");
		}
	}
	
	if(fileSet)
		runFile(file, bytecode);
	else
		printf("Usage: name [filename]\n");

	return 0;
}
