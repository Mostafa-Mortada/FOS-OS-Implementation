// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	char prefix[30] = "lock of sleeplock - ";
	char guardName[30+NAMELEN];
	strcconcat(prefix, name, guardName);
	init_kspinlock(&(lk->lk), guardName);
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
	//init_kspinlock(&guard,"guard");
}

void acquire_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #4 SLEEP LOCK - acquire_sleeplock
	//Your code is here
	//Comment the following line
//	panic("acquire_sleeplock() is not implemented yet...!!");
#if USE_KHEAP
	if(holding_sleeplock(lk))
			return;
	//acquire_kspinlock(&guard);
	acquire_kspinlock(&lk->lk);
	//law heya BUSY =locked yb2a ro7 sleep
	while((lk->locked)==1){
		sleep(&(lk->chan),&(lk->lk));
	}
	struct Env *my_process;
	my_process=get_cpu_proc();
	//22fel el lock
	if(my_process!=NULL){
	lk->locked=1;
	lk->pid=my_process->env_id;
	}
	// efta7 el guard
	release_kspinlock(&lk->lk);
#else
  panic("not handled when KERN HEAP is disabled");
#endif

}

void release_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #5 SLEEP LOCK - release_sleeplock
	//Your code is here
	//Comment the following line
	//panic("release_sleeplock() is not implemented yet...!!");
#if USE_KHEAP
	if(!holding_sleeplock(lk)){
		return;
	}
// 22fel el guard w 23mel wakeup all
	acquire_kspinlock(&lk->lk);
	wakeup_all(&lk->chan);
	//release el lock w el guard
		lk->locked=0;
		lk->pid=0;
		release_kspinlock(&lk->lk);
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}

int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_kspinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_kspinlock(&(lk->lk));
	return r;
}
