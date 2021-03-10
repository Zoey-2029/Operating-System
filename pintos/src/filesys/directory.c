#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  //printf("============= in dir_lookup =============\n");
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (strcmp (name, "..") == 0)
  {
    //printf("here: first\n");
    inode_read_at (dir->inode, &e, sizeof e, 0);
    *inode = inode_open (e.inode_sector);
    //printf("Open inode success? %d\n", inode != NULL);
  }
  else if (strcmp (name, ".") == 0)
  {
    //printf("here: second\n");
    *inode = inode_reopen (dir->inode);
    //printf("Open inode success? %d\n", inode != NULL);
  }
  else if (lookup (dir, name, &e, NULL))
  {
    //printf("here: third\n");
    *inode = inode_open (e.inode_sector);
    //printf("Open inode success? %d\n", inode != NULL);
  }
  else
  {
    //printf("here: forth\n");
    *inode = NULL;
    //printf("Open inode success? %d\n", inode != NULL);
  }
  //printf("==========================\n");
  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

/*struct inode* dir_get_parent_inode(struct dir* dir)
{
  // if(dir == NULL) return NULL;
  ASSERT(dir != NULL)
  
  block_sector_t sector = inode_get_parent(dir_get_inode(dir));
  return inode_open(sector);
}*/

struct dir*
get_dir_from_path(const char* name)
{
  //printf("============= in get_dir_from_path =============\n");
  struct thread *curr_thread = thread_current ();
  /* Absolute path */
  struct dir *curr_dir;
  if (name[0] == '/' || curr_thread->cwd == NULL)
  {
    curr_dir = dir_open_root ();
  }
  /* Relative path */
  else
  {
    curr_dir = dir_reopen (curr_thread->cwd);
  }

  // printf("Full path is:  %s\n", name);
  // printf("Open base success? %d\n", curr_dir != NULL);

  int length = strlen(name);
  char name_copy[length + 1];
  memcpy(name_copy, name, length + 1);

  //printf("Copied path is %s\n", name_copy);
  
  char *token, *save_ptr;
  struct inode *next_inode;

  for (token = strtok_r (name_copy, "/", &save_ptr); token != NULL;
      token = strtok_r (NULL, "/", &save_ptr))
  {
    //printf("Token: %s\n", token);
    if(strlen(token) > NAME_MAX)
    {
      //printf("Failed 1\n");
      dir_close (curr_dir);
      return NULL;
    }

    if (!dir_lookup (curr_dir, token, &next_inode)){
      //printf("Failed 3\n");
      return curr_dir;
    }

    if (inode_is_dir(next_inode))
    {
      //printf("Success 1\n");
      dir_close (curr_dir);
      curr_dir = dir_open(next_inode);
    }
    else 
    {
      //printf("Failed 2\n");
      inode_close(next_inode);
      // break;
    }
  }
  // printf("==========================\n");
  return curr_dir;
}

char*
get_file_name_from_path(const char* name)
{
  int length = strlen(name);
  char name_copy[length + 1];
  memcpy(name_copy, name, length + 1);

  char *token, *save_ptr, *prev_token = "";

  for (token = strtok_r (name_copy, "/", &save_ptr); token != NULL;
      token = strtok_r (NULL, "/", &save_ptr))
  {
    prev_token = token;
  }

  char* file_name = malloc(strlen(prev_token) + 1);
  if (file_name == NULL)
    return NULL;
  memcpy(file_name, prev_token, strlen(prev_token) + 1);
  return file_name;
}

struct dir *
dir_open_from_path (const char *name)
{
  struct thread *curr_thread = thread_current ();
  /* Absolute path */
  struct dir *curr_dir;
  if (name[0] == '/' || curr_thread->cwd == NULL)
  {
    curr_dir = dir_open_root ();
  }
  /* Relative path */
  else
  {
    curr_dir = dir_reopen (curr_thread->cwd);
  }

  int length = strlen(name);
  char name_copy[length + 1];
  memcpy(name_copy, name, length + 1);
  
  char *token, *save_ptr;
  struct inode *next_inode;

  for (token = strtok_r (name_copy, "/", &save_ptr); token != NULL;
      token = strtok_r (NULL, "/", &save_ptr))
  {
    if(strlen(token) > NAME_MAX || !dir_lookup (curr_dir, token, &next_inode))
    {
      dir_close (curr_dir);
      return NULL;
    }

    /* Open directory from next_inode*/
    struct dir *next_dir = dir_open (next_inode);

    /* Set next directory as current */
    if (!next_dir)
    {
      dir_close (curr_dir);
      return NULL;
    }
    else
    {
      dir_close (curr_dir);
      curr_dir = next_dir;
    }
    
  }

  if (curr_dir->inode && !inode_removed(curr_dir->inode))
  {
    return curr_dir;
  }
  else 
  {
    dir_close(curr_dir);
    return NULL;
  }  
}
