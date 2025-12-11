/*!
 * @file bifrost_hash_map.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *
 * @copyright Copyright (c) 2019
 */
#include "bifrost/bifrost_vm.h"  // VM Turn off GC

#include "bifrost_vm_gc.h"   // Allocation Functions
#include "bifrost_vm_obj.h"  // bf_flex_array_member

struct bfHashNode
{
  const BifrostObjStr* key;
  bfHashNode*          next;
  char                 value[bf_flex_array_member];
};

static bfHashNode* bfHashMap_newNode(struct BifrostVM* vm, const void* key, size_t value_size, void* value, bfHashNode* next)
{
  vm->gc_is_running = 1;

  bfHashNode* const node = bfGC_AllocMemory(vm, NULL, 0u, sizeof(bfHashNode) + value_size);

  node->key  = key;
  node->next = next;
  LibC_memcpy(node->value, value, value_size);

  vm->gc_is_running = 0;

  return node;
}

static unsigned bfHashMap_defaultHash(const void* key)
{
  const BifrostObjStr* str = (const BifrostObjStr*)key;
  return str->hash;
}

static int bfHashMap_defaultCmp(const BifrostObjStr* const lhs, const BifrostObjStr* const rhs)
{
  return StringCmp_Cmp(StringCmp_FromBStr(lhs), StringCmp_FromBStr(rhs));
}

static bfHashNode* bfHashMap_getNode(const BifrostHashMap* self, const BifrostObjStr* key, unsigned hash)
{
  bfHashNode* cursor = self->buckets[hash];

  while (cursor)
  {
    if (bfHashMap_defaultCmp(key, cursor->key))
    {
      break;
    }

    cursor = cursor->next;
  }

  return cursor;
}

static void bfHashMap_deleteNode(const BifrostHashMap* self, bfHashNode* node)
{
  bfGC_AllocMemory(self->params.vm, node, sizeof(bfHashNode) + self->params.value_size, 0u);
}

void bfHashMapParams_init(BifrostHashMapParams* self, struct BifrostVM* vm)
{
  self->vm         = vm;
  self->value_size = sizeof(void*);
}

void bfHashMap_ctor(BifrostHashMap* self, const BifrostHashMapParams* params)
{
  self->params      = *params;
  self->num_buckets = BIFROST_HASH_MAP_BUCKET_SIZE;

  for (unsigned i = 0; i < self->num_buckets; ++i)
  {
    self->buckets[i] = NULL;
  }
}

void bfHashMap_set(BifrostHashMap* self, const BifrostObjStr* key, void* value)
{
  const unsigned hash = bfHashMap_defaultHash(key) % self->num_buckets;
  bfHashNode*    node = bfHashMap_getNode(self, key, hash);

  if (node)
  {
    node->key = key;
    LibC_memcpy(node->value, value, self->params.value_size);
  }
  else
  {
    self->buckets[hash] = bfHashMap_newNode(self->params.vm, key, self->params.value_size, value, self->buckets[hash]);
  }
}

static int bfHashMap_has(const BifrostHashMap* self, const void* key)
{
  const unsigned hash = bfHashMap_defaultHash(key) % self->num_buckets;
  return bfHashMap_getNode(self, key, hash) != NULL;
}

void* bfHashMap_get(BifrostHashMap* self, const BifrostObjStr* key)
{
  const unsigned hash = bfHashMap_defaultHash(key) % self->num_buckets;
  bfHashNode*    node = bfHashMap_getNode(self, key, hash);

  return node ? node->value : NULL;
}

int bfHashMap_removeCmp(BifrostHashMap* self, const void* key, bfHashMapCmp cmp)
{
  const unsigned hash   = bfHashMap_defaultHash(key) % self->num_buckets;
  bfHashNode*    cursor = self->buckets[hash];
  bfHashNode*    prev   = NULL;

  while (cursor)
  {
    if (cmp(key, cursor->key))
    {
      if (prev)
      {
        prev->next = cursor->next;
      }
      else
      {
        self->buckets[hash] = cursor->next;
      }

      bfHashMap_deleteNode(self, cursor);
      return 1;
    }

    prev   = cursor;
    cursor = cursor->next;
  }

  return 0;
}

bfHashMapIter bfHashMap_itBegin(const BifrostHashMap* self)
{
  bfHashMapIter begin_it = {NULL, NULL, -1, NULL};

  for (int i = 0; i < (int)self->num_buckets; ++i)
  {
    bfHashNode* cursor = self->buckets[i];

    if (cursor)
    {
      begin_it.key   = cursor->key;
      begin_it.value = cursor->value;
      begin_it.index = i;
      begin_it.next  = cursor->next;
      break;
    }
  }

  return begin_it;
}

int bfHashMap_itIsValid(const bfHashMapIter* it)
{
  return it->index != -1 && it->key != NULL;
}

void bfHashMap_itGetNext(const BifrostHashMap* self, bfHashMapIter* it)
{
  if (it->next)
  {
    it->key   = it->next->key;
    it->value = it->next->value;
    it->next  = it->next->next;
  }
  else
  {
    for (int i = (it->index + 1); i < (int)self->num_buckets; ++i)
    {
      bfHashNode* const cursor = self->buckets[i];

      if (cursor != NULL)
      {
        it->key   = cursor->key;
        it->value = cursor->value;
        it->index = i;
        it->next  = cursor->next;
        return;
      }
    }
    it->key   = NULL;
    it->index = -1;
  }
}

void bfHashMap_clear(BifrostHashMap* self)
{
  for (unsigned i = 0; i < self->num_buckets; ++i)
  {
    bfHashNode* cursor = self->buckets[i];

    while (cursor != NULL)
    {
      bfHashNode* const next = cursor->next;
      bfHashMap_deleteNode(self, cursor);
      cursor = next;
    }

    self->buckets[i] = NULL;
  }
}

void bfHashMap_dtor(BifrostHashMap* self)
{
  bfHashMap_clear(self);
}
