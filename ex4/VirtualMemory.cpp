//
// Created by dbusbib123 on 7/17/24.
//
#include <cmath>
#include <iostream>
#include "VirtualMemory.h"
#include "PhysicalMemory.h"
uint64_t find_frame(uint64_t virtualAddress);
#define MIN(a, b) ((a) < (b) ? (a) : (b))
word_t find_empty(uint64_t virtualAddress, word_t prev ,uint64_t  cur_ind);
// Macro to calculate absolute value
#define ABS(x) ((x) < 0 ? -(x) : (x))
uint64_t frame_offset(uint64_t num ,uint64_t j);
/*
 * Initialize the virtual memory
 */
void dfs(uint64_t frame , uint64_t num, uint64_t *max_index_seen, uint64_t *cyclic_distance, uint64_t page_swapped_in
        , uint64_t current_page, uint64_t *max_distance_frame, uint64_t *max_frame_perent, bool* found_zero , uint64_t *index_for_page, uint64_t *max_distance_frame_parent
        , uint64_t *offset, uint64_t frames,uint64_t *old_fhater, uint64_t old);

void update_perent( uint64_t max_frame_perent, uint64_t frame_index);

void VMinitialize(){
    for(uint64_t i=0;i<PAGE_SIZE;i++){
        PMwrite(i,0);
    }
}
uint64_t frame_offset(uint64_t num, uint64_t j) {
    uint64_t shift_amount = (TABLES_DEPTH - j) * OFFSET_WIDTH;
    uint64_t denominator = 1ULL << shift_amount;
    uint64_t result = (num / denominator) & ((1ULL << OFFSET_WIDTH) - 1);
    return result;
}

/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value){

    if (virtualAddress >= VIRTUAL_MEMORY_SIZE && !value){
        return 0;
    }
    auto address = find_frame(virtualAddress);
    PMread(address, value);

    return 1;
}

/* writes a word to the given virtual address
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */

int VMwrite(uint64_t virtualAddress, word_t value){
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE){
        return 0;
    }
    PMwrite(find_frame(virtualAddress), value);
    return 1;

}

uint64_t find_frame(uint64_t virtualAddress ) {
    word_t addr = 0;
    word_t prev = 0;

    for (uint64_t i = 0; i < TABLES_DEPTH; i++) {

        PMread((prev * (PAGE_SIZE)) + frame_offset(virtualAddress,i), &addr);

        if (addr == 0){

            addr = find_empty(virtualAddress, prev,  i);
            PMwrite((prev * (PAGE_SIZE)) + frame_offset(virtualAddress,i), addr);
        }
//        std::cout << addr<<" "<<virtualAddress<<" "<<prev << std::endl;

        prev = addr;

    }
    PMrestore(addr, virtualAddress >> OFFSET_WIDTH);
//    std::cout << (addr)*PAGE_SIZE +(virtualAddress % PAGE_SIZE) << std::endl;

    return (addr)*PAGE_SIZE +(virtualAddress % PAGE_SIZE);
}

word_t find_empty(uint64_t virtualAddress,word_t prev ,uint64_t cur_ind){
    //first
    uint64_t max_index_seen=0;
    uint64_t max_distance_frame=0;
    uint64_t cyclic_distance=0;
    uint64_t max_frame_perent = 0;
    uint64_t current_page=0;
    uint64_t  num =0;
    uint64_t last_pag=0;
    uint64_t frame_index = 0;
    uint64_t max_distance_frame_parent=0;
    bool found_zero = false;
    uint64_t old_fhater =0;
    uint64_t offset = 0;
    dfs(frame_index, num, &max_index_seen, &cyclic_distance, virtualAddress / PAGE_SIZE, current_page,
        &max_distance_frame, &max_frame_perent, &found_zero, &last_pag, &max_distance_frame_parent, &offset, prev,&old_fhater,0);

    if(found_zero &&offset!=0 && offset != (uint64_t )prev){
        for(uint64_t i=0;i<PAGE_SIZE;i++){
            PMwrite(offset*PAGE_SIZE + i,0);
        }
        PMwrite(old_fhater, 0);

        return offset;
    }
        //second
    else if( max_index_seen + 1 < NUM_FRAMES){
        if (cur_ind < TABLES_DEPTH - 1){
            for(uint64_t i=0;i<PAGE_SIZE;i++){
                PMwrite((max_index_seen+1)*PAGE_SIZE + i,0);
            }
        }
        return max_index_seen + 1;
    }
        //third
    else{
//        std::cout <<"blalbla "<<virtualAddress<<" "<<last_pag << std::endl;

        PMevict(max_distance_frame,last_pag);
        update_perent(max_distance_frame_parent, max_distance_frame);
        if (cur_ind < TABLES_DEPTH - 1){

            for(uint64_t i=0;i<PAGE_SIZE;i++){
                PMwrite((max_distance_frame )*PAGE_SIZE + i,0);
            }
        }
        return max_distance_frame;
    }
}


void dfs(uint64_t frame , uint64_t num, uint64_t *max_index_seen, uint64_t *cyclic_distance, uint64_t page_swapped_in
        , uint64_t current_page, uint64_t *max_distance_frame, uint64_t *max_frame_perent,
         bool *found_zero , uint64_t *index_for_page, uint64_t *max_distance_frame_parent
        , uint64_t *offset, uint64_t prev,uint64_t *old_fhater
        ,uint64_t old){

    if (frame>*max_index_seen){
        *max_index_seen =frame;
    }
    if (num == TABLES_DEPTH){
        word_t cur_distance = MIN(NUM_PAGES - ABS(page_swapped_in - current_page), ABS(page_swapped_in - current_page));
        if (cur_distance > *cyclic_distance && (frame < NUM_FRAMES) && (*max_distance_frame != frame) && (frame!=prev)&& frame!=0){
            *cyclic_distance = cur_distance;
            *max_distance_frame = frame;
            *max_distance_frame_parent=*max_frame_perent;
            *index_for_page=current_page;
        }

        return ;
    }

    *max_frame_perent =frame;
    word_t value = 0;
    bool all_zeros = true;
    for (uint64_t i = 0; i<PAGE_SIZE; i++){
        PMread((frame * PAGE_SIZE) + i, &value);
        if (value != 0){

            all_zeros = false;
            dfs(value, num + 1, max_index_seen, cyclic_distance, page_swapped_in, (current_page << OFFSET_WIDTH ) + i,
                max_distance_frame, max_frame_perent, found_zero, index_for_page, max_distance_frame_parent, offset,
                prev,old_fhater,(frame * PAGE_SIZE) + i);
        }

    }
    *found_zero = *found_zero or all_zeros;
    if (all_zeros &&  frame !=prev) {
        *offset =frame;
        *old_fhater=old;
    }

}

void update_perent( uint64_t max_frame_perent, uint64_t frame_index) {

    for (uint64_t i = 0; i < PAGE_SIZE; i ++){
        word_t frame_num;
        PMread((max_frame_perent* PAGE_SIZE) + i, &frame_num);
        if ((uint64_t )frame_num== frame_index){
            PMwrite((max_frame_perent * PAGE_SIZE) + i, 0);
        }
    }



}