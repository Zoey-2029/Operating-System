#include "userprog/syscall.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "process.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "userprog/pagedir.h"
#include "vm/frame_table.h"
#include <console.h>
#include <string.h>
#include <syscall-nr.h>
#define MAX_WRITE_CHUNK 200

static void syscall_handler (struct intr_frame *);
static struct file_info *find_file_info (int fd);
static struct mmapped_file_entry *find_mmapped_file_info (mapid_t mapid);

/* Helper function(s). */
static void free_file_info (void);
static void free_child_processes_info (void);
static bool check_memory_validity (const void *, unsigned, void *);
// static void free_page_table (void);


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  if (!check_memory_validity (f->esp, 1 * sizeof (int *), NULL))
    sys_exit (-1);
  /* Number of args to check validity. */
  unsigned args = 0;
  switch (*(int *)f->esp)
    {
    case SYS_HALT:
      {
        args = 0;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        sys_halt ();
        break;
      }

    case SYS_EXIT:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int status = *((int *)f->esp + 1);
        sys_exit (status);
        break;
      }

    case SYS_EXEC:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        const char *cmd_line = (void *)(*((int *)f->esp + 1));
        if (!check_memory_validity (cmd_line, sizeof (char *), f->esp)
            || !check_memory_validity (cmd_line, strlen (cmd_line), f->esp))
          sys_exit (-1);
        f->eax = sys_exec (cmd_line);
        break;
      }

    case SYS_WAIT:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        pid_t pid = *((pid_t *)f->esp + 1);
        f->eax = sys_wait (pid);
        break;
      }

    case SYS_CREATE:
      {
        args = 2;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        const char *file = (void *)(*((int *)f->esp + 1));
        unsigned initial_size = *((unsigned *)f->esp + 2);
        if (!check_memory_validity (file, MAX_FILE_SIZE, f->esp)) {
          sys_exit (-1);
        }
        f->eax = sys_create (file, initial_size);
        break;
      }

    case SYS_REMOVE:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        const char *file = (void *)(*((int *)f->esp + 1));
        if (!check_memory_validity (file, MAX_FILE_SIZE, f->esp))
          sys_exit (-1);
        f->eax = sys_remove (file);
        break;
      }

    case SYS_OPEN:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        const char *file = (void *)(*((int *)f->esp + 1));
        if (!check_memory_validity (file, MAX_FILE_SIZE, f->esp))
          sys_exit (-1);
        f->eax = sys_open (file);
        break;
      }

    case SYS_FILESIZE:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        f->eax = sys_filesize (fd);
        break;
      }

    case SYS_READ:
      {
        args = 3;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);
        int fd = *((int *)f->esp + 1);
        void *buffer = (void *)(*((int *)f->esp + 2));
        unsigned size = *((unsigned *)f->esp + 3);
        if (buffer < (void *)f->eip)
          sys_exit (-1);
        if (!check_memory_validity (buffer, size, f->esp))
          sys_exit (-1);
        f->eax = sys_read (fd, buffer, size);
        break;
      }

    case SYS_WRITE:
      {
        args = 3;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        const void *buffer = (void *)(*((int *)f->esp + 2));
        unsigned size = *((unsigned *)f->esp + 3);
        if (!check_memory_validity (buffer, size, f->esp))
          sys_exit (-1);
        f->eax = sys_write (fd, buffer, size);
        break;
      }

    case SYS_SEEK:
      {
        args = 2;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        unsigned position = *((unsigned *)f->esp + 2);
        sys_seek (fd, position);
        break;
      }

    case SYS_TELL:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        f->eax = sys_tell (fd);
        break;
      }

    case SYS_CLOSE:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        sys_close (fd);
        break;
      }

    case SYS_MMAP:
     {
       args = 2;
       if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        void *addr = (void *)(*((int *)f->esp + 2));
        f->eax = sys_mmap(fd, addr);
        break;
     }
    
    case SYS_MUNMAP:
      {
        args= 1;
        if (!check_memory_validity ((int *)f->esp + 1, args * sizeof (int *),
                                    NULL))
          sys_exit (-1);
        mapid_t mapid = *((mapid_t *)f->esp + 1);
        sys_munmap(mapid);
        break;
      }

    default:
      {
        printf ("unimplemented system call \n");
        sys_exit (-1);
      }
    }
}

