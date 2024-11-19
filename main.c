/*
 * File:   main.c
 * Author: Jarred Eisbrenner, Angad Minhas, Sam Schroeder
 *
 * Created on: 11 Nov, 2024
 */

// FBS
#pragma config BWRP = OFF               // Table Write Protect Boot (Boot segment may be written)
#pragma config BSS = OFF                // Boot segment Protect (No boot program Flash segment)

// FGS
#pragma config GWRP = OFF               // General Segment Code Flash Write Protection bit (General segment may be written)
#pragma config GCP = OFF                // General Segment Code Flash Code Protection bit (No protection)

// FOSCSEL
#pragma config FNOSC = FRC              // Oscillator Select (Fast RC oscillator (FRC))
#pragma config IESO = OFF               // Internal External Switch Over bit (Internal External Switchover mode disabled (Two-Speed Start-up disabled))

// FOSC
#pragma config POSCMOD = NONE           // Primary Oscillator Configuration bits (Primary oscillator disabled)
#pragma config OSCIOFNC = ON            // CLKO Enable Configuration bit (CLKO output disabled; pin functions as port I/O)
#pragma config POSCFREQ = HS            // Primary Oscillator Frequency Range Configuration bits (Primary oscillator/external clock input frequency greater than 8 MHz)
#pragma config SOSCSEL = SOSCHP         // SOSC Power Selection Configuration bits (Secondary oscillator configured for high-power operation)
#pragma config FCKSM = CSECMD           // Clock Switching and Monitor Selection (Clock switching is enabled, Fail-Safe Clock Monitor is disabled)

// FWDT
#pragma config WDTPS = PS32768          // Watchdog Timer Postscale Select bits (1:32,768)
#pragma config FWPSA = PR128            // WDT Prescaler (WDT prescaler ratio of 1:128)
#pragma config WINDIS = OFF             // Windowed Watchdog Timer Disable bit (Standard WDT selected; windowed WDT disabled)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable bit (WDT disabled (control is placed on the SWDTEN bit))

// FPOR
#pragma config BOREN = BOR3             // Brown-out Reset Enable bits (Brown-out Reset enabled in hardware; SBOREN bit disabled)
#pragma config PWRTEN = ON              // Power-up Timer Enable bit (PWRT enabled)
#pragma config I2C1SEL = PRI            // Alternate I2C1 Pin Mapping bit (Default location for SCL1/SDA1 pins)
#pragma config BORV = V18               // Brown-out Reset Voltage bits (Brown-out Reset set to lowest voltage (1.8V))
#pragma config MCLRE = ON               // MCLR Pin Enable bit (MCLR pin enabled; RA5 input pin disabled)

// FICD
#pragma config ICS = PGx2               // ICD Pin Placement Select bits (PGC2/PGD2 are used for programming and debugging the device)

// FDS
#pragma config DSWDTPS = DSWDTPSF       // Deep Sleep Watchdog Timer Postscale Select bits (1:2,147,483,648 (25.7 Days))
#pragma config DSWDTOSC = LPRC          // DSWDT Reference Clock Select bit (DSWDT uses LPRC as reference clock)
#pragma config RTCOSC = SOSC            // RTCC Reference Clock Select bit (RTCC uses SOSC as reference clock)
#pragma config DSBOREN = ON             // Deep Sleep Zero-Power BOR Enable bit (Deep Sleep BOR enabled in Deep Sleep)
#pragma config DSWDTEN = ON             // Deep Sleep Watchdog Timer Enable bit (DSWDT enabled)

// #pragma config statements should precede project file includes.

#include <xc.h>
#include <p24F16KA101.h>

#include "clkChange.h"
#include "uart.h"
#include "functions.h"
#include "stdio.h"
#include "ADC.h"
#include "string.h"

#define CN_TIMEOUT_MAX 10 // Sets the number of times a button can be pressed and released before the button timer completes
#define MAX_DUTY_CYCLES 40000 // Sets the maximum number of clock cycles (on + off) for a duty cycle
#define DUTY_CYCLES_SAFETY 300 // Sets the safety margins within which the duty cycle must operate
#define DUTY_CYCLES_INCREMENT_DEFAULT 200 // Default increment for duty_amount

#define BUTTON1 (PORTAbits.RA2 == 0)
#define BUTTON2 (PORTBbits.RB4 == 0)
#define BUTTON3 (PORTAbits.RA4 == 0)

enum statey {
    WAIT,
    TIME_INPUT,
    COUNTDOWN_1,
    COUNTDOWN_2,
    PAUSE,
    FINISH,
};

uint8_t received;
uint8_t received_char = '0';
uint8_t RXFlag = 0;

uint16_t ADC_val = 0; // No glitches will occur from not implementing send/receive for ADC_val

// send_PB_ variables can be used freely in the CN and "CN timer" interrupts
uint8_t PB_last = 0b000; // Source of PB_last
uint8_t PB_achieved = 0b000;
uint8_t PB_current = 0b000; // Source of PB_current

