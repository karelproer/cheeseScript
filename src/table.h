#ifndef TABLE_H
#define TABLE_H
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct Bucket
{
    ObjString* key;
    Value value;
} Bucket;

typedef struct Table
{
    int count;
    int capacity;
    Bucket* buckets;
} Table;

void initTable(Table* table)
{
    table->count = 0;
    table->capacity = 0;
    table->buckets = 0;
}

void freetable(Table* table)
{
    free(table->buckets);
}

Bucket* findBucket(Bucket* buckets, int capacity, ObjString* key)
{
    uint32_t index = key->hash % capacity;
    
    Bucket* bucket = &buckets[index];
    Bucket* tombstone = NULL;
    while(true)
    {   if(bucket->key == NULL)
        {
            if(IS_NIL(bucket->value))
                return tombstone != NULL ? tombstone : bucket;
            else
                if(!tombstone)
                    tombstone = bucket;
        }
        else if(bucket->key == key)
            return bucket;
        index = (index + 1) % capacity;
        bucket = &buckets[index];
    }

    return bucket;
}

ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash)
{
    if(table->capacity == 0) return NULL;
    uint32_t index = hash % table->capacity;
    
    Bucket* bucket = &table->buckets[index];
    while(true)
    {   if(bucket->key == NULL)
        {
            if(IS_NIL(bucket->value))
                return NULL;
        }
        else if(bucket->key->length == length && bucket->key->hash == hash && memcmp(bucket->key->chars, chars, length) == 0)
            return bucket->key;
        
        index = (index + 1) % table->capacity;
        bucket = &table->buckets[index];
    }
}

void growTable(Table* table)
{
    int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
    table->count = 0;
    Bucket* buckets = (Bucket*)malloc(capacity * sizeof(Bucket));
    for(int i = 0; i < capacity; i++)
    {
        buckets[i].key = NULL;
        buckets[i].value = NIL_VAL;
    }

    for(int i = 0; i < table->capacity; i++)
    {
        Bucket* bucket = &table->buckets[i];
        if(bucket->key == NULL)
            continue;
        Bucket* dest = findBucket(buckets, capacity, bucket->key);
        dest->key = bucket->key;
        dest->value = bucket->value;
        table->count++;
    }

    free(table->buckets);
    table->buckets = buckets;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value)
{
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD)
        growTable(table);

    Bucket* bucket = findBucket(table->buckets, table->capacity, key);
    bool isNew = bucket->key == NULL;
    if(isNew && IS_NIL(bucket->value))
        table->count++;

    bucket->key = key;
    bucket->value = value;
    return isNew;
}

bool tableGet(Table* table, ObjString* key, Value* value)
{
    if(table->count == 0)
        return false;
    
    Bucket* bucket = findBucket(table->buckets, table->capacity, key);
    if(bucket->key == 0)
        return false;
    
    *value = bucket->value;
    return true;
}

bool tableDelete(Table* table, ObjString* key, Value* value)
{
    if(table->count == 0)
        return false;

    Bucket* bucket = findBucket(table->buckets, table->capacity, key);
    if(bucket->key == 0)
        return false;

    bucket->key = NULL;
    bucket->value = BOOL_VAL(true);
    return true;
}

#endif