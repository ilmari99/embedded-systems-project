/*

THIS IS THE SLAVE ARDUINO
*/

#define F_CPU 16000000UL
#define BAUD 9600
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h> // for itoa function
#include <stdio.h> // for printf function
#include <time.h> // for time function
#include <string.h> // for strcmp function
#include <avr/interrupt.h>
#include "keypad.h"
// Keypad (6 pins) is connected to PORTB
#define RowColDirection DDRC
#define ROW PORTC // Row will be an input
#define COL PINC // Column will be an output

// Movement in from 2, which is PD2
// led out to 4, which is PD4
int MOVEMENT_PIN_IN = PD2;
int LED_PIN = PD4;

int REARM_BTN = PD3;

int transmit(char data, FILE* stream)
{
    /*
    This function is used implicitly by printf() and scanf() to redirect i/o to the UART.
    Also flashes the led for 10ms
    */
	while (!( UCSR0A & (1<<UDRE0)));
	UDR0 = data;
	// Blink led
	PORTD |= (1 << LED_PIN);
	_delay_ms(10);
	PORTD &= ~(1 << LED_PIN);
	return 0;
}

int receive(FILE *stream)
{
	/*
    This function is used implicitly by printf() and scanf() to redirect i/o to the UART
    */
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

void greeting()
{
	/*
    Ligth up led to check its working
    */
	PORTD |= (1 << LED_PIN);
	_delay_ms(2000);
	// Turn off led
	PORTD &= ~(1 << LED_PIN);
}

void setup(){
    /*
    Setup the UART and keypad
    */
	FILE *uart = fdevopen(transmit, receive);
	stdout = stdin = uart;
	// Set baud rate
	UBRR0H = (BAUDRATE >> 8);
	UBRR0L = BAUDRATE;
	// Enable receiver and transmitter
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	// Set frame format: 8data, 1stop bit
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

	KEYPAD_Init();
	// Call greeting function
	greeting();
}

const char* get_password(){
    /*
    Returns the password inputted by the user.
	Due to the lack of pins (using 2 UNOs), and hence wokring buttons,
	Input of 7 means backspace and 8 means submit.

    Also, if the user takes more than 5 seconds to input the first or any character,
    the function returns "timeout".
    */
	int i = 0;
	int has_timed_out = 0;
	char* password = (char*)calloc(20,sizeof(char));
	while (i < 20){
		// Wait for 10 seconds for user to press a button
        // The library was modified, to take in a pointer to a variable,
        // which is set to 1 if a timeout occurs while getting a key  
		char c = KEYPAD_GetKey(&has_timed_out);
		if (has_timed_out == 1){
			strcpy(password, "timeout");
			return password;
		}
		// If character is 8, break
		if (atoi(&c) == 8){
			break;
		}
		// If character is 7, delete last character
		if (atoi(&c) == 7){
			if (i == 0){
				continue;
			}
			// Delete last character
			password[i-1] = '\0';
			i--;
			continue;
		}
		// Add character to password
		password[i] = c;
		i++;
	}
	if (i == 0){
		// If no character was inputted, return "timeout"
		strcpy(password, "no input");
	}
	else{
		// Add null character to end of password
		password[i] = '\0';
	}
	return password;
}


void on_movement(){
	/* If the movement sensor is triggered, this function is called.
    This function first sends 'movement' through the RX pin, and then
    calls get_password() to get the password from the user using the keypad and sends it.
    */
	printf("movement\n");
	// Get the user inputted password character by character
	const char* password = get_password();
	// Transmit the password through RX pin
	printf("%s\n",password);
	//free(&password);
}


int main(){
	setup();
	// Set movement pin as input
	DDRD &= ~(1 << MOVEMENT_PIN_IN);
	// Set led pin as output
	DDRD |= (1 << LED_PIN);
	// Set led pin low
	PORTD &= ~(1 << LED_PIN);
	// Set rearm button as input
	DDRD &= ~(1 << REARM_BTN);
	// Loop forever
	while(1){
		// If movement is detected
		if (PIND & (1 << MOVEMENT_PIN_IN)){
			// Call on_movement function
			on_movement();
		}
		else if(PIND & (1 << REARM_BTN)){
			// Rearm the alarm
			printf("rearm\n");
		}
	}
	
	return 0;
}