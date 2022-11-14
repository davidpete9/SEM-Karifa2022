/*! *******************************************************************************************************
* Copyright (c) 2022 Hekk_Elek
*
* \file rgbled.c
*
* \brief RGB LED driver using current-mode pulse control
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/***************************************< Includes >**************************************/
#include "stc8g.h"
#include <string.h>

// Own includes
#include "types.h"
#include "rgbled.h"


/***************************************< Definitions >**************************************/
#define COLOR_LEVELS   (16u)  //!< Number of brightness levels per color
#define PIN_R          (P54)  //!< GPIO pin for red LED
#define PIN_G          (P55)  //!< GPIO pin for green LED
#define PIN_B          (P16)  //!< GPIO pin for blue LED


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
//! \brief Global array for RGB LED color values
//! \note  Value set is between [0; COLOR_LEVELS)
volatile U8 gau8RGBLEDs[ NUM_RGBLED_COLORS ];


/***************************************< Static function definitions >**************************************/
static void RedPulseDelay( void );
static void GreenPulseDelay( void );
static void BluePulseDelay( void );


/***************************************< Private functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for red LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void RedPulseDelay( void )
{
  // Wait for 3 usec
	unsigned char i;

	i = 22;
	while (--i);
}

//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for green LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void GreenPulseDelay( void )
{
  // Wait for 1 usec
	unsigned char i;

	i = 22;
	while (--i);
}

//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for blue LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void BluePulseDelay( void )
{
  // Wait for 1 usec
	unsigned char i;

	i = 22;
	while (--i);
}


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initialize hardware and software layer
//! \param  -
//! \return -
//! \global gau8RGBLEDs
//-----------------------------------------------------------------------------
void RGBLED_Init( void )
{
  memset( gau8RGBLEDs, 0, NUM_RGBLED_COLORS );
  
  // Initialize GPIO pins
	// NOTE: Pin modes are set by PxM0 and PxM1 registers
	// ------------+------+---------------
	// Bit in PxM0 | PxM1 | Pin mode
	//          0  |   0  | Bidirectional
	//          0  |   1  | Input
	//          1  |   0  | Output
	//          1  |   1  | Open-drain
	// ------------+------+---------------
  // Red LED (P1.6)
  PIN_R = 1;
  P1M0 |=   1u<<6u;  // push-pull
  P1M1 &= ~(1u<<6u);
  // Green and blue LEDs (P5.4, P5.5)
  PIN_G = 1;
  PIN_B = 1;
  P5M0 |=  (1u<<4u) |  (1u<<5u);  // push-pull
  P5M1 &= ~(1u<<4u) & ~(1u<<5u);
}

//----------------------------------------------------------------------------
//! \brief  Interrupt routine for pulse-controlled RGB LED driver
//! \param  -
//! \return -
//! \global gau8RGBLEDs
//! \note   Should be called from periodic timer interrupt routine.
//-----------------------------------------------------------------------------
void RGBLED_Interrupt( void )
{
  static volatile u8Cnt = 0u;
  
  if( gau8RGBLEDs[ 0 ] > u8Cnt )  // Red
  {
    PIN_R = 0;
    RedPulseDelay();
    PIN_R = 1;
  }
  if( gau8RGBLEDs[ 1 ] > u8Cnt )  // Green
  {
    PIN_G = 0;
    GreenPulseDelay();
    PIN_G = 1;
  }
  if( gau8RGBLEDs[ 2 ] > u8Cnt )  // Blue
  {
    PIN_B = 0;
    BluePulseDelay();
    PIN_B = 1;
  }
  u8Cnt++;
  if( COLOR_LEVELS <= u8Cnt )
  {
    u8Cnt = 0u;
  }
}


/***************************************< End of file >**************************************/
