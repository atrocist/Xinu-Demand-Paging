/* pfint.c - pfint */

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint() {

	STATWORD ps;
	disable(ps);

	int store, pageth;
	int frame;
	unsigned long a;
	unsigned long vp;
	unsigned int p,q;
	fifo_q_t *FIFO_tmp;
	
	int i, j, pid;
	struct pentry *pptr;
	unsigned long pdbr;
	pd_t *pageDir_entry;
	pt_t *pageTable_entry;
	
	//Get the faulted address 'a'
	a = read_cr2();
	
	//Check if 'a' is a legal address i.e., it has been mapped or not. If not, print error message and kill process.
	//Using the backing store map, find the store s and page offset o which correspond to vp.
	if ((bsm_lookup(currpid, a, &store, &pageth) == SYSERR)) {
		kill(currpid);
		restore(ps);
		return (SYSERR);
	}
	
	//Let vp be the virtual page number of the page containing of the faulted address
	vp = VAD2VPN(a);
	//Let pd point to the current page directory.
	pd_t *pd = proctab[currpid].pdbr;
	//Let p be the upper ten bits of a. [What does p represent?] PD_OFFSET i.e offset to pd entry i.t the page table
	p = PD_OFFSET(a);
	//Let q be the bits [21:12] of a. [What does q represent?]	PT_OFFSET i.e offset to pt entry i.e the page table entry
	q = PT_OFFSET(a);
	//Let pt point to the pth page table.
	pd_t *pt = proctab[currpid].pdbr + (sizeof(pd_t) * p); //current page table

	//If the pth page table does not exist obtain a frame for it and initialize it	
	if (pt->pd_pres == 0) {
		frame = create_PT(currpid);
		if (frame == SYSERR) {
		//	kprintf("\nERROR:pfint:failed to create page table\n");
			return SYSERR;
		}
		pt->pd_pres = 1;
		pt->pd_write = 1;
		pt->pd_base = frame + FRAME0;
		//In the inverted page table increment the reference count of the frame which holds pt.This indicates that one more of pt's entries is marked "present."
		frm_tab[(unsigned int) pt / NBPG - FRAME0].fr_refcnt++;
	}
	
	
	//Obtain a free frame f
	if (SYSERR == get_frm(&frame)) {
	//	kprintf("\nERROR:pfint: failed to get new frame\n");
		kill(currpid);
		restore(ps);
		return SYSERR;
	}
	
	frm_tab[frame].fr_status = FRM_MAPPED;
	frm_tab[frame].fr_pid = currpid;
	frm_tab[frame].fr_vpno = VAD2VPN(a);
	frm_tab[frame].fr_refcnt = 1;
	frm_tab[frame].fr_type = FR_PAGE;
	frm_tab[frame].fr_dirty = CLEAN;
	frm_tab[frame].fr_loadtime = -1;

	//Copying page 'pageth' of store 'store' to frame.
	read_bs((frame + FRAME0) * NBPG, store, pageth);

	
	
	//Update pt to mark the appropriate entry as present and set any other fields. 
	pt_t *pageTableEntry = ((pt->pd_base) * NBPG) + (sizeof(pt_t) * q);
	pageTableEntry->pt_pres = 1;
	pageTableEntry->pt_write = 1;
	//Also set the address portion within the entry to point to frame
	pageTableEntry->pt_base = frame + FRAME0;
	frm_tab[(unsigned int) pageTableEntry / NBPG - FRAME0].fr_refcnt++;

	if (page_replace_policy == FIFO){
		FIFO_tmp = (fifo_q_t *) getmem(sizeof(fifo_q_t));
		FIFO_tmp->frameID = frame;
		FIFO_tmp->nextFrame = head_FIFO.nextFrame;
		head_FIFO.nextFrame = FIFO_tmp;
	}
	else if (page_replace_policy == LRU) {
		LRU_Count++;
		for (pid = 1; pid < NPROC; pid++) {
			pptr = &proctab[pid];
			if (pptr->pstate != PRFREE) {
				pdbr = pptr->pdbr;
				pageDir_entry = pdbr;
				for (i = 0; i < 1024; i++) {
					if (pageDir_entry->pd_pres == 1) {
						pageTable_entry = (pageDir_entry->pd_base) * NBPG;
						for (j = 0; j < 1024; j++) {
							if (pageTable_entry->pt_pres == 1 && pageTable_entry->pt_acc == 1) {
								frame = pageTable_entry->pt_base - FRAME0;
								pageTable_entry->pt_acc = 0;
								frm_tab[frame].fr_loadtime = LRU_Count;
							}
							pageTable_entry++;
						}
					}
					pageDir_entry++;
				}
			}
		}
	}
	write2CR3(currpid);
	restore(ps);
	return OK;
}


int write2CR3(int pid) {
	write_cr3(proctab[pid].pdbr);
	return OK;
}
