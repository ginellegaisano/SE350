/**
 * @file:   sys_proc.c
 * @brief:  6 user tests and 2 random procs
 * @author: GG (yes re), Reesey, RayMak, and LJ
 * @date:   2015/02/01
 * NOTE: Each process is in an infinite loop. Processes never terminate.
 */

#include "k_rtx.h"
#include "rtx.h"
#include "sys_proc.h"
#include "msg.h"
#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

void send_wall_clock_message(Envelope *msg, msgbuf *envelope);
void CRT_print(void);

void null_process(void) {
	while (1) {
		printf("NULL\n\r");
		release_processor();
	}
}


void wall_clock(void){
	int * blah; // the output parameter
	int hour=0;
	int minute=0;
	int second=0;
	int temp = 0;
	int i=0;
	msgbuf* envelope;
	Envelope *msg;
	
	//blocked on send!
	while(1){
		 msg = (Envelope *)receive_message(blah);
		 if (msg != NULL) { //checks if msg got deallocated?
			 if (msg->message->mtext[0] == ' ') { //increment second
				 second++;
				 if (second > 60) {
						minute++;
						if (minute > 60) {
							hour++;
							minute = 0;
						}
						second = 0;
					}
					printf("%02d:%02d:%02d", hour, minute, second);
					send_wall_clock_message(msg, envelope);
				} else if (msg->message->mtext[0] == 'R') {
						hour = 0;
						minute = 0;
						second = 0;
						printf("%02d:%02d:%02d", hour, minute, second);
						//deallocate then create a new one.
						send_wall_clock_message(msg, envelope);
				} else if (msg->message->mtext[0] == 'T') {
						hour = 0;
						minute = 0;
						second = 0;
				} else if (msg->message->mtext[0] == 'S') {
					for(i = 2; i < 10; i = i + 3) {
						temp = (msg->message->mtext[i] - '0') * 10 + msg->message->mtext[i + 1] - '0';
						switch(i) {
							case 2:
								hour = temp;
								break;
							case 5:
								minute = temp;
								break;
							case 8:
								second = temp;
								break;
						}
					}
					 if (second > 60) {
							minute++;
							if (minute > 60) {
								hour++;
								minute = minute % 60;
							}
							second = second % 60;
						}
					
					printf("%02d:%02d:%02d", hour, minute, second);
					send_wall_clock_message(msg, envelope);
				
			}
		}
	}
}
// void set_time(int *hour, int *minute, int *second){
// 	
// 	
// 	
// 	
// 	
// 	
// 	
// 	//DO SOMETHING HERE!
// 	
// 	
// 	
// 	
// 	
// 	
// 	
// // int m = second/60;
// // int h = 
// }
void CRT_print(void){
	char * str;
	int i;
	int * blah; // the output parameter
	Envelope *msg = (Envelope *)receive_message(blah);
	while(1){
		str = msg->message->mtext;
		k_release_memory_block(msg->message);
		k_release_memory_block(msg);
		//atomic(on)?????
		printf("\n\r");
		for (i = 0; i < sizeof(str)/sizeof(str[0]); i++) {
					printf("%c", str[i]);
		}
		//atomic(off)??
		
		
	}
}
void send_wall_clock_message(Envelope *msg, msgbuf *envelope){
					k_release_memory_block(msg->message);
					k_release_memory_block(msg);
					msg = (Envelope *) k_request_memory_block();
				  envelope = (msgbuf *) k_request_memory_block();
					envelope->mtype = 0; //DEFAULT TYPE???
					envelope->mtext[0] = ' ';
					msg->message = envelope;
					k_delayed_send(NUM_PROCS - 2, msg, 1); //assuming in ms
}
