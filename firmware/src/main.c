/*! *******************************************************************************************************
* Copyright (c) 2022 Hekk_Elek
*
* \file main.c
*
* \brief SEM-karifa main program
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/***************************************< Includes >**************************************/
// Standard C libraries
#include <string.h>

// Own includes
#include "stc8g.h"
#include "types.h"
#include "util.h"
#include "led.h"
#include "rgbled.h"
#include "animation.h"
#include "persist.h"
#include "batterylevel.h"


/***************************************< Definitions >**************************************/
#define BUTTON_PIN     (P32)  //!< Button for selecting animation and turning it off and on


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
//! \brief State machine for button debouncing
static enum
{
  BUTTON_UNPRESSED,  //!< The button is not pressed
  BUTTON_BOUNCING,   //!< The button just got pressed and it's currently bouncing
  BUTTON_PRESSED,    //!< The button got debounced
  BUTTON_LONGPRESS,  //!< The button has been pressed for long
  BUTTON_RELEASING   //!< The button just got released and it's currently bouncing
} geButtonState;

static U16 gu16ButtonPressTimer;  //!< Timer for the button debouncing state machine


/***************************************< Static function definitions >**************************************/
static void Timer0Init( void );


/***************************************< Private functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Timer 0 initialization (100 us interrupt period)
//! \param  -
//! \return -
//! \note   Assumes 24 MHz system clock. Generated using STC-ISP tool.
//-----------------------------------------------------------------------------
static void Timer0Init( void )
{
  TR0 = 0;       //Timer0 stop run
  AUXR |= 0x80;  //Timer clock is 1T mode
  TMOD &= 0xF0;  //Set timer work mode
  TL0 = 0xA0;    //Initial timer value
  TH0 = 0xF6;    //Initial timer value
  TF0 = 0;       //Clear TF0 flag
  TR0 = 1;       //Timer0 start run
}


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Main program entry point
//! \param  -
//! \return -
//-----------------------------------------------------------------------------
void main( void )
{
  U8   u8CurrentAnimation = 0u;
  BOOL bPressedLong = FALSE;

  // Initialize modules
  Util_Init();
  LED_Init();
  RGBLED_Init();
  Animation_Init();
  Persist_Init();
  BatteryLevel_Init();

  // Pushbutton @ P3.2 --> bidirectional with pullup
  // NOTE: this might not the best in terms of power consumption, but the input mode with pullup was not enough
  P3M0 &= ~(1u<<2u);  // 0
  P3M1 &= ~(1u<<2u);  // 0
  P3PU |= 1u<<2u;
  
  // Init global variables in this module
  geButtonState = BUTTON_UNPRESSED;
  gu16ButtonPressTimer = 0u;
  u8CurrentAnimation = gsPersistentData.u8AnimationIndex;
  
  // Init timer and start interrupts
  Timer0Init();  
  PT0  = 1;          // Timer0 IT priority: 1
  IPH &= ~(1u<<1u);  // PT0H = 0
  ET0 = 1;  // Enable Timer0 interrupts
  EA  = 1;  // Global interrupt enable

  // Wait if the button is pressed on power up
  // This is necessary, to avoid changing animation on power on
  while( 0 == BUTTON_PIN )
  {
    gu16ButtonPressTimer = Util_GetTimerMs() + 100u;  // 1 ms wait
    while( gu16ButtonPressTimer > Util_GetTimerMs() );
  }

  // Measure and show battery level
  BatteryLevel_Show();
    
  // Main loop
  while( TRUE )
  {
    // Debounce button in a nonblocking way
    switch( geButtonState )
    {
      case BUTTON_BOUNCING:   // The button just got pressed and it's currently bouncing
        if( Util_GetTimerMs() == gu16ButtonPressTimer )  // the debounce timer has just went off
        {
          if( 0 == BUTTON_PIN )  // if the button is still pressed
          {
            gu16ButtonPressTimer = Util_GetTimerMs() + 2000u;  // 2 sec long press
            geButtonState = BUTTON_PRESSED;
          }
          else  // not pressed anymore
          {
            geButtonState = BUTTON_UNPRESSED;
          }
        }
        break;
      
      case BUTTON_PRESSED:    // The button got debounced
        if( 1 == BUTTON_PIN )  // just got released
        {
          gu16ButtonPressTimer = Util_GetTimerMs() + 50u;  // 50 ms debounce time
          geButtonState = BUTTON_RELEASING;
          // Actions for short button press
          u8CurrentAnimation++;
          if( u8CurrentAnimation >= NUM_ANIMATIONS-1u )
          {
            u8CurrentAnimation = 0u;
          }
          Animation_Set( u8CurrentAnimation );
          // Save it
          Persist_Save();
        }
        else if( Util_GetTimerMs() == gu16ButtonPressTimer )  // the long press timer has just went off
        {
          geButtonState = BUTTON_LONGPRESS;
          // Actions for long button press
          // Signal that it will be shut down by setting a completely black animation
          u8CurrentAnimation = NUM_ANIMATIONS-1u;
          Animation_Set( u8CurrentAnimation );
          bPressedLong = TRUE;
        }
        break;
      
      case BUTTON_LONGPRESS:  // The button has been pressed for long
        if( 1 == BUTTON_PIN )  // just got released
        {
          gu16ButtonPressTimer = Util_GetTimerMs() + 50u;  // 50 ms debounce time
          geButtonState = BUTTON_RELEASING;
        }
        break;
      
      case BUTTON_RELEASING:  // The button just got released and it's currently bouncing
        if( Util_GetTimerMs() == gu16ButtonPressTimer )  // the debounce timer has just went off
        {
          if( 1 == BUTTON_PIN )  // if the button is released
          {
            gu16ButtonPressTimer = Util_GetTimerMs() + 2000u;  // 2 sec long press
            geButtonState = BUTTON_UNPRESSED;
            
            if( TRUE == bPressedLong )
            {
              // Go to power-down sleep
              EA = 0;   // Disable all interrupts
              TR0 = 0;  // Stop Timer 0
              ET0 = 0;  // Disable Timer 0 interrupt
              EX0 = 1;  // Enable INT0 interrupt
              P1 = 0xFFu;  // Set all pins to 1
              P3 = 0xFFu;
              P5 = 0x3Fu;
              P1M0 = 0x00u;  // All pins must be bidirectional
              P1M1 = 0x00u;
              P3M0 = 0x00u;
              P3M1 = 0x00u;
              P5M0 = 0x00u;
              P5M0 = 0x00u;
              EA = 1;  // Enable all interrupts
              PCON |= 0x02u;  // PD bit
              bPressedLong = FALSE;  // This should not be reached...
            }
          }
          else  // still pushed
          {
            gu16ButtonPressTimer = Util_GetTimerMs() + 50u;  // 50 ms debounce time
          }
        }
        break;
      
      default:  // BUTTON_UNPRESSED -- The button is not pressed
        if( 0 == BUTTON_PIN )  // if the button has just got pressed
        {
          gu16ButtonPressTimer = Util_GetTimerMs() + 50u;  // 50 ms debounce time
          geButtonState = BUTTON_BOUNCING;
        }
        break;
    }
    Animation_Cycle();
    // Sleep until next interrupt
    PCON |= 0x01u;  // IDL bit
  }
}

//----------------------------------------------------------------------------
//! \brief  INT0 interrupt handler
//! \param  -
//! \return -
//! \note   Should be placed at 0x0003 (==IT vector 0).
//-----------------------------------------------------------------------------
#pragma vector=0x0003
IT_PRE void INT0_ISR( void ) ITVECTOR0
{
  // Normally, only after waking up from power down mode should lead to here
  // Perform software reset
  IAP_CONTR |= 0x20u;
  while( 1 );  // This should not be reached
}

//----------------------------------------------------------------------------
//! \brief  Timer 0 interrupt handler
//! \param  -
//! \return -
//! \note   Should be placed at 0x000B (==IT vector 1).
//-----------------------------------------------------------------------------
#pragma vector=0x000B
IT_PRE void timer0_isr( void ) ITVECTOR1
{
  Util_Interrupt();  // Housekeeping, e.g. ms delay timer
  LED_Interrupt();  // Soft-PWM LED driver
  RGBLED_Interrupt();  // RGB LED driver
  // End of interrupt
  TF0 = 0;  // clear Timer0 IT flag
}


/***************************************< End of file >**************************************/
