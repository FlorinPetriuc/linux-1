#include "nba.h"

extern const struct block_device_operations nba_fops;

static struct nvm_tgt_type tt_nba;

static inline sector_t nvm_get_laddr(struct bio *bio)
{
    return bio->bi_iter.bi_sector / NR_PHY_IN_LOG;
}

static int nba_submit_io(struct nba *api, struct bio *bio, struct nvm_rq *rqd)
{
	rqd->phys_sector = nvm_get_laddr(bio) + NR_PHY_IN_LOG;
	rqd->bio = bio;
	rqd->ins = &api->instance;

	if (nvm_submit_io(api->dev, rqd))
	{
	    return NVM_IO_ERR;
	}

	return NVM_IO_OK;
}

static void nba_make_rq(struct request_queue *q, struct bio *bio)
{
    struct nba *api;
    struct nvm_rq *rqd;

    if (bio->bi_rw & REQ_DISCARD)
    {
	return;
    }

    api = q->queuedata;

    rqd = mempool_alloc(api->rq_pool, GFP_KERNEL);
    if (!rqd)
    {
	pr_err_ratelimited("nba: not able to queue bio.");
	bio_io_error(bio);
	return;
    }

    switch (nba_submit_io(api, bio, rqd))
    {
	case NVM_IO_OK:
		return;
	case NVM_IO_ERR:
		bio_io_error(bio);
		break;
	default:
		break;
    }

    mempool_free(rqd, api->rq_pool);
}

static void nba_end_io(struct nvm_rq *rqd, int error)
{
    struct nba *api = container_of(rqd->ins, struct nba, instance);

    mempool_free(rqd, api->rq_pool);
}

static sector_t nba_capacity(void *private)
{
    struct nba *api = private;

    return (api->nr_pages) / NR_PHY_IN_LOG - NR_PHY_IN_LOG;
}

static void nba_luns_free(struct nba *api)
{
    if(api->luns)
    {
	kfree(api->luns);
	api->luns = NULL;
    }
}

static void nba_free(struct nba *api)
{
    if(api)
    {
	nba_luns_free(api);

	kfree(api);
    }
}

static int nba_luns_init(struct nba *api, int lun_begin, int lun_end)
{
    struct nvm_dev      *dev = api->dev;
    struct nvm_lun	*luns;
    struct nvm_lun      *lun;
    struct nvm_block	*block;

    struct nba_lun *rlun;

    unsigned long j;
    unsigned long i;

    int ret = 0;

    luns = dev->bm->get_luns(dev, lun_begin, lun_end);
    if (!luns)
    {
	return -EINVAL;
    }

    api->luns = kcalloc(api->nr_luns, sizeof(struct nba_lun), GFP_KERNEL);
    if(!api->luns)
    {
        return -ENOMEM;
    }

    for(i = 0; i < api->nr_luns; ++i)
    {
	lun = &luns[i];

	rlun = &api->luns[i];

	rlun->api = api;

        rlun->parent = lun;

        rlun->nr_blocks = lun->nr_blocks;

	api->total_blocks += lun->nr_blocks;
	api->nr_pages += lun->nr_blocks * lun->nr_pages_per_blk;

	rlun->blocks = vzalloc(sizeof(struct nvm_block) * rlun->nr_blocks);
        if(!rlun->blocks)
        {
            ret = -ENOMEM;

            goto out;
	}

	for(j = 0; j < lun->nr_blocks; ++j)
	{
	    block = &rlun->blocks[j];

	    block->id = j;
	    block->lun = lun;

	    spin_lock_init(&block->lock);
	    INIT_LIST_HEAD(&block->list);

	    bitmap_zero(block->invalid_pages, lun->nr_pages_per_blk);
	    block->next_page = 0;
	    block->nr_invalid_pages = 0;
	    atomic_set(&block->data_cmnt_size, 0);
	}
    }

out:
    return ret;
}

static struct kmem_cache *nba_rq_cache;

static void *nba_init(struct nvm_dev *dev, struct gendisk *tdisk, int lun_begin, int lun_end)
{
    struct request_queue *bqueue = dev->q;
    struct request_queue *tqueue = tdisk->queue;
    struct nba *api;
    int ret;

    api = kzalloc(sizeof(struct nba), GFP_KERNEL);
    if (!api)
    {
	ret = -ENOMEM;
	goto err;
    }

    api->instance.tt = &tt_nba;
    api->dev = dev;
    api->disk = tdisk;

    api->luns = NULL;
    api->nr_luns = lun_end - lun_begin + 1;
    api->total_blocks = 0;
    api->nr_pages = 0;

    ret = nba_luns_init(api, lun_begin, lun_end);
    if(ret)
    {
	NBA_PRINT("error initializing luns");
	goto err;
    }

    nba_rq_cache = kmem_cache_create("nba_rq",
		    sizeof(struct nvm_rq),
		    0, 0, NULL);

    api->rq_pool = mempool_create_slab_pool(64, nba_rq_cache);
    if (!api->rq_pool)
    {
	ret = -ENOMEM;
	goto err;
    }

    tdisk->fops = &nba_fops;

    blk_queue_logical_block_size(tqueue, queue_physical_block_size(bqueue));
    blk_queue_max_hw_sectors(tqueue, queue_max_hw_sectors(bqueue));

    NBA_PRINT("initialized api with %lu luns, %lu blocks and %lu pages",    api->nr_luns,
									    api->total_blocks,
									    api->nr_pages);
    return api;

err:
    nba_free(api);
    return ERR_PTR(ret);
}

static void nba_exit(void *private)
{
    struct nba *api = private;

    nba_free(api);
}

static struct nvm_tgt_type tt_nba = {
	.name		= "nba",

	.make_rq	= nba_make_rq,
	.capacity	= nba_capacity,
	.end_io	    	= nba_end_io,

	.init		= nba_init,
	.exit		= nba_exit,
};

static int __init nba_module_init (void)
{
    return nvm_register_target(&tt_nba);
}

static void nba_module_exit (void)
{
    nvm_unregister_target(&tt_nba);
}

module_init(nba_module_init);
module_exit(nba_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Block API for IO");
