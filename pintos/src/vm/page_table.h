#include "kernel/hash.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum page_status
{
  DEFAULT,
  FILE,
  SWAP,
  MMAP
};

struct sup_page_table_entry
{
  void *user_vaddr;               /* user virtual address*/
  enum page_status source;        /* source of page*/
  bool writable;                
  struct frame_table_entry *fte;  /* corresponding frame */
  struct hash_elem elem;          /* hash element for page_table*/

  /* locate swap */
  bool pinned;                   
  size_t swap_index;

  /* locate mmap file */
  struct file *file;              /* a file to be mapped */
  int file_offset;                /* offset in the file*/
  uint32_t read_bytes;            /* bytes in the page mapped 
                                     by the file */
  uint32_t zero_bytes;            /* bytes to align page size*/
};

struct sup_page_table_entry *install_page_supplemental (void *);
struct sup_page_table_entry *find_in_table (void *);
unsigned page_hash_func (const struct hash_elem *, void *);
bool page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                     void *aux);
