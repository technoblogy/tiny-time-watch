/* Tiny Time Watch
   ATtiny85 @ 1 MHz (internal oscillator; BOD disabled)

   David Johnson-Davies - www.technoblogy.com - 3rd December 2016
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <avr/sleep.h>
#include <avr/power.h>

const int now = 1547;                     // To set the time; eg 15:47
const int Tickspersec = 250;              // Ticks per second

volatile byte Ticks = 0;
volatile unsigned long Secs = (unsigned long)((now/100) * 60 + (now%100)) * 60;
volatile int Timeout;
volatile byte Cycle = 0;
volatile boolean DisplayOn = false;
volatile int Step;                        // For setting time

volatile int Hours = 0;                   // From 0 to 11
volatile int Fivemins = 0;                // From 0 to 11

// Pin assignments

int Pins[5][5] = {{-1, 10, -1,  2,  0 },
                  {11, -1, -1,  3,  7 },
                  {-1, -1, -1, -1, -1 },
                  { 1,  9, -1, -1,  5 },
                  { 6,  8, -1,  4, -1 } };

// Display multiplexer **********************************************

void DisplayNextRow() {
  Cycle++;
  byte row = Cycle & 0x03;
  if (row > 1) row++;    // Skip PB2
  byte bits = 0;
  for (int i=0; i<5; i++) {
    if (Hours == Pins[row][i]) bits = bits | 1<<i;
    if ((Cycle & 0x20) && (Fivemins == Pins[row][i])) bits = bits | 1<<i;
  }
  DDRB = 1<<row | bits;
  PORTB = bits | 0x04;         // Keep PB2 high
}

// Timer/Counter0 interrupt - multiplexes display and counts ticks
ISR(TIM0_COMPA_vect) {
  Ticks++;
  if (Ticks == Tickspersec) {Ticks = 0; Secs++; }
  if (!DisplayOn) return;
  DisplayNextRow();
  Timeout--;
  if (Timeout != 0) return;
  if (PINB & 1<<PINB2) {
  // If button is now up, turn off display
    DDRB = 0;                     // Blank display - all inputs
    PORTB = 0xFF;                 // All pullups on
    DisplayOn = false;
  } else {
  // If button is still down, set time
    Timeout = Tickspersec/2;        // Half second delay
    Secs = (unsigned long)(Secs + 300);
    Fivemins = (unsigned long)((Secs+299)/300)%12;
    Hours = (unsigned long)((Secs+1799)/3600)%12;
  }
}

// Push button **********************************************
  
// Push button interrupt
ISR(INT0_vect) {
  // Turn on display
  Timeout = Tickspersec*4;              // Display for 4 secs
  DisplayOn = true;
  Fivemins = (unsigned long)((Secs+299)/300)%12;
  Hours = (unsigned long)((Secs+1799)/3600)%12;
  Step = 0;
}

// Setup **********************************************

void setup() {
  // Wait for 5 secs before reducing clock speed
  delay(5000);

  // Slow down clock by factor of 256 to save power
  CLKPR = 1<<CLKPCE;
  CLKPR = 4<<CLKPS0;            // Clock to .5 MHz

  // Set up button
  PORTB = 0xFF;                 // All pullups on
  MCUCR = MCUCR | 2<<ISC00;     // Interrupt on falling edge
  GIMSK = 1<<INT0;              // Enable INT0 interrupt

  // Set up Timer/Counter0 to multiplex the display
  TCCR0A = 2<<WGM00;            // CTC mode; count up to OCR0A
  TCCR0B = 0<<WGM02 | 2<<CS00;  // Divide by 8 = 62500Hz
  OCR0A = 240;                  // Divide by 250 -> 250Hz
  TIMSK = TIMSK | 1<<OCIE0A;    // Enable compare match interrupt
   
  // Disable what we don't need to save power
  ADCSRA &= ~(1<<ADEN);         // Disable ADC
  PRR = 1<<PRUSI | 1<<PRADC | 1<<PRTIM1;  // Turn off clocks
  set_sleep_mode(SLEEP_MODE_IDLE);
}

void loop() {
  sleep_enable();
  sleep_cpu();                  // Go back to idle after each interrupt
}
