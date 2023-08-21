#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries)
{
  // Check if valid
  if (num_entries < 2 || num_entries > 4096 || cache_enabled())
  {
    return -1;
  }

  // Allocate memory for cache
  cache = calloc(num_entries, sizeof(cache_entry_t));

  // Store size of cache
  cache_size = num_entries;

  return 1;
}

int cache_destroy(void)
{
  // Check if valid
  if (!cache_enabled())
  {
    return -1;
  }

  // Deallocate
  free(cache);

  // Reset
  cache = NULL;
  cache_size = 0;
  clock = 0;
  num_queries = 0;
  num_hits = 0;

  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf)
{
  // Check if valid
  if (!cache_enabled() || buf == NULL || disk_num < 0 || block_num < 0 || disk_num > 15 || block_num > 255)
  {
    return -1;
  }

  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      // Copy the block data to buffer
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);

      // Update the access time of the block
      cache[i].access_time = ++clock;

      // Increment hit count
      num_hits++;

      return 1;
    }
  }

  // Increment query count & return miss
  num_queries++;
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf)
{
  // Iterate through cache entries to find matching disk_num and block_num
  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      // Update the block content with new data
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);

      // Update the access_time
      cache[i].access_time = clock++;
      return;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf)
{
  // Check for invalid parameters or if cache is not enabled
  if (!cache_enabled() || buf == NULL || disk_num < 0 || block_num < 0 || disk_num > 15 || block_num > 255)
  {
    return -1;
  }

  // Look for LRU block
  int lru = 0;
  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
      return -1;

    if (cache[i].access_time < cache[lru].access_time)
      lru = i;
  }

  // Replace the LRU block with the new block
  cache[lru].valid = true;
  cache[lru].disk_num = disk_num;
  cache[lru].block_num = block_num;
  memcpy(cache[lru].block, buf, JBOD_BLOCK_SIZE);
  cache[lru].access_time = 1;

  return 1;
}

bool cache_enabled(void)
{
  if (cache != NULL && cache_size > 0)
  {
    return true;
  }

  return false;
}

void cache_print_hit_rate(void)
{
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float)num_hits / num_queries);
}