void
sys_halt ()
{
  shutdown_power_off ();
}

void
sys_exit (int status)
{
  struct thread_info *info = get_child_process (
      thread_current ()->tid, &thread_current ()->parent->child_processes);

  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  if (info)
    info->exit_status = status;

  /* Free allocated resources. */
  free_file_info ();
  free_child_processes_info ();
  free_page_table ();

  thread_exit ();
}

pid_t
sys_exec (const char *cmd_line)
{
 
  pid_t child_pid = process_execute (cmd_line);
   
  /* Wait for child process loading. */
  sema_down (&thread_current ()->sema_exec);
  /* Get child process from pid. */
  struct thread_info *child_process
      = get_child_process (child_pid, &thread_current ()->child_processes);

  if (child_process && child_process->load_status)
    return child_pid;
  else
    return TID_ERROR;
}

int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

bool
sys_create (const char *file, unsigned initial_size)
{

  lock_acquire_filesys ();
  bool success = filesys_create (file, initial_size);
  lock_release_filesys ();

  return success;
}

bool
sys_remove (const char *file)
{

  lock_acquire_filesys ();
  bool success = filesys_remove (file);
  lock_release_filesys ();

  return success;
}

int
sys_open (const char *file)
{

  lock_acquire_filesys ();
  struct file *f = filesys_open (file);
  struct file_info *info;

  if (!f)
    {
      lock_release_filesys ();
      return -1;
    }

  /* Use file_info struct to map file descriptors to files. */
  info = calloc (1, sizeof (*info));
  info->fd = thread_current ()->fd_count;
  thread_current ()->fd_count += 1;
  info->file = f;
  list_push_back (&thread_current ()->file_info_list, &info->elem);
  lock_release_filesys ();

  return info->fd;
}

int
sys_filesize (int fd)
{
  struct file_info *info = find_file_info (fd);
  lock_acquire_filesys ();
  if (!info)
    {
      lock_release_filesys ();
      return -1;
    }

  int size = file_length (info->file);
  lock_release_filesys ();

  return size;
}

int
sys_read (int fd, void *buffer, unsigned size)
{
  unsigned bytes_read = 0;

  if (fd == STDIN_FILENO)
    {
      while (bytes_read < size)
        ((char *)buffer)[bytes_read++] = input_getc ();
    }
  else
    {
      lock_acquire_filesys ();
      struct file_info *info = find_file_info (fd);

      if (!info)
        {
          lock_release_filesys ();
          return -1;
        }
      bytes_read = file_read (info->file, buffer, size);
      lock_release_filesys ();
    }

  return bytes_read;
}

int
sys_write (int fd, const void *buffer, unsigned size)
{

  /* Write to console and break the buffer to 200-byte chunks. */
  if (fd == STDOUT_FILENO)
    {
      size_t bytes_written = 0;
      while (bytes_written + MAX_WRITE_CHUNK < size)
        {
          putbuf ((char *)(buffer + bytes_written), MAX_WRITE_CHUNK);
          bytes_written += MAX_WRITE_CHUNK;
        }

      putbuf ((char *)(buffer + bytes_written),
              (size_t) (size - bytes_written));

      return size;
    }
  lock_acquire_filesys ();
  struct file_info *info = find_file_info (fd);

  if (!info)
    {
      lock_release_filesys ();
      return -1;
    }
  off_t bytes_written = file_write (info->file, buffer, size);
  lock_release_filesys ();
  return bytes_written;
}

