/**
 * @file:   k_memory.c
 * @brief:  kernel memory managment routines
 * @author: Yiqing Huang
 * @date:   2014/01/17
 */

#include "k_memory.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

/* ----- Global Variables ----- */
U32 *gp_stack; 
/* The last allocated stack low address. 8 bytes aligned */
/* The first stack starts at the RAM high address */
/* stack grows down. Fully decremental stack */

typedef struct Block Block;
struct Block { //fixed size, defined above
	Block* next;
	int pid;
} ;

BlockedElement* pop(Queue* self) {

	BlockedElement* element;
	#ifdef DEBUG_0
			printf("pop self: %d!\n\r",self == NULL);
	#endif /* DEBUG_0 */
	if (self == NULL || self->first == NULL) {
		return NULL;
	}
	else if (self->first->next == NULL) { //queue only has 1 element
		self->last = NULL;
	}
	element = self->first;
	self->first = self->first->next;
		#ifdef DEBUG_0
			printf("Exiting pop = %d!\n\r");
	#endif /* DEBUG_0 */
	return element;
};

int push(Queue* self,PCB* pcb) {
	BlockedElement* element;
	#ifdef DEBUG_0
		printf("\t\tpushing things!\n\r");
		printf("\t\telement: %d\n\r", element != NULL);
		printf("\t\tself: %d\n\r", self != NULL);
			printf("\t\tself->first: %d\n\r", self->first != NULL);

	#endif /* DEBUG_0 */
#ifdef DEBUG_0
			printf("\t\tchecking state of self now\n\r");
	#endif /* DEBUG_0 */
	if (self->first == NULL) { //queue was formerly empty
		#ifdef DEBUG_0
			printf("\t\tqueue was formerly empty\n\r");
	#endif /* DEBUG_0 */
			self->first = element;
	}
	else {
		#ifdef DEBUG_0
			printf("\t\tqueue HAD ELEMENTS\n\r");
	#endif /* DEBUG_0 */
		self->last->next = element;
	}

	element->process = pcb;
	element->next = NULL;
	self->last = element;
	#ifdef DEBUG_0
			printf("\tEND?\n\r");
	#endif /* DEBUG_0 */
	return RTX_OK;
};

Block* MSP;
//array of queues, organized by priority
Queue* blocked_resource_qs[NUM_PRIORITIES]; 
Queue* ready_qs[NUM_PRIORITIES];

/**
 * @brief: Initialize RAM as follows:

0x10008000+---------------------------+ High Address
          |    Proc 1 STACK           |
          |---------------------------|
          |    Proc 2 STACK           |
          |---------------------------|<--- gp_stack
          |                           |
          |        HEAP               |
          |                           |
          |---------------------------|
          |        PCB 2              |
          |---------------------------|
          |        PCB 1              |
          |---------------------------|
          |        PCB pointers       |
          |---------------------------|<--- gp_pcbs
          |        Padding            |
          |---------------------------|  
          |Image$$RW_IRAM1$$ZI$$Limit |
          |...........................|          
          |       RTX  Image          |
          |                           |
0x10000000+---------------------------+ Low Address

*/

void printInt (char c, int i) {
	#ifdef DEBUG_0
						printf("%c printInt: %d\n\r", c, i);
	#endif /* DEBUG_0 */
}

void pushToReadyQ (int priority, PCB* p_pcb_old) {
	#ifdef DEBUG_0
			printf("\tpushToReadyQ: %d!\n\r", priority);
	#endif /* DEBUG_0 */
	push(ready_qs[priority], p_pcb_old);
}

void popFromReadyQ (int priority) {
	#ifdef DEBUG_0
			printf("\tpopFromReadyQ: %d!\n\r", priority);
	#endif /* DEBUG_0 */
	pop(ready_qs[priority]);
}
	
void k_printMSP (void) {
	#ifdef DEBUG_0
						printf("MSP: 0x%x\n\r", MSP);
	#endif /* DEBUG_0 */
}

int getMSP (void) {
	return (int)MSP;
}

