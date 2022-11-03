/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file main.c
*
* \brief Valentine's heart main program
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
#include "radio.h"
#include "led.h"
#include "animation.h"
#include "persist.h"


/***************************************< Definitions >**************************************/
//#define DEBUG_HW  //!< Hardware on breadboard for debugging the radio protocol
#ifdef DEBUG_HW
  #define BUTTON_PIN     (P55)
#else
  #define BUTTON_PIN     (P32)  //!< Button for selecting animation and pairing devices
#endif

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
  BIT bitPairing = 0;
  BIT bitGotPartnerID = 0;
  BIT bitAnimationChange = 0;
  S_RADIO_PAYLOAD sPacket;
  
  // Initialize modules
  Radio_Init();
  Util_Init();
  LED_Init();
  Animation_Init();
  Persist_Init();
  
  // Init global variables in this module
  geButtonState = BUTTON_UNPRESSED;
  gu16ButtonPressTimer = 0u;
  u8CurrentAnimation = gsPersistentData.u8AnimationIndex;
  
#ifdef DEBUG_HW
  // Pushbutton @ P5.5 --> bidirectional with pullup
  // NOTE: this might not the best in terms of power consumption, but the input mode with pullup was not enough
  P5M0 &= ~(1u<<5u);  // 0
  P5M1 &= ~(1u<<5u);  // 0
  P5PU |= 1u<<5u;
#else
  // Pushbutton @ P3.2 --> bidirectional with pullup
  // NOTE: this might not the best in terms of power consumption, but the input mode with pullup was not enough
  P3M0 &= ~(1u<<2u);  // 0
  P3M1 &= ~(1u<<2u);  // 0
  P3PU |= 1u<<2u;
  // Testpoint @ P1.7 -- not connected, bidirectional with pullup
  P1M0 &= ~(1u<<7u);  // 0
  P1M1 &= ~(1u<<7u);  // 0
  P1PU |= 1u<<7u;
#endif

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
            // Clear pairing state  -- if it's in pairing state then cancel it
            bitPairing = 0;
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
          Radio_FlushTX();  // discard TX FIFO
          bitAnimationChange = 1;  // Send packet about animation change
          gbitRadioSyncEnabled = 0;  // Disable synchronization during the duration of animation change
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
          bitPairing = 1;
          bitGotPartnerID = 0;
          // Set pairing animation
          Animation_Set( NUM_ANIMATIONS-1u );  // last animation
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
    // Pairing packets
    if( 1 == bitPairing )
    {
      // Send out pairing request
      sPacket.u8Type = RADIO_PACKET_TYPE_BROADCAST;
      sPacket.au8Payload[ 0u ] = RADIO_HANDSHAKE_PAIRING_REQUEST;
      (void)Radio_Send( &sPacket );
    }
    else if( 1 == bitAnimationChange )  // If our partner had not acknowledged the animation change
    {
      // Send animation change request
      sPacket.u8Type = RADIO_PACKET_TYPE_UNICAST;
      sPacket.au8Payload[ 0u ] = RADIO_HANDSHAKE_ANIMATION_CHANGE_REQ;
      sPacket.au8Payload[ 1u ] = u8CurrentAnimation;
      (void)Radio_Send( &sPacket );
    }
    // Get RX packets
    if( TRUE == Radio_Receive( &sPacket ) )
    {
      // Got a pairing message
      if( RADIO_PACKET_TYPE_BROADCAST == sPacket.u8Type )
      {
        if( 0 == bitGotPartnerID )
        {
          bitGotPartnerID = 1;
          bitPairing = 0u;
          Radio_SetFilter( sPacket.au8SenderID );
          Persist_Save();
          Animation_Set( u8CurrentAnimation );
          Radio_FlushTX();  // discard TX FIFO
          // Send reply that we got its ID
          sPacket.u8Type = RADIO_PACKET_TYPE_BROADCAST;
          sPacket.au8Payload[ 0u ] = RADIO_HANDSHAKE_PAIRING_ACCEPTED;
          (void)Radio_Send( &sPacket );
        }
        else if( 0u == memcmp( (void*)sPacket.au8SenderID, (void*)gsPersistentData.au8PartnerUID, UID_LENGTH ) )  // got the pairing from someone we're already paired
        {
          bitPairing = 0u;
          // Send reply that we got its ID
          sPacket.u8Type = RADIO_PACKET_TYPE_BROADCAST;
          sPacket.au8Payload[ 0u ] = RADIO_HANDSHAKE_PAIRING_ACCEPTED;
          (void)Radio_Send( &sPacket );
        }
      }
      else if( RADIO_PACKET_TYPE_UNICAST == sPacket.u8Type )  // got a message from our designated pair
      {
        if( RADIO_HANDSHAKE_ANIMATION_CHANGE_REQ == sPacket.au8Payload[ 0u ] )
        {
          // If our current animation is not the same as our partner's
          if( u8CurrentAnimation != sPacket.au8Payload[ 1u ] )
          {
            // Set animation
            u8CurrentAnimation = sPacket.au8Payload[ 1u ];
            Animation_Set( u8CurrentAnimation );
            // Save it
            Persist_Save();
          }
          gbitRadioSyncEnabled = 1;  // Re-enable synchronization if it was disabled
          Radio_FlushTX();  // discard TX FIFO
          // Send animation change accept
          sPacket.u8Type = RADIO_PACKET_TYPE_UNICAST;
          sPacket.au8Payload[ 0u ] = RADIO_HANDSHAKE_ANIMATION_CHANGE_ACK;
          sPacket.au8Payload[ 1u ] = u8CurrentAnimation;
          (void)Radio_Send( &sPacket );
        }
        else if( RADIO_HANDSHAKE_ANIMATION_CHANGE_ACK == sPacket.au8Payload[ 0u ] )
        {
          // Our partner acknowledged the animation change
          if( u8CurrentAnimation == sPacket.au8Payload[ 1u ] ) // only if they acknowledged the current animation
          {
            Radio_FlushTX();  // discard TX FIFO, as our partner has already acknowledged the change
            bitAnimationChange = 0;
            gbitRadioSyncEnabled = 1;  // Re-enable synchronization
          }
        }
      }
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
//  P54 = 0;  // debug
//  P17 = 0;  // debug
  Util_Interrupt();  // Housekeeping, e.g. ms delay timer
  LED_Interrupt();  // Soft-PWM LED driver
  Radio_TimerInterrupt();  // Radio communication
  // End of interrupt
  TF0 = 0;  // clear Timer0 IT flag
//  P54 = 1;  // debug
//  P17 = 1;  // debug
}


/***************************************< End of file >**************************************/
