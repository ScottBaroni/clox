#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static uint32_t hashValue(Value value) {
  switch (value.type) {
    case VAL_BOOL: return AS_BOOL(value) ? 3 : 5;
    case VAL_NIL: return 7;
    case VAL_NUMBER: {
      double d = AS_NUMBER(value);
      if (d == 0) d = 0.0; // -0 and +0 are equal, so they must hash alike.
      uint64_t bits;
      memcpy(&bits, &d, sizeof(bits));
      return (uint32_t)(bits ^ (bits >> 32));
    }
    case VAL_OBJ: return AS_STRING(value)->hash; // Only strings so far.
  }
  return 0; // Unreachable.
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
  uint32_t index = hashValue(key) % capacity;
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];
    if (entry->state == ENTRY_EMPTY) {
      return tombstone != NULL ? tombstone : entry;
    } else if (entry->state == ENTRY_TOMBSTONE) {
      if (tombstone == NULL) tombstone = entry;
    } else if (valuesEqual(entry->key, key)) {
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(Table* table, int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].state = ENTRY_EMPTY;
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->state != ENTRY_FULL) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    *dest = *entry;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableGet(Table* table, Value key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->state != ENTRY_FULL) return false;

  *value = entry->value;
  return true;
}

bool tableSet(Table* table, Value key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->state != ENTRY_FULL;
  if (entry->state == ENTRY_EMPTY) table->count++;

  entry->state = ENTRY_FULL;
  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table* table, Value key) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->state != ENTRY_FULL) return false;

  entry->state = ENTRY_TOMBSTONE;
  entry->key = NIL_VAL;
  entry->value = NIL_VAL;
  return true;
}

void tableAddAll(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->state == ENTRY_FULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry* entry = &table->entries[index];
    if (entry->state == ENTRY_EMPTY) return NULL;
    if (entry->state == ENTRY_FULL && IS_STRING(entry->key)) {
      ObjString* string = AS_STRING(entry->key);
      if (string->length == length && string->hash == hash &&
          memcmp(string->chars, chars, length) == 0) {
        return string;
      }
    }

    index = (index + 1) % table->capacity;
  }
}