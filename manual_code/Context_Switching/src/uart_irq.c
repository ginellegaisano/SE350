/**
 * @brief: uart_irq.c 
 * @author: NXP Semiconductors
 * @author: Y. Huang
 * @date: 2014/02/08
 */

#include <LPC17xx.h>
#include "uart.h"
#include "k_memory.h"
#include "uart_polling.h"
#ifdef DEBUG_0
#include "printf.h"
#include "msg.h"
#include "rtx.h"
#endif


uint8_t g_buffer[]= "0123456789012345";
uint8_t *gp_buffer = g_buffer;
uint8_t g_send_char = 0;
uint8_t g_char_in;
uint8_t g_char_out;
bool waiting_for_command = false;
bool clock_on = false;
int char_count = 0;

/**
 * @brief: initialize the n_uart
 * NOTES: It only supports UART0. It can be easily extended to support UART1 IRQ.
 * The step number in the comments matches the item number in Section 14.1 on pg 298
 * of LPC17xx_UM
 */
 
bool check_format(char *str) {
	int i;
	for (i = 3; i < 10; i = i + 3) {
		if (str[i] < '0' && str[i] > '9' && str[i+1] < '0' && str[i+1] > '9')
			return false;
	}
	return true;
} 
int uart_irq_init(int n_uart) {

	LPC_UART_TypeDef *pUart;

	if ( n_uart ==0 ) {
		/*
		Steps 1 & 2: system control configuration.
		Under CMSIS, system_LPC17xx.c does these two steps
		 
		-----------------------------------------------------
		Step 1: Power control configuration. 
		        See table 46 pg63 in LPC17xx_UM
		-----------------------------------------------------
		Enable UART0 power, this is the default setting
		done in system_LPC17xx.c under CMSIS.
		Enclose the code for your refrence
		//LPC_SC->PCONP |= BIT(3);
	
		-----------------------------------------------------
		Step2: Select the clock source. 
		       Default PCLK=CCLK/4 , where CCLK = 100MHZ.
		       See tables 40 & 42 on pg56-57 in LPC17xx_UM.
		-----------------------------------------------------
		Check the PLL0 configuration to see how XTAL=12.0MHZ 
		gets to CCLK=100MHZin system_LPC17xx.c file.
		PCLK = CCLK/4, default setting after reset.
		Enclose the code for your reference
		//LPC_SC->PCLKSEL0 &= ~(BIT(7)|BIT(6));	
			
		-----------------------------------------------------
		Step 5: Pin Ctrl Block configuration for TXD and RXD
		        See Table 79 on pg108 in LPC17xx_UM.
		-----------------------------------------------------
		Note this is done before Steps3-4 for coding purpose.
		*/
		
		/* Pin P0.2 used as TXD0 (Com0) */
		LPC_PINCON->PINSEL0 |= (1 << 4);  
		
		/* Pin P0.3 used as RXD0 (Com0) */
		LPC_PINCON->PINSEL0 |= (1 << 6);  

		pUart = (LPC_UART_TypeDef *) LPC_UART0;	 
		
	} else if ( n_uart == 1) {
	    
		/* see Table 79 on pg108 in LPC17xx_UM */ 
		/* Pin P2.0 used as TXD1 (Com1) */
		LPC_PINCON->PINSEL4 |= (2 << 0);

		/* Pin P2.1 used as RXD1 (Com1) */
		LPC_PINCON->PINSEL4 |= (2 << 2);	      

		pUart = (LPC_UART_TypeDef *) LPC_UART1;
		
	} else {
		return 1; /* not supported yet */
	} 
	
	/*
	-----------------------------------------------------
	Step 3: Transmission Configuration.
	        See section 14.4.12.1 pg313-315 in LPC17xx_UM 
	        for baud rate calculation.
	-----------------------------------------------------
        */
	
	/* Step 3a: DLAB=1, 8N1 */
	pUart->LCR = UART_8N1; /* see uart.h file */ 

	/* Step 3b: 115200 baud rate @ 25.0 MHZ PCLK */
	pUart->DLM = 0; /* see table 274, pg302 in LPC17xx_UM */
	pUart->DLL = 9;	/* see table 273, pg302 in LPC17xx_UM */
	
	/* FR = 1.507 ~ 1/2, DivAddVal = 1, MulVal = 2
	   FR = 1.507 = 25MHZ/(16*9*115200)
	   see table 285 on pg312 in LPC_17xxUM
	*/
	pUart->FDR = 0x21;       
	
 

	/*
	----------------------------------------------------- 
	Step 4: FIFO setup.
	       see table 278 on pg305 in LPC17xx_UM
	-----------------------------------------------------
        enable Rx and Tx FIFOs, clear Rx and Tx FIFOs
	Trigger level 0 (1 char per interrupt)
	*/
	
	pUart->FCR = 0x07;

	/* Step 5 was done between step 2 and step 4 a few lines above */

	/*
	----------------------------------------------------- 
	Step 6 Interrupt setting and enabling
	-----------------------------------------------------
	*/
	/* Step 6a: 
	   Enable interrupt bit(s) wihtin the specific peripheral register.
           Interrupt Sources Setting: RBR, THRE or RX Line Stats
	   See Table 50 on pg73 in LPC17xx_UM for all possible UART0 interrupt sources
	   See Table 275 on pg 302 in LPC17xx_UM for IER setting 
	*/
	/* disable the Divisior Latch Access Bit DLAB=0 */
	pUart->LCR &= ~(BIT(7)); 
	
	//pUart->IER = IER_RBR | IER_THRE | IER_RLS; 
	pUart->IER = IER_RBR | IER_RLS;

	/* Step 6b: enable the UART interrupt from the system level */
	
	if ( n_uart == 0 ) {
		NVIC_EnableIRQ(UART0_IRQn); /* CMSIS function */
	} else if ( n_uart == 1 ) {
		NVIC_EnableIRQ(UART1_IRQn); /* CMSIS function */
	} else {
		return 1; /* not supported yet */
	}
	pUart->THR = '\0';
	return 0;
}


