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
  cprintf("swapping page pte %x\n", *pte);
  uint blk = balloc_page(1);
  uint physicalPageAddress = PTE_ADDR(*pte);

  //is P2V... actually the address of the page? CHECK
  cprintf("Going to write\n");
  write_page_to_disk(1,P2V(physicalPageAddress),blk);
  cprintf("Written page to disk\n");


  // Invalidate the TLB corresponding to the swapped virtual page.
  // Reloading the cr3 register should cause a TLB Flush
  lcr3(V2P(myproc()->pgdir));

  // Free the physical page.
  kfree((char *)PTE_ADDR(*pte));


  //save block-id in pte
  *pte = blk<<12 | PTE_FLAGS(*pte);

  //mark the pte as invalid(->present bit=>false)
  //last bit is present bit
  *pte &= ~PTE_P;
  *pte |= PTE_SWAPPED;
  cprintf("SWAPPED page pte %x\n", *pte);
}

/* Select a victim and swap the contents to the disk.
 */
int
swap_page(pde_t *pgdir)
{
  pte_t *pte = select_a_victim(pgdir);
  cprintf("Selected victim to be %x\n",*pte);
  cprintf("Victim's P Bit %d\n",(*pte)&PTE_P);
  cprintf("Victim's A Bit %d\n",(*pte)&PTE_A);
  cprintf("Victim's SWAPPED Bit %d\n",(*pte)&PTE_SWAPPED);
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
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

static pte_t *
walkpgdirDONTCreate(pde_t *pgdir, const void *va)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P) pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  else return 0;
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
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
        cprintf("allocatedPage: %d\n",allocatedPage);
        // panic("You want me to kalloc a page in map_address huh?. Sorry I wont!");
        cprintf("You want me to kalloc a page in map_address huh?. Sorry I wont!\n");
        cprintf("I can swap instead :D\n");
        swap_page(pgdir);
    }

    uint blkNumber;

    pte_t *pte = walkpgdirDONTCreate(pgdir,(void *)addr);
    if(*pte!=0){
        if( ((*pte) & PTE_SWAPPED) && (!((*pte) & PTE_P)) ){

            blkNumber = getswappedblk(pgdir, addr);
            read_page_from_disk(1,allocatedPage,blkNumber);
            
            *pte = V2P(allocatedPage) | PTE_FLAGS(*pte);

            *pte |= PTE_P; //Setting Present Bit to 1
            *pte &= ~PTE_SWAPPED; //Setting SWAPPED Bit to 0
            bfree_page(1,blkNumber);
        } else{
            cprintf("((*pte) & PTE_SWAPPED):%d \n((*pte) & PTE_P):%d\n", ((*pte) & PTE_SWAPPED),((*pte) & PTE_P));
            panic("map_address is being called on a pte which is !(SWAPPED=1,PTE_P=0)");  
        } 
        return;
    }

    mappages(pgdir,(void *)addr,PGSIZE, V2P(allocatedPage), PTE_W|PTE_U);
}

/* page fault handler */
void
handle_pgfault()
{
  // cprintf("got page fault. Handling it");
  unsigned addr;
  struct proc *curproc = myproc();

  asm volatile ("movl %%cr2, %0 \n\t" : "=r" (addr));
  addr &= ~0xfff;
  map_address(curproc->pgdir, addr);
}
