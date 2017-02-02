#include <conf.h>
#include <kernel.h>
#include <mark.h>
#include <bufpool.h>
#include <proc.h>
#include <paging.h>

SYSCALL read_bs(char *dst, bsd_t store, int page) {

	if (INVALID_BSID(store) || INVALID_NPAGES(page)) {
		//kprintf("\nERROR:read_bs:Invalid Arguments\n");
		return SYSERR;
	}

	void * phy_addr = (void*)(BACKING_STORE_BASE + (store * BACKING_STORE_UNIT_SIZE) + (page * NBPG));
	bcopy(phy_addr, (void*) dst, NBPG);
	return OK;
}

