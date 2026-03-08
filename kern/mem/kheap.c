#include "kheap.h"
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#include <inc/queue.h>
#include<inc/types.h>

void* virAddList [NUM_OF_KHEAP_PAGES]={0};

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{

		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}

#if USE_KHEAP
	LIST_INIT(&KernelFreePagesList);
	LIST_INIT(&KernelAllocatedPagesList);
	init_kspinlock(&lock,"lock");
#endif

}	//==================================================================================
	//==================================================================================


//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);

	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	//////
	uint32 pa = kheap_physical_address((uint32)va);
	uint32 frame_number = pa / PAGE_SIZE;
	virAddList[frame_number] = (void*)(uint32)va;
	//////
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	///////
	uint32 pa = kheap_physical_address((uint32)va);
	    if (pa != 0) {
	        uint32 frame_number = pa / PAGE_SIZE;
	        virAddList[frame_number] = 0;
	    }
   /////////
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//Comment the following line
	//kpanic_into_prompt("kmalloc() is not implemented yet...!!");
	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
#if USE_KHEAP
	acquire_kspinlock(&lock);

	if(size==0){
		release_kspinlock(&lock);
		return NULL;
	}
	if(size<=DYN_ALLOC_MAX_BLOCK_SIZE){
		   void* ptr=alloc_block(size);
		   release_kspinlock(&lock);
		   return ptr;
		}
	uint32 num_pages_needed=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 requiredsize=num_pages_needed*PAGE_SIZE;
	struct KernelPageInfoElement *page,*worstfit=NULL,*exactfit=NULL;
	//1-hndawar 3la ecaxtfit
	LIST_FOREACH(page,&KernelFreePagesList){
	  if(page->size==requiredsize){
		  if(exactfit==NULL||page->v_address<exactfit->v_address){
			  exactfit=page;
		  }
	  }
	  else if(page->size>requiredsize){
		  if(worstfit==NULL||page->size>worstfit->size||(page->size==worstfit->size&&page->v_address<worstfit->v_address)){
			  worstfit=page;
		  }
	  }
   }
   if(exactfit!=NULL){
	 exactfit->used=1;
	 exactfit->size=requiredsize;
	 LIST_REMOVE(&KernelFreePagesList,exactfit);
	 LIST_INSERT_HEAD(&KernelAllocatedPagesList,exactfit);

	 for(uint32 i=0;i<num_pages_needed;i++){
		 get_page((void*)(exactfit->v_address+i*PAGE_SIZE));
	 }
	 release_kspinlock(&lock);
	 return (void*)exactfit->v_address;
   }

   //2-hndawar 3la worstfit

   if(worstfit!=NULL){
	   for(uint32 i=0;i<num_pages_needed;i++){
	  		 get_page((void*)(worstfit->v_address+i*PAGE_SIZE));
	  	 }
		struct KernelPageInfoElement *new_worst=(struct KernelPageInfoElement*)alloc_block(sizeof(struct KernelPageInfoElement));
		new_worst->v_address=worstfit->v_address;
		new_worst->size=requiredsize;
		new_worst->used=1;
		LIST_REMOVE(&KernelFreePagesList,worstfit);
	    LIST_INSERT_HEAD(&KernelAllocatedPagesList,new_worst);
	    worstfit->v_address+=requiredsize;
	    worstfit->size-=requiredsize;
		if(worstfit->size>0){
		  LIST_INSERT_HEAD(&KernelFreePagesList,worstfit);
		}
		release_kspinlock(&lock);
		return(void*)new_worst->v_address;
   }

   uint32 va = kheapPageAllocBreak;
   if((uint64)((uint64)(va)+requiredsize)>KERNEL_HEAP_MAX){
	   release_kspinlock(&lock);
	   return NULL;
   }
    for(uint32 i=0;i<num_pages_needed;i++){
   		get_page((void*)va+i*PAGE_SIZE);
   	 }
    struct KernelPageInfoElement *newpage=(struct KernelPageInfoElement*)alloc_block(sizeof(struct KernelPageInfoElement));
    newpage->v_address=va;
    newpage->size=requiredsize;
    newpage->used=1;
    LIST_INSERT_HEAD(&KernelAllocatedPagesList,newpage);
    kheapPageAllocBreak=va+requiredsize;

    release_kspinlock(&lock);
    return(void*)va;
#else
	panic("USE_KHEAP = 0");
#endif
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================

