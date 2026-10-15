#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
  Chunk chunk;
  initChunk(&chunk);

  for (int i = 0; i < 300; i++) {
    writeConstant(&chunk, i, 123 + i / 50);
  }
  writeChunk(&chunk, OP_RETURN, 129);

  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);
  return 0;
}