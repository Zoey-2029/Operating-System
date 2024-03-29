#include "vm/frame_table.h"
#include "vm/swap.h"
/* A list of frame_table_entry as the frame table. */
static struct list frame_table;
/* only one thread is allowed to interact with frame table */
static struct lock f_lock;

void
frame_table_init ()
{
  lock_init (&f_lock);
  list_init (&frame_table);
}

void
lock_acquire_vm ()
{
  lock_acquire (&f_lock);
}

void
lock_release_vm ()
{
  lock_release (&f_lock);
}

/* allocate a frame in frame table, no internal sunchronization
needed since we external synchronization is implemented */
void *
allocate_frame ()
{
  struct thread *cur = thread_current ();
  void *kpage = (void *)palloc_get_page (PAL_USER | PAL_ZERO);

  /*evict one frame when the frame is full*/
  if (kpage == NULL)
    kpage = evict_frame ();
  
  /* panic the kernel if no frame can be evicted */
  if (kpage == NULL)
    PANIC("Swap is full, failed to allocate a frame \n");

  struct frame_table_entry *frame_table_entry
      = calloc (1, sizeof *frame_table_entry);
  if (!frame_table_entry)
    return NULL;
  
  frame_table_entry->owner = cur;
  frame_table_entry->frame = kpage;
  frame_table_entry->spte = NULL;
  list_push_back (&frame_table, &frame_table_entry->elem);

  return kpage;
}

/* free a frame from frame table */
void
free_frame (void *kpage)
{
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame_table_entry *entry
          = list_entry (e, struct frame_table_entry, elem);
      if (entry->frame == kpage)
        {
          palloc_free_page (kpage);
          list_remove (e);
          free (entry);
          break;
        }
    }
}

struct frame_table_entry *
find_in_frame_table (void *kpage)
{
  struct list_elem *e;
  struct list *l = &frame_table;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct frame_table_entry *f
          = list_entry (e, struct frame_table_entry, elem);
      if (f->frame == kpage)
          return f;
    }
  return NULL;
}

void *
evict_frame (void)
{
  struct thread *curr = thread_current ();
  while (!list_empty (&frame_table))
    {
      struct list_elem *e = list_pop_front (&frame_table);
      struct frame_table_entry *fte
          = list_entry (e, struct frame_table_entry, elem);

      /* Skip those not used with spte. */
      if (!fte->spte)
        {
          list_push_back (&frame_table, &fte->elem);
        }
      else if (fte->spte->pinned
               || pagedir_is_accessed (curr->pagedir, fte->spte->user_vaddr))
        {
          pagedir_set_accessed (curr->pagedir, fte->spte->user_vaddr, false);
          list_push_back (&frame_table, &fte->elem);
        }
      else
        {
          pagedir_clear_page (fte->owner->pagedir, fte->spte->user_vaddr);

          if (fte->spte->source == FILE && fte->spte->writable && 
              pagedir_is_accessed (curr->pagedir, fte->spte->user_vaddr))
              file_write_at (fte->spte->file, fte->spte->user_vaddr, 
                            fte->spte->read_bytes, fte->spte->file_offset);

          void *kpage = fte->frame;
          size_t index = write_to_block (fte->frame);
          fte->spte->source = SWAP;
          fte->spte->swap_index = index;
          fte->spte->fte = NULL;
          list_remove (&fte->elem);
          free (fte);
          return kpage;
        }
    }
  return NULL;
}


/* load page from excutable file */
bool
load_page_from_file (struct sup_page_table_entry *spte, void *kpage)
{
  file_seek (spte->file, spte->file_offset);
  if (file_read (spte->file, kpage, spte->read_bytes) != 
      (int)spte->read_bytes)
    return false;

  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);
  memset (kpage + spte->read_bytes, 0, spte->zero_bytes);

  return true;
}

/* load page from swap */
bool
load_page_from_swap (struct sup_page_table_entry *spte, void *kpage)
{
  read_from_block (kpage, spte->swap_index);
  return true;
}

/* load pages from swap, excutable file or file to be mapped*/
bool
load_page (struct sup_page_table_entry *spte)
{
  if (!spte)
    return false;

  void *upage = spte->user_vaddr;
  spte->pinned = true;
  void *kpage = allocate_frame ();
  if (!kpage)
    return false;

  bool writable = true;
  if (spte->source == FILE)
    writable = spte->writable;

  if (!install_page (upage, kpage, writable))
    {
      free_frame (kpage);
      return false;
    }

  bool success = false;
  switch (spte->source)
    {
    case FILE:
      success = load_page_from_file (spte, kpage);
      break;
    case SWAP:
      success = load_page_from_swap (spte, kpage);
      break;
    default:
      break;
    }

  if (!success)
    free_frame (kpage);

  spte->pinned = false;
  return success;
}

void
free_single_page (struct hash_elem *e, void *aux UNUSED)
{
  struct sup_page_table_entry *spte
      = hash_entry (e, struct sup_page_table_entry, elem);

  /* write file back to disk only when the bits are dirty */
  if (spte->source == FILE && spte->fte && spte->writable &&
      pagedir_is_dirty (thread_current ()->pagedir, spte->user_vaddr))
    file_write_at (spte->file, spte->user_vaddr, spte->read_bytes,
                   spte->file_offset);

  if (spte->fte)
    {
      palloc_free_page (spte->fte->frame);
      list_remove (&spte->fte->elem);
      free (spte->fte);
    }

  hash_delete (&thread_current ()->page_table, &spte->elem);
  pagedir_clear_page (thread_current ()->pagedir, spte->user_vaddr);
  free (spte);
}

void
free_page_table ()
{
  hash_destroy (&thread_current ()->page_table, free_single_page);
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with allocate_frame ();
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */

bool
install_page (void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  struct thread *t = thread_current ();
  if (pagedir_get_page (t->pagedir, upage) != NULL
      || !pagedir_set_page (t->pagedir, upage, kpage, writable))
    return false;

  struct frame_table_entry *entry = find_in_frame_table (kpage);
  if (entry == NULL)
    return false;

  struct sup_page_table_entry *spte = install_page_supplemental (upage);
  entry->spte = spte;
  spte->fte = entry;
  return true;
}