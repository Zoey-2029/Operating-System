#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include <list.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FILE  1
#define STACK 2
#define SWAP  3
#define MMAP  4

struct sup_page_table_entry
{
  void *user_vaddr;
  uint8_t source;
  //uint64_t access_time;
  //bool dirty;
  //bool accessed;
  struct file *file;
  size_t offset;
  size_t read_bytes;
  size_t zero_bytes;

  bool loaded;
  bool read_only;
  struct list_elem elem;
};

bool install_page_supplemental (void *upage);
struct sup_page_table_entry *find_in_table (void *upage);

struct sup_page_table_entry * allocate_page_from_file(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable);
        
bool load_page_from_file(struct sup_page_table_entry* spte);
bool load_page_from_stack(struct sup_page_table_entry* spte);
bool load_page_from_swap(struct sup_page_table_entry* spte);
bool load_page_from_mmap(struct sup_page_table_entry* spte);

#endif /* vm/page_table.h */