void memory_init(void)
{
	//need to set gp stack at some point 
	//(probably after doing STACK ALLOCATION)
	//so that process init doesn't mess up all the shit

	U8 *p_end = (U8 *)&Image$$RW_IRAM1$$ZI$$Limit;
	int i;
	
	#ifdef DEBUG_0  
		printf("\n\n\n\n\n\n\n\n\n\rNEW RUN\n\r");
	#endif
	/* 4 bytes padding */
	p_end += 4;

	/* allocate memory for pcb pointers   */
	gp_pcbs = (PCB **)p_end;
	p_end += NUM_TEST_PROCS * sizeof(PCB *);
	for ( i = 0; i < NUM_TEST_PROCS; i++ ) {
		gp_pcbs[i] = (PCB *)p_end;
		p_end += sizeof(PCB); 
	}
	
	/* initializing blocked and ready queues (array of queues, organized by PRIORITY) 
	Currently 1D for blocked, will need to eventually be 2D when we have multiple events */
	for ( i = 0; i < NUM_PRIORITIES; i++) {
		Queue* q = (Queue *)p_end;
		p_end += sizeof(Queue);
		q->first =NULL;
		q->last = NULL;
		blocked_resource_qs[i] = q;
	}
	
	for ( i = 0; i < NUM_PRIORITIES; i++) {
		Queue* q = (Queue *)p_end;
		p_end += sizeof(Queue);
		q->first = NULL;
		q->last = NULL;
		ready_qs[i] = q;
	}

	/* prepare for alloc_stack() to allocate memory for stacks */
	gp_stack = (U32 *)RAM_END_ADDR;
	if ((U32)gp_stack & 0x04) { /* 8 bytes alignment */
		--gp_stack; 
	}
	MSP = (Block *)(int)(p_end+4);
	MSP->pid = NULL;
	MSP->next = NULL;
	
	/* allocate memory for heap, not implemented yet here to initialize our memory linked list?*/	
	
	for ( i =(int)( p_end + 4 + BLOCK_SIZE); i < (int)(gp_stack - NUM_TEST_PROCS * USR_SZ_STACK); i+= BLOCK_SIZE ) {
		Block* current = (Block*)i;
		current->pid = NULL;
		current->next = MSP;
		MSP = current;
	}
}

/**
 * @brief: allocate stack for a process, align to 8 bytes boundary
 * @param: size, stack size in bytes
 * @return: The top of the stack (i.e. high address)
 * POST:  gp_stack is updated.
 */

U32 *alloc_stack(U32 size_b) 
{
	U32 *sp;
	sp = gp_stack; /* gp_stack is always 8 bytes aligned */
	
	/* update gp_stack */
	gp_stack = (U32 *)((U8 *)sp - size_b);

	/* 8 bytes alignement adjustment to exception stack frame */
	if ((U32)gp_stack & 0x04) {
		--gp_stack; 
	}
	return sp;
}

void *k_request_memory_block(void) {
//	atomic(on); does this mean...disable irq?
	Block * a = (Block *)MSP;
	int priority;
	 __disable_irq();

	while (MSP == NULL) {
			//get the priority of the current process by looking up pid in process table
		priority = g_proc_table[gp_current_process->m_pid-1].m_priority;
		#ifdef DEBUG_0 
			printf("Request memory: MSP is null.\n\r");
		#endif /* ! DEBUG_0 */
		//push PCB of current process on blocked_resource_qs;
		push(blocked_resource_qs[priority], gp_current_process);
		//update PCB of current process' state
		gp_current_process->m_state = BLOCKED_ON_RESOURCE;
		//k_release_processor(); //but we don't have access to this function here...
	}
	// increment MSP
	MSP = MSP->next;
	//assign process 1 mem block (need to make process table and memory table to assign shit too
	a->next = NULL;
	
	//atomic(off);
	 __enable_irq();
	return (void *) a;
}

int k_release_memory_block(void *p_mem_blk) {
	/** Shit to do
		*	Check that release block is valid
		* Check that the process actually owns that memory
		* Void all of the memory in that address
		* Make sure that we do not fubar any linked lists
		* Update memory table/process table ? 
		* Edge Cases:
		*		-Releasing start of memory list (need to upate memoy pointer in PCB)
		*		-Releasing middle of memory list (no swiss cheese list)
		*		- Releasing end of memory list (update null pointer)
	*/
	Block* released = (Block*)p_mem_blk;

	//atomic(on);

	if (released == NULL) {
		#ifdef DEBUG_0 
			printf("Memory block does not exist.\n\r");
		#endif /* ! DEBUG_0 */
		return RTX_ERR;
	} else if (released->pid != gp_current_process->m_pid) { //check if current process own memory block
		#ifdef DEBUG_0 
			printf("Current process does not own resource.\n\r");
		#endif /* ! DEBUG_0 */
			return RTX_ERR;
	}
		__disable_irq();

	released -> next = MSP -> next;
	released -> pid = NULL;
	MSP = released;
	
	#ifdef DEBUG_0 
		printf("k_release_memory_block: releasing block @ 0x%x\n\r", p_mem_blk);
	#endif /* ! DEBUG_0 */
	
	/* if (blocked_resource_q.size() > 0) {
			handle_process_ready(pop(blocked_resource_q));
	}*/
	
	//atomic(off);
	 __enable_irq();
	return RTX_OK;
}
