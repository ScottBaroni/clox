#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "object.h"
#include "table.h"


static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
  initVM();

    {
    enum { N = 200000 };
    static ObjString* keys[2 * N];
    char buf[32];
    for (int i = 0; i < 2 * N; i++) {
      int n = snprintf(buf, sizeof buf, "key%d", i);
      keys[i] = copyString(buf, n);
    }

    Table t;
    initTable(&t);
    clock_t s = clock();
    for (int i = 0; i < N; i++) tableSet(&t, keys[i], NUMBER_VAL(i));
    printf("insert:      %.3f s\n", (double)(clock() - s) / CLOCKS_PER_SEC);

    Value v;
    s = clock();
    for (int r = 0; r < 10; r++)
      for (int i = 0; i < N; i++) tableGet(&t, keys[i], &v);
    printf("lookup hit:  %.3f s\n", (double)(clock() - s) / CLOCKS_PER_SEC);

    s = clock();
    for (int r = 0; r < 10; r++)
      for (int i = N; i < 2 * N; i++) tableGet(&t, keys[i], &v);
    printf("lookup miss: %.3f s\n", (double)(clock() - s) / CLOCKS_PER_SEC);

    s = clock();
    for (int r = 0; r < 5; r++) {
      for (int i = 0; i < N; i++) tableDelete(&t, keys[i]);
      for (int i = 0; i < N; i++) tableSet(&t, keys[i], NUMBER_VAL(i));
    }
    printf("delete churn:%.3f s\n", (double)(clock() - s) / CLOCKS_PER_SEC);
    freeTable(&t);
  }

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVM();
  return 0;
}