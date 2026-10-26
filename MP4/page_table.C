#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

#define RECURSIVE_REF_OFFSET 1023

void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   // set global parameters
   kernel_mem_pool = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size = _shared_size;
}

PageTable::PageTable()
{
   num_pools = 0;
   for (unsigned int i = 0; i < MAX_POOLS; i++)
      registered_pools[i] = nullptr;

   // since the initial x MB have the same logical address, allocate some PT frames for it and set them valid
   unsigned long num_init_frames = PageTable::shared_size / (PAGE_SIZE);
   unsigned long num_init_pt_frames = (num_init_frames / ENTRIES_PER_PAGE) + (num_init_frames % ENTRIES_PER_PAGE > 0 ? 1 : 0); // ceil ( num_frames / num_pt_entries_per_frame )

   // The PD and the shared-space PT frames come from the PROCESS pool (mapped memory).
   // NOTE: the constructor runs before paging is enabled, so the physical addresses
   // can still be dereferenced directly here.
   page_directory = (unsigned long *) (process_mem_pool->get_frames(1 + num_init_pt_frames) << 12);
   unsigned long * init_page_table_frames = (unsigned long *) ((unsigned long) page_directory + PAGE_SIZE);

   if (page_directory == 0) {
      Console::puts("ERROR : Failed to allocate frames for PD and shared space PT frames\n");
      assert(false);
   } else {
      Console::puts("Successfully allocated frames for PD and shared space PT frames\n");
      Console::puts("Page directory physical address: ");
      Console::putui((unsigned long) page_directory);
      Console::puts("\n");
   }

   // initialize the first num_init_pt_frames entries of the PD, set the rest of them to invalid

   // [ <address of shared space page table frame(s)> <...> <U/S=0> <R/W=1><Present=1>]
   for(unsigned long i = 0; i < num_init_pt_frames; i++)
      page_directory[i] = ( ( (unsigned long) init_page_table_frames + i * PAGE_SIZE ) ) | (unsigned long)(3);
   // [ 0 <...> <U/S=0> <R/W=1><Present=0>]
   for(unsigned long i = num_init_pt_frames; i < ENTRIES_PER_PAGE; i++)
      page_directory[i] = (unsigned long)(2);

   // last PDE points back to the PD itself (recursive lookup)
   page_directory[RECURSIVE_REF_OFFSET] = ( (unsigned long) page_directory ) | (unsigned long)(3);

   unsigned long address = 0;
   // initialize the shared space page table(s): identity mapping
   // [ <phys_frame_number> <...> <U/S=0> <R/W=1><Present=1>]
   for(unsigned long i = 0; i < num_init_pt_frames; i++) {
      for(unsigned long j = 0; j < ENTRIES_PER_PAGE; j++) {
         init_page_table_frames[i*ENTRIES_PER_PAGE+j] = address | 3;
         address += PAGE_SIZE;
      }
   }

   Console::puts("Initialized page table object\n");
}


void PageTable::load()
{
   current_page_table = this;
   write_cr3((unsigned long) page_directory);
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   paging_enabled = 1;
   write_cr0(read_cr0() | 0x80000000); // enable paging bit in CR0
   Console::puts("Enabled paging\n");
}

// Logical address functions for recursive lookup:

unsigned long * PageTable::PDE_address(unsigned long addr)
{
   return (unsigned long *) (1023<<22 | 1023 << 12 | (addr >> 20));
}

unsigned long * PageTable::PTE_address(unsigned long addr)
{
   return (unsigned long *) (1023<<22 | (addr>>10) & (0x3FF << 12) | (addr >> 10) & 0xFFF);
}

void PageTable::handle_fault(REGS * _r)
{
   unsigned long fault_address = read_cr2();
   unsigned long error_code = _r->err_code;

   if (error_code & 0x1) {
      // page was present: this is a protection violation, not a missing page
      Console::puts("Protection fault, no access rights\n");
      assert(false);
   }

   // Legitimacy check against the registered VM pools. If no pool has been
   // registered (when only the page table is being tested), every address
   // is considered legitimate.
   if (current_page_table->num_pools > 0) {
      bool legitimate = false;
      for (unsigned int i = 0; i < current_page_table->num_pools; i++) {
         if (current_page_table->registered_pools[i]->is_legitimate(fault_address)) {
            legitimate = true;
            break;
         }
      }
      if (!legitimate) {
         Console::puts("Address is not legitimate in any VM pool: ");
         Console::putui(fault_address);
         Console::puts("\n");
         assert(false);
      }
   }

   unsigned long page_directory_index = fault_address >> 22;
   unsigned long * pde = current_page_table->PDE_address(fault_address);

   if (!(*pde & 0x1)) {
      // no page table page for this region yet: allocate one from the process pool
      unsigned long new_pt_frame = process_mem_pool->get_frames(1);
      if (new_pt_frame == 0) {
         Console::puts("Failed to allocate new page table page\n");
         assert(false);
      }
      *pde = (new_pt_frame << 12) | 3; // present, read/write

      // The new page table page must be cleared, otherwise stale frame contents
      // would be interpreted as valid entries. It is now reachable through the
      // recursive mapping: | 1023 : 10 | page_directory_index : 10 | 0 : 12 |
      unsigned long * new_pt = (unsigned long *) (1023<<22 | (page_directory_index << 12));
      for (unsigned long i = 0; i < ENTRIES_PER_PAGE; i++)
         new_pt[i] = 2; // not present, read/write
   }

   // allocate the data frame from the process pool (no swapping to secondary memory)
   unsigned long new_frame = process_mem_pool->get_frames(1);
   if (new_frame == 0) {
      Console::puts("Failed to allocate new page frame\n");
      assert(false);
   }
   unsigned long * pte = current_page_table->PTE_address(fault_address);
   *pte = (new_frame << 12) | 3; // present, read/write
}

void PageTable::register_pool(VMPool * _vm_pool)
{
   assert(num_pools < MAX_POOLS);
   registered_pools[num_pools++] = _vm_pool;
   Console::puts("registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no)
{
   unsigned long addr = _page_no << 12;

   // If the page table page does not exist, the page was never touched.
   // (Touching the PTE address in that case would itself cause a fault.)
   unsigned long * pde = PDE_address(addr);
   if (!(*pde & 0x1))
      return;

   unsigned long * pte = PTE_address(addr);
   if (!(*pte & 0x1))
      return; // page never referenced, no frame allocated

   // return the frame to the process pool and invalidate the entry
   process_mem_pool->release_frames(*pte >> 12);
   *pte = 2; // not present, read/write

   // flush the TLB by reloading CR3
   write_cr3((unsigned long) page_directory);
}