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
#include "main.h"
#include "types.h"
#include "led.h"


/***************************************< Definitions >**************************************/
#define PWM_LEVELS      (16u)  //!< PWM levels implemented: [0; PWM_LEVELS)

// Pin definitions
#define MPX1            GPIOB,LL_GPIO_PIN_0  //!< Pin of MPX1 multiplexer pin
#define MPX2            GPIOB,LL_GPIO_PIN_1  //!< Pin of MPX2 multiplexer pin
#define LED0            GPIOA,LL_GPIO_PIN_7  //!< Pin of LED0 common pin
#define LED1            GPIOA,LL_GPIO_PIN_6  //!< Pin of LED1 common pin
#define LED2            GPIOA,LL_GPIO_PIN_3  //!< Pin of LED2 common pin
#define LED3            GPIOA,LL_GPIO_PIN_2  //!< Pin of LED3 common pin
#define LED4            GPIOF,LL_GPIO_PIN_1  //!< Pin of LED4 common pin
#define LED5            GPIOF,LL_GPIO_PIN_0  //!< Pin of LED5 common pin


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
  LL_GPIO_InitTypeDef TIM1CH1MapInit= {0};
  U8 u8Index;
  
  // Init globals
  gu8PWMCounter = 0;
  for( u8Index = 0; u8Index < LEDS_NUM; u8Index++ )
  {
    gau8LEDBrightness[ u8Index ] = 0;
  }
  
  // Enable clocks
  LL_IOP_GRP1_EnableClock( LL_IOP_GRP1_PERIPH_GPIOA );
  LL_IOP_GRP1_EnableClock( LL_IOP_GRP1_PERIPH_GPIOB );
  LL_IOP_GRP1_EnableClock( LL_IOP_GRP1_PERIPH_GPIOF );
  
  // Initialize GPIO pins
  /* Default output states */
  LL_GPIO_WriteOutputPort( GPIOA, 0u );
  LL_GPIO_WriteOutputPort( GPIOB, LL_GPIO_PIN_0 );  // MPX1 starts as 1
  LL_GPIO_WriteOutputPort( GPIOF, 0u );
  /* GPIOA */
  TIM1CH1MapInit.Pin        = LL_GPIO_PIN_7 | LL_GPIO_PIN_6 | LL_GPIO_PIN_3 | LL_GPIO_PIN_2;
  TIM1CH1MapInit.Mode       = LL_GPIO_MODE_OUTPUT;
  TIM1CH1MapInit.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  TIM1CH1MapInit.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  LL_GPIO_Init( GPIOA, &TIM1CH1MapInit );

  /* GPIOB */
  TIM1CH1MapInit.Pin        = LL_GPIO_PIN_0 | LL_GPIO_PIN_1;
  TIM1CH1MapInit.Mode       = LL_GPIO_MODE_OUTPUT;
  TIM1CH1MapInit.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  TIM1CH1MapInit.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  LL_GPIO_Init( GPIOB, &TIM1CH1MapInit );

  /* GPIOF */
  TIM1CH1MapInit.Pin        = LL_GPIO_PIN_0 | LL_GPIO_PIN_1;
  TIM1CH1MapInit.Mode       = LL_GPIO_MODE_OUTPUT;
  TIM1CH1MapInit.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  TIM1CH1MapInit.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  LL_GPIO_Init( GPIOF, &TIM1CH1MapInit );
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
    LL_GPIO_TogglePin( MPX1 );
    LL_GPIO_TogglePin( MPX2 );
  }
  
  //NOTE: unfortunately SFRs cannot be put in an array, so this cannot be implented as a for cycle
  if( gbitSide )  // left side
  {
    if( gau8LEDBrightness[ 0u ] > gu8PWMCounter )  // D12
    {
      LL_GPIO_SetOutputPin( LED0 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED0 );
    }
    if( gau8LEDBrightness[ 1u ] > gu8PWMCounter )  // D4
    {
      LL_GPIO_SetOutputPin( LED1 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED1 );
    }
    if( gau8LEDBrightness[ 2u ] > gu8PWMCounter )  // D6
    {
      LL_GPIO_SetOutputPin( LED2 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED2 );
    }
    if( gau8LEDBrightness[ 3u ] > gu8PWMCounter )  // D10
    {
      LL_GPIO_SetOutputPin( LED3 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED3 );
    }
    if( gau8LEDBrightness[ 4u ] > gu8PWMCounter )  // D8
    {
      LL_GPIO_SetOutputPin( LED4 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED4 );
    }
    if( gau8LEDBrightness[ 5u ] > gu8PWMCounter )  // D2
    {
      LL_GPIO_SetOutputPin( LED5 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED5 );
    }
  }
  else  // right side
  {
    if( gau8LEDBrightness[ 6u ] > gu8PWMCounter )  // D3
    {
      LL_GPIO_SetOutputPin( LED5 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED5 );
    }
    if( gau8LEDBrightness[ 7u ] > gu8PWMCounter )  // D9
    {
      LL_GPIO_SetOutputPin( LED4 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED4 );
    }
    if( gau8LEDBrightness[ 8u ] > gu8PWMCounter )  // D11
    {
      LL_GPIO_SetOutputPin( LED3 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED3 );
    }
    if( gau8LEDBrightness[ 9u ] > gu8PWMCounter )  // D7
    {
      LL_GPIO_SetOutputPin( LED2 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED2 );
    }
    if( gau8LEDBrightness[ 10u ] > gu8PWMCounter )  // D5
    {
      LL_GPIO_SetOutputPin( LED1 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED1 );
    }
    if( gau8LEDBrightness[ 11u ] > gu8PWMCounter )  // D13
    {
      LL_GPIO_SetOutputPin( LED0 );
    }
    else
    {
      LL_GPIO_ResetOutputPin( LED0 );
    }
  }
}


/***************************************< End of file >**************************************/
