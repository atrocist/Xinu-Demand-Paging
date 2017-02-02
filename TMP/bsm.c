/* bsm.c - manage the backing store mapping*/

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>
bs_map_t bsm_tab[NUM_BS];


void init_BSEntry(int i){
	bsm_tab[i].bs_isPriv = BSM_SH;
	bsm_tab[i].bs_npages = 0;
	bsm_tab[i].bs_ref = 0;
	bsm_tab[i].bs_status = BSM_UNMAPPED;
	bsm_tab[i].bs_vpno = -1;
}


/*-------------------------------------------------------------------------
 * init_bsm- initialize bsm_tab
 *-------------------------------------------------------------------------
 */

SYSCALL init_bsm() {
	int i;
	for (i = 0; i < NUM_BS; i++) {
		init_BSEntry(i);
	}
	return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
 
SYSCALL get_bsm(int *avail) {
	int i;
	for (i = 0; i < NUM_BS; i++) {
		if (bsm_tab[i].bs_status == BSM_UNMAPPED){
			*avail = i;
			return OK;
		}
	}
	*avail = -1;
	//kprintf("\nERROR:get_bsm:no unmapped bsm available\n");
	return SYSERR;
}

/*-------------------------------------------------------------------------
 * free_bsm - free an entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i) {
	if( INVALID_BSID(i)){
		//kprintf("\nERROR:free_bsm:Invalid Arguments\n");
		return SYSERR;
	}
	if (bsm_tab[i].bs_ref > 0) {
		//kprintf("\nERROR:free_bsm:freeing a bsm whose ref count is not 0\n");
		return SYSERR;
	}
	init_BSEntry(i);
	return OK;

}

/*-------------------------------------------------------------------------
 * bsm_lookup - lookup bsm_tab and find the corresponding entry
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth) {

	unsigned int i;
	unsigned int vpno = VAD2VPN(vaddr);

	for (i = 0; i < NUM_BS; i++) {
		if ((proctab[pid].bs_map[i].bs_status == BSM_MAPPED) 
			&& (proctab[pid].bs_map[i].bs_vpno <= vpno) 
			&& (proctab[pid].bs_map[i].bs_vpno + proctab[pid].bs_map[i].bs_npages > vpno)) {
				break;
		}
	}
	if(INVALID_BSID(i)){
		*store = -1;
		*pageth = -1;
		//kprintf("\nERROR:bsm_lookup:page not mapped to any bsm\n");
		return SYSERR;
	}else{
		*pageth = vpno - proctab[pid].bs_map[i].bs_vpno;
		*store = i;			
		return OK;
	}
}

/*-------------------------------------------------------------------------
 * bsm_map - add an mapping into bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages) {

	//changed
	if (INVALID_BSID(source) || INVALID_NPAGES(npages)) {
		//kprintf("\nERROR:bsm_map:Invalid Arguments\n");
		return SYSERR;
	}
	/*trying to remap an already mapped bs*/
	if( proctab[pid].bs_map[source].bs_status != BSM_UNMAPPED ){
		//kprintf("\nERROR:bsm_map:Trying to map already mapped bs\n");
		return SYSERR;		
	}	
	/*trying to map a private BS*/
	if(bsm_tab[source].bs_isPriv == BSM_PR){
		//kprintf("\nERROR:bsm_map:Trying to map a private bs\n");
		return SYSERR;		
	}

	if (bsm_tab[source].bs_status == BSM_UNMAPPED) {
		bsm_tab[source].bs_status = BSM_MAPPED;
		bsm_tab[source].bs_isPriv = BSM_SH;
        bsm_tab[source].bs_npages = npages;
        bsm_tab[source].bs_vpno = vpno;
        bsm_tab[source].bs_ref = 1;
	}else{
        /*shared bs*/
			if((bsm_tab[source].bs_npages < npages)) 
			{
					//kprintf("\nERROR:bsm_map:requested number of pages exceeds available pages in shared bs\n");
					return SYSERR;
            }
			bsm_tab[source].bs_status = BSM_MAPPED;
			bsm_tab[source].bs_isPriv = BSM_SH;
			bsm_tab[source].bs_vpno = vpno;
            bsm_tab[source].bs_ref++;	
	}
	
	proctab[pid].bs_map[source].bs_status = BSM_MAPPED;
	proctab[pid].bs_map[source].bs_vpno = vpno;
	proctab[pid].bs_map[source].bs_npages = npages;
	proctab[pid].bs_map[source].bs_ref = 1;
	proctab[pid].bs_map[source].bs_isPriv = BSM_SH;
	return OK;
}

/*-------------------------------------------------------------------------
 * bsm_unmap - delete an mapping from bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno, int flag) {

	int store, pageth;
	int vpno_temp;
	int frame_ID;
	unsigned int vaddr;
	pd_t *pd_entry;
	pt_t *pt_entry;
	
	if (bsm_lookup(pid, VPN2VAD(vpno), &store, &pageth) == SYSERR) {
		//kprintf("\nERROR:bsm_unmap:Page not mapped to any bs\n");
		return SYSERR;
	}
	
	if (flag == BSM_PR) {
		proctab[pid].vmemlist.mlen = 0;
		proctab[pid].vmemlist.mnext = 0;	
		init_BSEntry(store);
	} else {
		vpno_temp = vpno;
		while (proctab[pid].bs_map[store].bs_npages + proctab[pid].bs_map[store].bs_vpno > vpno_temp) {
			vaddr = VPN2VAD(vpno_temp);
			pd_entry = proctab[pid].pdbr + PD_OFFSET(vaddr) * sizeof(pd_t);
			pt_entry = (pd_entry->pd_base) * NBPG + sizeof(pt_t) * PT_OFFSET(vaddr);
			frame_ID = pt_entry->pt_base - FRAME0;
			if (frm_tab[frame_ID].fr_status == FRM_MAPPED){
				free_frm(frame_ID);
			}
			if (pd_entry->pd_pres == 0){
				break;
			}
			vpno_temp++;
		}
	}
	bsm_tab[store].bs_ref--;
	
	proctab[pid].bs_map[store].bs_status = BSM_UNMAPPED;
	proctab[pid].bs_map[store].bs_vpno = -1;
	proctab[pid].bs_map[store].bs_npages = 0;
	proctab[pid].bs_map[store].bs_ref = 0;
	proctab[pid].bs_map[store].bs_isPriv = BSM_SH;
		
	return OK;
}