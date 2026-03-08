// Kernel-level Semaphore

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "ksemaphore.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_ksemaphore(struct ksemaphore *ksem, int value, char *name)
{
	init_channel(&(ksem->chan), "ksemaphore channel");
	init_kspinlock(&(ksem->lk), "lock of ksemaphore");
	strcpy(ksem->name, name);
	ksem->count = value;
}

void wait_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #6 SEMAPHORE - wait_ksemaphore
	//Your code is here
	//Comment the following line
//	panic("wait_ksemaphore() is not implemented yet...!!");
#if USE_KHEAP
	//protect the count by spinlock
	acquire_kspinlock(&ksem->lk);

	//count-- wlw ba2a 22al men 0 khaleh block (sleep)
	ksem->count-=1;

	if(ksem->count<0){
		sleep(&(ksem->chan),&(ksem->lk));
	}

	release_kspinlock(&ksem->lk);
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}

void signal_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #7 SEMAPHORE - signal_ksemaphore
	//Your code is here
	//Comment the following line
//	panic("signal_ksemaphore() is not implemented yet...!!");
#if USE_KHEAP
	//awel 7aga 22fel el lock w ++count
	acquire_kspinlock(&ksem->lk);
	ksem->count+=1;
	//law el count 22al men aw =0 23mel wakeup le awl process bas(wakeup one)
	if(ksem->count<=0){
		wakeup_one(&(ksem->chan));
		}
	release_kspinlock(&ksem->lk);
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}
