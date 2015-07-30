#ifndef _NBA_H_
#define _NBA_H_

#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>

#include <linux/lightnvm.h>
#include "nba_debug.h"

struct nba_lun;

struct nba
{
    struct nvm_tgt_instance instance;

    unsigned long nr_pages;
    unsigned long total_blocks;
    unsigned long nr_luns;

    struct nba_lun *luns;

    mempool_t *rq_pool;

    struct nvm_dev *dev;
    struct gendisk *disk;
};


struct nba_lun
{
    struct nba *api;

    struct nvm_lun	*parent;
    struct nvm_block	*blocks;

    unsigned long nr_blocks;
};

struct nba_block
{
    unsigned long lun;

    sector_t phys_addr;

    unsigned long id;

    void *internals;
};

struct nba_channel
{
        unsigned long int lun_idx;
        unsigned short int chnl_idx;

        unsigned int gran_write;
        unsigned int gran_read;
        unsigned int gran_erase;
};

#define NVMBLOCKPUT         21525
#define NVMBLOCKRRGET       21526
#define NVMLUNSNRGET        21527
#define NVMBLOCKSNRGET      21528
#define NVMBLOCKERASE       21529
#define NVMPAGESNRGET       21530
#define NVMBLOCKGETBYADDR   21531
#define NVMBLOCKGETBYID	    21532
#define NVMPAGESIZEGET      21533
#define NVMCHANNELSNRGET    21534

#endif
