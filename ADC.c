#include "ADC.h"

extern uint16_t ADC_val;

/*uint16_t do_ADC() {
    // Declare variable to store ADC reading
    uint16_t ADC_value;

    AD1CON1bits.FORM = 0b00;
    AD1CON1bits.SSRC = 0b111;
    AD1CON2bits.VCFG = 0b000;
    AD1CON3bits.ADRC = 0;
    AD1CHSbits.CH0SA = 0b0101;
    AD1CHSbits.CH0NA = 0;
    AD1CON2bits.ALTS = 0;
    AD1CON3bits.SAMC = 0b11111;
    
    AD1CON1bits.ADON = 1; // Turn ADC on
    AD1CON1bits.SAMP = 1; // Turn sampling on
    
    // Wait until sampling is done
    while (AD1CON1bits.DONE == 0);
    // Store reading from ADC
    ADC_value = ADC1BUF0;

    AD1CON1bits.SAMP = 0; // Turn sampling off
    AD1CON1bits.ADON = 0; // Turn ADC off
    
    return ADC_value;
}*/

void ADCinit() {
    AD1CON1bits.FORM = 0b00;
    AD1CON1bits.SSRC = 0b111;
    AD1CON1bits.ASAM = 0b0;
    AD1CON2bits.VCFG = 0b000;
    AD1CON3bits.ADRC = 0;
    AD1CHSbits.CH0SA = 0b0101;
    AD1CHSbits.CH0NA = 0;
    AD1CON2bits.ALTS = 0;
    AD1CON3bits.SAMC = 0b11111;
}

void get_sample() {
    AD1CON1bits.ADON = 1; // Turn ADC on
    AD1CON1bits.SAMP = 1; // Turn sampling on
    
    // Wait until sampling is done
    while (AD1CON1bits.DONE == 0);
    // Store reading from ADC
    ADC_val = ADC1BUF0;

    AD1CON1bits.SAMP = 0; // Turn ADC off
    AD1CON1bits.ADON = 0; // Turn sampling off
}
