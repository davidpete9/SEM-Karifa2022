/*! *******************************************************************************************************
* Copyright (c) 2022 Hekk_Elek
*
* \file batterylevel.c
*
* \brief Battery level indicator subprogram
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/***************************************< Includes >**************************************/
#include <math.h>
#include <string.h>

// Own includes
#include "stc8g.h"
#include "types.h"
#include "led.h"
#include "rgbled.h"
#include "util.h"
#include "batterylevel.h"


/***************************************< Definitions >**************************************/


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/


/***************************************< Static function definitions >**************************************/
void Delay( U16 u16DelayMs );


/***************************************< Private functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Waits for the given number of milliseconds
//! \param  u16DelayMs: wait time
//! \return -
//! \global -
//! \note   Should be called from init block
//-----------------------------------------------------------------------------
void Delay( U16 u16DelayMs )
{
  U16 u16DelayEnd = Util_GetTimerMs() + u16DelayMs;
  while( Util_GetTimerMs() < u16DelayEnd );
}


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initializes battery level measurement
//! \param  -
//! \return -
//! \global -
//! \note   Should be called from init block
//-----------------------------------------------------------------------------
void BatteryLevel_Init( void )
{
  ADCTIM = 0x3Fu;      // 32 clock sampling time, 2 clocks channel selection hold time
  ADCCFG = 0x2Fu;      // Right-aligned results registers, slowest conversion
  ADC_CONTR = 0x80u;   // Enable ADC
  ADC_CONTR |= 0x0Fu;  // Select internal 1.19V reference
}

//----------------------------------------------------------------------------
//! \brief  Shows battery level on LEDs as a gauge
//! \param  -
//! \return -
//! \global -
//! \note   Should be called only once! Blocking function! Deinitializes ADC.
//-----------------------------------------------------------------------------
void BatteryLevel_Show( void )
{
  U16 u16MeasuredLevel;
  U8  u8ChargeLevel;
  U8  u8Index;
  float f32BatteryVoltage;
  // Set all LED brightness to maximum, to ensure a significant current draw
  memset( gau8LEDBrightness, 15, LEDS_NUM );
  gau8RGBLEDs[ 0u ] = 15u;
  Delay( 100u );
  // Measure battery voltage
  ADC_CONTR |= 0x40u;  // Start conversion
  _nop_();
  _nop_();
  while( !( ADC_CONTR & 0x20u ) );  // Wait for completion flag
  u16MeasuredLevel = ADC_RES<<8u | ADC_RESL;
  // Disable ADC to save power
  ADC_CONTR = 0x00u;
  // Calculate battery voltage
  f32BatteryVoltage = 1.19f/( (float)u16MeasuredLevel/1024.0f );
  // As CR2032 batteries quickly drop to 2.8V under load, we assume that 2.8V means full charge
  // And since at 2.0V our LEDs can be barely seen, at 2.0V we assume that our battery is completely depleted
  // As we have 6 + 1 LED levels, we divide this range to 7 levels
  u8ChargeLevel = floor( 7.0f*( f32BatteryVoltage - 2.0f )/0.8f + 0.5f );
  for( u8Index = 0u; u8Index < LEDS_NUM/2u; u8Index++ )
  {
    if( u8ChargeLevel >= u8Index )
    {
      gau8LEDBrightness[ u8Index ] = 15u;
      gau8LEDBrightness[ LEDS_NUM - u8Index - 1u ] = 15u;
    }
    else
    {
      gau8LEDBrightness[ u8Index ] = 0u;
      gau8LEDBrightness[ LEDS_NUM - u8Index - 1u ] = 0u;
    }
  }
  if( u8ChargeLevel > LEDS_NUM/2u )
  {
    gau8RGBLEDs[ 0u ] = 15u;  // Light up red LED
  }
  else
  {
    gau8RGBLEDs[ 0u ] = 0u;
  }
  Delay( 1000u );
}

/***************************************< End of file >**************************************/
