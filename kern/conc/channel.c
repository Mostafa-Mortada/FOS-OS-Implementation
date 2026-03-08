/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct kspinlock* lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #1 CHANNEL - sleep
	//Your code is here
	//Comment the following line
	//panic("sleep() is not implemented yet...!!");
#if USE_KHEAP
    //lock 3ala el queue el awel
	acquire_kspinlock(&ProcessQueues.qlock);
	release_kspinlock(lk);
	struct Env* current_process= get_cpu_proc();
		if(current_process==NULL){
			release_kspinlock(&ProcessQueues.qlock);
			return;
		}

	//7otaha fe el queue ba3den efta7 el guard


	//release_kspinlock(lk);   //== release(&guard) ba3d ma n7ot el thread fe el wait queue

	//khale el process blocked (sleep)
   // current_process->channel=chan;
	current_process->env_status=ENV_BLOCKED;
	enqueue(&(chan->queue),current_process);
    //e3mel shedual w efta7 lock el queue w 22fel el guard tane
	sched();
	release_kspinlock(&ProcessQueues.qlock);
	acquire_kspinlock(lk);  //guard==1

	return;
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #2 CHANNEL - wakeup_one
	//Your code is here
	//Comment the following line
//	panic("wakeup_one() is not implemented yet...!!");
#if USE_KHEAP
//	int already_locked= holding_kspinlock(&ProcessQueues.qlock);
	//if(already_locked==0){
	acquire_kspinlock(&ProcessQueues.qlock);
	//}
	struct Env* current_process;
	//nakhod awel process fe el blocked queue 3alashan nkhaleha ready
	current_process= dequeue(&(chan->queue));
	if(current_process!=NULL){
	//current_process->channel=NULL;
	current_process->env_status=ENV_READY;
	sched_insert_ready(current_process);

	}
	//if(already_locked==0)
	release_kspinlock(&ProcessQueues.qlock);
	return;
#else
  panic("not handled when KERN HEAP is disabled");
#endif

}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #3 CHANNEL - wakeup_all
	//Your code is here
	//Comment the following line
	//panic("wakeup_all() is not implemented yet...!!");
#if USE_KHEAP
	while(chan->queue.size!=0){
		wakeup_one(chan);

	}

//

	// han3mel deque lkol el blocked queue w nkhalehom ready
//	/acquire_kspinlock(&ProcessQueues.qlock);
//	while(queue_size(&(chan->queue))>0){
//		struct Env* current_process;
//			current_process= dequeue(&(chan->queue));
//		//	current_process->channel=NULL;
//			current_process->env_status=ENV_READY;
//			sched_insert_ready(current_process);
//	}
//	release_kspinlock(&ProcessQueues.qlock);
#else
  panic("not handled when KERN HEAP is disabled");
#endif
}

