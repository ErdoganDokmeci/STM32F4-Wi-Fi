/*
 * UartRingbuffer.c
 *
 *  Created on: 10-Jul-2019
 *      Author: Controllerstech
 *
 *  Modified on: 11-April-2020
 */

// Modified by Erdogan Dokmeci, 17 Jan 2020

#include "UartRingbuffer.h"
#include <string.h>

uint16_t timeout;

ring_buffer rx_buffer = { { 0 }, 0, 0};			// UART4
ring_buffer tx_buffer = { { 0 }, 0, 0};			// UART4
ring_buffer rx_buffer2 = { { 0 }, 0, 0};		// USART2
ring_buffer tx_buffer2 = { { 0 }, 0, 0};		// USART2

ring_buffer *_rx_buffer;
ring_buffer *_tx_buffer;
ring_buffer *_rx_buffer2;
ring_buffer *_tx_buffer2;


void Ringbuf_init_Usart2(void) {
		_rx_buffer2 = &rx_buffer2;
		_tx_buffer2 = &tx_buffer2;
		USART2->CR1 |= 0x0020;		// Enable the USART Data Register not empty Interrupt
		USART2->CR3 |= 0x0001;		// Enable the USART Error Interrupt: (Frame error, noise error, overrun error)
}

void Ringbuf_init_Uart4(void) {
		_rx_buffer = &rx_buffer;
		_tx_buffer = &tx_buffer;
		UART4->CR1 |= 0x0020;		// Enable the USART Data Register not empty Interrupt
		UART4->CR3 |= 0x0001;		// Enable the USART Error Interrupt: (Frame error, noise error, overrun error)
}

void store_char(unsigned char c, ring_buffer *buffer) {
		uint16_t i = (uint16_t)(buffer->head + 1) % UART_BUFFER_SIZE;
		// if we should be storing the received character into the location
		// just before the tail (meaning that the head would advance to the
		// current location of the tail), we're about to overflow the buffer
		// and so we don't write the character or advance the head.
		if (i != buffer->tail) {
				buffer->buffer[buffer->head] = c;
				buffer->head = i;
		}
}


int Uart_read(void) {
		// if the head isn't ahead of the tail, we don't have any characters
		if (_rx_buffer->head == _rx_buffer->tail) {
				return -1;
		}
		else {
				unsigned char c = _rx_buffer->buffer[_rx_buffer->tail];
				_rx_buffer->tail = (uint16_t)(_rx_buffer->tail + 1) % UART_BUFFER_SIZE;
				return c;
		}
}


int Uart_read_Usart2(void) {
		// if the head isn't ahead of the tail, we don't have any characters
		if(_rx_buffer2->head == _rx_buffer2->tail) {
				return -1;
		}
		else {
				unsigned char c = _rx_buffer2->buffer[_rx_buffer2->tail];
				_rx_buffer2->tail = (uint16_t)(_rx_buffer2->tail + 1) % UART_BUFFER_SIZE;
				return c;
		}
}


void Uart_write(int c) {
		if (c >= 0) {
				uint16_t i = (_tx_buffer->head + 1) % UART_BUFFER_SIZE;
				while (i == _tx_buffer->tail);

				_tx_buffer->buffer[_tx_buffer->head] = (uint8_t)c;
				_tx_buffer->head = i;			
				UART4->CR1 |= 0x0080;			// Enable UART transmission interrupt
		}
}


void Uart_write_Usart2(int c) {
		if (c >= 0) {
				uint16_t i = (_tx_buffer2->head + 1) % UART_BUFFER_SIZE;
				while (i == _tx_buffer2->tail);

				_tx_buffer2->buffer[_tx_buffer2->head] = (uint8_t)c;
				_tx_buffer2->head = i;			
				USART2->CR1 |= 0x0080;			// Enable UART transmission interrupt
		}
}



/* checks if the new data is available in the incoming buffer */
int IsDataAvailable(void) {
		return (uint16_t)(UART_BUFFER_SIZE + _rx_buffer->head - _rx_buffer->tail) % UART_BUFFER_SIZE;
}

/* sends the string to the uart */
void Uart_sendstring(USART_TypeDef *huart, const char *s) {
		//while(*s) Uart_write(*s++);
		if (huart == UART4)
				while(*s) Uart_write(*s++);
		else
				while(*s) Uart_write_Usart2(*s++);
}

