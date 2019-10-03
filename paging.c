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
    if( !( (*pte)&PTE_P ) )
        panic("trying to swap page from pte, which is not present");

    uint blk = balloc_page(1);
    write_page_to_disk(1,P2V(PTE_ADDR(*pte)),blk);
    kfree(P2V(PTE_ADDR(*pte))); // Free the physical page.

    *pte = blk<<12 | PTE_FLAGS(*pte); //save block-id in pte
    *pte &= ~PTE_P; //mark the pte as invalid(->present bit=>false)
    *pte |= PTE_SWAPPED;

    if( (*pte)&PTE_P )
        panic("Present Bit should be 0 after swapping, but we get non-zero");

    // Invalidate the TLB corresponding to the swapped virtual page.
    // Reloading the cr3 register should cause a TLB Flush
    lcr3(V2P(myproc()->pgdir)); //maybe optimize it - not necessary
}

/* Select a victim and swap the contents to the disk.
 */
int
swap_page(pde_t *pgdir)
{
    pte_t *pte = select_a_victim(pgdir);
    swap_page_from_pte(pte);
    return 1;
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
    pde_t *pde;
    pte_t *pgtab;

    pde = &pgdir[PDX(va)];
    if(*pde & PTE_P){
        pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
    } else {
        if(!alloc) return 0;
        if((pgtab = (pte_t*)kalloc()) == 0) return 0;
        
        // Make sure all those PTE_P bits are zero.
        memset(pgtab, 0, PGSIZE);
        
        // The permissions here are overly generous, but they can
        // be further restricted by the permissions in the page table
        // entries, if necessary.
        *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
    }
    return &pgtab[PTX(va)];
}

/* Map a physical page to the virtual address addr.
 * If the page table entry points to a swapped block
 * restore the content of the page from the swapped
 * block and free the swapped block.
 */
void
map_address(pde_t *pgdir, uint addr)
{
    char* allocatedPage;
    if( (allocatedPage = kalloc()) == 0 ){
        swap_page(pgdir);

        if( (allocatedPage = kalloc()) == 0 )
          panic("Not able to kalloc even after swap");

        memset(allocatedPage, 0, PGSIZE);
    }

    uint blkNumber;
    pte_t *pte;

    if( (pte = walkpgdir(pgdir,(void *)addr,1)) == 0 )
        panic("walkpgdir returned 0 to map_address");
    if( (*pte)&PTE_P )
        panic("Trying to set a page for VA that is already present");

    if( (*pte)&PTE_SWAPPED ){
        blkNumber = getswappedblk(pgdir, addr);
        if(blkNumber == -1)
            panic("getswappedblk returned -1 even though we thought the block was swapped");
        read_page_from_disk(1,allocatedPage,blkNumber);

        if(V2P(allocatedPage)!=PTE_ADDR(V2P(allocatedPage)))
          panic("You were assuming kalloc gives page aligned but NADA :(");
        *pte = V2P(allocatedPage) | PTE_FLAGS(*pte) | PTE_P | PTE_W | PTE_U;
        *pte &= ~PTE_SWAPPED; //Setting SWAPPED Bit to 0

        bfree_page(1,blkNumber);
        return;
    }

    *pte = V2P(allocatedPage) | PTE_W | PTE_U | PTE_P;
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
