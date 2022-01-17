// ESP-01 Application using STM32F446RE-Nucleo
// This program was tested with Keil uVision v5.33.0.0 with DFP v2.15.0
// By default, the clock is running at 16 MHz.
// 17 Jan 2022, Eskisehir

#include "stm32f4xx.h"
#include "UartRingbuffer.h"
#include <stdio.h>

#define MAIN_BUFFER_SIZE 	256

char SSID[] = "XXXXXXXXXXXXXXXXXXXXX";			// Update this array (SSID)
char PASSWD[] = "YYYYYYYYYYYY";				// Update this array (Password)

static uint16_t main_buffer_index = 0;
uint8_t main_buffer[MAIN_BUFFER_SIZE] = {0};

void USART2_init(void);
void USART2_write(int ch);
void send_string_to_USART2(char *str);

void UART4_init(void);
void UART4_write (int ch);
void send_string_to_UART4(char *str);

void ESP_01_init(void);
void delayMs(int n);
void UART4_to_USART2(uint8_t rb[]);
void flush_main_buffer(void);

extern ring_buffer rx_buffer; 			// UART4
extern ring_buffer tx_buffer; 			// UART4
extern ring_buffer rx_buffer2; 			// USART2
extern ring_buffer tx_buffer2;			// USART2

extern ring_buffer *_rx_buffer;			// UART4
extern ring_buffer *_tx_buffer;			// UART4
extern ring_buffer *_rx_buffer2;
extern ring_buffer *_tx_buffer2;


int main(void) {
	__disable_irq();
	USART2_init();          		// initialize USART2
	
	GPIOA->MODER &= ~0x00000C00;    	// clear pin mode
  	GPIOA->MODER |=  0x00000400;    	// set pin to output mode
	
	UART4_init(); 				// Initialize UART4 for ESP-01 data

	NVIC_EnableIRQ(USART2_IRQn);		// enable interrupt in NVIC
	NVIC_EnableIRQ(UART4_IRQn);		// enable interrupt in NVIC
			
	__enable_irq();				// global enable IRQs
	ESP_01_init();				// initialize ESP-01 module
	
	while(1) {     				// Loop forever
		if (strstr((char *)main_buffer, "LED_ON")) {
			Uart_sendstring(USART2, "Data received\n");
			GPIOA->BSRR = 0x00000020;   // turn on LED   
		}
				
		else if (strstr((char *)main_buffer, "LED_OFF")) {
			Uart_sendstring(USART2, "Data received\n");						     
			GPIOA->BSRR = 0x00200000;   // turn off LED			
		}
							
		else 
			Uart_sendstring(USART2, "not received\n");
		flush_main_buffer();
		delayMs(5000);	
		}
}


/*----------------------------------------------------------------------------
  Initialize USART2 to receive and transmit at 115200 Baud
 *----------------------------------------------------------------------------*/
void USART2_init (void) {
    RCC->AHB1ENR |= 1;          // Enable GPIOA clock
    RCC->APB1ENR |= 0x20000;    // Enable USART2 clock
	
		// Configure PA2 for USART2_TX
    GPIOA->AFR[0] &= ~0x0F00;
    GPIOA->AFR[0] |=  0x0700;   // alt7 for USART2
    GPIOA->MODER  &= ~0x0030;
    GPIOA->MODER  |=  0x0020;   // enable alternate function for PA2
	
		// Configure PA3 for USART2 RX
    GPIOA->AFR[0] &= ~0xF000;
    GPIOA->AFR[0] |=  0x7000;   // alt7 for USART2
    GPIOA->MODER  &= ~0x00C0;
    GPIOA->MODER  |=  0x0080;   // enable alternate function for PA3
		
		USART2->BRR = 0x008B;       // 115200 baud @ 16 MHz	
    USART2->CR1 = 0x000C;       // enable Rx/Tx, 8-bit data
    USART2->CR2 = 0x0000;       // 1 stop bit
    USART2->CR3 = 0x0000;       // no flow control
    USART2->CR1 |= 0x2000;      // enable USART2		
}


/* Write a character to USART2 */
void USART2_write (int ch) {
	while (!(USART2->SR & 0x0080)) {}   // wait until Tx buffer empty
  USART2->DR = (ch & 0xFF);
}


void send_string_to_USART2(char *str) {
	char temp_str[80];
	strcpy(temp_str, str);
	strcat(temp_str, "\r\n");
	int i = 0;
	while(temp_str[i] != '\0')
		USART2_write(temp_str[i++]);
}


/* initialize UART4 to receive/transmit at 115200 Baud */
void UART4_init (void) {
	RCC->AHB1ENR |= 1;    // Enable GPIOA clock
  RCC->APB1ENR |= 0x80000;    // Enable UART4 clock

  /* Configure PA0, PA1 for UART4 TX, RX */
  GPIOA->AFR[0] &= ~0x00FF;
  GPIOA->AFR[0] |=  0x0088;   // alt8 for UART4
  GPIOA->MODER  &= ~0x000F;
  GPIOA->MODER  |=  0x000A;   // enable alternate function for PA0, PA1 

  UART4->BRR = 0x008B;        // 115200 baud @ 16 MHz
  UART4->CR1 = 0x000C;        // enable Tx, Rx, 8-bit data
  UART4->CR2 = 0x0000;        // 1 stop bit
  UART4->CR3 = 0x0000;        // no flow control
  UART4->CR1 |= 0x2000;       // enable UART4
}


