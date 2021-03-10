#include "filesys/inode.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdio.h>
#include <string.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* number of direct, indirect and double-indirect blocks in
a inode-disk struct*/ 
#define INODE_NUM 12
#define INODE_NUM_DIRECT 10
#define INDIRECT_INDEX INODE_NUM - 2
#define DOUBLE_INDIRECT_INDEX INODE_NUM - 1
#define INDIRECT_BLOCK_NUM_PER_SECTOR 128
#define DOUBLE_INDIRECT_NUM_PER_SECTOR 128 * 128

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* In-memory inode. */
struct inode
{
  struct list_elem elem; /* Element in inode list. */
  block_sector_t sector; /* Sector number of disk location. */
  int open_cnt;          /* Number of openers. */
  bool removed;          /* True if deleted, false otherwise. */
  int deny_write_cnt;    /* 0: writes ok, >0: deny writes. */
  // struct inode_disk data; /* Inode content. */
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  block_sector_t blocks[INODE_NUM];
  off_t length;         /* File size in bytes. */
  unsigned magic;       /* Magic number. */
  bool is_dir;           /* is directory? */
  uint32_t unused[113]; /* Not used. */
};

struct inode_indirect_sector {
  block_sector_t blocks[INDIRECT_BLOCK_NUM_PER_SECTOR];
};



block_sector_t find_block (struct inode_disk *disk_inode, size_t index);
bool extend_block (struct inode *inode, off_t offset);

/* allocate blocks */
bool check_extend (block_sector_t *sector);
bool allocate_single_block (block_sector_t *sector);
bool inode_allocate_double_indirect 
    (block_sector_t sector_idx, off_t start_index, size_t num_sectors);
bool inode_allocate_indirect 
    (block_sector_t sector_idx, off_t start_index, size_t num_sectors);
bool inode_allocate 
    (struct inode_disk *disk_inode, int start_index, int num_sectors);

/*free blocks*/
void free_double_indirect_block (block_sector_t sector, off_t sector_nums);
void free_indirect_block (block_sector_t sector, off_t sector_nums);
void free_blocks (struct inode *inode);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


static inline size_t
min (size_t a, size_t b)
{
  return a < b ? a : b;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);

  struct inode_disk data;
  block_read_cache (inode->sector, &data, 0, BLOCK_SECTOR_SIZE, 0, false);

  if (pos >= data.length) return -1;

  if (pos < data.length)
    return find_block (&data, pos / BLOCK_SECTOR_SIZE);
  else
    return -1;
}


/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;
      if (inode_allocate (disk_inode, 0, bytes_to_sectors (disk_inode->length)))
        {
          block_write_cache (sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0, 0);
          success = true;
        }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  // block_read (fs_device, inode->sector, &inode->data);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  struct inode_disk data;
  block_read_cache (inode->sector, &data, 0, BLOCK_SECTOR_SIZE, 0, false);
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
          free_blocks (inode);
          //free_map_release (data.start, bytes_to_sectors (data.length));
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  // uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      if (sector_idx == -1u)
        break;
        
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      block_read_cache (sector_idx, buffer, sector_ofs, chunk_size,
                        bytes_read, true);
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  // free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  // uint8_t *bounce = NULL;


  if (inode->deny_write_cnt)
    return 0;

  //printf("****** %d %d %d %d\n", inode, offset, size, inode_length (inode));
  if (offset + size >= inode_length (inode))
    {
      struct inode_disk disk_inode;
      block_read_cache (inode->sector, &disk_inode, 0, BLOCK_SECTOR_SIZE, 0, 0);
      int start_index = bytes_to_sectors (disk_inode.length);
      int num_sectors = bytes_to_sectors (offset + size) - start_index;
      disk_inode.length = offset + size;
      //printf("!!!!! %d %d, %d \n", disk_inode->length, start_index, num_sectors);
      if (num_sectors > 0)
        inode_allocate (&disk_inode, start_index, num_sectors);
      block_write_cache (inode->sector, &disk_inode, 0, BLOCK_SECTOR_SIZE, 0, 0);
    }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      //printf("^^^^^ %d \n", sector_idx);

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      block_write_cache (sector_idx, buffer, sector_ofs, chunk_size,
                         sector_left, bytes_written);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  // free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk data;
  block_read_cache (inode->sector, &data, 0, BLOCK_SECTOR_SIZE, 0, false);
  return data.length;
}

