#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

SYSCALL release_bs(bsd_t bs_id) {

	if (INVALID_BSID(bs_id)) {
		//kprintf("\nERROR:release_bs:Invalid Arguments\n");
		return SYSERR;
	}
	free_bsm(bs_id);
	return OK;
}

