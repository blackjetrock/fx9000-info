////////////////////////////////////////////////////////////////////////////////
//
// Casio FX9000P Video RAM replacement
//
// Emulates a single 4044 type RAM chip
//
////////////////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"

#include "f_util.h"

#include "ff.h"
#include "ff_stdio.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"

// Use this if breakpoints don't work
#define DEBUG_STOP {volatile int x = 1; while(x) {} }

#define TEST_SINGLE_ADDRESS  0
#define TEST_ALL_ADDRESS     1

//-----------------------------------------------------------------------------
//
// ROM Emulator Flags

// OE pin used to switch data direction
// If 0 then DDIR_A15 used, under processor conrol

#define USE_OE_FOR_DATA_DIRECTION 1

// The address lines we are looking at

// Do we run emulation on second core?
#define EMULATE_ON_CORE1   1

// RAM chip is 4K

#define ROM_SIZE  4*1024
#define ADDRESS_MASK (ROM_SIZE - 1)

// Map from memory space to ROM address space
#define MAP_ROM(X) (X & ADDRESS_MASK)

volatile uint8_t rom_data[ROM_SIZE] =
  {
   // ASSEMBLER_EMBEDDED_CODE_START

   // ASSEMBLER_EMBEDDED_CODE_END
  };

//------------------------------------------------------------------------------

// The 4044 has a data out pin and an data in pin
const int DOUT_PIN = 0;       // Can be Hi Z or output
const int DIN_PIN = 1;        // Always input

//
// The address lines (always inputs)
//
const int  A0_PIN  =  8;
const int  A1_PIN  =  9;
const int  A2_PIN  = 10;
const int  A3_PIN  = 11;
const int  A4_PIN  = 12;
const int  A5_PIN  = 13;
const int  A6_PIN  = 14;
const int  A7_PIN  = 15;
const int  A8_PIN  = 16;
const int  A9_PIN  = 17;
const int  A10_PIN = 18;
const int  A11_PIN = 19;
const int  A12_PIN = 20;
const int  A13_PIN = 21;

const int LINKOUT_PIN  = 22;
const int LINKIN_PIN   = 28;

const int INPUT1_PIN   = 26;
const int INPUT0_PIN   = 27;

// 4044 has a select pin and an write pin, both active low
const int S_PIN       = INPUT1_PIN;
const int W_PIN       = INPUT0_PIN;

// Arrays for setting GPIOs up
#define NUM_ADDR 14
#define NUM_DATA 8

const int address_pins[NUM_ADDR] =
  {
   A0_PIN,
   A1_PIN,
   A2_PIN,
   A3_PIN,
   A4_PIN,
   A5_PIN,
   A6_PIN,
   A7_PIN,
   A8_PIN,
   A9_PIN,
   A10_PIN,
   A11_PIN,
   A12_PIN,
   A13_PIN,
  };

////////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//
// Put a value on the data out line. Value is LS bit of the rom array.

inline void set_data(BYTE data)
{
  int states;
  int dat = data & 1;
  
  // Direct register access to make things faster
  sio_hw->gpio_set = (  dat  << DOUT_PIN);
  sio_hw->gpio_clr = ((dat ^ 0x01) << DOUT_PIN);
}

// Only deals with DOUT pin
inline void set_data_inputs(void)
{
  //sio_hw->gpio_oe_clr = 0x00000001;
  sio_hw->gpio_oe_clr = (1 << DOUT_PIN);
}

inline void set_data_outputs(void)
{
  //  sio_hw->gpio_oe_set = 0x00000001;
  sio_hw->gpio_oe_set = (1<<DOUT_PIN);
}


////////////////////////////////////////////////////////////////////////////////
//
// Set things up then sit in a loop waiting for the emulated device to
// be selected
//
////////////////////////////////////////////////////////////////////////////////

#define MAX_ADDR_TRACE 1024*64
int trace_on = 0;
volatile int addr_trace_index = 0;
volatile uint16_t addr_trace[MAX_ADDR_TRACE];
volatile unsigned int number_ce_assert = 0;


////////////////////////////////////////////////////////////////////////////////
//
// Emulate a RAM chip
//
////////////////////////////////////////////////////////////////////////////////