void set_inode_length (struct inode *inode, off_t length)
{
  struct inode_disk data;
  block_read_cache (inode->sector, &data, 0, BLOCK_SECTOR_SIZE, 0, false);
  data.length = length;
  block_write_cache (inode->sector, &data, 0, BLOCK_SECTOR_SIZE, 0, false);
}

bool 
allocate_single_block (block_sector_t *sector)
{
  ASSERT (*sector == 0);
  static char zeros[BLOCK_SECTOR_SIZE];
  if (!free_map_allocate (1, sector))
    return false;
  block_write_cache (*sector, zeros, 0, BLOCK_SECTOR_SIZE, 0, 0);
  return true;
}

bool 
check_extend (block_sector_t *sector)
{
  if (*sector == 0)
    {
      if (!allocate_single_block (sector))
        return false;
    }
  return true;
}

bool 
inode_allocate_indirect (block_sector_t sector_idx, 
                        off_t start_index, size_t num_sectors)
{
  struct inode_indirect_sector indirect_sector;
  block_read_cache (sector_idx, &indirect_sector, 0, 
                    BLOCK_SECTOR_SIZE, 0, 0);
  for (int i = start_index; i < INDIRECT_BLOCK_NUM_PER_SECTOR; i++) 
    {
      if (! allocate_single_block (&indirect_sector.blocks[i]))
        return false;
      num_sectors--;
      if (num_sectors == 0)
        break;
    }

  block_write_cache (sector_idx, &indirect_sector, 0, 
                     BLOCK_SECTOR_SIZE, 0, 0);
  return true;
}

bool 
inode_allocate_double_indirect (block_sector_t sector_idx, 
                    off_t start_index, size_t num_sectors)
{
  struct inode_indirect_sector double_indirect;
  block_read_cache (sector_idx, &double_indirect, 
                    0, BLOCK_SECTOR_SIZE, 0, 0);

  for (int i = start_index / INDIRECT_BLOCK_NUM_PER_SECTOR; 
            i < INDIRECT_BLOCK_NUM_PER_SECTOR; i++)
    {
      if (! check_extend (&double_indirect.blocks[i]))
        return false;

      int indirect_sectors = min(num_sectors, INDIRECT_BLOCK_NUM_PER_SECTOR);
      int start = 0;
      if (i == start_index / INDIRECT_BLOCK_NUM_PER_SECTOR) 
        start = start_index % INDIRECT_BLOCK_NUM_PER_SECTOR;

      if (! inode_allocate_indirect (
      double_indirect.blocks[i], start, indirect_sectors))
        return false;
      
      num_sectors -= indirect_sectors;
      if (num_sectors == 0)
        break;
    }
  
  block_write_cache (sector_idx, &double_indirect, 
                    0, BLOCK_SECTOR_SIZE, 0, 0);
  return true;
}


bool 
inode_allocate (struct inode_disk *disk_inode, int start_index, int num_sectors)
{ 
  if (num_sectors == 0)
    return true;
  int min_index = 0;
  int max_index = INODE_NUM_DIRECT;

  /* allocate direct block */
  if (start_index < max_index)
    {
      for (int i = start_index; i < INODE_NUM_DIRECT; i++) 
        {
          allocate_single_block (&disk_inode->blocks[i]);
          num_sectors--;
          if (num_sectors == 0)
            return true;
        }
    }

  min_index = max_index;
  max_index += INDIRECT_BLOCK_NUM_PER_SECTOR;

  /* allocate indirect */
  if (start_index < max_index)
    {
      if (start_index < min_index)
        start_index = min_index;
  
      int indirect_sectors = min (INDIRECT_BLOCK_NUM_PER_SECTOR, num_sectors);
      int start = start_index - min_index;

      
      if (!check_extend (& disk_inode->blocks[INDIRECT_INDEX] ))
        return false;
      if (! inode_allocate_indirect (
      disk_inode->blocks[INDIRECT_INDEX], start, indirect_sectors))
        return false;

      num_sectors -= indirect_sectors;
      if (num_sectors == 0)
        return true;
    }
    
    /* allocate double indirect */
    min_index = max_index;
    max_index += DOUBLE_INDIRECT_NUM_PER_SECTOR;
    if (start_index < max_index)
      {
        if (start_index < min_index)
          start_index = min_index;
        int start = start_index - min_index;

        if (!check_extend (&disk_inode->blocks[DOUBLE_INDIRECT_INDEX]))
          return false;
        
        if (!inode_allocate_double_indirect (
        disk_inode->blocks[DOUBLE_INDIRECT_INDEX], start, num_sectors))
          return false;
      }

    return true;
}


