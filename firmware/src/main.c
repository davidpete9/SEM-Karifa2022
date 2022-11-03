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
#include "animation.h"
#include "persist.h"


/***************************************< Definitions >**************************************/
#define BUTTON_PIN     (P32)  //!< Button for selecting animation and turning it off and on

// Handshake types for radio
#define RADIO_HANDSHAKE_PAIRING_REQUEST         (0x00u)  //!< (Broadcast) Seeking partners
#define RADIO_HANDSHAKE_PAIRING_ACCEPTED        (0x01u)  //!< (Broadcast) Pairing request accepted
#define RADIO_HANDSHAKE_ANIMATION_CHANGE_REQ    (0xF0u)  //!< (Unicast) Animation change request
#define RADIO_HANDSHAKE_ANIMATION_CHANGE_ACK    (0xF1u)  //!< (Unicast) Animation change accepted


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
  U8  u8CurrentAnimation = 0u;

  // Initialize modules
  Util_Init();
  LED_Init();
  Animation_Init();
  Persist_Init();

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
//          Persist_Save();
        }
        else if( Util_GetTimerMs() == gu16ButtonPressTimer )  // the long press timer has just went off
        {
          geButtonState = BUTTON_LONGPRESS;
          // Actions for long button press
//          bitPairing = 1;
//          bitGotPartnerID = 0;
          // Set pairing animation
//          Animation_Set( NUM_ANIMATIONS-1u );  // last animation
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
  // End of interrupt
  TF0 = 0;  // clear Timer0 IT flag
}


/***************************************< End of file >**************************************/
