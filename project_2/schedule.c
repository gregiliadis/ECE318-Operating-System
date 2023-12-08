/* schedule.c
 * This file contains the primary logic for the 
 * scheduler.
 */
#include "schedule.h"
#include "macros.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "privatestructs.h"

#define NEWTASKSLICE (NS_TO_JIFFIES(100000000))
#define constant_A 0.5

/* Local Globals
 * rq - This is a pointer to the runqueue that the scheduler uses.
 * current - A pointer to the current running task.
 */
struct runqueue *rq;
struct task_struct *current;

/* External Globals
 * jiffies - A discrete unit of time used for scheduling.
 *			 There are HZ jiffies in a second, (HZ is 
 *			 declared in macros.h), and is usually
 *			 1 or 10 milliseconds.
 */
extern long long jiffies;
extern struct task_struct *idle;

/*-----------------Initilization/Shutdown Code-------------------*/
/* This code is not used by the scheduler, but by the virtual machine
 * to setup and destroy the scheduler cleanly.
 */
 
 /* initscheduler
  * Sets up and allocates memory for the scheduler, as well
  * as sets initial values. This function should also
  * set the initial effective priority for the "seed" task 
  * and enqueu it in the scheduler.
  * INPUT:
  * newrq - A pointer to an allocated rq to assign to your
  *			local rq.
  * seedTask - A pointer to a task to seed the scheduler and start
  * the simulation.
  */
void initschedule(struct runqueue *newrq, struct task_struct *seedTask)
{
	seedTask->next = seedTask->prev = seedTask;
	newrq->head = seedTask;
	newrq->nr_running++;
}

/* killschedule
 * This function should free any memory that 
 * was allocated when setting up the runqueu.
 * It SHOULD NOT free the runqueue itself.
 */
void killschedule()
{
	return;
}


void print_rq () {
	struct task_struct *curr;
	
	printf("Rq: \n");
	curr = rq->head;
	if (curr)
		printf("%p", curr);
	while(curr->next != rq->head) {
		curr = curr->next;
		printf(", %p", curr);
	};
	printf("\n");
}

/*-------------Scheduler Code Goes Below------------*/
/* This is the beginning of the actual scheduling logic */

/* schedule
 * Gets the next task in the queue
 */
void schedule()
{
	struct task_struct *curr, *min;
	double temp_last_burst, temp_exp_burst;
	
	printf("In schedule\n");
	current->need_reschedule = 0; /* Always make sure to reset that, in case 
				       * we entered the scheduler because current*
				       * had requested so by setting this flag   */

	/*SHORTEST JOB FIRST*/
	switch(rq->nr_running){
		case 1: {
			curr = rq->head;
			break;
		}
		case 2: {
			curr = rq->head->next;
			/*If we had not the CPU in the previous cycle.*/
			/*Update last_burst and total waiting_time_rq.*/ 
			/*No need to compute the expected burst at this point
			 *since there are no other threads.*/
			/*If one thread will be created then rq->nr_running will become greater than 2
			 *so we will computed at the default case (since we are the current thread).*/
			/*In case we have 3 threads and the current thread will be interrupted current's 
			 *expected burst will be computed at the deactivate_task, schedule.c:300-309*/ 
			if( curr != current) {  
				curr->last_burst = sched_clock(); 
				curr->waiting_time_rq = curr->waiting_time_rq + sched_clock () - curr->rq_wait_time;
			}
			break;
		}
		default: {
			/****************************** COMPUTE EXPECTED_BURST ***********************************************/
			/* When a process is finished, current is set to idle. */
			/* idle should not change its last_burst and exp_burst */
			if (current != idle){
				/* Save the old values of last_burst and exp_burst*/
				/* in case current thread retains the CPU. */
			        /* We compute the updated values for the proper calculation
				 * of goodness.*/	
				temp_last_burst = current->last_burst;
				current->last_burst = sched_clock() - current->last_burst;
				temp_exp_burst = current->exp_burst;
				current->exp_burst = (current->last_burst + constant_A * current->exp_burst) / ( 1 + constant_A );
			}
			
			/*Find the minimun expected Burst.*/
			for (min = rq->head->next, curr = min->next; curr != rq->head; curr = curr->next){
				if (min->exp_burst > curr->exp_burst){
					min = curr;
				}
			}
			/*****************************************************************************************************/
		
			// min -> minimum exp_Burst
		
			/****************************** COMPUTE_WAITING_TIME **************************************************/	
			struct task_struct *max;
			/*No need to compute waiting time for the current thread. */
			/*So skip it.*/
			max = (current == rq->head->next) ? current->next: rq->head->next;

			/*At the task_struct we store the time that we enter the run queue or we lose the cpu and we
			 * remain at the run queue.*/
			/*value stored at ->rq_wait_time.*/
			for (curr = max->next; curr != rq->head; curr = curr->next){
				
				if( curr == current){ continue; } /* No need to check the current thread that is running
								     on CPU for waiting time in rq. */
			      	if ( max->rq_wait_time > curr->rq_wait_time){
					max = curr; /* Instead of maximum rq_wait_time we compute the minimum since
						       sched_clock - max->rq_wait_time < sched_clock - curr->rq_wait_time
						       so we compute max->rq_wait_time > curr->rq_wait_time. */
				}
			}
			/******************************************************************************************************/
			// We compute the max waiting waiting according to the minimun rq_wait entry.

			/****************************** COMPUTE_GOODNESS ******************************************************/ 
			struct task_struct *min_goodness;
			double min_goodness_val;
			double goodness_temp;
			
			/*Compute goodness according to formula.*/

			/*Waiting time for each process is equal to sched_clock  - ...->rq_wait_time.*/
			/*                                          current time - time we enter the run queue/lost CPU.*/ 
			min_goodness = rq->head->next;
			if (min_goodness != current){
					min_goodness_val =  ((1 + min_goodness->exp_burst) / (1 + min->exp_burst) ) * 
		                  	    		    ((1 + (sched_clock() - max->rq_wait_time)) /(1 + sched_clock() - min_goodness->rq_wait_time));
			}
			else{		/*Current thread has 0 rq wait time.*/
					min_goodness_val =  ((1 + min_goodness->exp_burst) / (1 + min->exp_burst) ) *
		                  	    		    (1 + (sched_clock() - max->rq_wait_time));
			}
			
			
			for (curr = min_goodness->next; curr != rq->head; curr = curr->next){
				if (curr != current){
					goodness_temp = ((1 + curr->exp_burst) /(1 + min->exp_burst) ) *
				        	        ((1 + (sched_clock() - max->rq_wait_time)) /(1 + sched_clock() - curr->rq_wait_time));
				}
				else{	/*Current thread has 0 rq wait time.*/
					goodness_temp = ((1 + curr->exp_burst) /(1 + min->exp_burst)) * (1 + (sched_clock() - max->rq_wait_time));
				}
				/*Update the min goodness. */
				if ( goodness_temp < min_goodness_val){
					min_goodness_val = goodness_temp;
					min_goodness = curr;
				}
			}
			
			/******************************************************************************************************/
			/*curr points to the thread that WILL get the CPU. */
			curr = min_goodness;
			/*We update the last_burst since we will now get the CPU. */ 
			curr->last_burst = sched_clock();
		
			if( curr == current ) { 
				/*When current thread keeps CPU.*/
				curr->last_burst = temp_last_burst;
				curr->exp_burst = temp_exp_burst;

				/* When thread gets CPU. */
				curr->rq_wait_time = 0;
			}
			else{
				// Curr thread will get CPU, compute waiting time.
				curr->waiting_time_rq = curr->waiting_time_rq + sched_clock () - curr->rq_wait_time;
				
				/* When thread gets CPU. */
				curr->rq_wait_time = 0;

				if (current != idle){
					/*When current thread loses CPU. */
					current->rq_wait_time = sched_clock();
				}
			}
			
			break;
		}
	}
	
	context_switch(curr);

}


