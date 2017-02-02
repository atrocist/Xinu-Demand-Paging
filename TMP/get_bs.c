#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

int get_bs(bsd_t store, unsigned int npages) {

	STATWORD ps;
	if (INVALID_BSID(store) || INVALID_NPAGES(npages)) {
		//kprintf("\nERROR:get_bs: Invalid arguments\n");
		return SYSERR;
	}
	disable(ps);
	/*requested bs is private heap*/
	if (bsm_tab[store].bs_status == BSM_UNMAPPED) {
		restore(ps);
		return npages;
	} else {
		if ((bsm_tab[store].bs_isPriv == BSM_PR)) {
			//kprintf("\nERROR:get_bs:Trying to get a private bs\n");
			restore(ps);
			return SYSERR;
		}
		restore(ps);
		return bsm_tab[store].bs_npages;
	}
}

