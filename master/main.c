/*
 * main.c
 *
 * Created: 5/14/2023 5:39:00 PM
 *  Author: ilmari
 */ 
/*
THIS IS THE MASTER ARDUINO, communicating through RX and TX pins
*/
// Define CPU frequency and baud rate
#define F_CPU 16000000UL
#define BAUD 9600
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)

#include <avr/io.h>
#include <util/delay.h>
#include "lcd.h" // lcd header file made by Peter Fleury, edited
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Define states
typedef enum {
	ARMED,
	DISARMED,
	MOVEMENT_DETECTED,
	CORRECT_PASSWORD,
	FAULT,
	ALARM
} State;


// LCD pins
int LCD_RS = 9;
int LCD_RW = 10;
int LCD_EN = 11;

// LCD data pins
int LCD_D4 = 4;
int LCD_D5 = 5;
int LCD_D6 = 6;
int LCD_D7 = 7;

// Led pin 12, so pin PB4
int LED_PIN = PB4;

// Alarm pin 3, so pin PD3
int ALARM_PIN = PD3;

// Define the password
const char* PASSWORD = "1245";

int transmit(char data, FILE* stream)
{
    /*
    This function is used implicitly by printf() and scanf() to redirect i/o to the UART
    */
    while (!( UCSR0A & (1<<UDRE0)));
    UDR0 = data;
    return 0;
}

int receive(FILE *stream)
{
    /*
    This function is used implicitly by printf() and scanf() to redirect i/o to the UART
    */
    // Wait for data to be received
    while (!(UCSR0A & (1 << RXC0)));
    // Blink led
    PORTB |= (1 << LED_PIN);
    _delay_ms(10);
    // Turn led off
    PORTB &= ~(1 << LED_PIN);
    return UDR0;
}

void greeting()
{
    /*
    This function is used to greet the user and to check that the led, LCD, and alarm are working
    */
    // clear display
    lcd_clrscr();
    // Check LCD
    lcd_puts("Hello World!");
    // Check led and alarm
    PORTB |= (1 << LED_PIN);
    PORTD |= (1 << ALARM_PIN);
    _delay_ms(2000);
    PORTB &= ~(1 << LED_PIN);
    PORTD &= ~(1 << ALARM_PIN);
    lcd_clrscr();
}

void setup(){
    /*
    This function is called at the beginning of the program to set up the system

    This sets up:
    - UART, by setting the baud rate and setting the frame format
    - LCD
    - Led and alarm pins
    */
    // Set baud rate
    FILE *uart = fdevopen(transmit, receive);
    stdin = uart;
    stdout = uart;
    // Prepare comms
    // Set baud rate
    UBRR0H = (BAUDRATE >> 8);
    UBRR0L = BAUDRATE;
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
    // Set frame format: 8data, 1stop bit
    UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);
    // Set led and alarm pins as output
    DDRB |= (1 << LED_PIN);
    DDRD |= (1 << ALARM_PIN);
    // Initialize LCD
    lcd_init(LCD_DISP_ON);
    // Clear display
    lcd_clrscr();
    // Print string
    greeting();
}

State on_armed(State state){
    /*
    This function is called when the system is in ARMED state.
    In this state, the system listens to the slave device with scanf().
    If the slave device sends a string with "movement" in it, then the state is changed to MOVEMENT_DETECTED
    If the slave sends anything else, then the state remains ARMED, and this function is called again.
    */
    char buffer[100];
    lcd_clrscr();
    lcd_puts("System online");
    // Clear buffer
    memset(buffer, 0, sizeof(buffer));
    scanf("%s", buffer);
    // For debugging
    printf("Received: %s\n", buffer);
    if (strstr(buffer, "movement") != NULL){
        state = MOVEMENT_DETECTED;
    }
    return state;
}

State on_movement(State state){
    /*
    This function is called when the system is in MOVEMENT_DETECTED state.
    In this state, the system listens to the slave device with scanf().
    If the slave device sends a string with PASSWORD in it, then the state is changed to CORRECT_PASSWORD.
    If something else is sent from the slave, then the state is changed to ALARM.
    */
    char buffer[100];
    lcd_clrscr();
    lcd_puts("Movement detected");
    // Clear buffer
    memset(buffer, 0, sizeof(buffer));
    scanf("%s", buffer);
    printf("Received password: %s\n", buffer);
	lcd_clrscr();
	lcd_puts(buffer);
	_delay_ms(3000);
    // Check if the substring PASSWORD is in the buffer
    if(strstr(buffer,PASSWORD) != NULL){
        state = CORRECT_PASSWORD;
    }
    // Check if the substring 'timeout' is in the buffer
    else if (strstr(buffer,"timeout") != NULL){
        printf("No password given in time!\n");
        state = ALARM;
    }
    else{
        state = ALARM;
    }
    return state;
}

State on_correct_password(State state){
    /*
    This function is called when the system is in CORRECT_PASSWORD state.
    Here, the master just changes the state to DISARMED, and puts a message on the LCD.
    */
    lcd_clrscr();
    lcd_puts("Correct password!");
    _delay_ms(1000);
    state = DISARMED;
    return state;
}

State on_disarmed(State state){
    /*
    This function is called when the system is in DISARMED state.
    Here, the master listens to the slave device with scanf().
    If the slave device sends a string with "rearm" in it, then the state is changed to ARMED.
    In all other cases, the state remains DISARMED, and this function is called again.
    */
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    lcd_clrscr();
    // Disable alarm
    PORTD &= ~(1 << ALARM_PIN);
    lcd_puts("System disarmed");
    _delay_ms(1000);
    scanf("%s", buffer);
    if (strstr(buffer,"movement") != NULL || strstr(buffer,"timeout") != NULL){
        return state;
    }
    else if(strstr(buffer,"rearm") != NULL){
        state = ARMED;
        printf("Arming\n");
    }
    return state;
}

State on_alarm(State state){
    /* 
        This function is called when the system is in ALARM state.
        This is a final state, and the system will stay in this state until the user resets it.
        In this state, the system sounds the alarm and varies the pulse sent to the alarm with PWM through manipulation.
    */
    lcd_clrscr();
    lcd_puts("ALARM");
    // Enable alarm
    PORTD |= (1 << ALARM_PIN);
    // Set up PWM in pin2, reg OCR2B
    TCCR2A |= (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
    TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);
    OCR2B = 0;
    // Vary the pulse width
    int i = 100;
    int direction = 1;
    while (1){
        OCR2B = i;
        i += direction;
        if (i == 255){
            direction = -1;
        }
        else if (i == 100){
            direction = 1;
        }
        _delay_ms(1);
    }
 return state;
}

int main(){
    setup();
    // Create a state variable, so that it can be changed in functions if STATE is passed to the function

    State state = ARMED;

	while (1) {
		switch (state) {
			case ARMED:
			state = on_armed(state);
			break;

			case MOVEMENT_DETECTED:
			state = on_movement(state);
			break;

			case CORRECT_PASSWORD:
			state = on_correct_password(state);
			break;

			case ALARM:
			state = on_alarm(state);
			break;

			case DISARMED:
			state = on_disarmed(state);
			break;

			case FAULT:
			default:
			lcd_clrscr();
			lcd_puts("FAULT");
			break;
            
		    }
		    _delay_ms(100);
		    lcd_clrscr();
	    }
	    return 0;
}