block_sector_t
find_block (struct inode_disk *disk_inode, size_t index)
{
  size_t index_max = INODE_NUM_DIRECT;

  /* direct block */
  if (index < index_max)
    return disk_inode->blocks[index];
    
  index -= INODE_NUM_DIRECT;
  /* indirect block */
  if (index < INDIRECT_BLOCK_NUM_PER_SECTOR)
    {
      struct inode_indirect_sector indirect;
      block_read_cache (disk_inode->blocks[INDIRECT_INDEX], &indirect, 0, 
                        BLOCK_SECTOR_SIZE, 0, 0);
                        
      block_sector_t sector = indirect.blocks[index];
      return sector;
    }
  
  index -= INDIRECT_BLOCK_NUM_PER_SECTOR;
  /* double indirect block */
  if (index < DOUBLE_INDIRECT_NUM_PER_SECTOR)
    {
      struct inode_indirect_sector double_indirect;
      block_read_cache (disk_inode->blocks[DOUBLE_INDIRECT_INDEX], 
                        &double_indirect, 0, BLOCK_SECTOR_SIZE, 0, 0);

      struct inode_indirect_sector indirect;
      block_read_cache (double_indirect.blocks[index / BLOCK_SECTOR_SIZE],
                        &indirect,0, BLOCK_SECTOR_SIZE, 0, 0);

      block_sector_t sector = indirect.blocks[index % BLOCK_SECTOR_SIZE];
      return sector;
    }

  return -1;
}

void free_double_indirect_block (block_sector_t sector, off_t sector_nums)
{
  ASSERT (sector != 0);
  struct inode_indirect_sector double_indirect;
  block_read_cache (sector, &double_indirect, 0, BLOCK_SECTOR_SIZE, 0, 0);

  for (int i = 0; i < INDIRECT_BLOCK_NUM_PER_SECTOR; i++)
    {
      if (sector_nums == 0)
        return;
      ASSERT (double_indirect.blocks[i] != 0);
      off_t indirect_sector_num = sector_nums > INDIRECT_BLOCK_NUM_PER_SECTOR
                                  ? INDIRECT_BLOCK_NUM_PER_SECTOR 
                                  : sector_nums;
      free_indirect_block (double_indirect.blocks[i], indirect_sector_num);
      sector_nums -= indirect_sector_num;
    }
}

void free_indirect_block (block_sector_t sector, off_t sector_nums)
{
  ASSERT (sector != 0);
  struct inode_indirect_sector indirect;
  block_read_cache (sector, &indirect, 0, BLOCK_SECTOR_SIZE, 0, 0);

  for (int i = 0; i < INDIRECT_BLOCK_NUM_PER_SECTOR; i++) 
   {
     if (sector_nums == 0)
      return;
    
     ASSERT (indirect.blocks[i] != 0);
     free_map_release (indirect.blocks[i], 1);
     sector_nums--;
   }
}


void 
free_blocks (struct inode *inode)
{
  struct inode_disk disk_inode;
  block_read_cache (inode->sector, &disk_inode, 0, BLOCK_SECTOR_SIZE, 0, 0);
  off_t sector_nums = bytes_to_sectors(disk_inode.length);

  /* free direct block*/
  for (int i = 0; i < INODE_NUM_DIRECT; i++) 
   {
    if (sector_nums == 0)
      return;  
    ASSERT (disk_inode.blocks[i] != 0);
    free_map_release (disk_inode.blocks[i], 1);
    sector_nums--; 
   }

  off_t indirect_sector_nums = min (sector_nums, INDIRECT_BLOCK_NUM_PER_SECTOR);
  free_indirect_block (disk_inode.blocks[INDIRECT_INDEX], indirect_sector_nums);
  sector_nums -= indirect_sector_nums;
  if (sector_nums == 0)
    return;
  
  free_double_indirect_block (disk_inode.blocks[DOUBLE_INDIRECT_INDEX], sector_nums);
};

bool inode_removed(const struct inode *inode)
{
  ASSERT (inode != NULL);
  return inode->removed;
}


bool inode_is_dir (const struct inode *inode)
{
  ASSERT (inode != NULL);
  struct inode_disk data;
  block_read_cache (inode->sector, &data, 0, BLOCK_SECTOR_SIZE, 0, false);
  bool is_dir = data.is_dir;
  //free (disk_inode);
  return is_dir;
}