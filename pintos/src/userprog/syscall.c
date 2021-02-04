#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <console.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#define MAX_WRITE_CHUNK 200

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_pointer_validity(f->esp);
  //printf ("system call! %d \n", *(size_t *) f->esp);

  switch(*(int *) f->esp) 
  {
    case SYS_HALT:
    {
      break;
    }
      
    case SYS_EXIT: 
    {
      int status = *((int*)f->esp + 1);
      sys_exit(status);
      break;
    } 

    case SYS_EXEC:
    {
      break;
    }

    case SYS_WAIT:
    {
      break;
    }
      
    case SYS_CREATE:
    {
      break;
    }
      
    case SYS_REMOVE:
    {
      break;
    }

    case SYS_OPEN:
    {
      break;
    }

    case SYS_FILESIZE:
    {
      break;
    }
      
    case SYS_READ:
    {
      break;
    }

    case SYS_WRITE:
    {
      int fd = *((int*)f->esp + 1);
      void* buffer = (void *)(*((int *)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      f->eax = sys_write(fd, buffer, size);
      break;
    }

    case SYS_SEEK:
    {
      break;
    }
      

    case SYS_TELL:
    {
      break;
    }
      
      
    case SYS_CLOSE:
    {
      break;
    }
      
    default:
    {
      printf("unimplemented system call \n");
      sys_exit(-1);
    }
  }
}

void 
sys_exit (int status) {
  printf("exit *** %d \n", status);
  thread_exit();
}

int 
sys_write(int fd, void * buffer, unsigned size) {
  //printf("write to concole \n");
  check_pointer_validity(buffer);
  if (fd == STDOUT_FILENO) {
    size_t bytes_written = 0;
    while(bytes_written + MAX_WRITE_CHUNK < size) 
    {
      putbuf((char *)(buffer + bytes_written), MAX_WRITE_CHUNK);
      bytes_written += MAX_WRITE_CHUNK;
    }
    putbuf((char *)(buffer + bytes_written), 
            (size_t)(size - bytes_written));
    //printf("complete !!!!\n ");
    return size;
  }
  return 0;
}

void 
check_pointer_validity(void *uvaddr) 
{
  /* invalid user virtual pointer: 
     NULL pointer, 
     a pointer to unmapped virtual memory, 
     or a pointer to kernel virtual address space */
  if (uvaddr == NULL ||                                   
      !is_user_vaddr(uvaddr) || 
      pagedir_get_page(thread_current ()->pagedir, uvaddr) == NULL) {
        thread_exit();
      }
}
