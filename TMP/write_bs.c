#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <mark.h>
#include <bufpool.h>
#include <paging.h>

int write_bs(char *src, bsd_t store, int page) {

	if (INVALID_BSID(store) || INVALID_NPAGES(page)) {
		//kprintf("\nERROR:write_bs:Invalid Arguments\n");
		return SYSERR;
	}

	void * phy_addr = (void*)(BACKING_STORE_BASE + (store * BACKING_STORE_UNIT_SIZE) + (page * NBPG));
	bcopy((void*) src, phy_addr, NBPG);
	return OK;
}

