#include <stdlib.h>
#include <string.h>

#include "memory.h"

#define HEAP_SIZE (64 * 1024 * 1024)
#define ALIGN 16
#define ROUND_UP(n) (((n) + ALIGN - 1) & ~(size_t)(ALIGN - 1))

// Lives inside freed memory. Assumes a 64-bit machine (16 bytes).
typedef struct FreeBlock {
  size_t size;
  struct FreeBlock* next;
} FreeBlock;

static uint8_t* heapStart = NULL;
static size_t heapUsed = 0;      // bump pointer: everything past this is untouched
static FreeBlock* freeList = NULL; // sorted by address

static void* allocBlock(size_t size) {
  size = ROUND_UP(size);

  // First fit from the free list.
  FreeBlock* prev = NULL;
  for (FreeBlock* b = freeList; b != NULL; prev = b, b = b->next) {
    if (b->size < size) continue;

    if (b->size - size >= sizeof(FreeBlock)) {
      // Split: the leftover stays on the list.
      FreeBlock* rest = (FreeBlock*)((uint8_t*)b + size);
      rest->size = b->size - size;
      rest->next = b->next;
      if (prev) prev->next = rest; else freeList = rest;
    } else {
      // Exact fit (all sizes are multiples of 16, so no slack).
      if (prev) prev->next = b->next; else freeList = b->next;
    }
    return b;
  }

  // Nothing fits: carve from the untouched part of the heap.
  if (heapUsed + size > HEAP_SIZE) exit(1);
  void* result = heapStart + heapUsed;
  heapUsed += size;
  return result;
}

static void freeBlock(void* pointer, size_t size) {
  size = ROUND_UP(size);
  FreeBlock* block = (FreeBlock*)pointer;
  block->size = size;

  // Insert in address order.
  FreeBlock* prev = NULL;
  FreeBlock* cur = freeList;
  while (cur != NULL && cur < block) {
    prev = cur;
    cur = cur->next;
  }
  block->next = cur;
  if (prev) prev->next = block; else freeList = block;

  // Coalesce with the next block.
  if (cur != NULL && (uint8_t*)block + block->size == (uint8_t*)cur) {
    block->size += cur->size;
    block->next = cur->next;
  }
  // Coalesce with the previous block.
  if (prev != NULL && (uint8_t*)prev + prev->size == (uint8_t*)block) {
    prev->size += block->size;
    prev->next = block->next;
  }
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  // The single allowed malloc() call.
  if (heapStart == NULL) {
    heapStart = malloc(HEAP_SIZE);
    if (heapStart == NULL) exit(1);
  }

  if (newSize == 0) {
    if (pointer != NULL) freeBlock(pointer, oldSize);
    return NULL;
  }

  if (oldSize == 0) return allocBlock(newSize);

  if (ROUND_UP(newSize) == ROUND_UP(oldSize)) return pointer;

  if (newSize < oldSize) {
    // Shrink in place: give the tail back.
    size_t keep = ROUND_UP(newSize);
    freeBlock((uint8_t*)pointer + keep, ROUND_UP(oldSize) - keep);
    return pointer;
  }

  // Grow: allocate, copy, free.
  void* result = allocBlock(newSize);
  memcpy(result, pointer, oldSize);
  freeBlock(pointer, oldSize);
  return result;
}