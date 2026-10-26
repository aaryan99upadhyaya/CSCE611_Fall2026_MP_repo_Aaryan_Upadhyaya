/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/
ContFramePool* ContFramePool::fp_ll_head = nullptr; // search enters from this node
ContFramePool* ContFramePool::fp_ll_tail = nullptr; // constructor enters from this node
/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
    unsigned int bitmap_index = _frame_no / 4;
    unsigned int shift_amount = 2 * (_frame_no % 4);
    unsigned char mask = 0x3 << shift_amount;
    unsigned int state = (bitmap[bitmap_index] & mask) >> shift_amount;
    return (state == 0) ? FrameState::Free : (state == 1) ? FrameState::Used : FrameState::HoS;
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
    unsigned int bitmap_index = _frame_no / 4;
    unsigned int shift_amount = 2 * (_frame_no % 4);
    unsigned char mask1 = 0;
    unsigned char mask2 = 0;

    switch(_state) {
        case FrameState::Free:
            mask1 = ~(0x3 << shift_amount);
            bitmap[bitmap_index] &= mask1;
            break;
        case FrameState::Used:
            mask1 = ~(0x3 << shift_amount);
            mask2 = (0x1 << shift_amount);
            bitmap[bitmap_index] &= mask1;
            bitmap[bitmap_index] |= mask2;
            break;
        case FrameState::HoS:
            mask1 = ~(0x3 << shift_amount);
            mask2 = (0x2 << shift_amount);
            bitmap[bitmap_index] &= mask1;
            bitmap[bitmap_index] |= mask2;
            break;
    }
    
}

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{

    // Bitmap must fit in a single frame!
    assert(_n_frames <= FRAME_SIZE * 4); // 2 bits per frame | FRAMEs tracked per FRAME = ( FRAME_SIZE (bytes) * 8 ) / 2

    if(fp_ll_head == nullptr) {
        fp_ll_head = this;
        fp_ll_tail = this;
        fp_ll_prev = nullptr;
        fp_ll_next = nullptr;
    } else {
        fp_ll_tail->fp_ll_next = this;
        fp_ll_prev = fp_ll_tail;
        fp_ll_next = nullptr;
        fp_ll_tail = this;
    }

    base_frame_no = _base_frame_no;
    n_frames = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;

    // If _info_frame_no is zero then we keep management info in the first
    // frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }

    // Everything ok. Proceed to mark all frame as free.
    for (int fno = 0; fno < n_frames; fno++) {
        set_state(fno, FrameState::Free);
    }

    // Mark the first frames as being used if it is being used
    if(_info_frame_no == 0) {
        unsigned long info_frames = needed_info_frames(n_frames);
        for(unsigned long fno = 0; fno < info_frames; fno++) {
            if(fno==0) set_state(fno, FrameState::HoS);
            else set_state(fno, FrameState::Used);
            nFreeFrames--;
        }
    }

}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    assert(nFreeFrames > 0);
    // Find a free block of frames
    unsigned long skipped = 0; // skipped frames counter
    for (unsigned long i=0; i<n_frames; i++) {
        if(get_state(i) == FrameState::Free) {
            // Check if we can allocate the requested number of frames
            bool can_allocate = true;
            for(unsigned long j=1; j<_n_frames; j++) {
                if(get_state(i+j) != FrameState::Free) {
                    can_allocate = false;
                    skipped = j+1;
                    break;
                }
            }
            if(can_allocate) {
                // Mark the frames as used
                set_state(i, FrameState::HoS);
                for(unsigned long j=1; j<_n_frames; j++) {
                    set_state(i+j, FrameState::Used);
                }
                nFreeFrames -= _n_frames;
                return (i + base_frame_no); // Return the base frame number + i
            } else {
                i += skipped; // Skip the checked frames
            }
        }
    }
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Mark all frames in the range as being used.
    set_state(_base_frame_no - this->base_frame_no, FrameState::HoS);
    for (unsigned long fno = _base_frame_no+1; fno < _base_frame_no + _n_frames; fno++){
        set_state(fno - this->base_frame_no, FrameState::Used);
    }
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // Check which frame pool contains _first_frame_no
    ContFramePool *pool = fp_ll_head;
    while (pool != nullptr) {
        if (_first_frame_no >= pool->base_frame_no &&
            _first_frame_no < pool->base_frame_no + pool->n_frames) {
            // Is _first_frame_no an HoS frame?
            unsigned long relative_pos = _first_frame_no - pool->base_frame_no;
            if(pool->get_state(relative_pos) == FrameState::HoS) {
                // Release frames in this pool starting from _first_frame_no until the next Free frame is found
                for (unsigned long fno = relative_pos; fno < pool->n_frames; fno++) {
                    if (pool->get_state(fno) == FrameState::Free) {
                        break;
                    }
                    pool->set_state(fno, FrameState::Free);
                }
            } else {
                Console::puts("_first_frame_no is not an HoS frame!\n");
            }
            break;
        }
        pool = pool->fp_ll_next;
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    unsigned long info_frames = (_n_frames * 2) / (4 * 1024 * 8) + ( (_n_frames * 2) % (4 * 1024 * 8) > 0 ? 1 : 0 );
    return info_frames;
}
