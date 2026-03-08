/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}


	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here

	dynAllocStart = daStart;
	dynAllocEnd   = daEnd;

	int Npages = (int)(dynAllocEnd - dynAllocStart)/ PAGE_SIZE;
	LIST_INIT(&freePagesList);
	for(int i = 0; i< Npages; i++){
		pageBlockInfoArr[i].num_of_free_blocks= 0;
		pageBlockInfoArr[i].block_size = 0;
		LIST_INSERT_TAIL(&freePagesList , &pageBlockInfoArr[i]);
	}
	for(int i = 0 ; i<=8;i++){
		LIST_INIT(&freeBlockLists[i]);
	}
	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");


}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	struct PageInfoElement *page = to_page_info((uint32)va);
		return page->block_size;
	//Comment the following line
	//	panic("get_block_size() Not implemented yet");

}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here


	if(size == 0){
		return NULL;
	}
	uint32 myBlk = 8;
	uint32 index = 0;
	while(myBlk< size){
		myBlk = myBlk*2;
		index++;
	}
	if(LIST_SIZE(&freeBlockLists[index]) == 0){
		if(LIST_SIZE(&freePagesList) > 0){

			struct PageInfoElement *newpage = LIST_FIRST(&freePagesList);
			LIST_REMOVE(&freePagesList, newpage);

			uint32 virtAdd = to_page_va(newpage);
			get_page((void*)virtAdd);


			int numBlks = PAGE_SIZE / myBlk;
			newpage->block_size = myBlk;
			newpage->num_of_free_blocks = numBlks;
			for(int i = 0; i < numBlks; i++){
				struct BlockElement *newblock = (struct BlockElement*) (virtAdd + i*myBlk);
				LIST_INSERT_TAIL(&freeBlockLists[index],newblock);
			}

		}
		else{

	            bool found = 0;
	            for (int i = index + 1; i <= (LOG2_MAX_SIZE - LOG2_MIN_SIZE); i++)
	            {
	                if (LIST_SIZE(&freeBlockLists[i]) > 0)
	                {
	                    struct BlockElement *blk = LIST_FIRST(&freeBlockLists[i]);
	                    LIST_REMOVE(&freeBlockLists[i], blk);
	                    struct PageInfoElement *page = to_page_info((uint32)blk);
	                    page->num_of_free_blocks--;
	                    found = 1;
	                    return blk;
	                }
	            }
	            if (!found)
	           //cprintf("Bonus function: no suitable free block or free page found!");
	           return NULL;
		}
	}


	struct BlockElement *newblock = LIST_FIRST(&freeBlockLists[index]);
	LIST_REMOVE(&freeBlockLists[index],newblock);
	struct PageInfoElement* PageElm = to_page_info((uint32)newblock);
	PageElm->num_of_free_blocks--;
	return newblock;

	//Comment the following line
	//panic("alloc_block() Not implemented yet");
	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	//Comment the following line/

	int index = 0;
	int counter = 8;
	uint32 offset = (uint32)va % 4096 ;
	uint32 page_start = (uint32)va - offset;

	uint32 myBlk = get_block_size(va);
	struct PageInfoElement *mypage = to_page_info((uint32)page_start);
	if (myBlk == 0 ){
		cprintf("block has size of zero");
	return;
	}
	uint32 Nblks = 4096 / myBlk;
	while(counter < myBlk){
			counter = counter*2;
			index++;
		}

	struct BlockElement *newblock = (struct BlockElement *)va ;
	LIST_INSERT_TAIL(&freeBlockLists[index],newblock);

	mypage->num_of_free_blocks++;

	if(mypage->num_of_free_blocks == (Nblks)){

		mypage->block_size = 0;
		mypage->num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList ,mypage);
		for(int i = 0; i < Nblks; i++){
				struct BlockElement *newblock = (struct BlockElement*) (page_start + i*myBlk);
				LIST_REMOVE(&freeBlockLists[index],newblock);
		}
		return_page((void *)page_start);

	}

//	panic("free_block() Not implemented yet");

}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
