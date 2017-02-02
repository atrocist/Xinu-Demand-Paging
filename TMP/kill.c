/* kill.c - kill */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>
#include <paging.h>

/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid) {
	STATWORD ps;
	struct pentry *pptr; /* points to proc. table for pid*/
	int dev;
	int i;

	disable(ps);
	if (isbadpid(pid) || (pptr = &proctab[pid])->pstate == PRFREE) {
		restore(ps);
		return (SYSERR);
	}
	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (!isbaddev(dev))
		close(dev);
	dev = pptr->pdevs[1];
	if (!isbaddev(dev))
		close(dev);
	dev = pptr->ppagedev;
	if (!isbaddev(dev))
		close(dev);
	
	send(pptr->pnxtkin, pid);
	freestk(pptr->pbase, pptr->pstklen);
	
	//unmap bs and free page directory
	for (i = 0; i < NUM_BS; i++) {
		if (proctab[pid].bs_map[i].bs_status == BSM_MAPPED){
			if(SYSERR == bsm_unmap(pid, proctab[pid].bs_map[i].bs_vpno, proctab[pid].bs_map[i].bs_isPriv)){
				//kprintf("\nERROR:kill:bsm_unmap failed for bsid %d\n",i);
				restore(ps);
				return SYSERR;
			}
		}
	}
	if(SYSERR == free_frm(proctab[pid].pdbr/NBPG - FRAME0)){
		//kprintf("\nERROR:kill:free_frm failed\n");
		restore(ps);
		return SYSERR;
	}
	
	switch (pptr->pstate) {

	case PRCURR:
		pptr->pstate = PRFREE; /* suicide */
		resched();

	case PRWAIT:
		semaph[pptr->psem].semcnt++;

	case PRREADY:
		dequeue(pid);
		pptr->pstate = PRFREE;
		break;

	case PRSLEEP:
	case PRTRECV:
		unsleep(pid);
		/* fall through	*/
	default:
		pptr->pstate = PRFREE;
	}
	restore(ps);
	return (OK);
}
