#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <list.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FILE 1
#define STACK 2
#define SWAP 3
#define MMAP 4

typedef int off_t;

enum page_status 
{
  FROM_FILESYS,     /* need to read from file system */
  ALL_ZEROS         /* all zero page */
};

struct sup_page_table_entry
{
  void *user_vaddr;
  uint8_t source;
  // uint64_t access_time;
  // bool dirty;
  // bool accessed;
  bool read_only;
  struct list_elem elem;
<<<<<<< HEAD
  enum page_status status;

  /* mmap file */
  struct file *file;
  off_t file_offset;
  uint32_t read_bytes;
  uint32_t zero_bytes;
=======
  struct frame_table_entry *fte;
  size_t swap_index;
>>>>>>> origin/page
};

struct sup_page_table_entry *install_page_supplemental (void *upage);
struct sup_page_table_entry *find_in_table (void *upage);
<<<<<<< HEAD
=======

bool load_page_from_file (struct sup_page_table_entry *entry);
bool load_page_from_stack (struct sup_page_table_entry *entry);
bool load_page_from_swap (struct sup_page_table_entry *entry);
bool load_page_from_mmap (struct sup_page_table_entry *entry);
>>>>>>> origin/page
