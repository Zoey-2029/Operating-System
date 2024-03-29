#include "filesys/cache.h"
#include "devices/block.h"
#include "devices/timer.h"
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
static struct list read_ahead_list; /* List of entries to read ahead. */
static int cache_size = 0;
static bool evict_cache (void);          /* LRU eviction for cache. */
static struct lock cache_lock;           /* Lock for cache table. */
static struct lock read_ahead_lock;      /* Lock for read ahead. */
static struct semaphore read_ahead_sema; /* Sema for read ahead. */

/* Initialize cache. */
void
cache_init ()
{
  list_init (&cache);
  list_init (&read_ahead_list);
  lock_init (&cache_lock);
  sema_init (&read_ahead_sema, 0);
  lock_init (&read_ahead_lock);
  thread_create ("write_behind", PRI_DEFAULT, write_behind, NULL);
  thread_create ("read_ahead", PRI_DEFAULT, read_ahead, NULL);
}

struct cache_entry *
find_in_cache (block_sector_t sector)
{
  struct list_elem *e;

  for (e = list_begin (&cache); e != list_end (&cache); e = list_next (e))
    {
      struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
      if (sector == ce->sector)
        return ce;
    }

  return NULL;
}

struct cache_entry *
insert_cache (block_sector_t sector)
{
  struct cache_entry *ce;
  ce = calloc (1, sizeof *ce);
  if (!ce)
    {
      return NULL;
    }
  /* Evict cache if full. */
  if (cache_size == CACHE_MAX_SIZE)
    {
      bool success = evict_cache ();
      if (!success)
        {
          return NULL;
        }
      cache_size -= 1;
    }
  ce->sector = sector;
  sema_init (&ce->sema, 0);
  list_push_back (&cache, &ce->elem);
  cache_size += 1;
  return ce;
}

static bool
evict_cache ()
{

  while (!list_empty (&cache))
    {
      struct list_elem *e = list_pop_front (&cache);
      struct cache_entry *ce = list_entry (e, struct cache_entry, elem);

      if (ce->accessed)
        {
          ce->accessed = false;
          list_push_back (&cache, &ce->elem);
        }
      else if (ce->loaded)
        {
          block_write (fs_device, ce->sector, ce->data);
          free (ce);
          return true;
        }
    }
  return false;
}

/* Write to disk when finished. */
void
destroy_cache ()
{

  lock_acquire_cache ();
  struct list_elem *e;
  while (!list_empty (&cache))
    {
      e = list_pop_front (&cache);
      struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
      if (ce->dirty)
        block_write (fs_device, ce->sector, ce->data);
      free (ce);
    }
  lock_release_cache ();
}

void
block_read_cache (block_sector_t sector_idx, void *buffer, int sector_ofs,
                  int chunk_size, int bytes_read, bool ahead)
{
  lock_acquire_cache ();
  struct cache_entry *ce = find_in_cache (sector_idx);

  if (ce)
    {
      /* If not loaded, wait for the entry to be loaded. */
      if (!ce->loaded)
        {
          lock_release_cache ();
          sema_down (&ce->sema);
          lock_acquire_cache ();
        }
    }
  else
    {
      ce = insert_cache (sector_idx);
      block_read (fs_device, sector_idx, ce->data);
    }
  if (buffer)
    memcpy (buffer + bytes_read, ce->data + sector_ofs, chunk_size);
  ce->loaded = true;
  ce->accessed = true;

  if (ahead && sector_idx + 1 < block_size (fs_device))
    {
      ce = find_in_cache (sector_idx + 1);
      if (ce)
        {
          lock_release_cache ();
          return;
        }
      else
        {
          ce = insert_cache (sector_idx + 1);
          lock_acquire (&read_ahead_lock);
          list_push_back (&read_ahead_list, &ce->read_elem);
          lock_release (&read_ahead_lock);
          lock_release_cache ();
          sema_up (&read_ahead_sema);
        }
    }
  else
    {
      lock_release_cache ();
    }
}

void
block_write_cache (block_sector_t sector_idx, const void *buffer,
                   int sector_ofs, int chunk_size, int sector_left UNUSED,
                   int bytes_written)
{
  lock_acquire_cache ();
  struct cache_entry *ce = find_in_cache (sector_idx);
  if (ce && !ce->loaded)
    {
      lock_release_cache ();
      sema_down (&ce->sema);
      lock_acquire_cache ();
    }
  if (ce == NULL)
    {
      ce = insert_cache (sector_idx);
    }
  memcpy (ce->data + sector_ofs, buffer + bytes_written, chunk_size);
  ce->dirty = true;
  ce->accessed = true;
  ce->loaded = true;
  lock_release_cache ();
}

void
write_behind (void *aux UNUSED)
{
  struct list_elem *e;
  while (true)
    {
      lock_acquire_cache ();
      for (e = list_begin (&cache); e != list_end (&cache);)
        {
          struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
          if (ce->dirty)
            {
              struct list_elem *tmp = e;
              e = list_next (e);
              block_write (fs_device, ce->sector, ce->data);
              list_remove (tmp);
              free (ce);
              cache_size -= 1;
            }
          else
            {
              e = list_next (e);
            }
        }
      lock_release_cache ();
      timer_sleep (10);
    }
}

void
read_ahead (void *aux UNUSED)
{
  struct list_elem *e;
  while (true)
    {
      sema_down (&read_ahead_sema);
      lock_acquire_cache ();
      lock_acquire (&read_ahead_lock);
      while (!list_empty (&read_ahead_list))
        {
          e = list_pop_front (&read_ahead_list);
          struct cache_entry *ce
              = list_entry (e, struct cache_entry, read_elem);
          block_read (fs_device, ce->sector, ce->data);
          ce->loaded = true;
          sema_up (&ce->sema);
        }
      lock_release (&read_ahead_lock);
      lock_release_cache ();
    }
}

void
lock_acquire_cache (void)
{
  lock_acquire (&cache_lock);
}

void
lock_release_cache (void)
{
  lock_release (&cache_lock);
}
