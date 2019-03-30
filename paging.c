#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "paging.h"
#include "fs.h"

/* Allocate eight consecutive disk blocks.
 * Save the content of the physical page in the pte
 * to the disk blocks and save the block-id into the
 * pte.
 */
void
swap_page_from_pte(pte_t *pte)
{
	uint blk = balloc_page(1);
	uint physicalPageAddress = PTE_ADDR(*pte);

	//is P2V... actually the address of the page? CHECK
	write_page_to_disk(1,P2V(physicalPageAddress),blk);

	//save block-id in pte
	*pte = blk<<12 | PTE_FLAGS(*pte);

	//mark the pte as invalid(->present bit=>false)
	//last bit is present bit
	*pte &= ~PTE_P;
	*pte |= PTE_SWAPPED;
}

/* Select a victim and swap the contents to the disk.
 */
int
swap_page(pde_t *pgdir)
{
	panic("swap_page is not implemented");
	return 1;
}

/* Map a physical page to the virtual address addr.
 * If the page table entry points to a swapped block
 * restore the content of the page from the swapped
 * block and free the swapped block.
 */
void
map_address(pde_t *pgdir, uint addr)
{
	panic("map_address is not implemented");
}

/* page fault handler */
void
handle_pgfault()
{
	unsigned addr;
	struct proc *curproc = myproc();

	asm volatile ("movl %%cr2, %0 \n\t" : "=r" (addr));
	addr &= ~0xfff;
	map_address(curproc->pgdir, addr);
}
