/* vgetmem.c - vgetmem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>

/*------------------------------------------------------------------------
 * vgetmem  --  allocate virtual heap storage, returning lowest WORD address
 *------------------------------------------------------------------------
 */
WORD *vgetmem(unsigned int nbytes) {

        STATWORD ps;
        struct mblock *leftover;
		struct pentry *pptr = &proctab[currpid];
        struct mblock *vmlist = &pptr->vmemlist;
		struct mblock *prev = vmlist;
        struct mblock *next = vmlist->mnext;
        disable(ps);
        
		
        if ( nbytes == 0 || vmlist->mnext == (struct mblock *) NULL ) {
			//	kprintf("\nERROR:vgetmem:Invalid Arguments\n");
                restore(ps);
                return (WORD*) ( SYSERR);
        }      
		nbytes = (unsigned int) roundmb(nbytes);
        for (prev= vmlist,next=vmlist->mnext ;
	     next != (struct mblock *) NULL ;
	     prev=next,next=next->mnext)
		if ( next->mlen == nbytes) {
			prev->mnext = next->mnext;
			restore(ps);
			return( (WORD *)next );
		} else if ( next->mlen > nbytes ) {
			leftover = (struct mblock *)( (unsigned)next + nbytes );
			prev->mnext = leftover;
			leftover->mnext = next->mnext;
			leftover->mlen = next->mlen - nbytes;
			restore(ps);
			return( (WORD *)next );
		}
        restore(ps);
		//kprintf("ERROR:vgetmem:Out of memory\n");
        return (WORD*) ( SYSERR);

}