void GetDataFromBuffer(char *start_string, char *end_string, char *buffer_to_copy_from, char *buffer_to_copy_into) {
		uint16_t start_string_length = strlen(start_string);
		uint16_t end_string_length   = strlen(end_string);
		uint16_t so_far = 0;
		uint16_t index = 0;
		uint16_t start_position = 0;
		uint16_t end_position   = 0;

repeat1:
		while (start_string[so_far] != buffer_to_copy_from[index]) 
				index++;
		if (start_string[so_far] == buffer_to_copy_from[index]) {
				while (start_string[so_far] == buffer_to_copy_from[index]) {
						so_far++;
						index++;
				}
		}

		if (so_far == start_string_length) 
				start_position = index;
		else {
				so_far = 0;
				goto repeat1;
		}

		so_far = 0;

repeat2:
		while (end_string[so_far] != buffer_to_copy_from[index]) 
				index++;
		if (end_string[so_far] == buffer_to_copy_from[index]) {
				while (end_string[so_far] == buffer_to_copy_from[index]) {
						so_far++;
						index++;
				}
		}

		if (so_far == end_string_length)
				end_position = index - end_string_length;
		else {
				so_far = 0;
				goto repeat2;
		}

		so_far = 0;
		index = 0;

		for (int i = start_position; i < end_position; i++) {
				buffer_to_copy_into[index] = buffer_to_copy_from[i];
				index++;
		}
}

void Uart_flush(void) {
		memset(_rx_buffer->buffer, '\0', UART_BUFFER_SIZE);
		_rx_buffer->head = 0;
		_rx_buffer->tail = 0;
}

void Uart_flush_USART2(void) {
		memset(_rx_buffer2->buffer, '\0', UART_BUFFER_SIZE);
		_rx_buffer2->head = 0;
		_rx_buffer2->tail = 0;
}


int Uart_peek(void) {
		if(_rx_buffer->head == _rx_buffer->tail)
				return -1;		
		else 
				return _rx_buffer->buffer[_rx_buffer->tail];		
}

/* copies the data from the incoming buffer into our buffer
 * Must be used if you are sure that the data is being received
 * it will copy irrespective of, if the end string is there or not
 * if the end string gets copied, it returns 1 or else 0
 * Use it either after (IsDataAvailable) or after (Wait_for) functions
 */
int Copy_upto(char *string, char *buffer_to_copy_into) {
		uint16_t so_far =0;
		uint16_t len = strlen (string);
		uint16_t index = 0;

again:
		while (Uart_peek() != string[so_far]) {
				buffer_to_copy_into[index] = _rx_buffer->buffer[_rx_buffer->tail];
				_rx_buffer->tail = (uint16_t)(_rx_buffer->tail + 1) % UART_BUFFER_SIZE;
				index++;
				while (!IsDataAvailable());
		}
		while (Uart_peek() == string [so_far]) {
				so_far++;
				buffer_to_copy_into[index++] = Uart_read();
				if (so_far == len) 
						return 1;
				timeout = TIMEOUT_DEF;
				while ((!IsDataAvailable())&&timeout);
				if (timeout == 0) 
						return 0;
		}

		if (so_far != len) {
				so_far = 0;
				goto again;
		}

		if (so_far == len) 
				return 1;
		else 
				return 0;
}

/* must be used after wait_for function
 * get the entered number of characters after the entered string
 */
int Get_after(char *string, uint8_t number_of_chars, char *buffer_to_save) {
		for (uint16_t index = 0; index < number_of_chars; index++) {
				timeout = TIMEOUT_DEF;
				while ((!IsDataAvailable())&&timeout);  // wait until some data is available
				if (timeout == 0) 
						return 0;  													// if data isn't available within time, then return 0
				buffer_to_save[index] = Uart_read();  	// save the data into the buffer... increments the tail
		}
		return 1;
}

/* Waits for a particular string to arrive in the incoming buffer... It also increments the tail
 * returns 1, if the string is detected
 */
// added timeout feature so the function won't block the processing of the other functions
int Wait_for(char *string) {
		uint16_t so_far = 0;
		uint16_t len = strlen(string);

again:
		timeout = TIMEOUT_DEF;
		while ((!IsDataAvailable())&&timeout);  // let's wait for the data to show up
		if (timeout == 0) 
				return 0;
		while (Uart_peek() != string[so_far])  	// peek in the rx_buffer to see if we get the string
				{
				if (_rx_buffer->tail != _rx_buffer->head) {
						_rx_buffer->tail = (uint16_t)(_rx_buffer->tail + 1) % UART_BUFFER_SIZE;  // increment the tail
				}

				else {
						return 0;
				}
		}
		while (Uart_peek() == string [so_far]) // if we got the first letter of the string
				{
				// now we will peek for the other letters too
				so_far++;
				_rx_buffer->tail = (uint16_t)(_rx_buffer->tail + 1) % UART_BUFFER_SIZE;  // increment the tail
				if (so_far == len) 
						return 1;
				timeout = TIMEOUT_DEF;
				while ((!IsDataAvailable())&&timeout);
				if (timeout == 0) 
						return 0;
		}

		if (so_far != len) {
				so_far = 0;
				goto again;
		}

		if (so_far == len) 
				return 1;
		else 
				return 0;
}