void
sys_seek (int fd, unsigned position)
{
  lock_acquire_filesys ();
  struct file_info *info = find_file_info (fd);
  if (info)
    file_seek (info->file, position);
  lock_release_filesys ();
}

unsigned
sys_tell (int fd)
{
  lock_acquire_filesys ();
  struct file_info *info = find_file_info (fd);
  if (!info)
    {
      lock_release_filesys ();
      return -1;
    }
  off_t position = file_tell (info->file);
  lock_release_filesys ();

  return position;
}

void
sys_close (int fd)
{
  lock_acquire_filesys ();
  struct file_info *info = find_file_info (fd);
  if (info)
    {
      file_close (info->file);
      list_remove (&info->elem);
      free (info);
    }
  lock_release_filesys ();
}

/* Check if the user pointer is valid.
   virtual_addr: the first pointer.
   size: number of pointers to check validity. */
static bool
check_memory_validity (const void *virtual_addr, unsigned size, void *esp)
{
  // at least check one pointer
  
  if (size == 0)
    size = 1;
  for (unsigned i = 0; i < size; i++)
    {
      const void *addr = virtual_addr + i;
      /* Invalid user virtual pointer:
       NULL pointer,
       a pointer below the code segment
       a pointer to kernel virtual address space,
       a pointer to unmapped virtual memory.  */
      if (addr == NULL || addr < (void *)0x08048000 || !is_user_vaddr (addr))
        {
          // printf ("fff virtual_addr %p\n", addr);
          return false;
        }
      if (!pagedir_get_page (thread_current ()->pagedir, addr))
        {
          /* If page not found but addr is above esp,
             we need to allocate memory. */
          if (esp && esp <= addr)
            {
              // printf("fault above esp\n");
              grow_stack (addr);
              continue;
            }
          else
            {
              return false;
            }
        }
    }
  // printf ("passes\n");
  return true;
}

mapid_t 
sys_mmap (int fd, void *addr) 
{
  /* check validity of addr and fd */
  if (addr == NULL || addr == 0 || pg_ofs (addr) != 0 || fd <= 1) 
    return -1;

  lock_acquire_filesys();
  /* search for the file to map */
  struct file_info *info = find_file_info (fd);
  if (!info ||!info->file) 
    return -1;
  
  struct file *file = file_reopen(info->file);
  off_t file_size = file_length(file);
  if (file_size == 0) 
    return -1;
  
  /* validate space in [addr, addr + file_size] is unmapped */
  void *curr_addr = addr;
  while (curr_addr < addr + file_size) 
    {
      if (!is_user_vaddr(curr_addr) || find_in_table(curr_addr))
        return -1;
      curr_addr += PGSIZE;
    }
  

  /* map the file into pages */
  for (off_t offset = 0; offset < file_size; offset += PGSIZE)
    {
      
      curr_addr = addr + offset;
      uint32_t page_read_bytes = offset + PGSIZE < file_size ? 
                                  PGSIZE : 
                                  file_size - offset;
      uint32_t page_zero_bytes = PGSIZE - page_read_bytes;

      struct sup_page_table_entry *spte = 
                    install_page_supplemental (curr_addr);
      
      spte->file = file;
      spte->file_offset = offset;
      spte->read_bytes = page_read_bytes;
      spte->zero_bytes = page_zero_bytes;
      spte->source = MMAP;
      spte->read_only = false;
    }
    
    /* assign a unique mapid to the mapped file */
    mapid_t mapid = 1;
    if (!list_empty(&thread_current ()->mmapped_file_list))
      mapid = list_entry(
              list_back(&thread_current ()->mmapped_file_list), 
              struct mmapped_file_entry, elem)->mapid + 1;
    
    /* keep track of the mma ped file */
    struct mmapped_file_entry *mmap_entry = 
              malloc(sizeof (struct mmapped_file_entry));
    mmap_entry->mapid = mapid;
    mmap_entry->file_size = file_size;
    mmap_entry->file = file;
    mmap_entry->user_vaddr = addr;
    list_push_back(&thread_current ()->mmapped_file_list, 
                    &mmap_entry->elem);                    
    
    /* assign a mapping id */
    lock_release_filesys ();
    return mapid;
}

