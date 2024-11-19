#include "functions.h"

extern uint8_t T1_triggered;

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