void kfree(void* virtual_address)
{

	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	//panic("kfree() is not implemented yet...!!");
#if USE_KHEAP
	acquire_kspinlock(&lock);
    if (virtual_address == NULL) {
    	release_kspinlock(&lock);
    	return;
    }
    if ((uint32)virtual_address >= KERNEL_HEAP_START && (uint32)virtual_address < dynAllocEnd) {
            free_block(virtual_address);
            release_kspinlock(&lock);
            return;
        }
    struct KernelPageInfoElement *page=NULL,*ptr;
    LIST_FOREACH(ptr, &KernelAllocatedPagesList) {
         if (ptr->v_address == (uint32)virtual_address) {
        	 page=ptr;
        	 break;
         }
    }
    //by7awel yfree 7aga mesh fe el freelist aslan
    if(page==NULL){
    	release_kspinlock(&lock);
    	return;
    }

    uint32 num_pages_tofree=ROUNDUP(page->size,PAGE_SIZE)/PAGE_SIZE;
   // uint32 allocated_size=num_pages_tofree*PAGE_SIZE;

   // uint32 start_index = (page->v_address -KERNEL_HEAP_START)/PAGE_SIZE;
  //  uint32 start_index = (kheap_physical_address(page->v_address))/PAGE_SIZE;//
    for(uint32 i =0 ; i < num_pages_tofree; i++){
    	return_page((void*)(page->v_address+i*PAGE_SIZE));//
    }
//    int flag=-1;
//    if (page->v_address + page->size == kheapPageAllocBreak) {
//                  kheapPageAllocBreak -= page->size;
//                  flag=1;
//              }

       LIST_REMOVE(&KernelAllocatedPagesList, page);
       page->used = 0;
       LIST_INSERT_HEAD(&KernelFreePagesList, page);

    struct KernelPageInfoElement *next=NULL,*prev=NULL,*curr;
    LIST_FOREACH(curr,&KernelFreePagesList){
    	if(page->v_address+page->size==curr->v_address){
    		next=curr;
    	}
    	else if(curr->v_address+curr->size==page->v_address){
    		prev=curr;
    	}
    }
    if(next==NULL && prev==NULL){
    	//mafesh 7aga a3melaha merge
    	if (page->v_address + page->size == kheapPageAllocBreak) {
                 kheapPageAllocBreak -= page->size;
                 LIST_REMOVE(&KernelFreePagesList,page);
                 free_block(page);
    	}
    	release_kspinlock(&lock);
    	return;
    }
    if(next!=NULL){
    	page->size+=next->size;
    	 LIST_REMOVE(&KernelFreePagesList,next);
    	 free_block(next);
    }
    if(prev!=NULL){
        	page->v_address=prev->v_address;
        	page->size+=prev->size;
        	 LIST_REMOVE(&KernelFreePagesList,prev);
        	 free_block(prev);

        		if (page->v_address+page->size==kheapPageAllocBreak){
        				kheapPageAllocBreak=page->v_address;
        				LIST_REMOVE(&KernelFreePagesList,page);
        	        	 free_block(page);

        		}
//        	 if(flag==1){
//        		 kheapPageAllocBreak -= prev->size;
//        	 }
        }
    release_kspinlock(&lock);
    return;
#else
	panic("USE_KHEAP = 0");
#endif
         }


//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{

#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	//Your code is here

	//if(physical_address < KERNEL_HEAP_START || physical_address >= KERNEL_HEAP_MAX ){
		//return 0;
	//}
		uint32 frame_num = (physical_address) / PAGE_SIZE;
		void* va = virAddList[frame_num];
		if(va == 0){
			return 0;
		}
		uint32 offset = physical_address & 0XFFF;
		return (uint32)va+ offset;

		//Comment the following line
		//panic("kheap_virtual_address() is not implemented yet...!!");
#else
	panic("USE_KHEAP = 0");
#endif
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{

	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here
	//Comment the following line

	//unsigned int offset = virtual_address & ;
	uint32 * ptr_page_table = NULL;
	get_page_table(ptr_page_directory , virtual_address ,&ptr_page_table) ;
	if (ptr_page_table != NULL){
		 uint32 entry =  ptr_page_table [PTX(virtual_address)] ;
		if (!(entry & PERM_PRESENT)){
		return 0 ;
		}
		uint32 frame_num =entry & 0xFFFFF000 ;
		//uint32 base_add = frame_num * PAGE_SIZE ;
		uint32 offset = virtual_address & 0x00000FFF;
		//offset = offset >> 12 ;
		uint32 pa = frame_num + offset ;
		return pa ;

	}else {
		return 0 ; // no table
	}

//	panic("USE_KHEAP = 0 (3)");
//	panic("kheap_physical_address() is not implemented yet...!!");


	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}
