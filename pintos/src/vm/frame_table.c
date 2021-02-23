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
void *
allocate_frame ()
{
  lock_acquire (&f_lock);
  struct thread *cur = thread_current ();
  void *kpage = (void *)palloc_get_page (PAL_USER | PAL_ZERO);
  /*If cannot get a page, evict one*/
  if (kpage == NULL)
    {

      kpage = evict_frame ();
      // kpage = (void *)palloc_get_page (PAL_USER | PAL_ZERO);
      // printf("need to evict %p\n", kpage);
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

  // lock_acquire (&f_lock);
  struct list_elem *e;
  struct list *l = &frame_table;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct frame_table_entry *f
          = list_entry (e, struct frame_table_entry, elem);
      if (f->frame == kpage)
        {
          // lock_release (&f_lock);
          return f;
        }
    }
  // lock_release (&f_lock);
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
      // printf ("aaaa %p %p\n", fte, fte->spte);

      /* Skip those not used with spte. */
      if (!fte->spte)
        {
          list_push_back (&frame_table, &fte->elem);
        }
      else if (fte->spte->pinned
               || pagedir_is_accessed (curr->pagedir, fte->spte->user_vaddr))
        {
          // printf ("bbbb\n");
          pagedir_set_accessed (curr->pagedir, fte->spte->user_vaddr, false);
          list_push_back (&frame_table, &fte->elem);
        }
      else
        {
          //  printf ("found!\n");
          pagedir_clear_page (fte->owner->pagedir, fte->spte->user_vaddr);
          void *kpage = fte->frame;
          // printf ("evicting %p %d\n", fte->spte->user_vaddr,
          //         fte->spte->source);
          size_t index = write_to_block (fte->frame);
          // printf ("kpage %p\n", kpage);
          fte->spte->source = SWAP;
          fte->spte->swap_index = index;
          fte->spte->fte = NULL;
          // palloc_free_page (kpage);
          list_remove (&fte->elem);
          free (fte);
          return kpage;
        }
    }
  return NULL;
}

bool
load_page_from_file (struct sup_page_table_entry *spte, void *kpage)
{
  bool held = filesys_lock_held_by_current_thread ();
  if (!held)
    lock_acquire_filesys ();

  file_seek (spte->file, spte->file_offset);
  /* Load this page. */
  if (file_read (spte->file, kpage, spte->read_bytes) != (int)spte->read_bytes)
    {
      // printf ("failed\n");
      if (!held)
        lock_release_filesys ();
      return false;
    }
  if (!held)
    lock_release_filesys ();

  lock_acquire (&f_lock);
  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);
  memset (kpage + spte->read_bytes, 0, spte->zero_bytes);
  lock_release (&f_lock);

  return true;
}

bool
load_page_from_stack (struct sup_page_table_entry *entry UNUSED)
{
  return false;
}

bool
load_page_from_swap (struct sup_page_table_entry *spte, void *kpage)
{
  lock_acquire (&f_lock);
  read_from_block (kpage, spte->swap_index);
  lock_release (&f_lock);
  return true;
}

bool
load_page_from_mmap (struct sup_page_table_entry *spte, void *kpage)
{
  bool held = filesys_lock_held_by_current_thread ();
  if (!held)
    lock_acquire_filesys ();
  file_seek (spte->file, spte->file_offset);

  // read bytes from the file
  int n_read = file_read (spte->file, kpage, spte->read_bytes);
  if (n_read != (int)spte->read_bytes)
    {
      if (!held)
        lock_release_filesys ();
      return false;
    }
  if (!held)
    lock_release_filesys ();
  // the remaining bytea are zero
  lock_acquire (&f_lock);
  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);
  memset (kpage + n_read, 0, spte->zero_bytes);
  // lock_release_filesys ();
  lock_release (&f_lock);
  return true;
}

bool
load_page (struct sup_page_table_entry *spte UNUSED)
{
  void *upage = spte->user_vaddr;
  void *kpage = allocate_frame ();
  bool writable = true;
  if (!kpage)
    return false;
  if (spte->source == FILE)
    {
      writable = spte->writable;
    }
  if (!install_page (upage, kpage, writable)) {
    printf("faild\n");
    free_frame(kpage);
    return false;
  }
  // printf("load page %p from %d\n", spte->user_vaddr, spte->source);
  switch (spte->source)
    {
    case FILE:
      return load_page_from_file (spte, kpage);
    case MMAP:
      return load_page_from_mmap (spte, kpage);
    case SWAP:
      return load_page_from_swap (spte, kpage);
    default:
      return true;
    }
}

void
free_single_page (struct sup_page_table_entry *spte)
{

  /* write file back to disk is  */
  if (spte->source == MMAP && spte->fte
      && pagedir_is_dirty (thread_current ()->pagedir, spte->user_vaddr))
    {
      // printf("test dirty %x \n", spte->fte->frame);
      file_write_at (spte->file, spte->user_vaddr, spte->read_bytes,
                     spte->file_offset);
    }

  if (spte->fte)
    {
      // printf("source %d\n", spte->source);
      free_frame (spte->fte->frame);
    }

  list_remove (&spte->elem);
  pagedir_clear_page (thread_current ()->pagedir, spte->user_vaddr);
  free (spte);
}

void
free_page_table ()
{
  lock_acquire_filesys ();
  struct list *l = &thread_current ()->page_table;
  struct list_elem *e;
  for (e = list_begin (l); e != list_end (l);)
    {
      struct sup_page_table_entry *entry
          = list_entry (e, struct sup_page_table_entry, elem);
      struct list_elem *next = list_next (e);
      e = next;
      free_single_page (entry);
    }
  lock_release_filesys ();
}

bool
install_page (void *upage, void *kpage, bool writable)
{
  // lock_acquire (&f_lock);
  // printf("acq\n");
  struct thread *t = thread_current ();
  // printf("%p upage\n", upage);
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  if (pagedir_get_page (t->pagedir, upage) != NULL
      || !pagedir_set_page (t->pagedir, upage, kpage, writable))
    {
      // printf("%d\n", pagedir_get_page (t->pagedir, upage) != NULL);
      // lock_release (&f_lock);
      printf("pagedir failed\n");
      return false;
    }

  struct frame_table_entry *entry = find_in_frame_table (kpage);
  struct sup_page_table_entry *spte = install_page_supplemental (upage);
  if (entry == NULL || spte == NULL)
    {
      // printf("kpage %p, %d %d\n", kpage, entry == NULL, spte == NULL);
      // lock_release (&f_lock);
      printf("some NULL\n");
      return false;
    }
  entry->spte = spte;
  spte->fte = entry;
  // printf("release\n");
  // lock_release (&f_lock);
  // printf("release\n");
  return true;
}