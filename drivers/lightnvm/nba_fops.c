#include "nba.h"

static int nba_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd,
                            unsigned long arg)
{
    struct nba		*api;

    struct nvm_block    *process_blk;
    struct nvm_dev      *process_dev;
    struct nvm_lun	*process_lun;
    struct nvm_id_chnl  *process_chnl;

    struct nba_lun      *process_nba_lun;
    struct nba_block    *process_nba_blk;
    struct nba_channel	*process_nba_channel;


    unsigned long temp_long;

    if(bdev == NULL ||
       bdev->bd_disk == NULL ||
       bdev->bd_disk->private_data == NULL)
    {
	NBA_PRINT("device is not lightnvm device");
        return -EINVAL;
    }

    if(arg == 0)
    {
        return -EINVAL;
    }

    api = (struct nba *)bdev->bd_disk->private_data;

    switch(cmd)
    {
        //puts a block to storage
	//input is a nba_block
        case NVMBLOCKPUT:
        {
	    process_nba_blk = (struct nba_block*)arg;
	    process_blk = process_nba_blk->internals;

	    nvm_put_blk(api->dev, process_blk);
        }
        return 0;

	//erases a block arg is a nba_block
        case NVMBLOCKERASE:
        {
	    process_nba_blk = (struct nba_block *)arg;
	    process_blk = (struct nvm_block *)process_nba_blk->internals;

	    nvm_erase_blk(api->dev, process_blk);
	    nvm_put_blk(api->dev, process_blk);
        }
        return 0;

	//gets a block from a specified id on array
	//input is a nba_block
	case NVMBLOCKGETBYID:
	{
	    process_nba_blk = (struct nba_block *)arg;

	    if(process_nba_blk->lun >= api->nr_luns)
	    {
		NBA_PRINT("out of bounds");
		return -EINVAL;
	    }

	    process_nba_lun = &api->luns[process_nba_blk->lun];

	    process_lun = process_nba_lun->parent;

	    if(process_nba_blk->id >= process_nba_lun->nr_blocks)
	    {
		NBA_PRINT("out of bounds");
		return -EINVAL;
	    }

	    process_blk = &process_nba_lun->blocks[process_nba_blk->id];

	    process_nba_blk->phys_addr = process_lun->nr_pages_per_blk * process_nba_blk->id;
	    process_nba_blk->internals = process_blk;
	}
	return 0;

        //gets a block from a specified address on flash
	//input is a nba_block
        case NVMBLOCKGETBYADDR:
        {
	    process_nba_blk = (struct nba_block *)arg;

	    if(process_nba_blk->lun >= api->nr_luns)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_nba_lun = &api->luns[process_nba_blk->lun];

	    process_lun = process_nba_lun->parent;

	    temp_long = process_nba_blk->phys_addr / process_lun->nr_pages_per_blk;

	    if(temp_long >= process_nba_lun->nr_blocks)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_blk = &process_nba_lun->blocks[temp_long];

	    process_nba_blk->id = process_blk->id;
	    process_nba_blk->internals = process_blk;
        }
        return 0;

        //gets the next free block from lightnvm's list
        case NVMBLOCKRRGET:
        {
	    process_nba_blk = (struct nba_block *)arg;

	    if(process_nba_blk->lun >= api->nr_luns)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_nba_lun = &api->luns[process_nba_blk->lun];

	    process_blk = nvm_get_blk(api->dev, process_nba_lun->parent, 0);

	    process_nba_blk->id = process_blk->id;
	    process_nba_blk->internals = process_blk;
        }
        return 0;

        //returns number of luns in API
        case NVMLUNSNRGET:
        {
	    (*(unsigned long *)arg) = api->nr_luns;
        }
        return 0;

        //returns number of blocks in lun index *arg
        case NVMBLOCKSNRGET:
        {
            temp_long = *(unsigned long *)arg;

	    if(temp_long >= api->nr_luns)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    (*(unsigned long *)arg) = api->luns[temp_long].nr_blocks;
        }
        return 0;

        //returns nr of pages for block
        //arg is lun index
        case NVMPAGESNRGET:
        {
            temp_long = *(unsigned long *)arg;

	    if(temp_long >= api->nr_luns)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_nba_lun = &api->luns[temp_long];

	    process_lun = process_nba_lun->parent;

	    (*(unsigned long *)arg) = process_lun->nr_pages_per_blk;
        }
        return 0;

        //returns nr of channels for lun
        //arg is lun index
        case NVMCHANNELSNRGET:
        {
            temp_long = *(unsigned long *)arg;

	    if(temp_long >= api->nr_luns)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_nba_lun = &api->luns[temp_long];

	    process_dev = process_nba_lun->parent->dev;

            (*(unsigned long *)arg) = process_dev->identity.nchannels;
        }
        return 0;

        //gets the page size
        //arg is a long (lun index)
        case NVMPAGESIZEGET:
        {
	    process_nba_channel = (struct nba_channel *)arg;

	    if(process_nba_channel->lun_idx >= api->nr_luns)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_nba_lun = &api->luns[process_nba_channel->lun_idx];

	    process_dev = process_nba_lun->parent->dev;

	    if(process_nba_channel->chnl_idx >= process_dev->identity.nchannels)
            {
		NBA_PRINT("out of bounds");
                return -EINVAL;
            }

	    process_chnl = &process_dev->identity.chnls[process_nba_channel->chnl_idx];

	    process_nba_channel->gran_write = process_chnl->gran_write;
	    process_nba_channel->gran_read = process_chnl->gran_read;
	    process_nba_channel->gran_erase = process_chnl->gran_erase;
        }
        return 0;

        default:
        {
	    NBA_PRINT("unknown command");
        }
        return -EINVAL;
    }
}

static int nba_compat_ioctl(struct block_device *bdev, fmode_t mode,
                                        unsigned int cmd, unsigned long arg)
{
    return 0;
}

static int nba_open(struct block_device *bdev, fmode_t mode)
{
    return 0;
}

static void nba_release(struct gendisk *disk, fmode_t mode)
{
    return;
}

const struct block_device_operations nba_fops = {
        .owner		= THIS_MODULE,
	.ioctl		= nba_ioctl,
	.compat_ioctl	= nba_compat_ioctl,
	.open		= nba_open,
	.release	= nba_release,
};
