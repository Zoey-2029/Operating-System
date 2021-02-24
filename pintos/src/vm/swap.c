#include "vm/swap.h"
#include "devices/block.h"
#include "userprog/syscall.h"
#include <bitmap.h>

// /* Make the swap block global. */
// static struct block *global_swap_block;
static struct block *global_swap_block;
static struct bitmap *swap_table;
static struct lock swap_lock;

#define PAGE_BLOCKS  8

void
swap_init ()
{
  /* Get the block device when we initialize our swap code. */
  global_swap_block = block_get_role (BLOCK_SWAP);
  swap_table = bitmap_create (block_size (global_swap_block) / PAGE_BLOCKS);
  lock_init (&swap_lock);
}

size_t
write_to_block (uint8_t *frame)
{
  size_t bit = bitmap_scan_and_flip (swap_table, 0, 1, false);
 
  if (bit == BITMAP_ERROR)
      sys_exit (-1);

  size_t index = bit * PAGE_BLOCKS;
  for (size_t i = 0; i < PAGE_BLOCKS; i++)
      block_write (global_swap_block, index + i,
                   frame + (i * BLOCK_SECTOR_SIZE));

  return index;
}

void
read_from_block (uint8_t *frame, int index)
{
  for (int i = 0; i < PAGE_BLOCKS; i++)
      block_read (global_swap_block, index + i,
                  frame + (i * BLOCK_SECTOR_SIZE));
                  
  bitmap_flip (swap_table, index / PAGE_BLOCKS);
}
