/* vcreate.c - vcreate */

#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <paging.h>

/*
 static unsigned long esp;
 */

LOCAL newpid();
/*------------------------------------------------------------------------
 *  create  -  create a process to start running a procedure
 *------------------------------------------------------------------------
 */
SYSCALL vcreate(procaddr, ssize, npages, priority, name, nargs, args)
	int *procaddr; /* procedure address		*/
	int ssize; /* stack size in words		*/
	int npages; /* virtual heap size in pages	*/
	int priority; /* process priority > 0		*/
	char *name; /* name (for debugging)		*/
	int nargs; /* number of args that follow	*/
	long args; /* arguments (treated like an	*/
/* array in the code)		*/
{
	STATWORD ps;
	disable(ps);
	struct mblock *first_block;
	int source = 0;		
		
	if(INVALID_NPAGES(npages)){
		//kprintf("\nERROR:vcreate:Invalid Arguments\n");
		restore(ps);
		return SYSERR;		
	}
	if ((SYSERR == get_bsm(&source))) {
		//kprintf("\nERROR:vcreate:No bs available\n");
		restore(ps);
		return SYSERR;
	}	
	int pid = create(procaddr, ssize, priority, name, nargs, args);
	if(isbadpid(pid)){
		//kprintf("\nERROR:vcreate:Create proc failed\n");
		restore (ps);
		return SYSERR;
	}
	
	if (bsm_map(pid, 4096, source, npages) == SYSERR) {
		//kprintf("\nERROR:vcreate:bsm_map failed\n");
		restore(ps);
		return SYSERR;
	}

	first_block = (struct mblock*)BSID2PA(source);
	first_block->mnext = NULL;
	first_block->mlen = npages*NBPG;
	
	proctab[pid].store = source;
	proctab[pid].vhpno = 4096;
	proctab[pid].vhpnpages = npages;
	proctab[pid].bs_map[source].bs_isPriv = BSM_PR;
	proctab[pid].vmemlist.mlen = npages * NBPG;
	proctab[pid].vmemlist.mnext = (struct mblock*) (4096*NBPG);
			
	bsm_tab[source].bs_isPriv = BSM_PR;
	bsm_tab[source].bs_ref = 1;
	
	restore(ps);
	return pid;
}

/*------------------------------------------------------------------------
 * newpid  --  obtain a new (free) process id
 *------------------------------------------------------------------------
 */
LOCAL newpid() {
	int pid; /* process id to return		*/
	int i;

	for (i = 0; i < NPROC; i++) { /* check all NPROC slots	*/
		if ((pid = nextproc--) <= 0)
			nextproc = NPROC - 1;
		if (proctab[pid].pstate == PRFREE)
			return (pid);
	}
	return (SYSERR);
}
