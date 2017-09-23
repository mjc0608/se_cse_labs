#include "inode_manager.h"
#include <iterator>
#include <ctime>
using namespace std;

// disk layer -----------------------------------------

disk::disk() {
    bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf) {
    /*
     *your lab1 code goes here.
     *if id is smaller than 0 or larger than BLOCK_NUM 
     *or buf is null, just return.
     *put the content of target block into buf.
     *hint: use memcpy
    */

    if ((int32_t) id < 0 || id >= BLOCK_NUM || buf == NULL)
        return;

    unsigned char *src = blocks[id];
    memcpy(buf, src, BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf) {
    /*
     *your lab1 code goes here.
     *hint: just like read_block
    */

    if ((int32_t) id < 0 || id >= BLOCK_NUM || buf == NULL)
        return;

    unsigned char *dest = blocks[id];
    memcpy(dest, buf, BLOCK_SIZE);
}

// block layer -----------------------------------------
// Set a bit in the bitmap
void
block_manager::set_bit(uint32_t id, uint32_t val) {
    if (val != 0 && val != 1)
        return;

    char buf[BLOCK_SIZE];
    uint32_t src_block = BBLOCK(id);
    d->read_block(src_block, buf);
    uint32_t bit_num = id % BPB;
    uint32_t byte_num = bit_num / 8;
    uint32_t inbyte_offset = bit_num % 8;

    unsigned char mask = (char) val << inbyte_offset;
    buf[byte_num] &= mask;

    d->write_block(src_block, buf);
}

// Allocate a free disk block.
blockid_t
block_manager::alloc_block() {
    /*
     * your lab1 code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.

     *hint: use macro IBLOCK and BBLOCK.
            use bit operation.
            remind yourself of the layout of disk.
     */

    uint32_t i;
    for (i = 0; i < BLOCK_NUM; i++) {
        if (using_blocks.find(i) == using_blocks.end()) {
            using_blocks.insert(pair<uint32_t, int>(i, 1));
            break;
        } else if (using_blocks[i] == 0) {
            using_blocks[i] = 1;
            break;
        }
    }
    set_bit(i, 1);
    return i;
}

void
block_manager::free_block(uint32_t id) {
    /* 
     * your lab1 code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */

    if (using_blocks.find(id) == using_blocks.end())
        return;
    else if (using_blocks[id] == 1) {
        using_blocks[id] = 0;
        set_bit(id, 0);
    }
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() {
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

    sb.fb_bitmap_block_s = BBLOCK(0);
    sb.inode_table_block_s = IBLOCK(0, BLOCK_NUM);
    sb.data_block_s = IBLOCK(INODE_NUM, BLOCK_NUM) + 1;

    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);

    for (int i = 0; i < sb.data_block_s; i++) {
        d->write_block(i, buf);
    }

    memcpy(buf, (char *) &sb, sizeof(sb));
    d->write_block(0, buf);

    for (int i = 0; i < sb.data_block_s; i++) {
        alloc_block();
    }
}

void
block_manager::read_block(uint32_t id, char *buf) {
    d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf) {
    d->write_block(id, buf);
}

// inode layer -----------------------------------------
inode_manager::inode_manager() {
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type) {
    /* 
     * your lab1 code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     *       the 1st is used for root_dir, see inode_manager::inode_manager().
        
     * if you get some heap memory, do not forget to free it.
     */

    

    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    inode_t *inode = (inode_t *) buf;

    for (int i = 1; i < INODE_NUM; i++) {
        bm->read_block(bm->sb.inode_table_block_s + i, buf);
        if (inode->type == 0) {
            inode->type = type;
            inode->size = 0;

            time_t t;
            t = time(&t);
            inode->ctime = t;
            inode->atime = t;
            inode->mtime = t;

            bm->write_block(bm->sb.inode_table_block_s + i, buf);
            return i;
        }
    }

    return 0;
}

void
inode_manager::free_inode(uint32_t inum) {
    /* 
     * your lab1 code goes here.
     * note: you need to check if the inode is already a freed one;
     *       if not, clear it, and remember to write back to disk.
     *       do not forget to free memory if necessary.
     */

    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    bm->write_block(bm->sb.inode_table_block_s + inum, buf);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum) {
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if ((int32_t) inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode *) buf + inum % IPB;
    if (ino_disk->type == 0) {
        printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode *) malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino) {
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *) buf + inum % IPB;

    if (ino_disk->size!=ino->size) 
        ino->ctime = time(NULL);

    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size) {
    /*
     * your lab1 code goes here.
     * note: read blocks related to inode number inum,
     *       and copy them to buf_out
     */

    if ((int32_t) inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return;
    }

    inode_t *inode = get_inode(inum);
    uint32_t nblocks = (inode->size - 1) / BLOCK_SIZE + 1;
    if (inode->size == 0)
        nblocks = 0;
    *size = inode->size;

    // if the block number is larger than NDIRECT, read indirect blocks
    if (nblocks <= NDIRECT) {
        *buf_out = (char *) malloc(nblocks * BLOCK_SIZE);
        for (int i = 0; i < nblocks; i++) {
            bm->read_block(inode->blocks[i], (*buf_out) + i * BLOCK_SIZE);
        }
    } else {
        blockid_t indirect = inode->blocks[NDIRECT];
        uint32_t indirect_blocks[NINDIRECT];
        bm->read_block(indirect, (char *) indirect_blocks);

        *buf_out = (char *) malloc(nblocks * BLOCK_SIZE);
        for (int i = 0; i < NDIRECT; i++) {
            bm->read_block(inode->blocks[i], (*buf_out) + i * BLOCK_SIZE);
        }
        for (int i = NDIRECT; i < nblocks; i++) {
            bm->read_block(indirect_blocks[i - NDIRECT], (*buf_out) + i * BLOCK_SIZE);
        }
    }

    time_t t;
    t = time(&t);
    inode->atime = t;
    put_inode(inum, inode);

    delete[] inode;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size) {
    /*
     * your lab1 code goes here.
     * note: write buf to blocks of inode inum.
     *       you need to consider the situation when the size of buf 
     *       is larger or smaller than the size of original inode.
     *       you should free some blocks if necessary.
     */

    if ((int32_t) inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return;
    }

    inode_t *inode = get_inode(inum);
    uint32_t nblocks_pre = (inode->size - 1) / BLOCK_SIZE + 1;
    uint32_t nblocks_post = (size - 1) / BLOCK_SIZE + 1;
    if (inode->size == 0) nblocks_pre = 0;
    if (size == 0) nblocks_post = 0;
    inode->size = size;

    char *written_buf = (char*)malloc(nblocks_post*BLOCK_SIZE);
    memcpy(written_buf, buf, size);

    // if block number is larger than NDIRECT, alloc a block and store the rest in it
    if (nblocks_pre <= NDIRECT && nblocks_post <= NDIRECT) {
        if (nblocks_pre == nblocks_post) {
            for (int i = 0; i < nblocks_pre; i++) {
                bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
            }
        } else if (nblocks_pre < nblocks_post) {
            for (int i = 0; i < nblocks_pre; i++) {
                bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
            }
            for (int i = nblocks_pre; i < nblocks_post; i++) {
                inode->blocks[i] = bm->alloc_block();
                bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
            }
        } else {
            for (int i = 0; i < nblocks_post; i++) {
                bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
            }
            for (int i = nblocks_post; i < nblocks_pre; i++) {
                bm->free_block(inode->blocks[i]);
                inode->blocks[i] = 0;
            }
        }
    } else if (nblocks_pre <= NDIRECT && nblocks_post > NDIRECT) {
        for (int i = 0; i < nblocks_pre; i++) {
            bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
        }
        for (int i = nblocks_pre; i < NDIRECT; i++) {
            inode->blocks[i] = bm->alloc_block();
            bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
        }

        blockid_t indirect = bm->alloc_block();
        inode->blocks[NDIRECT] = indirect;
        uint32_t indirect_blocks[NINDIRECT];

        for (int i = NDIRECT; i < nblocks_post; i++) {
            indirect_blocks[i - NDIRECT] = bm->alloc_block();
            bm->write_block(indirect_blocks[i - NDIRECT], written_buf + i * BLOCK_SIZE);
        }
        bm->write_block(inode->blocks[NDIRECT], (char *) indirect_blocks);

    } else if (nblocks_pre > NDIRECT && nblocks_post <= NDIRECT) {
        blockid_t indirect = inode->blocks[NDIRECT];
        uint32_t indirect_blocks[NINDIRECT];
        bm->read_block(indirect, (char *) indirect_blocks);
        for (int i = 0; i < nblocks_pre - NDIRECT; i++) {
            bm->free_block(indirect_blocks[i]);
        }
        bm->free_block(inode->blocks[NDIRECT]);
        inode->blocks[NDIRECT] = 0;

        for (int i = 0; i < nblocks_post; i++) {
            bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
        }
        for (int i = nblocks_post; i < NDIRECT; i++) {
            bm->free_block(inode->blocks[i]);
            inode->blocks[i] = 0;
        }
    } else {
        for (int i = 0; i < NDIRECT; i++) {
            bm->write_block(inode->blocks[i], written_buf + i * BLOCK_SIZE);
        }

        blockid_t indirect = inode->blocks[NDIRECT];
        uint32_t indirect_blocks[NINDIRECT];
        bm->read_block(indirect, (char *) indirect_blocks);

        if (nblocks_pre == nblocks_post) {
            for (int i = NDIRECT; i < nblocks_pre; i++) {
                bm->write_block(indirect_blocks[i - NDIRECT], written_buf + i * BLOCK_SIZE);
            }
        } else if (nblocks_pre < nblocks_post) {
            for (int i = NDIRECT; i < nblocks_pre; i++) {
                bm->write_block(indirect_blocks[i - NDIRECT], written_buf + i * BLOCK_SIZE);
            }
            for (int i = nblocks_pre; i < nblocks_post; i++) {
                indirect_blocks[i - NDIRECT] = bm->alloc_block();
                bm->write_block(indirect_blocks[i - NDIRECT], written_buf + i * BLOCK_SIZE);
            }
        } else {
            for (int i = NDIRECT; i < nblocks_post; i++) {
                bm->write_block(indirect_blocks[i - NDIRECT], written_buf + i * BLOCK_SIZE);
            }
            for (int i = nblocks_post; i < nblocks_pre; i++) {
                bm->free_block(indirect_blocks[i - NDIRECT]);
                indirect_blocks[i - NDIRECT] = 0;
            }
        }
        bm->write_block(indirect, (char *) indirect_blocks);
    }

    time_t t;
    t = time(&t);
    inode->atime = t;
    inode->mtime = t;

    put_inode(inum, inode);
    delete[] inode;
    free(written_buf);

}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a) {
    /*
     * your lab1 code goes here.
     * note: get the attributes of inode inum.
     *       you can refer to "struct attr" in extent_protocol.h
     */

    inode_t *inode = get_inode(inum);
    if (inode==NULL)
        return;

    a.type = inode->type;
    a.atime = inode->atime;
    a.mtime = inode->mtime;
    a.ctime = inode->ctime;
    a.size = inode->size;
    delete inode;
}

void
inode_manager::remove_file(uint32_t inum) {
    /*
     * your lab1 code goes here
     * note: you need to consider about both the data block and inode of the file
     *       do not forget to free memory if necessary.
     */

    inode_t *inode = get_inode(inum);
    uint32_t nblocks = (inode->size - 1) / BLOCK_SIZE + 1;
    if (inode->size == 0)
        nblocks = 0;

    if (nblocks <= NDIRECT) {
        for (int i = 0; i < nblocks; i++) {
            bm->free_block(inode->blocks[i]);
        }
    } else {
        blockid_t indirect = inode->blocks[NDIRECT];
        uint32_t indirect_blocks[NINDIRECT];
        bm->read_block(indirect, (char *) indirect_blocks);

        for (int i = 0; i < NDIRECT; i++) {
            bm->free_block(inode->blocks[i]);
        }
        for (int i = NDIRECT; i < nblocks; i++) {
            bm->free_block(indirect_blocks[i - NDIRECT]);
        }
        bm->free_block(inode->blocks[NDIRECT]);
    }

    free_inode(inum);
}
