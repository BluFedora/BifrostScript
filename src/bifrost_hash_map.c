/*!
 * @file bifrost_hash_map.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *
 * @copyright Copyright (c) 2019
 */
#include "bifrost/bifrost_vm.h"  // VM Turn off GC

#include "bifrost_vm_gc.h"     // Allocation Functions
#include "bifrost_vm_obj.h"    // bf_flex_array_member
#include "bifrost_vm_value.h"  // bfVMValue_fromNull

struct bfHashNode
{
  const BifrostObjStr* key;
  BifrostValue         value;
  bfHashNode*          next;
};

static bfHashNode* bfHashMap_newNode(struct BifrostVM* vm)
{
  vm->gc_is_running      = 1;
  bfHashNode* const node = bfGC_AllocMemory(vm, NULL, 0u, sizeof(bfHashNode));
  vm->gc_is_running      = 0;

  return node;
}

static unsigned bfHashMap_defaultHash(const void* key)
{
  const BifrostObjStr* str = (const BifrostObjStr*)key;
  return str->hash;
}

static int bfHashMap_defaultCmp(const BifrostObjStr* const lhs, const BifrostObjStr* const rhs)
{
  return StringCmp_Cmp(StringCmp_FromStr(lhs), StringCmp_FromStr(rhs));
}

static bfHashNode* bfHashMap_getNode(const BifrostHashMap* self, const StringCmp key, unsigned hash)
{
  bfHashNode* cursor = self->buckets[hash];

  while (cursor)
  {
    if (StringCmp_Cmp(key, StringCmp_FromStr(cursor->key)))
    {
      break;
    }

    cursor = cursor->next;
  }

  return cursor;
}

static void bfHashMap_deleteNode(const BifrostHashMap* self, bfHashNode* node)
{
  bfGC_AllocMemory(self->vm, node, sizeof(bfHashNode), 0u);
}

void bfHashMap_ctor(BifrostHashMap* self, struct BifrostVM* vm)
{
  self->vm          = vm;
  self->num_buckets = bfCArraySize(self->buckets);

  for (unsigned i = 0; i < self->num_buckets; ++i)
  {
    self->buckets[i] = NULL;
  }
}

void bfHashMap_set(BifrostHashMap* self, const BifrostObjStr* key, const BifrostValue value)
{
  const unsigned hash = bfHashMap_defaultHash(key) % self->num_buckets;
  bfHashNode*    node = bfHashMap_getNode(self, StringCmp_FromStr(key), hash);

  if (!node)
  {
    node                = bfHashMap_newNode(self->vm);
    node->next          = self->buckets[hash];
    self->buckets[hash] = node;
  }

  node->key   = key;
  node->value = value;
}

#if 0
static int bfHashMap_has(const BifrostHashMap* self, const void* key)
{
  const unsigned hash = bfHashMap_defaultHash(key) % self->num_buckets;
  return bfHashMap_getNode(self, key, hash) != NULL;
}
#endif

BifrostValue bfHashMap_get(BifrostHashMap* self, const StringCmp key)
{
  const uint32_t hash = key.hash % self->num_buckets;
  bfHashNode*    node = bfHashMap_getNode(self, key, hash);

  return node ? node->value : bfVMValue_fromNull();
}

int bfHashMap_remove(BifrostHashMap* self, const StringCmp key)
{
  const unsigned hash   = key.hash % self->num_buckets;
  bfHashNode*    cursor = self->buckets[hash];
  bfHashNode*    prev   = NULL;

  while (cursor)
  {
    const StringCmp cursor_str = StringCmp_FromStr(cursor->key);

    if (StringCmp_Cmp(key, cursor_str))
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
  bfHashMapIter begin_it = {NULL, 0, -1, NULL};

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
