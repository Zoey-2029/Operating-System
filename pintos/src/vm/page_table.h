#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <list.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef int off_t;

enum page_status 
{
  FROM_FILESYS,     /* need to read from file system */
  ALL_ZEROS         /* all zero page */
};

struct sup_page_table_entry
{
  void *user_vaddr;
  uint64_t access_time;
  bool dirty;
  bool accessed;
  bool read_only;
  struct list_elem elem;
  enum page_status status;

  /* mmap file */
  struct file *file;
  off_t file_offset;
  uint32_t read_bytes;
  uint32_t zero_bytes;
};

struct sup_page_table_entry *install_page_supplemental (void *upage);
struct sup_page_table_entry *find_in_table (void *upage);
