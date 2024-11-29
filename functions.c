#include "functions.h"
#include "stdio.h"
#include "stdlib.h"
#include "ADC.h"
#include "string.h"
#include "uart.h"

#define MAX_DUTY_CYCLES 40000 // Sets the maximum number of clock cycles (on + off) for a duty cycle
#define DUTY_CYCLES_SAFETY 300 // Sets the safety margins within which the duty cycle must operate
#define DUTY_CYCLES_INCREMENT_DEFAULT 200 // Default increment for duty_amount

#define CN_TIMEOUT_MAX 10 // Sets the number of times a button can be pressed and released before the button timer completes

#define PB3_HOLD_SECONDS 3

#define BUTTON1 (PORTAbits.RA2 == 0)
#define BUTTON2 (PORTBbits.RB4 == 0)
#define BUTTON3 (PORTAbits.RA4 == 0)

extern enum statey {
    WAIT,
    TIME_INPUT,
    COUNTDOWN_1,
    COUNTDOWN_2,
    PAUSE,
    FINISH,
};

//uint8_t received;
extern uint8_t received_char;
extern uint8_t RXFlag;

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

uint8_t do_PB3_count = 0;
uint16_t PB3_count = 0;

uint16_t counter_seconds = 0;

extern char micro_nums[6];
uint8_t do_reverse_parse = 0;

extern enum statey the_state;

void CNinit() {
    IPC4bits.CNIP = 6;
    IFS1bits.CNIF = 0;
    IEC1bits.CNIE = 1;
}

void IOinit() {
    TRISBbits.TRISB8 = 0;
    LATBbits.LATB8 = 0;
    
    TRISAbits.TRISA4 = 1;
    CNPU1bits.CN0PUE = 1;
    CNEN1bits.CN0IE = 1;
    
    TRISBbits.TRISB4 = 1;
    CNPU1bits.CN1PUE = 1;
    CNEN1bits.CN1IE = 1;
    
    TRISAbits.TRISA2 = 1;
    CNPU2bits.CN30PUE = 1;
    CNEN2bits.CN30IE = 1;
}

void Timerinit() {
    IEC0bits.T1IE = 1; // eabled timer 1 interrupt
    T1CONbits.TCKPS = 0b11;
    PR1 = 15625; // count for 1 s
    
    IEC0bits.T2IE = 1; // enable timer interrupt
    //T2CONbits.TCKPS = 0b01;
    PR2 = 2000; // count for 1 ms
    
    IEC0bits.T3IE = 1; // enable timer interrupt
    T3CONbits.TCKPS = 0b11;
    PR3 = 235; // count for 150 ms
}

void wait_until_T1() {
    // So long as timer 1 has not been triggered, go into Idle
    while (T1_triggered == 0) Idle();

    T1_triggered = 0;
    TMR1 = 0;
}

static inline void micro_nums_parse() {
    uint16_t minutes;
    uint16_t seconds;
    
    minutes = atoi(&micro_nums[0]);
    seconds = atoi(&micro_nums[3]);
    
    counter_seconds = minutes * 60 + seconds;
}

static inline void micro_nums_reverse_parse(uint16_t the_seconds) {
    uint16_t minutes = the_seconds / 60;
    uint16_t seconds = the_seconds % 60;
    
    sprintf(micro_nums, "%02d %02d", minutes, seconds);
}

static inline void get_new_duty() {
    if (duty_amount >= MAX_DUTY_CYCLES - DUTY_CYCLES_SAFETY) duty_direction = 0;
    else if (duty_amount <= DUTY_CYCLES_SAFETY) duty_direction = 1;
    
    if (duty_direction) duty_amount += duty_increment;
    else duty_amount -= duty_increment;
}

