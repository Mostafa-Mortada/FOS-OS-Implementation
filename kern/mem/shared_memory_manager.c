#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{

#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	//Comment the following line
	//panic("alloc_share() is not implemented yet...!!");
	struct Share *shared_object=(struct Share*)kmalloc(sizeof(struct Share));
	if(shared_object==NULL){
		return NULL;
	}
	shared_object->ownerID = ownerID;
	shared_object->references=1;
	//strncpy(shared_object->name, shareName, sizeof(shared_object->name));
	strcpy(shared_object->name,shareName);
	shared_object->size=size;
	shared_object->isWritable=isWritable ;
	uint32 va_from_kmalloc=(uint32) shared_object;
	//el id = virtual address ba3d ma n3mel msb =0 3alashan n5aly eleshara dayman (+)
	shared_object->ID= (va_from_kmalloc & 0x7FFFFFFF);

	uint32 number_of_frame_storage_we_need=ROUNDUP(shared_object->size,PAGE_SIZE)/PAGE_SIZE;
	//lma a3mel kmalloc framestorage hab3at size kol 3onsor gowa struct frameinfo w adrapoh fi 3adad el frames kolaha 3alashan ageep elhagm koloh
	shared_object->framesStorage = (struct FrameInfo**) kmalloc(sizeof(struct FrameInfo*) * number_of_frame_storage_we_need);
	//lw m3reftsh ahgez a3mel undo lel shared object with kfree
	if (shared_object->framesStorage == NULL)
	    {
	        kfree(shared_object);
	        return NULL;
	    }
    // for (uint32 i = 0; i < number_of_frame_storage_we_need; i++){
		//        shared_object->framesStorage[i] = 0;
	 //}

	    memset((void *) shared_object->framesStorage, 0, number_of_frame_storage_we_need * sizeof(struct FrameInfo*));


  return shared_object;
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");
#if USE_KHEAP
	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object
	//uint32 num_pages = size/ PAGE_SIZE;
	uint32 num_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	//1-hashoof el object da mawgood wala la
	struct Share *is_shared_object_exist=find_share(ownerID, shareName);
	if (is_shared_object_exist != NULL) {
	    return E_SHARED_MEM_EXISTS;
	}

	//2-h3mel creat el object
	struct Share * shared_object = alloc_share(ownerID, shareName, size, isWritable);
	if (shared_object == NULL) {
	    return E_NO_SHARE;
	}
	 //ha7gez we a3mel map le kol elmesaha elmatlopa mn awel el start va l7ad el size elle 3andy
	 //hadeef el frames elle 7agaztaha fe frame storage
    struct FrameInfo* frame ;
	for (uint32 i = 0; i < num_pages; i++)
	  {
		//hanehgez elfremat elle 3ayzenha 3la hasap elsize elle m3ana
		//lw mafesh mesaha 3and el memory ahgez feha el frames araga3 error en mafesh mesaha
		 int ret = allocate_frame(&frame);
	      if (ret ==E_NO_MEM) {
	    	  //lw m3reftsh a7gez bamsa7 el object elle et3amal we a3mel free el framestorag
	    	  kfree(shared_object->framesStorage);
	    	  kfree(shared_object);
	    	  return E_NO_SHARE;
	     }

	   shared_object->framesStorage[i] = frame;
	  }
	//hena el permisstion read write 3alashan ana elle 3amalt creat a3raf a read we a3raf a write
	  int perm = PERM_WRITEABLE | PERM_USER|PERM_UHPAGE;
	    for (uint32 i = 0; i < num_pages; i++)
	    {
	    	//hena ha3mel map lekol el frames m3a el va ell hagaznah we 3alashan a7aded el page allocator wala la 3alasahan malloc w smalloc
	        //matsta5demsh elpage de tani ba7ot (1<<9) we deh 3and PERM_AVAILABLE 0xE00 (9,10,11)	// Available for software use
	        //we keda 3malt en el page deh 3ayenfa3sh ahgez 3andaha tani aw momken a3mel #define PERM_UHPAGE 	0x400 	//Page in User Heap
	    	//el hagteen pye3meloo nafs elhaga
	        map_frame(myenv->env_page_directory, shared_object->framesStorage[i], (uint32) virtual_address, perm);
	       //kol mara ba7gez fehe bazawed page wahda 3alashan ana bamshy page py page
	        virtual_address += PAGE_SIZE;
	    }
	    bool wasHeld = holding_kspinlock(&AllShares.shareslock);
	  	if (!wasHeld) acquire_kspinlock(&AllShares.shareslock);
	  	LIST_INSERT_HEAD(&AllShares.shares_list, shared_object);
	  	if (!wasHeld) release_kspinlock(&AllShares.shareslock);
  return shared_object->ID;
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}

