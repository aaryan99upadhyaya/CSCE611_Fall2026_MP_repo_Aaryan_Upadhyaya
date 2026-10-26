/*
 File: vm_pool.C
 
 Author:
 Date  : 2024/09/20
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {

   base_address     = _base_address;
   size             = _size;
   phys_frame_pool  = _frame_pool;
   page_table       = _page_table;

   max_regions      = Machine::PAGE_SIZE / sizeof(Region);
   allocated_list   = (Region *) base_address;
   free_list        = (Region *) (base_address + Machine::PAGE_SIZE);
   n_allocated      = 0;
   n_free           = 0;

   // Register first, so that the page faults caused by touching the (not yet
   // mapped) metadata pages below are recognized as legitimate.
   page_table->register_pool(this);

   // Initially: nothing allocated, and one big free region behind the metadata pages.
   free_list[0].start  = base_address + META_PAGES * Machine::PAGE_SIZE;
   free_list[0].length = size - META_PAGES * Machine::PAGE_SIZE;
   n_free = 1;

   Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {

   if (_size == 0)
      return 0;

   // always allocate whole pages
   unsigned long length = ((_size + Machine::PAGE_SIZE - 1) / Machine::PAGE_SIZE) * Machine::PAGE_SIZE;

   if (n_allocated >= max_regions) {
      Console::puts("VMPool: allocated-region table is full\n");
      return 0;
   }

   // first fit
   for (unsigned long i = 0; i < n_free; i++) {
      if (free_list[i].length >= length) {
         unsigned long start = free_list[i].start;

         allocated_list[n_allocated].start  = start;
         allocated_list[n_allocated].length = length;
         n_allocated++;

         // shrink the free region, or remove it if it is used up
         free_list[i].start  += length;
         free_list[i].length -= length;
         if (free_list[i].length == 0) {
            free_list[i] = free_list[n_free - 1];
            n_free--;
         }
         return start;
      }
   }

   Console::puts("VMPool: no free region large enough\n");
   return 0;
}

void VMPool::release(unsigned long _start_address) {

   // find the region that starts at _start_address
   unsigned long idx = n_allocated;
   for (unsigned long i = 0; i < n_allocated; i++) {
      if (allocated_list[i].start == _start_address) {
         idx = i;
         break;
      }
   }
   if (idx == n_allocated) {
      Console::puts("VMPool: release of an address that is not a region start\n");
      assert(false);
   }
   if (n_free >= max_regions) {
      Console::puts("VMPool: free-region table is full\n");
      assert(false);
   }

   Region region = allocated_list[idx];

   // give back the frames and invalidate the pages
   for (unsigned long a = region.start; a < region.start + region.length; a += Machine::PAGE_SIZE)
      page_table->free_page(a / Machine::PAGE_SIZE);

   // move the region from the allocated list to the free list
   free_list[n_free++] = region;
   allocated_list[idx] = allocated_list[n_allocated - 1];
   n_allocated--;
}

bool VMPool::is_legitimate(unsigned long _address) {

   // The metadata pages are always legitimate. This check must not read the
   // tables, because the fault being resolved may be on the tables themselves.
   if (_address >= base_address &&
       _address <  base_address + META_PAGES * Machine::PAGE_SIZE)
      return true;

   for (unsigned long i = 0; i < n_allocated; i++) {
      if (_address >= allocated_list[i].start &&
          _address <  allocated_list[i].start + allocated_list[i].length)
         return true;
   }
   return false;
} 