//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

 //extern inline uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{

//	TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
//	Your code is here
//	Comment the following line

	//panic("krealloc() is not implemented yet...!!");
#if USE_KHEAP

	if(virtual_address==NULL && new_size==0){
	    	return NULL;
	    }
	     if(virtual_address==NULL){
		    return kmalloc((unsigned int)new_size);
	    }
	    if (new_size==0){
	    	kfree(virtual_address);
		    return NULL;
	    }
	    int flag_dynamic_alloc=0;
	    uint32 old_size=0;

	    acquire_kspinlock(&lock);
	    if((uint32)virtual_address>=dynAllocStart&&(uint32)virtual_address<dynAllocEnd){
	    	old_size=get_block_size(virtual_address);
	    	flag_dynamic_alloc=1;

	    	if(old_size==0){
	    		release_kspinlock(&lock);
	    		return NULL;
	    	}
	    }

	    //yb2a e7na hena fe el page allocator
	    if(flag_dynamic_alloc==0){
	        struct KernelPageInfoElement *old_page=NULL,*ret_to_freelist=NULL,*itr;
	        // hadawar 3ala el old page fe el page allocator
	    	if(new_size>DYN_ALLOC_MAX_BLOCK_SIZE){
	    		// handawar law feh warahom 3alatol
	    		  LIST_FOREACH(itr,&KernelAllocatedPagesList){
	    			  if(itr->v_address==(uint32)virtual_address){
	    				  old_page=itr;
	    				  break;
	    			  }
	    		  }
	    	}

	    	if(old_page==NULL){
	    		release_kspinlock(&lock);
	    		return NULL;
	    	}
	    	 old_size=old_page->size;

	    	 //case 1 in page allocator: expand ...w de gowaha 3 cases tanyeen
	    		  if(new_size>old_page->size){
	    	    	 int check_in_freelist=0;
	    		  old_size=old_page->size;
	    		  uint32 extrasize=new_size-old_page->size;
	    		  uint32 extra_num_pages_needed=ROUNDUP(extrasize,PAGE_SIZE)/PAGE_SIZE;
	    		  uint32 extra_requiredsize=extra_num_pages_needed*PAGE_SIZE;

	    		  LIST_FOREACH(itr,&KernelFreePagesList){
	    			  if(itr->v_address==old_page->v_address+old_page->size){
	    				  check_in_freelist=1;
	        			  //1.1 law el extra size mawgoda fe el free list belzabt ba3d el old page
	    				  if(itr->size==extra_requiredsize){
	    					  for (uint32 i=0;i<extra_num_pages_needed;++i){
	    		    			get_page((void*)old_page->v_address+(old_page->size)+i*PAGE_SIZE);
	    			   		}
	    					  old_page->size+=extra_requiredsize;
	    					  ret_to_freelist=itr;
	    					  LIST_REMOVE(&KernelFreePagesList,ret_to_freelist);
	    					  release_kspinlock(&lock);
	    					  return (void*)old_page->v_address;
	    				  }
	    				  //1.2 law fe el free list fe block el size bta3o akbar men el extra req. size
	    				  //yb2a sa3etha hakhod el page de w 22semha goz2en wa7ed el h expand feh w wa7ed return to freelist
	    				  else if(itr->size>extra_requiredsize){
	    					  ret_to_freelist=itr;
	       					  LIST_REMOVE(&KernelFreePagesList,ret_to_freelist);
	    					  for (uint32 i=0;i<extra_num_pages_needed;++i){
	    			    			get_page((void*)old_page->v_address+(old_page->size)+i*PAGE_SIZE);
	    				     	}

	    	    			  struct  KernelPageInfoElement *ret_freelist=(struct KernelPageInfoElement*)alloc_block(sizeof(struct KernelPageInfoElement));
	    					  ret_freelist->v_address=itr->v_address+extra_requiredsize;
	    					  ret_freelist->size=itr->size-extra_requiredsize;
	    					  ret_freelist->used=0;
	    					  old_page->size+=extra_requiredsize;
	    					  LIST_INSERT_HEAD(&KernelFreePagesList,ret_freelist);
	    					  release_kspinlock(&lock);
	    					  return (void*)old_page->v_address;
	    				  }
	    				  //1.3 law el size bta3 el free block el warah 22al men el extra req.size
	    				  else if(itr->size<extra_requiredsize){
	    					  release_kspinlock(&lock);
	    					void* ptr= kmalloc((unsigned int)new_size);
	    					if(ptr==NULL)
	    						return NULL;
	    					  memcpy(ptr,virtual_address,old_size);
	    					  kfree(virtual_address);
	    					  return ptr;
	    				  }
	    			  }
	    		  }
	    		  if(check_in_freelist==0){
	    			  release_kspinlock(&lock);
	    	        	void* ptr= kmalloc((unsigned int)new_size);
	    	        	if(ptr==NULL){
	    	            	 return NULL;
	    	       		}
	    			  memcpy(ptr,virtual_address,old_size);
	    			  kfree(virtual_address);
	    			  return ptr;
	    		  }

	    		  }
	    		  //hena shrink gowa el page alloc
	    		   if(new_size<old_page->size){
	    			  //shrink l7ad ma dakhal fe 7agm el block alloc
	    			  if(new_size<=DYN_ALLOC_MAX_BLOCK_SIZE){
	    		     	// release_kspinlock(&lock);
	    				 void*ptr= alloc_block(new_size);
	    				 if(ptr==NULL){
	    		 			 release_kspinlock(&lock);
	    		    		 return NULL;
	    		       	}
	    				  memcpy(ptr,virtual_address,new_size);
	    				  release_kspinlock(&lock);
	 					  kfree(virtual_address);
	         			  return ptr;
	    			  }
	    			  if(new_size>DYN_ALLOC_MAX_BLOCK_SIZE){
	    				 // old_size=old_page->size;
	    				  uint32 newreqsize=ROUNDUP(new_size,PAGE_SIZE);
	    				  if(newreqsize<old_size){
	    					  uint32 extrasize=old_size-newreqsize;
	    			  		  uint32 extra_num_pages_tofree=extrasize/PAGE_SIZE;
	    				      uint32 extra_tofreesize=extra_num_pages_tofree*PAGE_SIZE;

	    				      for (uint32 i=0;i<extra_num_pages_tofree;++i){
	    				      	return_page((void*)old_page->v_address+newreqsize+i*PAGE_SIZE);
	  				     	}
	    				      old_page->size=newreqsize;
	    				      if(extra_num_pages_tofree>0){
	    				 struct  KernelPageInfoElement *ret_to_freelist=(struct KernelPageInfoElement*)alloc_block(sizeof(struct KernelPageInfoElement));
	    				      ret_to_freelist->v_address=old_page->v_address+newreqsize;
	    				      ret_to_freelist->size=extra_tofreesize;
	    				      ret_to_freelist->used=0;
	    				      LIST_INSERT_HEAD(&KernelFreePagesList,ret_to_freelist);
	    			  }
	    				  }
	    				  release_kspinlock(&lock);
	    				      return (void*)old_page->v_address;




	    			  }
	    			  } else if(new_size==old_page->size){
	    				  release_kspinlock(&lock);
	    				  return virtual_address;
	    			  }

	    	}
	    if(flag_dynamic_alloc==1){
	    	//expand
	    	if(new_size>old_size&&new_size>DYN_ALLOC_MAX_BLOCK_SIZE){
	    		 release_kspinlock(&lock);
	    		void* ptr=kmalloc((unsigned int)new_size);
	    		if(ptr==NULL){
	    			 release_kspinlock(&lock);
	    			 return NULL;
	    		}
	    		memcpy(ptr,virtual_address,old_size);
	    		free_block(virtual_address);
	    		return ptr;
	    	}
	    	if(new_size>old_size&&new_size<=DYN_ALLOC_MAX_BLOCK_SIZE){
	    		 release_kspinlock(&lock);
	    		void* ptr= realloc_block(virtual_address,new_size);
	    		return ptr;
	    	}
	    	//shrink
	    	if(new_size<old_size&&new_size<=DYN_ALLOC_MAX_BLOCK_SIZE){
	    		 release_kspinlock(&lock);
	    		void* ptr= realloc_block(virtual_address,new_size);
	         	return ptr;
	    	}


	    }
		 release_kspinlock(&lock);
	    return NULL;
#else
	panic("USE_KHEAP = 0");
#endif

}
