#include "threads/synch.h"
#include <devices/block.h>
#include <list.h>

struct cache_entry
{
  block_sector_t sector;
  struct list_elem elem;      /* list element for frame table */
  struct list_elem read_elem; /* list element for frame table */
  char data[BLOCK_SECTOR_SIZE];
  bool dirty;
  bool loaded;
  bool accessed;
  struct semaphore sema;
};

struct cache_entry *find_in_cache (block_sector_t sector);
struct cache_entry *insert_cache (block_sector_t sector);
void cache_init (void);
void destroy_cache (void);
void block_read_cache (block_sector_t sector_idx, void *buffer, int sector_ofs,
                       int chunk_size, int bytes_read, bool ahead);
void block_write_cache (block_sector_t sector_idx, const void *buffer,
                        int sector_ofs, int chunk_size, int sector_left,
                        int bytes_written);

void write_behind (void *);
void read_ahead (void *);

void lock_acquire_cache (void);
void lock_release_cache (void);
