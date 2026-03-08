#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//struct uspinlock mlock;
//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
#if USE_KHEAP
		LIST_INIT(&userFreePagesList);
		LIST_INIT(&userMarkedPagesList);
#endif
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
#if USE_KHEAP
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	//Comment the following line
	//panic("malloc() is not implemented yet...!!");
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		void * ptr = alloc_block(size);
		return ptr ;
	}

	uint32 num_pages_needed=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 requiredsize=num_pages_needed*PAGE_SIZE;
	struct userPageInfoElement *page,*worstfit=NULL,*exactfit=NULL;
	LIST_FOREACH_SAFE(page,&(userFreePagesList),userPageInfoElement){
		//LIST_FOREACH(page,&userFreePagesList){
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
		 exactfit->marked=1;
		 LIST_REMOVE(&userFreePagesList,exactfit);
		 LIST_INSERT_HEAD(&userMarkedPagesList,exactfit);
		 sys_allocate_user_mem(exactfit->v_address,requiredsize);
		 return (void*)exactfit->v_address;
	   }
	 if(worstfit!=NULL){
		 sys_allocate_user_mem(worstfit->v_address , requiredsize);
		struct userPageInfoElement *new_worst=(struct userPageInfoElement*)alloc_block(sizeof(struct userPageInfoElement));
		new_worst->v_address=worstfit->v_address;
		new_worst->size=requiredsize;
		new_worst->marked=1;
		LIST_REMOVE(&userFreePagesList,worstfit);
		LIST_INSERT_HEAD(&userMarkedPagesList,new_worst);
		worstfit->v_address+=requiredsize;
		worstfit->size-=requiredsize;
		if(worstfit->size>0){
		  LIST_INSERT_HEAD(&userFreePagesList,worstfit);
		}

		return(void*)new_worst->v_address;
	 }

	 uint32 va = uheapPageAllocBreak;

	   if((uint64)((uint64)(va)+requiredsize)>USER_HEAP_MAX){
		   return NULL;
	   }
	 sys_allocate_user_mem(va ,requiredsize);
	 struct userPageInfoElement *newpage=(struct userPageInfoElement*)alloc_block(sizeof(struct userPageInfoElement));
	    newpage->v_address=va;
	    newpage->size=requiredsize;
	    newpage->marked=1;
	    LIST_INSERT_HEAD(&userMarkedPagesList,newpage);
	    uheapPageAllocBreak=va+requiredsize;
	    return(void*)va;
#else
	panic("USE_KHEAP = 0");
#endif

}

//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	//Comment the following line
	//panic("free() is not implemented yet...!!");
	 if (virtual_address == NULL) {
	    	return;
	    }
	 if ((uint32)virtual_address >= USER_HEAP_START && (uint32)virtual_address < dynAllocEnd) {
	            free_block(virtual_address);
	            return;
	        }
	 struct userPageInfoElement *page=NULL,*iterator;
	    LIST_FOREACH_SAFE(iterator, &(userMarkedPagesList),userPageInfoElement) {
    //	  LIST_FOREACH(iterator, &userMarkedPagesList) {
	          if (iterator->v_address == (uint32)virtual_address) {
	         	 page=iterator;
	         	 break;

	          }
	     }
	     if(page==NULL){
	    	 return ;
	     }
	sys_free_user_mem(page->v_address,page->size);
	/////////////////////////////////////////////////////////////////////////////////////////
