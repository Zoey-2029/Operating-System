#include "vm/frame_table.h"
#include "vm/swap.h"
/* A list of frame_table_entry as the frame table. */
static struct list frame_table;
static struct lock f_lock;

void
frame_table_init ()
{
  lock_init (&f_lock);
  list_init (&frame_table);
}

void *
allocate_frame ()
{
  lock_acquire (&f_lock);
  struct thread *cur = thread_current ();
  void *kpage = (void *)palloc_get_page (PAL_USER | PAL_ZERO);
  /*If cannot get a page, evict one*/
  if (kpage == NULL)
    {
      // printf("need to evict\n");
      kpage = evict_frame ();
    }

  struct frame_table_entry *frame_table_entry
      = calloc (1, sizeof *frame_table_entry);
  frame_table_entry->owner = cur;
  frame_table_entry->frame = kpage;
  // frame_table_entry->spte = NULL;
  list_push_back (&frame_table, &frame_table_entry->elem);
  // printf("frame %p\n", find_in_frame_table(kpage));
  lock_release (&f_lock);

  return kpage;
}

void
free_frame (void *kpage)
{
  lock_acquire (&f_lock);
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

  lock_release (&f_lock);
}

struct frame_table_entry *
find_in_frame_table (void *kpage)
{

  lock_acquire (&f_lock);
  struct list_elem *e;
  struct list *l = &frame_table;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct frame_table_entry *f
          = list_entry (e, struct frame_table_entry, elem);
      if (f->frame == kpage)
        {
          lock_release (&f_lock);
          return f;
        }
    }
  lock_release (&f_lock);
  return NULL;
}

void *
evict_frame (void)
{
  /*
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame_table_entry *entry
      = list_entry (e, struct frame_table_entry, elem);
    }
  */
  struct thread *curr = thread_current ();
  while (!list_empty (&frame_table))
    {
      struct list_elem *e = list_pop_front (&frame_table);
      struct frame_table_entry *fte
          = list_entry (e, struct frame_table_entry, elem);
      // printf ("aaaa %p %p\n", fte, fte->spte);
      if (pagedir_is_accessed (curr->pagedir, fte->spte->user_vaddr))
        {
          // printf ("bbbb\n");
          pagedir_set_accessed (curr->pagedir, fte->spte->user_vaddr, false);
          list_push_back (&frame_table, &fte->elem);
        }
      else
        {
          //  printf ("found!\n");
          pagedir_clear_page (fte->owner->pagedir, fte->spte->user_vaddr);
          void* kpage = fte->frame;
          // printf ("kpage %p\n", kpage);
          size_t index = write_to_block (fte->frame);
          // printf ("kpage %p\n", kpage);
          fte->spte->source = SWAP;
          fte->spte->swap_index = index;
          // list_remove (&fte->elem);
          free (fte);
          return kpage;
        }
    }
  return NULL;
}