//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
	//Your code is here
	//Comment the following line
	//panic("get_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	//1-ageep el object elle ba3mel search 3aleh
	 struct Share* shared_object_we_search_for = find_share(ownerID, shareName);
	    if (shared_object_we_search_for == NULL)
	        return E_SHARED_MEM_NOT_EXISTS;
	    //2-ahseb 3adad el pages elle hagezha
	    uint32 num_pages_we_will_need = ROUNDUP(shared_object_we_search_for->size, PAGE_SIZE) / PAGE_SIZE;
	    //3- a3raf el permission writable aw read only
	    //  int perm = PERM_USER;
	    //if (shared_object_we_search_for->isWritable==1)
	      //  perm |= PERM_WRITEABLE;
	    //ageep el frames storage elle betshawer 3aleha we a3mel map m3a el process el gded
	    for (uint32 i = 0; i < num_pages_we_will_need; i++)
	    {
	        struct FrameInfo* frame = shared_object_we_search_for->framesStorage[i];
	        if(shared_object_we_search_for->isWritable==1)
	        {

	        	int result = map_frame(myenv->env_page_directory, frame, (uint32)virtual_address+i*PAGE_SIZE ,PERM_UHPAGE|PERM_WRITEABLE | PERM_USER);
	        }else{
	        	int result = map_frame(myenv->env_page_directory, frame, (uint32)virtual_address+i*PAGE_SIZE ,PERM_UHPAGE|PERM_USER);

	        }

	      }
	    //hazawed 3adad el reference elle byshawer 3ala el shared object da we lazem asta3mel el locks 3alashan el object ma3mol we mawgood gowa el sharelist
	   bool wasHeld = holding_kspinlock(&AllShares.shareslock);
	   if (!wasHeld) acquire_kspinlock(&AllShares.shareslock);
	   shared_object_we_search_for->references =shared_object_we_search_for->references +1;
	   if (!wasHeld) release_kspinlock(&AllShares.shareslock);
  return shared_object_we_search_for->ID;
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}
//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================

//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	//panic("free_share() is not implemented yet...!!");
#if USE_KHEAP
	//1-asoof feh sharedobject aslan wala laa
	if(ptrShare==NULL){
		return;
	}
	//2- lw feh object a3mel free mn el share list we a3mel free lel framestorage
	if(ptrShare->framesStorage!=NULL){
		 kfree(ptrShare->framesStorage);
	}
	ptrShare->framesStorage=NULL;
	bool wasHeld = holding_kspinlock(&AllShares.shareslock);
    if (!wasHeld) acquire_kspinlock(&AllShares.shareslock);
    LIST_REMOVE(&AllShares.shares_list, ptrShare);
    if (!wasHeld) release_kspinlock(&AllShares.shareslock);
	kfree(ptrShare);
 return;

#endif
}

//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	//panic("delete_shared_object() is not implemented yet...!!");
#if USE_KHEAP
	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"
	 struct Share * shared_object = NULL;
	 bool wasHeld = holding_kspinlock(&AllShares.shareslock);
	 if (!wasHeld) acquire_kspinlock(&AllShares.shareslock);
	 struct Share* shared_object_we_search_for;
	 //1-ageep elsharedobject mn el list
	 LIST_FOREACH(shared_object_we_search_for,&(AllShares.shares_list))
		if(shared_object_we_search_for->ID== sharedObjectID )
	     {
			shared_object=shared_object_we_search_for;
		    break;
		  }
	 if (!wasHeld) release_kspinlock(&AllShares.shareslock);
    if(shared_object==NULL){
    	return E_SHARED_MEM_NOT_EXISTS;
    }
    //2,3-ashoof 3adad el pages elle hamsaha we a3mel unmap we lw kan el table 5alas baa fady bamsaho
    uint32 number_of_pages = ROUNDUP(shared_object->size, PAGE_SIZE) / PAGE_SIZE;
     for (uint32 i = 0; i < number_of_pages; i++)
     {
    //     uint32 va = (uint32)startVA + i * PAGE_SIZE;
         unmap_frame(myenv->env_page_directory, (uint32)startVA + i * PAGE_SIZE);
         uint32 *ptr_page_table = NULL;
         get_page_table(myenv->env_page_directory, (uint32)startVA + i * PAGE_SIZE, &ptr_page_table);

         if (ptr_page_table != NULL)
         {

             bool empty = 1;
             for (uint32 j = 0; j < NPTENTRIES; j++)
             {
                 if (ptr_page_table[j] != 0) {
                     empty = 0;
                     break;
                 }
             }

             if (empty==1) {
                 del_page_table(myenv->env_page_directory, (uint32)startVA + i * PAGE_SIZE);
             }
         }
     }
     //4- b3mel update el elreference batrah menha 1 elle howa bta3 elshared object elle masa7nah
    bool wasHeld2 = holding_kspinlock(&AllShares.shareslock);
    if (!wasHeld2) acquire_kspinlock(&AllShares.shareslock);
    shared_object->references-=1;
    //5-law kan da a5er reference ma3na keda mafesh haga betshawer 3ala el object da tani fahnemsa7o mn el lista kolaha we hanesta5dem free_share
   if(shared_object->references==0){
    	free_share(shared_object);
    }
  if (!wasHeld2) release_kspinlock(&AllShares.shareslock);
  tlbflush();
  //lw el function neghet traga3 0
  return 0;

#else
	panic("USE_KHEAP = 0");
#endif

}
