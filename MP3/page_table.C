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

   // since the initial x MB have the same logical address, allocate some PT frames for it and set them valid
   unsigned long num_init_frames = PageTable::shared_size / (PAGE_SIZE);
   unsigned long num_init_pt_frames = (num_init_frames / ENTRIES_PER_PAGE) + (num_init_frames % ENTRIES_PER_PAGE > 0 ? 1 : 0); // ceil ( num_frames / num_pt_entries_per_frame )
   
   // get kernel pool frames for paging the first x MB
   page_directory = (unsigned long *) (kernel_mem_pool->get_frames(1 + num_init_pt_frames) << 12); // physical frame number shifted to obtain address
   unsigned long * shared_space_page_table_frames = (unsigned long *) ((unsigned long) page_directory + PAGE_SIZE);

   if (page_directory == 0) {
      assert(false);
      Console::puts("ERROR : Failed to allocate frames for PD and shared space PT frames\n");
   }

   // initialize the first num_init_pt_frames entries of the PD, set the rest of them to invalid

   // [ <address of shared space page table frame(s)> <5 bits unused> <Dirty=0> <Accessed=0> <2 bits unused> <U/S=0> <R/W=1><Present=1>]
   for(unsigned long i = 0; i < num_init_pt_frames; i++)
      page_directory[i] = ( ( (unsigned long) shared_space_page_table_frames + i * PAGE_SIZE ) ) | (unsigned long)(3);
   // [ <address of shared space page table frame(s)> <5 bits unused> <Dirty=0> <Accessed=0> <2 bits unused> <U/S=0> <R/W=1><Present=0>]
   for(unsigned long i = num_init_pt_frames; i < ENTRIES_PER_PAGE; i++)
      page_directory[i] = ( ( (unsigned long) shared_space_page_table_frames + i * PAGE_SIZE ) ) | (unsigned long)(2);
   
   unsigned long address = 0;
   // initialize the first num_init_frames of the shared space page table(s)
   // [ <phys_frame_number> <5 bits unused> <Dirty=0> <Accessed=0> <2 bits unused> <U/S=0> <R/W=1><Present=1>]
   for(unsigned long i = 0; i < num_init_pt_frames; i++) {
      for(unsigned long j = 0; j < ENTRIES_PER_PAGE; j++) {
         shared_space_page_table_frames[i*ENTRIES_PER_PAGE+j] = address | 3;
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

void PageTable::handle_fault(REGS * _r)
{
   // check fault address
   unsigned long fault_address = read_cr2();

  // check error code of fault
  unsigned long error_code = _r->err_code;
  if (!(error_code & 0x1)) {
      Console::puts("Page FAULT! Checking PD entry\n");
      unsigned long page_directory_index = (fault_address >> 22) & 0x3FF;
      Console::puts("Page directory entry: ");
      Console::puti(page_directory_index);
      Console::puts("\n");
      unsigned long page_directory_entry = current_page_table->page_directory[page_directory_index];
      unsigned long * page_table_addr = nullptr;
      unsigned long page_table_index = (fault_address >> 12) & 0x3FF;
      int pt_assignment_success = 1;

      if(page_directory_entry & 0x1) { // checking bit 0 to confirm if page table exists
         Console::puts("Page directory entry present, updating PT entry\n");
         page_table_addr = (unsigned long *) (page_directory_entry & 0xFFFFF000); // get page table address
      } else { // allocate page table
         Console::puts("Page directory entry not present, adding PT entry and PT frame\n");
         unsigned long new_page_table_frame = kernel_mem_pool->get_frames(1);
         page_table_addr = (unsigned long *) (new_page_table_frame << 12);
         if (new_page_table_frame) {
            Console::puts("Allocated new PT frame from kernel pool: ");
            Console::puti((unsigned long) new_page_table_frame);
            Console::puts("\n");
            current_page_table->page_directory[page_directory_index] = ((unsigned long) page_table_addr) | 3; // present, read/write
         } else {
            Console::puts("Failed to allocate new page table\n");
            pt_assignment_success = 0;
            current_page_table->page_directory[page_directory_index] &= 0xFFFFF000; // clear present bit
         }
      }

      // allocate a page frame from the process pool, it is assumed that no swapping is done with secondary memory
      if(pt_assignment_success) {
         unsigned long new_page_frame = process_mem_pool->get_frames(1);
         if (new_page_frame) {
            Console::puts("Allocated new process pool page frame: ");
            Console::puti(new_page_frame);
            Console::puts("\n");
            page_table_addr[page_table_index] = (new_page_frame << 12) | 3; // present, read/write
         } else {
            Console::puts("Failed to allocate new page frame\n");
         }
      }

  } else if (error_code & 0x1) {
      Console::puts("Protection fault, no access rights\n");
  }

  Console::puts("handled page fault\n");
}

