#include "filesys/cache.h"
#include "devices/block.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdio.h>
#include <string.h>

#define CACHE_MAX_SIZE 64
static struct list cache;
static int cache_size = 0;
static bool evict_cache (void);
static struct lock cache_lock;

void
cache_init ()
{
  list_init (&cache);
  lock_init (&cache_lock);
}

struct cache_entry *
find_in_cache (block_sector_t sector)
{
  //   printf ("asdad %p\n", &cache);
  struct list_elem *e;

  for (e = list_begin (&cache); e != list_end (&cache); e = list_next (e))
    {
      //  printf("asdasd\n");
      struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
      if (sector == ce->sector)
        return ce;
    }

  return NULL;
}

struct cache_entry *
insert_cache (block_sector_t sector)
{
//   lock_acquire (&cache_lock);
  struct cache_entry *ce;
  ce = calloc (1, sizeof *ce);
  if (!ce)
    {
    //   lock_release (&cache_lock);
      return NULL;
    }
  if (cache_size == CACHE_MAX_SIZE)
    {
      bool success = evict_cache ();
      if (!success)
        {
        //   lock_release (&cache_lock);
          return NULL;
        }
      cache_size -= 1;
    }
  ce->sector = sector;
  //   printf ("sector%d\n", sector);
  list_push_back (&cache, &ce->elem);
  cache_size += 1;
//   lock_release (&cache_lock);
  return ce;
}

static bool
evict_cache ()
{
  struct list_elem *e;
//   printf ("evict\n");
  e = list_pop_front (&cache);
  struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
  block_write (fs_device, ce->sector, ce->data);
  free (ce);
  return true;
}

void
destroy_cache ()
{
//   lock_acquire (&cache_lock);
  struct list_elem *e;
//   printf ("destroying\n");
  while (!list_empty (&cache))
    {
      e = list_pop_front (&cache);
      struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
      // printf("%d\n",  ce->sector);
      if (ce->dirty)
        block_write (fs_device, ce->sector, ce->data);
      free (ce);
    }
//   lock_release (&cache_lock);
}

void
block_read_cache (block_sector_t sector_idx, void *buffer, int sector_ofs,
                  int chunk_size, int bytes_read)
{
  //   printf("asdasd\n");

  struct cache_entry *ce = find_in_cache (sector_idx);

  if (ce)
    {
      //   printf ("found in cache!\n");
    }
  else
    {
      // printf ("not found in cache!\n");
      ce = insert_cache (sector_idx);
      //   printf ("ce inserted \n");
      block_read (fs_device, sector_idx, ce->data);
    }
  if (buffer)
    memcpy (buffer + bytes_read, ce->data + sector_ofs, chunk_size);
}

void
block_write_cache (block_sector_t sector_idx, const void *buffer,
                   int sector_ofs, int chunk_size, int sector_left UNUSED,
                   int bytes_written)
{
  struct cache_entry *ce = find_in_cache (sector_idx);
  if (ce == NULL)
    {
      ce = insert_cache (sector_idx);
    }
  memcpy (ce->data + sector_ofs, buffer + bytes_written, chunk_size);
  ce->dirty = true;
  //   block_write (fs_device, sector_idx, ce->data);
}