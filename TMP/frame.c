/* frame.c - manage physical frames */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>
fr_map_t frm_tab[NFRAMES];

int globalPageTable[4];
/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL init_frm() {
	int i;
	head_FIFO.frameID = -1;
	head_FIFO.nextFrame = NULL;
	LRU_Count = 0;
	for (i = 0; i < NFRAMES; i++) {
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = -1;		
		frm_tab[i].fr_dirty = 0;
		frm_tab[i].fr_loadtime = -1;		
	}
	return OK;
}

/*-------------------------------------------------------------------------
 * get_frm - get a free frame according page replacement policy
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int *avail) {
	STATWORD ps;
	int i,j,policy;
	unsigned long int min = 4294967295;
	fr_map_t *tmp_frame;
	
	disable(ps);
	//Search inverted page table for an empty frame. If one exists stop.
	for (i = 0; i < NFRAMES; i++) {
		if (frm_tab[i].fr_status == FRM_UNMAPPED) {
			*avail = i;
			restore(ps);
			return OK;
		}
	}
	//Else, pick a page to replace.
	policy = grpolicy();
	if (policy == LRU) {
		for (i = 0; i < NFRAMES; i++) {
			tmp_frame = &frm_tab[i];
			if (tmp_frame->fr_type == FR_PAGE) {
				if (tmp_frame->fr_loadtime < min) {
					min = tmp_frame->fr_loadtime;
					j = i;
				}
				else if (tmp_frame->fr_loadtime == min) {
					if (frm_tab[j].fr_vpno < tmp_frame->fr_vpno){
						j = i;
					}
				}
			}
		}
		free_frm(j);
		*avail = j;
		restore(ps);
		return OK;
	}else if (policy == FIFO) {
		fifo_q_t *FIFO_tail = &head_FIFO;
		if(FIFO_tail == NULL){
			//kprintf("error: empty fifo queue");
			restore(ps);
			return SYSERR;
		}
		while (FIFO_tail->nextFrame != NULL){
			FIFO_tail = FIFO_tail->nextFrame;
		}		
		*avail = FIFO_tail->frameID;
		if(SYSERR == free_frm(FIFO_tail->frameID)){		
			*avail = -1;
			restore(ps);
			return SYSERR;
		}		
		if(SYSERR == freemem(FIFO_tail, sizeof(fifo_q_t))){
			*avail = -1;
			restore(ps);
			return SYSERR;
		}
		
		restore(ps);
		return OK;
	}
	restore(ps);
	return SYSERR;
}

/*-------------------------------------------------------------------------
 * free_frm - free a frame
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int i) {

	STATWORD ps;
	disable(ps);
	unsigned int vaddr;
	if (INVALID_NFRAMES(i)) {
		restore(ps);
		return SYSERR;
	}


	if(frm_tab[i].fr_type == FR_PAGE)
	{
		int store, pageth;
		unsigned int p,q;
		//Using the inverted page table, get vp, the virtual page number of the page to be replaced.
		//Let vaddr be vp*NBPG (the first virtual address on page vp).
		vaddr = VPN2VAD(frm_tab[i].fr_vpno);
		//Let p be the high 10 bits of vaddr. Let q be bits [21:12] of a.
		p = PD_OFFSET(vaddr);
		q = PT_OFFSET(vaddr);
		
		if ((bsm_lookup(frm_tab[i].fr_pid, frm_tab[i].fr_vpno * NBPG, &store, &pageth) == SYSERR) || (store == -1) || (pageth == -1)) {
			//kprintf("\nERROR:free_frm:page not mapped to any bs\n");
			restore(ps);
			return SYSERR;
		}
		//Let pd point to the page directory of process pid.
		pd_t *pd = proctab[frm_tab[i].fr_pid].pdbr;
		//Let pt point to the pid's pth page table.
		pd_t *pt = proctab[frm_tab[i].fr_pid].pdbr + (p * sizeof(pd_t));
		//Mark the appropriate entry of pt as not present.
		pt_t *pt_entry = (pt->pd_base) * NBPG + (q * sizeof(pt_t));
		if (pt_entry->pt_pres == 0) {
			restore(ps);
			return SYSERR;
		}
		//If the dirty bit for page vp was set in its page table you must do the following:
		//Using the backing store map to find the store and page offset within store given pid and a. 
		//If the lookup fails, something is wrong. Print an error message and kill the process pid.
		//Write the page back to the backing store.
		if( pt_entry->pt_dirty == DIRTY){
			write_bs((i + FRAME0) * NBPG, store, pageth);
		}
		clear_PTE(pt_entry);
		clear_FTE(pt, i);
		//If the page being removed belongs to the current process, invalidate the TLB entry for the page vp using the invlpg instruction (see Intel Volume III/II).
		if(frm_tab[i].fr_pid == currpid){
			write2CR3(currpid);
		}
		restore(ps);
		return OK;
	}else if(frm_tab[i].fr_type == FR_TBL){
		int j;
		for (j = 0; j < 1024; j++) {
			pt_t *pt_entry = (i + FRAME0) * NBPG + j * sizeof(pt_t);
			if (pt_entry->pt_pres == 1) {
				free_frm(pt_entry->pt_base - FRAME0);
			}
		}
		for (j = 0; j < 1024; j++) {
			pd_t *pt = proctab[frm_tab[i].fr_pid].pdbr + j * sizeof(pd_t);
			if (pt->pd_base - FRAME0 == i) {
				pt->pd_pres = 0;
				pt->pd_write = 1;
				pt->pd_user = 0;
				pt->pd_pwt = 0;
				pt->pd_pcd = 0;
				pt->pd_acc = 0;
				pt->pd_mbz = 0;
				pt->pd_fmb = 0;
				pt->pd_global = 0;
				pt->pd_avail = 0;
				pt->pd_base = 0;
			}
		}
		clear_FTE(NULL, i);
		restore(ps);
		return OK;
	}else{
		int j;
		//first 4 page tables are global page tables 
		for (j = 4; j < 1024; j++) {
			pd_t *pt = proctab[frm_tab[i].fr_pid].pdbr + j * sizeof(pd_t);
			if (pt->pd_pres == 1) {
				free_frm(pt->pd_base - FRAME0);
			}
		}
		clear_FTE(NULL, i);
		restore(ps);
		return OK;
	}
	restore(ps);
	return SYSERR;
}

int create_PD(int pid) {
	int i;
	int frame;

	if (SYSERR == get_frm(&frame)) {
		return SYSERR;
	}
	
	frm_tab[frame].fr_dirty = CLEAN;
	frm_tab[frame].fr_loadtime = -1;
	frm_tab[frame].fr_pid = pid;
	frm_tab[frame].fr_refcnt = 1; 
	frm_tab[frame].fr_status = FRM_MAPPED;
	frm_tab[frame].fr_type = FR_DIR;
	frm_tab[frame].fr_vpno = -1;

	proctab[pid].pdbr = (FRAME0 + frame) * NBPG;
	for (i = 0; i < 1024; i++) {
		pd_t *pt = proctab[pid].pdbr + (i * sizeof(pd_t));
		pt->pd_pres = 0;
		pt->pd_write = 1;
		pt->pd_user = 0;
		pt->pd_pwt = 0;
		pt->pd_pcd = 0;
		pt->pd_acc = 0;
		pt->pd_mbz = 0;		
		pt->pd_fmb = 0;
		pt->pd_global = 0;
		pt->pd_avail = 0;
		pt->pd_base = 0;
		
		if (i < 4) {
			pt->pd_pres = 1;
			pt->pd_write = 1;
			pt->pd_base = globalPageTable[i];
		}
	}
	return frame;
}

int create_GlobalPT() {
	int i, j, k;
	for (i = 0; i < 4; i++) {
		k = create_PT(NULLPROC);
		if (k == SYSERR) {
			return SYSERR;
		}
		globalPageTable[i] = FRAME0 + k;

		for (j = 0; j < 1024; j++) {
			pt_t *pt = globalPageTable[i] * NBPG + j * sizeof(pt_t);

			pt->pt_pres = 1;
			pt->pt_write = 1;
			pt->pt_base = j + i * 1024;

			frm_tab[k].fr_refcnt++;
		}
	}
	return OK;
}

void clear_PTE(pt_t *pt_entry) {
	pt_entry->pt_pres = 0;
	pt_entry->pt_write = 1;
	pt_entry->pt_user = 0;
	pt_entry->pt_pwt = 0;
	pt_entry->pt_pcd = 0;
	pt_entry->pt_acc = 0;
	pt_entry->pt_dirty = 0;
	pt_entry->pt_mbz = 0;
	pt_entry->pt_global = 0;
	pt_entry->pt_avail = 0;
	pt_entry->pt_base = 0;
}

void clear_FTE(pd_t *pt, int i) {

	if (pt != NULL) {
		frm_tab[pt->pd_base - FRAME0].fr_refcnt--;
		if (frm_tab[pt->pd_base - FRAME0].fr_refcnt <= 0) {
			free_frm(pt->pd_base - FRAME0);
		}
	}
	frm_tab[i].fr_status = FRM_UNMAPPED;
	frm_tab[i].fr_pid = -1;
	frm_tab[i].fr_vpno = -1;
	frm_tab[i].fr_refcnt = 0;
	frm_tab[i].fr_type = -1;
	frm_tab[i].fr_dirty = CLEAN;
	frm_tab[i].fr_loadtime = -1;

}

int create_PT(int pid) {

	int i;
	int frame;
	if (SYSERR == get_frm(&frame)) {
		return SYSERR;
	}

	frm_tab[frame].fr_status = FRM_MAPPED;
	frm_tab[frame].fr_pid = pid;
	frm_tab[frame].fr_vpno = -1;
	frm_tab[frame].fr_refcnt = 0;
	frm_tab[frame].fr_type = FR_TBL;
	frm_tab[frame].fr_dirty = CLEAN;
	frm_tab[frame].fr_loadtime = -1;

	//clear all page table entries
	for (i = 0; i < 1024; i++) {
		pt_t *pt = ((FRAME0 + frame) * NBPG) + (i * sizeof(pt_t));
		pt->pt_user = 0;
		pt->pt_pwt = 0;
		pt->pt_pcd = 0;
		pt->pt_acc = 0;
		pt->pt_dirty = CLEAN;
		pt->pt_write = 1;
		pt->pt_mbz = 0;
		pt->pt_global = 0;
		pt->pt_avail = 0;
		pt->pt_base = 0;
		pt->pt_pres = 0;
	}
	return frame;
}
