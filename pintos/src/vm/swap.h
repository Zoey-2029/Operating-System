#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include <debug.h>
#include <list.h>
#include <stdint.h>

void swap_init (void);
size_t write_to_block (uint8_t *);
void read_from_block (uint8_t *frame, int index);
