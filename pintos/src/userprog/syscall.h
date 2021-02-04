#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"


void syscall_init (void);
static void syscall_handler (struct intr_frame *);
void check_pointer_validity(void *uvaddr);


// system call handles
void sys_exit (int status);
int sys_write(int fd, void* buffer, unsigned size);


#endif /* userprog/syscall.h */