/* Write a character to UART4 */
void UART4_write (int ch) {
	while (!(UART4->SR & 0x0080)) {}   // wait until Tx buffer empty
  UART4->DR = (ch & 0xFF);
}


void send_string_to_UART4(char *str) {
	char temp_str[80];	
	strcpy(temp_str, str);
	strcat(temp_str, "\r\n");
	int i = 0;
	while(temp_str[i] != '\0')
		UART4_write(temp_str[i++]);
}


void ESP_01_init(void) {
	char data[80];		
	Ringbuf_init_Usart2();
	Ringbuf_init_Uart4();
			
	/********* AT+RST **********/
	Uart_flush();
	Uart_sendstring(UART4, "AT+RST\r\n");				// Reset ESP-01
	while (!(Wait_for("OK\r\n")));
	UART4_to_USART2(main_buffer);
			
	/********* AT **********/
	Uart_flush();
	Uart_flush_USART2();
	Uart_sendstring(UART4, "AT\r\n");
	while (!(Wait_for("OK\r\n")));
	UART4_to_USART2(main_buffer);	
	
	/********* AT+CWMODE=1 *********/
	Uart_flush();
	Uart_flush_USART2();
	Uart_sendstring(UART4, "AT+CWMODE=1\r\n");	// Run in STATION mode
	while (!(Wait_for("OK\r\n")));
	UART4_to_USART2(main_buffer);
			
	/********* AT+CWJAP="SSID","PASSWD" **********/
	Uart_flush();
	Uart_flush_USART2();
	sprintf (data, "AT+CWJAP=\"%s\",\"%s\"\r\n", SSID, PASSWD);
	Uart_sendstring(UART4, data);
	while (!(Wait_for("WIFI GOT IP\r\n")));		
	delayMs(500);
	UART4_to_USART2(main_buffer);
		
			
	/********* AT+CIPMUX **********/
	Uart_flush();
	Uart_flush_USART2();		
	Uart_sendstring(UART4, "AT+CIPMUX=0\r\n");
	while (!(Wait_for("OK\r\n")));		
	UART4_to_USART2(main_buffer);
	
	/********* AT+CIPSTART **********/
	Uart_flush();
	Uart_flush_USART2();
	Uart_sendstring(UART4, "AT+CIPSTART=\"UDP\",\"0.0.0.0\",5000,5000,2\r\n");
	while (!(Wait_for("OK\r\n")));
	UART4_to_USART2(main_buffer);
			
	flush_main_buffer();
}


void UART4_to_USART2(uint8_t rb[]) {
	Uart_flush_USART2();
	Uart_sendstring(USART2, (char *)rb);		
	flush_main_buffer();		
}


void flush_main_buffer(void) {
	memset(main_buffer, '\0', MAIN_BUFFER_SIZE);		
	main_buffer_index = 0;	
}


void delayMs(int n) {
	int i;
	SysTick->LOAD = 16000-1;
	SysTick->VAL  = 0;
	SysTick->CTRL = 0x5;
	for(i = 0; i < n; i++) {
		while((SysTick->CTRL & 0x10000) == 0);	// Wait until the COUNTFLAG is set
		}
	SysTick->CTRL = 0;										// Stop the timer
}


void USART2_IRQHandler(void) {
	/* if DR is not empty and the Rx Int is enabled */	
	if ((USART2->SR & 0x0020) && (USART2->CR1 & 0x0020)) {
		uint8_t data;
		data = USART2->DR;
		store_char (data, _rx_buffer2);			// store data in buffer					
    return;			
	}
			
	/*If interrupt is caused due to Transmit Data Register Empty */
	if ((USART2->SR & 0x0080) && (USART2->CR1 & 0x0080)) {    	
		if(tx_buffer2.head == tx_buffer2.tail) {
			// Buffer empty, so disable interrupts							
			USART2->CR1 &= ~0x0080;
		}

		else {
			// There is more data in the output buffer. Send the next byte
			unsigned char c = tx_buffer2.buffer[tx_buffer2.tail];
			tx_buffer2.tail = (tx_buffer2.tail + 1) % UART_BUFFER_SIZE;
			USART2->DR = c;
		}
		return;		
		}
}


void UART4_IRQHandler(void) {
	/* if DR is not empty and the Rx Int is enabled */	
	if ((UART4->SR & 0x0020) && (UART4->CR1 & 0x0020)) {
		uint8_t data;
		data = UART4->DR;				
		store_char (data, _rx_buffer);  // store data in buffer
		main_buffer[main_buffer_index] = data;
		main_buffer_index = (main_buffer_index + 1) % MAIN_BUFFER_SIZE;			
    return;			
	}
			
	/*If interrupt is caused due to Transmit Data Register Empty */
	if ((UART4->SR & 0x0080) && (UART4->CR1 & 0x0080)){    	
		if (tx_buffer.head == tx_buffer.tail) {
			// Buffer empty, so disable interrupts							
			UART4->CR1 &= ~0x0080;					
    }

		else {
			// There is more data in the output buffer. Send the next byte
			unsigned char c = tx_buffer.buffer[tx_buffer.tail];
			tx_buffer.tail = (tx_buffer.tail + 1) % UART_BUFFER_SIZE;
			UART4->DR = c;
			}
		return;		
	}
}