/**
 * @brief: use CMSIS ISR for UART0 IRQ Handler
 * NOTE: This example shows how to save/restore all registers rather than just
 *       those backed up by the exception stack frame. We add extra
 *       push and pop instructions in the assembly routine. 
 *       The actual c_UART0_IRQHandler does the rest of irq handling
 */
__asm void UART0_IRQHandler(void)
{
	PRESERVE8
	IMPORT c_UART0_IRQHandler
	PUSH{r4-r11, lr}
	BL c_UART0_IRQHandler
	POP{r4-r11, pc}
} 
/**
 * @brief: c UART0 IRQ Handler
 */
void c_UART0_IRQHandler(void)
{
	uint8_t IIR_IntId;	    // Interrupt ID from IIR 		 
	LPC_UART_TypeDef *pUart = (LPC_UART_TypeDef *)LPC_UART0;
	int i;
	msgbuf *message;
	  
	/* Reading IIR automatically acknowledges the interrupt */
	IIR_IntId = (pUart->IIR) >> 1 ; // skip pending bit in IIR 
	if (IIR_IntId & IIR_RDA) { // Receive Data Avaialbe
		/* read UART. Read RBR will clear the interrupt */
		__disable_irq();
		g_char_in = pUart->RBR;
		message = k_allocate_message(DEFAULT, " ");
		message->mtext[0] = g_char_in;
		k_send_message(NUM_PROCS - 1, message); //send a message to UART.
		__enable_irq();
/*
#ifdef DEBUG_0
		//printf("Reading a char = ");
		//printf("%d", g_char_in);
		//printf("\n\r");
		printf("Reading a char = %c \n\r", g_char_in);
#endif // DEBUG_0
*/
// 		printf("%c", g_char_in);
// 		if (!waiting_for_command) {
// 				if(g_char_in == 'r') {
// 					printReadyQ(" ");
// 			}
// 			else if (g_char_in == 'b') {
// 					printBlockedQ(" ");
// 			}
// 			else if(g_char_in == 'm'){
// 				printBlockedReceiveQ(" ");
// 			}
// 			else if (g_char_in == '%') {
// 				waiting_for_command = true;
// 			}
// 		} else {
// 			if (g_char_in == '\x08'){
// 				if(char_count > 0){
// 					char_count--;
// 				}
// 				//printf("\n\rbackspace is now allowed! \n\r press enter to continue.\n\r");
// 			}else if (char_count < 16 && g_char_in != '\x0D') {
// 				g_buffer[char_count] = g_char_in;
// 				char_count++;
// 			}
// 			else {
// 				if (char_count == 1 && g_buffer[0] == 'W') {
// 					if (clock_on == false){
// 						//request the memory to make the message block
// 						//we made a message envelope. called envelope
// 						
// 						clock_on = true;
// 					}
// 						msg = (Message *) k_request_memory_block();
// 						envelope = (msgbuf *) k_request_memory_block();
// 						envelope->mtype = 0;
// 						envelope->mtext[0] = ' ';
// 						msg->message = envelope;
// 						k_send_message(NUM_PROCS - 2, msg);
// 				} else if (clock_on == true && char_count == 11 && g_buffer[0] == 'W' && g_buffer[1] == 'S' && check_format((char *)g_buffer)) {
// 						msg = (Message *) k_request_memory_block();
// 						envelope = (msgbuf *) k_request_memory_block();
// 						envelope->mtype = 0;
// 						for (i = 1; i < char_count; i++) {
// 							envelope->mtext[i-1] = g_buffer[i];
// 						}
// 						msg->message = envelope;
// 						k_send_message(NUM_PROCS - 2, msg);
// 				}
// 					else if (clock_on == true && char_count == 2 && g_buffer[0] == 'W' && g_buffer[1] == 'R') {
// 						msg = (Message *) k_request_memory_block();
// 						envelope = (msgbuf *) k_request_memory_block();
// 						envelope->mtype = 0;
// 						envelope->mtext[0] = g_buffer[1];
// 						msg->message = envelope;
// 						k_send_message(NUM_PROCS - 2, msg); 
// 					}
// 					else if (clock_on == true && char_count == 2 && g_buffer[0] == 'W' && g_buffer[1] == 'T') {
// 						msg = (Message *) k_request_memory_block();
// 						envelope = (msgbuf *) k_request_memory_block();
// 						envelope->mtype = 0;
// 						envelope->mtext[0] = g_buffer[1];
// 						msg->message = envelope;
// 						k_send_message(NUM_PROCS - 2, msg);
// 						clock_on = false;
// 					}
// 				else {
// 					msg = (Message *) k_request_memory_block();
// 					envelope = (msgbuf *) k_request_memory_block();
// 					envelope->mtype = 0;
// 					for (i = 0; i < char_count; i++) {
// 							envelope->mtext[i] = g_buffer[i];
// 					}
// 					msg->message = envelope;
// 					k_send_message(NUM_PROCS - 1, msg); 
//  					
// 					waiting_for_command = false;
// 					char_count = 0;
// 				}
// 				waiting_for_command = false;
// 				char_count = 0;
// 			}
// 		}
		
		//g_buffer[12] = g_char_in; // nasty hack		g_send_char = 1;
	} else if (IIR_IntId & IIR_THRE) {
	/* THRE Interrupt, transmit holding register becomes empty */
/*
		if (*gp_buffer != '\0' ) {
			g_char_out = *gp_buffer;
#ifdef DEBUG_0
			//uart1_put_string("Writing a char = ");
			//uart1_put_char(g_char_out);
			//uart1_put_string("\n\r");
			
			// you could use the printf instead
			printf("Writing a char = %c \n\r", g_char_out);
#endif // DEBUG_0
			pUart->THR = g_char_out;
			gp_buffer++;
		} else {

#ifdef DEBUG_0
			uart1_put_string("Finish writing. Turning off IER_THRE\n\r");
#endif // DEBUG_0

			pUart->IER ^= IER_THRE; // toggle the IER_THRE bit 
			pUart->THR = '\0';
			g_send_char = 0;
			gp_buffer = g_buffer;		
		}
*/
	} else {  /* not implemented yet */
		
		//TODO: figure out what goes here
		return;
	}	
}