void 
sys_munmap (mapid_t mapid)
{
  
  struct mmapped_file_entry *mp = find_mmapped_file_info (mapid);
  if (!mp)
    sys_exit(-1);
  
  lock_acquire_filesys ();
  void *upage = mp->user_vaddr;
  size_t file_size = mp->file_size;
  
  while (upage < mp->user_vaddr + file_size)
  {
    struct sup_page_table_entry *spte = find_in_table(upage);
    free_single_page(spte);
    upage += PGSIZE;
  }
  
  file_close(mp->file);;
  list_remove(&mp->elem);
  free(mp);
  lock_release_filesys ();
}

static struct file_info *
find_file_info (int fd)
{
  struct list_elem *e;
  struct list *l = &thread_current ()->file_info_list;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct file_info *f = list_entry (e, struct file_info, elem);
      if (f->fd == fd)
        {
          return f;
        }
    }

  return NULL;
}

static struct mmapped_file_entry *
find_mmapped_file_info (mapid_t mapid)
{
  struct list_elem *e;
  struct list *l = &thread_current ()->mmapped_file_list;
  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct mmapped_file_entry *mf = 
                    list_entry(e, struct mmapped_file_entry, elem);
      if (mf->mapid == mapid)
          return mf;
    }

  return NULL;
}

static void
free_file_info ()
{
  lock_acquire_filesys ();
  struct list_elem *e;
  struct list *l = &thread_current ()->file_info_list;
  for (e = list_begin (l); e != list_end (l);)
    {
      struct file_info *f = list_entry (e, struct file_info, elem);
      e = list_next (e);
      if (f)
        {
          file_close (f->file);
          free (f);
        }
    }
  lock_release_filesys ();
}

static void
free_child_processes_info ()
{
  struct list_elem *e;
  struct list *l = &thread_current ()->child_processes;

  for (e = list_begin (l); e != list_end (l);)
    {
      struct thread_info *f = list_entry (e, struct thread_info, elem);
      e = list_next (e);
      if (f)
        free (f);
    }
}

// static void
// free_page_table ()
// {
//   struct list *l = &thread_current ()->page_table;
//   struct list_elem *e;
//   for (e = list_begin (l); e != list_end (l);)
//     {
//       struct sup_page_table_entry *entry
//           = list_entry (e, struct sup_page_table_entry, elem);
//       struct list_elem *next = list_next (e);
//       list_remove (e);
//       list_remove(&entry->fte->elem);
//       free (entry);
//       e = next;
//     }
// }

bool
grow_stack (const void *fault_addr)
{
  void *kpage = allocate_frame ();
  // printf("after grow_stack\n");
  if (kpage != NULL)
    {
      void *upage = pg_round_down (fault_addr);
      bool writable = true;
      // bool success = pagedir_set_page (thread_current ()->pagedir, upage,
      //                                  kpage, writable);
      // if (success)
      //   {
      //     /* Create entry in supplemental page table. */
      //    //  printf ("install success %p\n", upage);
      //     struct frame_table_entry *entry = find_in_frame_table(kpage);
      // struct sup_page_table_entry *spte = install_page_supplemental (upage);
      // entry->spte = spte;

      //     return true;
      //   }
      // else
      //   {
      //    //  printf ("install failed\n");
      //     free_frame (kpage);
      //     return false;
      //   }
      if (!install_page (upage, kpage, writable))
        {
          free_frame (kpage);
          return false;
        }
      else
        {
          return true;
        }
    }
  else
    {
      // printf ("allocate failed\n");
      free_frame (kpage);
      return false;
    }
}