//	int flag=-1;
//	if (page->v_address + page->size == uheapPageAllocBreak) { /////////////////
//			uheapPageAllocBreak -= page->size;
//			flag=1;
//	}
	///////////////////////////////////////////////////////////////////////////
   LIST_REMOVE(&userMarkedPagesList, page);
   page->marked = 0;
   LIST_INSERT_HEAD(&userFreePagesList, page);
   struct userPageInfoElement *next=NULL,*prev=NULL;
   struct userPageInfoElement *curr;
 //LIST_FOREACH_SAFE(curr, &(userFreePagesList),userPageInfoElement){
    LIST_FOREACH(curr,&userFreePagesList){
		if(page->v_address+page->size==curr->v_address){
			next=curr;
		}
		else if(curr->v_address+curr->size==page->v_address){
			prev=curr;
		}
    }

   if(next==NULL && prev==NULL){
	   /////////////////////////////////////////////////////
	   if (page->v_address + page->size == uheapPageAllocBreak) {
	   			uheapPageAllocBreak -= page->size;
	   			LIST_REMOVE(&userFreePagesList,page);
	   			free_block(page);
	   }
	   //////////////////////////////////////////////////////
	    	return;
	    }

   if(next!=NULL){
		page->size+=next->size;
		 LIST_REMOVE(&userFreePagesList,next);
		 free_block(next);/////////////////////////////////////
	}
	if(prev!=NULL){
		page->v_address=prev->v_address;
		page->size+=prev->size;
		LIST_REMOVE(&userFreePagesList,prev);
		free_block(prev);

	if (page->v_address+page->size==uheapPageAllocBreak){
			uheapPageAllocBreak=page->v_address;
			LIST_REMOVE(&userFreePagesList,page);
			free_block(page);
	}
//		 if(flag==1){
//			uheapPageAllocBreak -= prev->size;
//			uheapPageAllocBreak=page->v_address;
//		 }
    }
