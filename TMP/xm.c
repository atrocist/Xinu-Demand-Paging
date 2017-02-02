/* xm.c = xmmap xmunmap */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * xmmap - xmmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmmap(int virtpage, bsd_t source, int npages) {
	/* sanity check ! */
	STATWORD ps;
	if ((virtpage < 4096) || INVALID_BSID(source) || INVALID_NPAGES(npages)) {
		//kprintf("\nERROR:xmmap:Invalid Arguments\n");
		return SYSERR;
	}
	disable(ps);
	if (bsm_map(currpid, virtpage, source, npages) == SYSERR) {
		//kprintf("\nERROR:xmmap:bsm_map failed\n");
		restore(ps);
		return SYSERR;
	}
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * xmunmap - xmunmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmunmap(int virtpage) {
	/* sanity check ! */
	STATWORD ps;
	if ((virtpage < 4096)) {
		//kprintf("\nERROR:xmunmap:Invalid Arguments\n");
		return SYSERR;
	}

	disable(ps);
	if (bsm_unmap(currpid, virtpage, 0) == SYSERR) {
		//kprintf("\nERROR:xmunmap:bsm_unmap failed\n");
		restore(ps);
		return SYSERR;
	}
	restore(ps);
	return OK;
}
