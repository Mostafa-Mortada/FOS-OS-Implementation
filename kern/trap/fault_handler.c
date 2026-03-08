/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>
// 2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
//  0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
// 2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE;
}
void setPageReplacmentAlgorithmCLOCK() { _PageRepAlgoType = PG_REP_CLOCK; }
void setPageReplacmentAlgorithmFIFO() { _PageRepAlgoType = PG_REP_FIFO; }
void setPageReplacmentAlgorithmModifiedCLOCK() { _PageRepAlgoType = PG_REP_MODIFIEDCLOCK; }
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal() { _PageRepAlgoType = PG_REP_DYNAMIC_LOCAL; }
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps)
{
	_PageRepAlgoType = PG_REP_NchanceCLOCK;
	page_WS_max_sweeps = PageWSMaxSweeps;
}
/*2024*/ void setFASTNchanceCLOCK(bool fast) { FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL() { _PageRepAlgoType = PG_REP_OPTIMAL; };

// 2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE) { return _PageRepAlgoType == LRU_TYPE ? 1 : 0; }
uint32 isPageReplacmentAlgorithmCLOCK()
{
	if (_PageRepAlgoType == PG_REP_CLOCK)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmFIFO()
{
	if (_PageRepAlgoType == PG_REP_FIFO)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmModifiedCLOCK()
{
	if (_PageRepAlgoType == PG_REP_MODIFIEDCLOCK)
		return 1;
	return 0;
}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal()
{
	if (_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL)
		return 1;
	return 0;
}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK()
{
	if (_PageRepAlgoType == PG_REP_NchanceCLOCK)
		return 1;
	return 0;
}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL()
{
	if (_PageRepAlgoType == PG_REP_OPTIMAL)
		return 1;
	return 0;
}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt) { _EnableModifiedBuffer = enableIt; }
uint8 isModifiedBufferEnabled() { return _EnableModifiedBuffer; }

void enableBuffering(uint32 enableIt) { _EnableBuffering = enableIt; }
uint8 isBufferingEnabled() { return _EnableBuffering; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length; }
uint32 getModifiedBufferLength() { return _ModifiedBufferLength; }

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	// setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	// setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	// setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0);
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault = 0;
extern uint32 sys_calculate_free_frames();

struct Env *last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	// cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	// If same fault va for 3 times, then panic
	// UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env *cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va;
	last_faulted_env = cur_env;
	/******************************************************/
	// 2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3)
	{
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu *c = mycpu();
		// cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	// 2017: Check stack underflow for User
	else
	{
		// cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	// get a pointer to the environment that caused the fault at runtime
	// cprintf("curenv = %x\n", curenv);
	struct Env *faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	// check the faulted address, is it a table or not ?
	// If the directory entry of the faulted address is NOT PRESENT then
	if ((faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter++;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			// TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			// your code is here
#if USE_KHEAP
			uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);

			if (fault_va >= KERNEL_BASE)
			{
				env_exit();
			}
			else if ((perm & PERM_PRESENT) && !(perm & PERM_USER))
			{
				env_exit();
			}

			else if ((perm & PERM_PRESENT) && !(perm & PERM_WRITEABLE))
			{
				env_exit();
			}

			else if ((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) && !(perm & PERM_UHPAGE))
			{
				env_exit();
			}

#else
  panic("not handled when KERN HEAP is disabled");
#endif
			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va);
		/*============================================================================================*/

		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter++;

		//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
		//				cprintf("\nPage working set BEFORE fault handler...\n");
		//				env_page_ws_print(faulted_env);
		// int ffb = sys_calculate_free_frames();

		if (isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	// Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env *curenv, uint32 fault_va)
{
	// panic("table_fault_handler() is not implemented yet...!!");
	// Check if it's a stack page
	uint32 *ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

struct optimalWS_element
{
	unsigned int virtual_address;
	int next_use_index;
};

struct refArr
{
	unsigned int virtual_address;
};

#define INF 1000000
//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize, struct PageRef_List *pageReferences)
{

#if USE_KHEAP
	// TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	// Your code is here
	// Comment the following line
	// panic("get_optimal_num_faults() is not implemented yet...!!");

	const int n = LIST_SIZE(pageReferences);
	struct optimalWS_element *optimaWS = (struct optimalWS_element *)kmalloc(maxWSSize * sizeof(struct optimalWS_element));
	struct refArr *refStream = (struct refArr *)kmalloc(n * sizeof(struct refArr));

	// copy from init WS
	struct WorkingSetElement *it;
	int ws_size = 0;
	LIST_FOREACH_SAFE(it, initWorkingSet, WorkingSetElement)
	{
		optimaWS[ws_size].virtual_address = it->virtual_address;
		optimaWS[ws_size].next_use_index = INF;
		ws_size++;
	}
	// copy from page references
	struct PageRefElement *it2;
	int refIndex = 0;
	LIST_FOREACH(it2, pageReferences)
	{
		refStream[refIndex].virtual_address = it2->virtual_address;
		refIndex++;
	}

	// find next use index for initial WS
	for (int i = 0; i < ws_size; i++)
	{
		for (int j = 0; j < n; j++)
		{
			if (optimaWS[i].virtual_address == refStream[j].virtual_address)
			{
				optimaWS[i].next_use_index = j;
				break;
			}
		}
	}

	int pageFaults = 0;

	for (int RefIt = 0; RefIt < n; ++RefIt)
	{
		// check if it's in the WS
		bool found = 0;
		for (int wsIt = 0; wsIt < ws_size; ++wsIt)
		{
			if (optimaWS[wsIt].virtual_address == refStream[RefIt].virtual_address)
			{
				found = 1;
				// update next use index
				optimaWS[wsIt].next_use_index = INF;
				for (int k = RefIt + 1; k < n; k++)
				{
					if (optimaWS[wsIt].virtual_address == refStream[k].virtual_address)
					{
						optimaWS[wsIt].next_use_index = k;
						break;
					}
				}
				break;
			}
		}
		// not in the WS
		if (!found)
		{
			pageFaults++;
			// if there is space in the WS
			if (ws_size < maxWSSize)
			{
				optimaWS[ws_size].virtual_address = refStream[RefIt].virtual_address;
				optimaWS[ws_size].next_use_index = INF;

				for (int k = RefIt + 1; k < n; k++)
				{
					if (optimaWS[ws_size].virtual_address == refStream[k].virtual_address)
					{
						optimaWS[ws_size].next_use_index = k;
						break;
					}
				}
				ws_size++;
			}
			else
			{
				// choose victim
				int victimIndex = -1;
				int mx = -1;
				for (int i = 0; i < ws_size; i++)
				{
					if (optimaWS[i].next_use_index > mx)
					{
						mx = optimaWS[i].next_use_index;
						victimIndex = i;
					}
				}
				optimaWS[victimIndex].virtual_address = refStream[RefIt].virtual_address;
				optimaWS[victimIndex].next_use_index = INF;

				// update next use index
				for (int k = RefIt + 1; k < n; k++)
				{
					if (optimaWS[victimIndex].virtual_address == refStream[k].virtual_address)
					{
						optimaWS[victimIndex].next_use_index = k;
						break;
					}
				}
			}
		}
	}

	kfree(optimaWS);
	kfree(refStream);
	return pageFaults;
#else
	panic("USE_KHEAP = 0");
#endif
}

void page_fault_handler(struct Env *faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	{
		// TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		uint32 activeListSize = LIST_SIZE(&(faulted_env->ActiveList));

		if (activeListSize == 0)
		{
			struct WorkingSetElement *i;
			LIST_FOREACH_SAFE(i, &(faulted_env->page_WS_list), WorkingSetElement)
			{
				// add to the active list
				struct WorkingSetElement *e = env_page_ws_list_create_element(faulted_env, i->virtual_address);
				LIST_INSERT_TAIL(&(faulted_env->ActiveList), e);
			}
		}

		// check if it's in the memory
		uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);

		if (!(perm & PERM_PRESENT))
		{

			uint32 *page_table = NULL;

			get_page_table((uint32 *)faulted_env->env_page_directory, (uint32)fault_va, &page_table);

			if ((page_table == NULL) || (page_table[PTX(fault_va)] == 0))
			{
				struct FrameInfo *frame = NULL;

				allocate_frame(&frame);

				map_frame(faulted_env->env_page_directory, frame, fault_va, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);

				int res2 = pf_read_env_page(faulted_env, (void *)fault_va);
			}
			else
				pt_set_page_permissions(faulted_env->env_page_directory, fault_va, PERM_PRESENT, 0);
		}
		bool found = 0;

		struct WorkingSetElement *it;
		LIST_FOREACH_SAFE(it, &(faulted_env->ActiveList), WorkingSetElement)
		{
			unsigned int va = ROUNDDOWN(fault_va, PAGE_SIZE);
			if (it->virtual_address == va)
			{
				found = 1;
				break;
			}
		}

		if (found == 0)
		{
			uint32 wsSize = LIST_SIZE(&(faulted_env->ActiveList));

			// make pages not present and delete if from the Active List
			if (wsSize >= faulted_env->page_WS_max_size)
			{
				struct WorkingSetElement *it2;
				while (!LIST_EMPTY(&faulted_env->ActiveList))
				{
					it2 = LIST_FIRST(&faulted_env->ActiveList);
					pt_set_page_permissions(faulted_env->env_page_directory, it2->virtual_address, 0, PERM_PRESENT);
					LIST_REMOVE(&(faulted_env->ActiveList), it2);
				}
			}

			// add to the active list
			struct WorkingSetElement *e = env_page_ws_list_create_element(faulted_env, fault_va);
			LIST_INSERT_TAIL(&(faulted_env->ActiveList), e);
		}
		// add to the reference stream
		struct PageRefElement *elem = (struct PageRefElement *)kmalloc(sizeof(struct PageRefElement));
		elem->virtual_address = ROUNDDOWN(fault_va, PAGE_SIZE);
		LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), elem);
	}
	/*==============================================================================*/
	/*==============================================================================*/
	/*==============================================================================*/
	/*==============================================================================*/
	/*==============================================================================*/
	else
	{
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if (wsSize < (faulted_env->page_WS_max_size))
		{
			#if USE_KHEAP
			// TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
			// Your code is here
			// Comment the following line
			//  panic("page_fault_handler().PLACEMENT is not implemented yet...!!");

			struct FrameInfo *frame = NULL;
			int res = allocate_frame(&frame);
			if (res == E_NO_MEM)
			{
				panic("out of memory");
			}
			map_frame(faulted_env->env_page_directory, frame, fault_va, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);

			int res2 = pf_read_env_page(faulted_env, (void *)fault_va);
			if (res2 == E_PAGE_NOT_EXIST_IN_PF)
			{
				if (!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va >= USTACKBOTTOM && fault_va < USTACKTOP)))
				{
					unmap_frame(faulted_env->env_page_directory, fault_va);
					free_frame(frame);
					env_exit();
				}
			}
			struct WorkingSetElement *WSel = env_page_ws_list_create_element(faulted_env, fault_va);
			LIST_INSERT_TAIL(&(faulted_env->page_WS_list), WSel);
			faulted_env->page_last_WS_element = NULL;

			if ((wsSize + 1) == (faulted_env->page_WS_max_size))
			{
				faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
			}
			#else
				panic("not handled when KERN HEAP is disabled");
			#endif
		}
		else
		{
			if (isPageReplacmentAlgorithmCLOCK())
			{

				// TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				// Your code is here
				// Comment the following line
				// panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

				struct FrameInfo *frame = NULL;
				int res = allocate_frame(&frame);
				if (res == E_NO_MEM)
				{
					panic("out of memory");
				}
				map_frame(faulted_env->env_page_directory, frame, fault_va, PERM_USER | PERM_WRITEABLE | PERM_PRESENT | PERM_USED);

				int res2 = pf_read_env_page(faulted_env, (void *)fault_va);
				if (res2 == E_PAGE_NOT_EXIST_IN_PF)
				{
					if (!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va >= USTACKBOTTOM && fault_va < USTACKTOP)))
					{
						unmap_frame(faulted_env->env_page_directory, fault_va);
						free_frame(frame);
						env_exit();
					}
				}

				while (1)
				{
					uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory, faulted_env->page_last_WS_element->virtual_address);
					if (perm & PERM_USED)
					{
						pt_set_page_permissions(faulted_env->env_page_directory, faulted_env->page_last_WS_element->virtual_address, 0, PERM_USED);

						// move the pointer forward
						if (faulted_env->page_last_WS_element == LIST_LAST(&(faulted_env->page_WS_list)))
						{
							faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
						}
						else
						{
							faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
						}
					}
					else
					{
						struct WorkingSetElement *victim = faulted_env->page_last_WS_element;
						struct WorkingSetElement *newElement = env_page_ws_list_create_element(faulted_env, fault_va);

						uint32 vic_va = victim->virtual_address;
						uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory, vic_va);
						if (perm & PERM_MODIFIED)
						{
							uint32 *page_table = NULL;
							get_page_table((uint32 *)faulted_env->env_page_directory, (uint32)vic_va, &page_table);
							struct FrameInfo *vic_frame = get_frame_info(faulted_env->env_page_directory, vic_va, &page_table);

							int res = pf_update_env_page(faulted_env, vic_va, vic_frame);
							if (res == E_NO_PAGE_FILE_SPACE)
							{
								panic("the page isn't in the page file!");
							}
						}
						unmap_frame(faulted_env->env_page_directory, vic_va);

						if (victim == LIST_LAST(&(faulted_env->page_WS_list)))
						{
							LIST_REMOVE(&(faulted_env->page_WS_list), victim);
							LIST_INSERT_TAIL(&(faulted_env->page_WS_list), newElement);
							faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
						}
						else
						{
							struct WorkingSetElement *nxt = LIST_NEXT(faulted_env->page_last_WS_element);
							LIST_REMOVE(&(faulted_env->page_WS_list), victim);
							LIST_INSERT_BEFORE(&(faulted_env->page_WS_list), nxt, newElement);
							faulted_env->page_last_WS_element = nxt;
						}
						struct WorkingSetElement *it = LIST_FIRST(&(faulted_env->page_WS_list));

						while (1)
						{
							struct WorkingSetElement *t = it->prev_next_info.le_next;
							if (it == faulted_env->page_last_WS_element)
								break;

							LIST_REMOVE(&(faulted_env->page_WS_list), it);
							LIST_INSERT_TAIL(&(faulted_env->page_WS_list), it);
							it = t;
						}

						break;
					}
				}
			}
			else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			{
				// TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				// Your code is here
				// Comment the following line
				panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK())
			{
				// TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				// Your code is here
				// Comment the following line
				panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
		}
	}
#endif
}

void __page_fault_handler_with_buffering(struct Env *curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}
