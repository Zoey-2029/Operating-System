#include "vm/frame_table.h"

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

  if (kpage == NULL)
    return NULL;

  struct frame_table_entry *frame_table_entry
      = calloc (1, sizeof *frame_table_entry);
  frame_table_entry->owner = cur;
  frame_table_entry->frame = kpage;
  list_push_back (&frame_table, &frame_table_entry->elem);
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