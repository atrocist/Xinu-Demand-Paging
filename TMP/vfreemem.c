/* vfreemem.c - vfreemem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>

/*------------------------------------------------------------------------
 *  vfreemem  --  free a virtual memory block, returning it to vmemlist
 *------------------------------------------------------------------------
 */
SYSCALL vfreemem(struct mblock *block, unsigned int size) {
        STATWORD ps;
		struct pentry *pptr = &proctab[currpid];
        struct mblock *vmlist = &pptr->vmemlist;
        struct mblock *prev = vmlist;
        struct mblock *next = prev->mnext; 		
		unsigned top;		
		top = (unsigned)block;		
		disable(ps);
		
        if (size == 0 || (size > pptr->vhpnpages*NBPG) 
			|| (top < (pptr->vhpno * NBPG))
			|| (top > ((pptr->vhpno *NBPG) + (pptr->vhpnpages * NBPG)))
			|| ((top + size) > ((pptr->vhpno *NBPG)+(pptr->vhpnpages * NBPG)))){
		//	kprintf("\nERROR:vfreemem:Invalid Arguments\n");
			restore(ps);
			return SYSERR;        
		}
		size = roundmb(size);
		
		if(next == NULL){
			prev->mnext = block;
			next = prev->mnext;
			next->mlen = size;
			restore (ps);
			return OK;
		}
		 while((next != (struct mblock *) NULL) && (next < block)) {
			prev=next;
			next=next->mnext;
		}
		top = prev->mlen + (unsigned)prev;
		if( (prev != vmlist) && (top > (unsigned)block) )
		{
			//kprintf("\nERROR:vfreemem:top of free mem > block\n");
			restore(ps);
			return SYSERR;
		}
		
		if( (next != NULL) && ((size + (unsigned)block) > (unsigned)next) )
		{
			//kprintf("\nERROR:vfreemem:memory is already free\n");
			restore(ps);
			return SYSERR;
		}
		
		if ((prev != vmlist) && (top == (unsigned)block)) {
			prev->mlen += size;
		} else {
			block->mlen = size;
			block->mnext = next;
			prev->mnext = block;
			prev = block;
		}
		if ((unsigned)(prev->mlen + (unsigned)prev) == (unsigned)next) {
			prev->mlen += next->mlen;
			prev->mnext = next->mnext;
		}
		restore(ps);
		return(OK);
}