/* sched_fork
 * Sets up schedule info for a newly forked task
 */
void sched_fork(struct task_struct *p)
{
	p->time_slice = 100;
	
	/*extra*/
	p->last_burst = 0;
	p->exp_burst = 0;
	p->rq_wait_time = 0;
	p->waiting_time_rq = 0;
}

/* scheduler_tick
 * Updates information and priority
 * for the task that is currently running.
 */
#define CLOCK_HZ 500000
#define TICKS_TO_MS(tick) ((long long)((tick) / (long double)CLOCK_HZ * (1000)))

void scheduler_tick(struct task_struct *p)
{	
	schedule();
}

/* wake_up_new_task
 * Prepares information for a task
 * that is waking up for the first time
 * (being created).
 */
void wake_up_new_task(struct task_struct *p)
{	
	p->next = rq->head->next;
	p->prev = rq->head;
	p->next->prev = p;
	p->prev->next = p;
	/*Time we enter the run_queue. 
	 *For first time.*/
	p->rq_wait_time = sched_clock();
	rq->nr_running++;
}

/* activate_task
 * Activates a task that is being woken-up
 * from sleeping.
 */
void activate_task(struct task_struct *p)
{
	p->next = rq->head->next;
	p->prev = rq->head;
	p->next->prev = p;
	p->prev->next = p;
	/*Time we enter the run_queue after an I/O interrupt.*/
	p->rq_wait_time = sched_clock();	
	rq->nr_running++;
	
}

/* deactivate_task
 * Removes a running task from the scheduler to
 * put it to sleep.
 */
void deactivate_task(struct task_struct *p)
{
	p->prev->next = p->next;
	p->next->prev = p->prev;
	p->next = p->prev = NULL; /* Make sure to set them to NUL *  		
				   * next is checked in cpu.c      */
	
	rq->nr_running--;
	/*In case there are 1(rq->nr_running == 2) or 0(rq->nr_running == 1) active threads (without init).
	 *since in the code at schedule.c:99-109 current_expected_burst is not computed, we have to update
 	 *current expected burst because we lose the cpu due to interrupt.*/	 
	if( rq->nr_running <= 2){
		p->last_burst = sched_clock() - current->last_burst;
		p->exp_burst = (p->last_burst + constant_A * current->exp_burst) / ( 1 + constant_A );
	}
}