return;
#else
panic("USE_KHEAP = 0");
#endif
}

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{

#if USE_KHEAP
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");

    uint32 number_of_page_we_need = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    uint32 size_we_will_allocate_it = number_of_page_we_need * PAGE_SIZE;
    struct userPageInfoElement *page, *exact_fit = NULL, *worst_fit = NULL;

    //1-hndawar 3la exactfit + worstfit
    LIST_FOREACH(page, &userFreePagesList)
    {
        if (page->size == size_we_will_allocate_it)
        {
            if (exact_fit == NULL || page->v_address < exact_fit->v_address)
                exact_fit = page;
        }
        else if (page->size > size_we_will_allocate_it)
        {
            if (worst_fit == NULL || page->size > worst_fit->size ||
                (page->size == worst_fit->size && page->v_address < worst_fit->v_address))
                worst_fit = page;
        }
    }

    uint32 va ;
    if (exact_fit != NULL)
    {
        va = exact_fit->v_address;
        exact_fit->marked = 1;
        LIST_REMOVE(&userFreePagesList, exact_fit);
        LIST_INSERT_HEAD(&userMarkedPagesList, exact_fit);
        int shared_id = sys_create_shared_object(sharedVarName, size, isWritable, (void*)va);
    	if (shared_id == E_NO_MEM || shared_id == E_SHARED_MEM_EXISTS)
        {
            exact_fit->marked = 0;
            LIST_REMOVE(&userMarkedPagesList, exact_fit);
            LIST_INSERT_HEAD(&userFreePagesList, exact_fit);
            return NULL;
        }
        return (void*)va;
    }
    if (worst_fit != NULL)
    {
        va = worst_fit->v_address;
        int shared__object_id = sys_create_shared_object(sharedVarName, size, isWritable, (void*)va);
        if (shared__object_id == E_NO_MEM || shared__object_id == E_SHARED_MEM_EXISTS){
            return NULL;
        }

        struct userPageInfoElement *new_worst =(struct userPageInfoElement*) alloc_block(sizeof(struct userPageInfoElement));
        new_worst->v_address = va;
        new_worst->size = size_we_will_allocate_it;
        new_worst->marked = 1;
        LIST_REMOVE(&userFreePagesList,worst_fit);
        LIST_INSERT_HEAD(&userMarkedPagesList, new_worst);
        //lw el worst kan > el size elle 3awzah hatarh el size elle 5adtoh menoh we araga3 el size elle fedel
        worst_fit->v_address += size_we_will_allocate_it;
        worst_fit->size-= size_we_will_allocate_it;
        //lw el  worst size = el size elle 5adtoh 5alas asheel elpage mn el list
        if(worst_fit->size>0){
            LIST_INSERT_HEAD(&userFreePagesList,worst_fit);
        }
        return (void*)va;
    }
// lw mafesh mesaha harfa3 el break
    va = uheapPageAllocBreak;
    if((uint64)((uint64)(va)+size_we_will_allocate_it)>USER_HEAP_MAX){
 		   return NULL;
 	   }
    int shared_object_id = sys_create_shared_object(sharedVarName, size, isWritable, (void*)va);
    if (shared_object_id == E_NO_MEM || shared_object_id == E_SHARED_MEM_EXISTS){
               return NULL;
           }

    struct userPageInfoElement *new_page =(struct userPageInfoElement*) alloc_block(sizeof(struct userPageInfoElement));
    new_page->v_address = va;
    new_page->size = size_we_will_allocate_it;
    new_page->marked = 1;
    LIST_INSERT_HEAD(&userMarkedPagesList, new_page);
    uheapPageAllocBreak = va + size_we_will_allocate_it;
    return (void*)va;
#else
panic("USE_KHEAP = 0");
#endif
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
#if USE_KHEAP
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	//Comment the following line
   //panic("sget() is not implemented yet...!!");

	//1-hangeep el size bta3 el object
    int size_of_shared_object = sys_size_of_shared_object(ownerEnvID, sharedVarName);
    if (size_of_shared_object <= 0)
        return NULL;
//2-hanehsep el mesahaa elle ha7gezha fe el process eltani
    uint32 num_pages = ROUNDUP(size_of_shared_object, PAGE_SIZE) / PAGE_SIZE;
    uint32 required_size = num_pages * PAGE_SIZE;

    struct userPageInfoElement *page, *exact_fit = NULL, *worst_fit = NULL;
    //ashof el exact fit or worst fit
    LIST_FOREACH(page, &userFreePagesList)
    {
        if (page->size == required_size)
        {
            if (exact_fit == NULL || page->v_address < exact_fit->v_address)
            	exact_fit = page;
        }
        else if (page->size > required_size)
        {
            if (worst_fit == NULL || page->size > worst_fit->size ||
                (page->size == worst_fit->size && page->v_address < worst_fit->v_address))
            	worst_fit = page;
        }
    }

    uint32 va;
    if (exact_fit != NULL)
    {
        va = exact_fit->v_address;
        exact_fit->marked = 1;
        LIST_REMOVE(&userFreePagesList, exact_fit);
        LIST_INSERT_HEAD(&userMarkedPagesList, exact_fit);
        //ageep el shared object
        int shared_object_id = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)va);
        if (shared_object_id == E_SHARED_MEM_NOT_EXISTS)
        {
            exact_fit->marked = 0;
            LIST_REMOVE(&userMarkedPagesList, exact_fit);
            LIST_INSERT_HEAD(&userFreePagesList, exact_fit);
            return NULL;
        }
        return (void*)va;
    }

    if (worst_fit != NULL)
    {
        va = worst_fit->v_address;
        int shared_object_id = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)va);
        if (shared_object_id == E_SHARED_MEM_NOT_EXISTS)
            return NULL;

        struct userPageInfoElement *new_worst = (struct userPageInfoElement*)alloc_block(sizeof(struct userPageInfoElement));
        new_worst->v_address = va;
        new_worst->size = required_size;
        new_worst->marked = 1;
        LIST_REMOVE(&userFreePagesList,worst_fit);
        LIST_INSERT_HEAD(&userMarkedPagesList, new_worst);
        worst_fit->v_address += required_size;
        worst_fit->size -= required_size;
        if(worst_fit->size>0){
           LIST_INSERT_HEAD(&userFreePagesList,worst_fit);
         }
        return (void*)va;
    }
//lw mafesh mesaha harfa3 el break
    va = uheapPageAllocBreak;
    if((uint64)((uint64)(va)+required_size)>USER_HEAP_MAX){
         return NULL;
     }
    int shared_object_id = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)va);
    if (shared_object_id == E_SHARED_MEM_NOT_EXISTS){
        return NULL;
    }
    struct userPageInfoElement *new_page = (struct userPageInfoElement*)alloc_block(sizeof(struct userPageInfoElement));
    new_page->v_address = va;
    new_page->size = required_size;
    new_page->marked = 1;
    LIST_INSERT_HEAD(&userMarkedPagesList, new_page);
    uheapPageAllocBreak += required_size;
    return (void*)va;
#else
panic("USE_KHEAP = 0");
#endif
}
//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
