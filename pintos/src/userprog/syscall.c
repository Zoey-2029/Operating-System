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
#include "userprog/pagedir.h"
#include <console.h>
#include <syscall-nr.h>
#define MAX_WRITE_CHUNK 200

static void syscall_handler (struct intr_frame *);
static struct file_info *find_file_info (int fd);

/* Helper function(s). */
static void free_file_info (void);
static void free_child_processes_info (void);
static bool check_memory_validity (const void *addr, unsigned size);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  if (!check_memory_validity (f->esp, 1))
    sys_exit (-1);

  /* Number of args to check validity. */
  unsigned args = 0;
  switch (*(int *)f->esp)
    {
    case SYS_HALT:
      {
        args = 0;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        sys_halt ();
        break;
      }

    case SYS_EXIT:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int status = *((int *)f->esp + 1);
        sys_exit (status);
        break;
      }

    case SYS_EXEC:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        const char *cmd_line = (void *)(*((int *)f->esp + 1));
        f->eax = sys_exec (cmd_line);
        break;
      }

    case SYS_WAIT:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        pid_t pid = *((pid_t *)f->esp + 1);
        f->eax = sys_wait (pid);
        break;
      }

    case SYS_CREATE:
      {
        args = 2;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        const char *file = (void *)(*((int *)f->esp + 1));
        unsigned initial_size = *((unsigned *)f->esp + 2);
        f->eax = sys_create (file, initial_size);
        break;
      }

    case SYS_REMOVE:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        const char *file = (void *)(*((int *)f->esp + 1));
        f->eax = sys_remove (file);
        break;
      }

    case SYS_OPEN:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        const char *file = (void *)(*((int *)f->esp + 1));
        f->eax = sys_open (file);
        break;
      }

    case SYS_FILESIZE:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        f->eax = sys_filesize (fd);
        break;
      }

    case SYS_READ:
      {
        args = 3;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        void *buffer = (void *)(*((int *)f->esp + 2));
        unsigned size = *((unsigned *)f->esp + 3);
        f->eax = sys_read (fd, buffer, size);
        break;
      }

    case SYS_WRITE:
      {
        args = 3;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        const void *buffer = (void *)(*((int *)f->esp + 2));
        unsigned size = *((unsigned *)f->esp + 3);
        f->eax = sys_write (fd, buffer, size);
        break;
      }

    case SYS_SEEK:
      {
        args = 2;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        unsigned position = *((unsigned *)f->esp + 2);
        sys_seek (fd, position);
        break;
      }

    case SYS_TELL:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        f->eax = sys_tell (fd);
        break;
      }

    case SYS_CLOSE:
      {
        args = 1;
        if (!check_memory_validity ((int *)f->esp + 1, args))
          sys_exit (-1);

        int fd = *((int *)f->esp + 1);
        sys_close (fd);
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

  thread_exit ();
}

pid_t
sys_exec (const char *cmd_line)
{
  if (!check_memory_validity (cmd_line, 1))
    {
      sys_exit (-1);
    }

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
  if (!check_memory_validity (file, 1))
    {
      sys_exit (-1);
    }
  lock_acquire_filesys ();
  bool success = filesys_create (file, initial_size);
  lock_release_filesys ();

  return success;
}

bool
sys_remove (const char *file)
{
  if (!check_memory_validity (file, 1))
    {
      sys_exit (-1);
    }

  lock_acquire_filesys ();
  bool success = filesys_remove (file);
  lock_release_filesys ();

  return success;
}

int
sys_open (const char *file)
{
  if (!check_memory_validity (file, 1))
    {
      sys_exit (-1);
    }
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
  if (!check_memory_validity (buffer, 1))
    {
      sys_exit (-1);
    }

  unsigned bytes_read = 0;

  if (fd == STDIN_FILENO)
    {
      while (bytes_read < size)
        {
          ((char *)buffer)[bytes_read++] = input_getc ();
        }
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
  if (!check_memory_validity (buffer, 1))
    sys_exit (-1);

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
    {
      file_seek (info->file, position);
    }
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
  struct file_info *info = find_file_info (fd);
  if (info)
    {
      file_close (info->file);
      list_remove (&info->elem);
      free (info);
    }
}

/* Check if the user pointer is valid.
   virtual_addr: the first pointer.
   size: number of pointers to check validity. */
static bool
check_memory_validity (const void *virtual_addr, unsigned size)
{
  for (unsigned i = 0; i < size * sizeof (void *); i++)
    {
      const void *addr = virtual_addr + i;
      /* Invalid user virtual pointer:
       NULL pointer,
       a pointer below the code segment
       a pointer to kernel virtual address space,
       a pointer to unmapped virtual memory.  */
      if (addr == NULL || addr < (void *)0x08048000 || !is_user_vaddr (addr)
          || !pagedir_get_page (thread_current ()->pagedir, addr))
        return false;
    }

  return true;
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

static void
free_file_info ()
{
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