void ram_emulate(void)
{
  //printf("\nEmulating RAM...");

  irq_set_mask_enabled( 0xFFFFFFFF, 0 );

  while(1)
    {
      uint32_t gpio_states;
      BYTE db;
      unsigned int addr;
      
      // We look for S low
	if( (gpio_states = sio_hw->gpio_in) & (1 << S_PIN) )
      	{
	  // S high, we are not selected
	  // Data lines inputs
	  set_data_inputs();
      	}
      else
      	{
	  // S low, we are selected
	  //	  printf("\nSEl");

	  // We have to monitor W for a write pulse
	  // if we see it then we write the data on the rising edge of W
	  // While W is high we treat this as a read and present data

	  while(1)
	    {
	      gpio_states = sio_hw->gpio_in;

	      if( gpio_states & (1 << S_PIN) )
		{
		  // S gone high, exit the loop
		  // Delay for 100ns or so as the real 4044 does this
		  // and data is available for up to 100ns after CE rises.
#if 0
		  for(volatile int d=0; d<2; d++)
		    {
		    }
#endif		  
		  set_data_inputs();
		  break;
		}

	      // Is this a read or a write?
	      if( !(gpio_states & ( 1<< W_PIN)) )
		{
		  //printf("\nWR");
		  // Write
		  // data lines inputs
		  set_data_inputs();
		  
		  // Wait for W to go high then latch data

		  while( !((gpio_states = sio_hw->gpio_in) & (1 << W_PIN)) )
		    {
		    }
		  addr = (gpio_states >> 8) & ADDRESS_MASK;
		  
		  // We have only 1 bit of data to store, it is read from the DIN pin
		  rom_data[addr] = ((gpio_states >> 0) & (1 << DIN_PIN))>>DIN_PIN;
		}
	      else
		{
		  //printf("\nRD");
		  // Read
		  // make DOUT an output
		  set_data_outputs();
		  
		  // ROM emulation so always a read of us
		  // get address
		  addr = (gpio_states >> 8) & ADDRESS_MASK;
		  
		  // Get data and present it on bus (single bit)
		  set_data(rom_data[addr]);
		  
		  //set_data(0xF8);
		}
	    }
#if 0	  
	  // Trace address
	  if( addr == 3 )
	    {
	      trace_on = 1;
	    }
	  
	  if( trace_on)
	    {
	      if( addr_trace_index < MAX_ADDR_TRACE )
		{
		  int tv = addr;
		  if( addr_trace[addr_trace_index] == tv )
		    {
		    }
		  else
		    {
		      addr_trace_index++;
		      addr_trace[addr_trace_index] = tv;
		    }
		  //addr_trace_index %= MAX_ADDR_TRACE;
		  
		}
	    }
	  number_ce_assert++;

#endif
#if 0
	  
	  // Wait for CE to be de-asserted
	  while(1)
	    {
	      // S high, we are not selected
	      // data lines inputs
	      gpio_states = sio_hw->gpio_in;
	      //printf("\nWAIT %08X", gpio_states);
	      //printf("  %02X", gpio_get(S_PIN));
	      // We look for S
	      if( gpio_states & (1 << S_PIN) )
		{
		  // S high, we are not selected
		  // data lines inputs
		  set_data_inputs();
		  //printf("\nDONE WAIT");
		  break;
		}
	    }
#endif
	}
    }
}  

void set_gpio_input(int gpio_pin)
{
  gpio_init(gpio_pin);
  gpio_set_dir(gpio_pin, GPIO_IN);
  gpio_set_pulls(gpio_pin, 0, 0);
}

void set_gpio_output(int gpio_pin)
{
  gpio_init(gpio_pin);
  gpio_set_dir(gpio_pin, GPIO_OUT);
}

////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////

int main()
{

  //DEBUG_STOP;
  
  char line[80];

#if TEST_SINGLE_ADDRESS
  int count = 0;
  
  for (int i=0; i<NUM_ADDR; i++)
    {
      set_gpio_output(address_pins[i]);
    }

  while(1)
    {
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 0);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
      gpio_put(A0_PIN, 1);
    }

#endif

  
#if TEST_ALL_ADDRESS
  int count = 0;
  
  for (int i=0; i<NUM_ADDR; i++)
    {
      set_gpio_output(address_pins[i]);
    }

  while(1)
    {
      
      for (int i=0; i<NUM_ADDR; i++)
	{
	  gpio_put(address_pins[i], count & (1 <<i));
	}
      count++;
    }

#endif


  
  //#define OVERCLOCK 135000
//#define OVERCLOCK 200000
#define OVERCLOCK 270000
//#define OVERCLOCK 360000

  #if OVERCLOCK > 270000
  /* Above this speed needs increased voltage */
  vreg_set_voltage(VREG_VOLTAGE_1_20);
  sleep_ms(1);
#endif

  /* Overclock */
  set_sys_clock_khz( OVERCLOCK, 1 );

#if 0  
  stdio_init_all();

  printf("\n\n");
  printf("\n/------------------------------\\");
  printf("\n| Casio FX9000P Pico Cartridge |");
  printf("\n| Bus Pico                     |");
  printf("\n/------------------------------/");
  printf("\n");
  
  printf("\nSetting GPIOs...");
#endif

  
  for (int i=0; i<NUM_ADDR; i++)
    {
      set_gpio_input(address_pins[i]);
    }

  set_gpio_input(DIN_PIN);
  set_gpio_input(DOUT_PIN);
  
  set_gpio_input(S_PIN);
  set_gpio_input(W_PIN);

  multicore_launch_core1(ram_emulate);

  int i = 0;
  int p = 0xff;
  while(1)
    {
    }
  
  while(1)
    {
      //      rom_data[0]++;
      for(volatile int j=0; j<10000; j++)
	{
	}
      rom_data[i++] = p;
      
      if( i>= ROM_SIZE )
	{
	  i=0;
	  p ^=0xff;
	}
    }
  
  //ram_emulate();

#if 0  
  // As a test send some data over the link
  sleep_ms(2000);
#endif
  
  int ce = 0;

  //set_data_outputs();
#if 0
#if !USE_OE_FOR_DATA_DIRECTION


  // LS_DIR is an output
  gpio_set_dir(LS_DIR_PIN, GPIO_OUT);
#else
  gpio_put(DDIR_A15_PIN, 0);
  gpio_set_dir(DDIR_A15_PIN, GPIO_OUT);
#endif
#endif
  

  // Use a polling loop for minimum latency
  // Turn off timer interrupts
  irq_set_mask_enabled(0xf, false);
  
  // We set the CE of the level shifters to be driven by OE on the
  // host board, and the direction (A->B) to be set up by the Pico
  // The data direction can be left as output from the Pico as
  // the OE/CE line will drive on to the data bus when OE is asserted

  // Core 0 handles the link traffic
  
  // Emulate RAM/ROM on core 1
  multicore_launch_core1(ram_emulate);
#if 0
  printf("\nMain Loop...");

  printf("\nSend idle command...");
#endif
  while(1)
  //  for(int s=0; s<10; s++)
    {
      //      sleep_ms(100);
    }
}
