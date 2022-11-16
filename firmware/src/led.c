/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file led.c
*
* \brief Soft-PWM LED driver
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/***************************************< Includes >**************************************/
// Own includes
#include "stc8g.h"
#include "types.h"
#include "led.h"


/***************************************< Definitions >**************************************/
#define PWM_LEVELS      (16u)  //!< PWM levels implemented: [0; PWM_LEVELS)

// Pin definitions
#define MPX1            (P10)  //!< Pin of MPX1 multiplexer pin
#define MPX2            (P11)  //!< Pin of MPX2 multiplexer pin
#define LED0            (P17)  //!< Pin of LED0 common pin
#define LED1            (P34)  //!< Pin of LED1 common pin
#define LED2            (P35)  //!< Pin of LED2 common pin
#define LED3            (P37)  //!< Pin of LED3 common pin
#define LED4            (P36)  //!< Pin of LED4 common pin
#define LED5            (P33)  //!< Pin of LED4 common pin


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
DATA U8 gau8LEDBrightness[ LEDS_NUM ];  //!< Array for storing individual brightness levels
DATA U8 gu8PWMCounter;                  //!< Counter for the base of soft-PWM
DATA BIT gbitSide;                      //!< Stores which side of the panel is active


/***************************************< Static function definitions >**************************************/


/***************************************< Private functions >**************************************/


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initialize all IO pins associated with LEDs
//! \param  -
//! \return -
//! \global gau8LEDBrightness[], gu8PWMCounter
//! \note   Should be called in the init block
//-----------------------------------------------------------------------------
void LED_Init( void )
{
  U8 u8Index;
  
  // Init globals
  gu8PWMCounter = 0;
  for( u8Index = 0; u8Index < LEDS_NUM; u8Index++ )
  {
    gau8LEDBrightness[ u8Index ] = 0;
  }
  
  // Starting with MPX1
  gbitSide = 0;
  
	// NOTE: Pin modes are set by PxM0 and PxM1 registers
	// ------------+------+---------------
	// Bit in PxM0 | PxM1 | Pin mode
	//          0  |   0  | Bidirectional
	//          0  |   1  | Input
	//          1  |   0  | Output
	//          1  |   1  | Open-drain
	// ------------+------+---------------
  // MPX1 (P1.0)
  P1M0 |= 1u<<0u;  // open drain
  P1M1 |= 1u<<0u;
  P1PU |= 1u<<0u;  // pull up
  // MPX2 (P1.1)
  P1M0 |= 1u<<1u;  // open drain
  P1M1 |= 1u<<1u;
  P1PU |= 1u<<1u;  // pull up
  // P3.3, P3.4, P3.5, P3.6, P3.7
  P3M0 |= (1u<<3u) | (1u<<4u) | (1u<<5u)  | (1u<<6u)  | (1u<<7u);  // push-pull
  P3M1 &= ~(1u<<3u) & ~(1u<<4u) & ~(1u<<5u) & ~(1u<<6u) & ~(1u<<7u);
  // P1.7
  P1M0 |= 1u<<7u;  // push-pull
  P1M1 &= ~(1u<<7u);
}

//----------------------------------------------------------------------------
//! \brief  Interrupt routine to implement soft-PWM
//! \param  -
//! \return -
//! \global gau8LEDBrightness[], gu8PWMCounter
//! \note   Should be called from periodic timer interrupt routine.
//-----------------------------------------------------------------------------
void LED_Interrupt( void )
{
  gu8PWMCounter++;
  if( gu8PWMCounter == PWM_LEVELS )
  {
    gu8PWMCounter = 0;
    gbitSide ^= 1;
    // Set multiplexer pins
    MPX1 = gbitSide;
    MPX2 = ~gbitSide;
  }
  
  //NOTE: unfortunately SFRs cannot be put in an array, so this cannot be implented as a for cycle
  if( gbitSide )  // left side
  {
    if( gau8LEDBrightness[ 0u ] > gu8PWMCounter )  // D12
    {
      LED0 = 1;
    }
    else
    {
      LED0 = 0;
    }
    if( gau8LEDBrightness[ 1u ] > gu8PWMCounter )  // D4
    {
      LED1 = 1;
    }
    else
    {
      LED1 = 0;
    }
    if( gau8LEDBrightness[ 2u ] > gu8PWMCounter )  // D6
    {
      LED2 = 1;
    }
    else
    {
      LED2 = 0;
    }
    if( gau8LEDBrightness[ 3u ] > gu8PWMCounter )  // D10
    {
      LED3 = 1;
    }
    else
    {
      LED3 = 0;
    }
    if( gau8LEDBrightness[ 4u ] > gu8PWMCounter )  // D8
    {
      LED4 = 1;
    }
    else
    {
      LED4 = 0;
    }
    if( gau8LEDBrightness[ 5u ] > gu8PWMCounter )  // D2
    {
      LED5 = 1;
    }
    else
    {
      LED5 = 0;
    }
  }
  else  // right side
  {
    if( gau8LEDBrightness[ 6u ] > gu8PWMCounter )  // D3
    {
      LED5 = 1;
    }
    else
    {
      LED5 = 0;
    }
    if( gau8LEDBrightness[ 7u ] > gu8PWMCounter )  // D9
    {
      LED4 = 1;
    }
    else
    {
      LED4 = 0;
    }
    if( gau8LEDBrightness[ 8u ] > gu8PWMCounter )  // D11
    {
      LED3 = 1;
    }
    else
    {
      LED3 = 0;
    }
    if( gau8LEDBrightness[ 9u ] > gu8PWMCounter )  // D7
    {
      LED2 = 1;
    }
    else
    {
      LED2 = 0;
    }
    if( gau8LEDBrightness[ 10u ] > gu8PWMCounter )  // D5
    {
      LED1 = 1;
    }
    else
    {
      LED1 = 0;
    }
    if( gau8LEDBrightness[ 11u ] > gu8PWMCounter )  // D13
    {
      LED0 = 1;
    }
    else
    {
      LED0 = 0;
    }
  }
}


/***************************************< End of file >**************************************/