void primary_loop() {
    char input_str[40];
    input_str[39] = '\0';
    
    get_sample();
        
    if (do_reverse_parse) {
        micro_nums_reverse_parse(counter_seconds);
        do_reverse_parse = 0;
    }
        
    switch (the_state) {
        case WAIT:
            dynamic_duty_enable = 1;
            strcpy(input_str, "Press PB1 to input time            \r");
            break;
        case TIME_INPUT:
            dynamic_duty_enable = 0;
            duty_amount = 100;
            strcpy(input_str, "Give an input 00:00 (min:sec)  \r");
            input_str[14] = micro_nums[0]; input_str[15] = micro_nums[1];
            input_str[17] = micro_nums[3]; input_str[18] = micro_nums[4];
            break;
        case COUNTDOWN_1:
            // add code to dispaly current counter
            led_off = !led_off;
            duty_amount = 100 + ADC_val * 38 * led_off;
                
                
            micro_nums_reverse_parse(counter_seconds);
            //do_reverse_parse = 1;
                
                
            sprintf(input_str, "Here is the timer %02d:%02d      \r", counter_seconds / 60, counter_seconds % 60);
                
            if (counter_seconds <= 1) {
                the_state = FINISH;
                do_reverse_parse = 1;
            } else {
                counter_seconds -= 1;
            }
            
            if (do_PB3_count) PB3_count += 1;
            break;
        case COUNTDOWN_2:
            // add code to display adc reading
            led_off = !led_off;
            duty_amount = 100 + ADC_val * 38 * led_off;
                
            micro_nums_reverse_parse(counter_seconds);
            
            uint16_t duty_percent = ((uint32_t) duty_amount * 100) / MAX_DUTY_CYCLES;
            sprintf(input_str, "%02d%% %04d %02d:%02d               \r", duty_percent, ADC_val, counter_seconds / 60, counter_seconds % 60);
                
            if (counter_seconds <= 1) {
                the_state = FINISH;
                do_reverse_parse = 1;
            } else {
                counter_seconds -= 1;
            }
            
            if (do_PB3_count) PB3_count += 1;
            break;
        case PAUSE:
            // add code to display countdown time
            duty_amount = 100 + ADC_val * 38 * led_off;
            sprintf(input_str, "Paused at %02d:%02d            \r", counter_seconds / 60, counter_seconds % 60);
            break;
        case FINISH:
            strcpy(input_str, "Countdown done!            \r");
            duty_amount = 100 + ADC_val * 38;
                
            counter_seconds += 1;
            if (counter_seconds == 5) {
                the_state = WAIT;
                counter_seconds = 0;
            }
            break;
    }
            
    Disp2String(input_str);
        
    wait_until_T1();
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
    
    if (PB_current == 0b000) {
        if (the_state == WAIT) {
            if (PB_last == 0b001) the_state = TIME_INPUT;
        } else if (the_state == TIME_INPUT) {
            micro_nums_parse();
            if (PB_last == 0b100) {
                the_state = COUNTDOWN_1;
            } else if (PB_last == 0b010) {
                counter_seconds = 0;
                do_reverse_parse = 1;
            }
        } else if (the_state == COUNTDOWN_1) {
            if (PB_last == 0b100) {
                if (PB3_count >= PB3_HOLD_SECONDS) {
                    PB3_count = 0;
                    counter_seconds = 0;
                    the_state = TIME_INPUT;
                    do_reverse_parse = 1;
                } else the_state = PAUSE;
            }
        } else if (the_state == COUNTDOWN_2) {
            if (PB_last == 0b100) {
                if (PB3_count >= PB3_HOLD_SECONDS) {
                    PB3_count = 0;
                    counter_seconds = 0;
                    the_state = TIME_INPUT;
                    do_reverse_parse = 1;
                } else the_state = PAUSE;
            }
        } else if (the_state == PAUSE) {
            if (PB_last == 0b100) the_state = COUNTDOWN_1;
        }
    }
    
    if (PB_current == 0b100 && (the_state == COUNTDOWN_1 || the_state == COUNTDOWN_2)) {
        do_PB3_count = 1;
    } else {
        do_PB3_count = 0;
    }
    
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