uint16_t CN_timeout_count = 0;

uint8_t T1_triggered = 0;

int8_t duty_direction = 1; // 1 for increasing duty cycle, 0 for decreasing duty cycle
uint8_t cycle_half = 1; // 1 for HIGH part of cycle, 0 for LOW part of cycle
uint16_t duty_amount = MAX_DUTY_CYCLES / 2;
uint16_t duty_increment = DUTY_CYCLES_INCREMENT_DEFAULT;
uint8_t dynamic_duty_enable = 1;

uint16_t led_off = 0;

char micro_nums[4] = "0000";

enum statey the_state = WAIT;

static inline void get_new_duty() {
    if (duty_amount >= MAX_DUTY_CYCLES - DUTY_CYCLES_SAFETY) duty_direction = 0;
    else if (duty_amount <= DUTY_CYCLES_SAFETY) duty_direction = 1;
    
    if (duty_direction) duty_amount += duty_increment;
    else duty_amount -= duty_increment;
}

int main(void) {
    newClk(8);
    
    AD1PCFG = 0b1111111111011111;
    
    InitUART2();
    CNinit();
    IOinit();
    Timerinit();
    ADCinit();
    
    T1CONbits.TON = 1;
    T2CONbits.TON = 1;
    while(1) {
        get_sample();
        
        switch (the_state) {
            case WAIT:
                if (PB_achieved == 0b001) the_state = TIME_INPUT;
                dynamic_duty_enable = 1;
                Disp2String("Press PB1 to input time\r");
                break;
            case TIME_INPUT:
                dynamic_duty_enable = 0;
                duty_amount = 100;
                char input_str[] = "Give an input 00:00 (min:sec)\r\0";
                input_str[14] = micro_nums[0]; input_str[15] = micro_nums[1];
                input_str[17] = micro_nums[2]; input_str[18] = micro_nums[3];
                Disp2String(input_str);
                // add code to accept user input
                break;
            case COUNTDOWN_1:
                // add code to dispaly current counter
                led_off = !led_off;
                duty_amount = 100 + ADC_val * 38 * led_off;
                break;
            case COUNTDOWN_2:
                // add code to display adc reading
                led_off = !led_off;
                duty_amount = 100 + ADC_val * 38 * led_off;
                break;
            case PAUSE:
                // add code to display countdown time
                duty_amount = 100 + ADC_val * 38 * led_off;
                break;
            case FINISH:
                Disp2String("Countdown done!\r");
                duty_amount = 100 + ADC_val * 38;
                break;
        }
        
        wait_until_T1();
    }
    
    return 0;
}

// This is the "tick timer" responsible for setting the overall loop time
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void){    
    IFS0bits.T1IF = 0; // Clear interrupt flag
    
    T1_triggered = 1;
}

// This is the "duty timer" responsible for generating the PWM signal
void __attribute__((interrupt, no_auto_psv)) _T2Interrupt(void){    
    IFS0bits.T2IF = 0; // Clear interrupt flag
    
    // Flip from counting led on-time to off-time, or vice versa
    if (cycle_half) {
        PR2 = duty_amount;
        LATBbits.LATB8 = 1;
        cycle_half = 0;
    } else {
        PR2 = MAX_DUTY_CYCLES - duty_amount;
        LATBbits.LATB8 = 0;
        cycle_half = 1;
    }
    //PR2 = cycle_half ? duty_amount : MAX_DUTY_CYCLES - duty_amount;
    //cycle_half = !cycle_half;
    
    if (dynamic_duty_enable) get_new_duty();
}

// This is the "CN timer" responsible for registering the effects of a CN and
// restoring the timeout counter 
void __attribute__((interrupt, no_auto_psv)) _T3Interrupt(void){
    IFS0bits.T3IF = 0; // Clear interrupt flag
    
    PB_achieved = PB_last;
    
    PB_last = 0b000; // Resets the last button state
    //send_PB_current = 0b000; // Resets the current button state
    
    CN_timeout_count = 0; // Reset CN timeout counter
    IEC1bits.CNIE = 1; // Enable CN interrupts, which may have been disabled due to the timeout counter in the _CNInterrupt
    
    T3CONbits.TON = 0; // Turns this timer off
}


void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void){
    IFS1bits.CNIF = 0; // Clear interrupt flag
    
    CN_timeout_count += 1;
    if (CN_timeout_count >= CN_TIMEOUT_MAX) IEC1bits.CNIE = 0; // Disable CN interrupts if the limit has been exceeded
    
    // PB_last = PB_current;
    PB_last |= PB_current; // accumulates the buttons that have been pressed
    PB_current = ((BUTTON1) | (BUTTON2 << 1) | (BUTTON3 << 2));
   
    // Starts timer 3 if it is not yet started
    if (T3CONbits.TON == 0) {
        TMR3 = 0;
        T3CONbits.TON = 1;
    }
}