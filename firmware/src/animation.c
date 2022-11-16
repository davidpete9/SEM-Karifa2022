/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file animation.c
*
* \brief Implementation of LED animations
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/*----------------------------------------------------------------------------------------
How it works
============
Animations are implemented on a virtual machine. This machine has opcodes that operate on
the LED brightness state variables. Each animation program runs in a loop. Instructions have
the following format:
  [ LED brightness array -- signed integer ] [ Opcode ] [ Opcode specific operand ]

----------------------------------------------------------------------------------------*/

/***************************************< Includes >**************************************/
// Standard C libraries
#include <string.h>

// Own includes
#include "types.h"
#include "led.h"
#include "rgbled.h"
#include "util.h"
#include "animation.h"
#include "persist.h"


/***************************************< Definitions >**************************************/
#define RIGHT_LEDS_START    (6u)  //!< Index of the first LED on the right side of the board


/***************************************< Types >**************************************/
//! \brief Opcode bits used in animation virtual machine
typedef enum
{
  LOAD      = 0x00u,  //!< Loads the LED brightness array to the PWM driver
  ADD       = 0x01u,  //!< Adds the LED brightness array elements to the current brightness level; if overflows, it sets to zero
  RSHIFT    = 0x02u,  //!< Shifts all the current LED brightness levels clockwise
  LSHIFT    = 0x04u,  //!< Shifts all the current LED brightness levels anticlockwise
  USHIFT    = 0x08u,  //!< Shifts all the current LED brightness levels upwards
  DSHIFT    = 0x10u,  //!< Shifts all the current LED brightness levels downwards
  REPEAT    = 0x20u   //!< Do the instruction and repeat by (operand)-times
} E_ANIMATION_OPCODE;

//! \brief Instruction used by the animation state machine
typedef struct
{
  U16 u16TimingMs;                               //!< How long the machine should stay in this state
  U8  au8LEDBrightness[ LEDS_NUM ];              //!< Brightness of each LED
  U8  au8RGBLEDBrightness[ NUM_RGBLED_COLORS ];  //!< Brightness of each color
  U8  u8AnimationOpcode;                         //!< Opcode (E_ANIMATION_OPCODE)
  U8  u8AnimationOperand;                        //!< Opcode-specific operand
} S_ANIMATION_INSTRUCTION;

//! \brief Animation structure
typedef struct
{
  U8                                  u8AnimationLength;  //!< How many instructions this animation has
  const S_ANIMATION_INSTRUCTION CODE* psInstructions;     //!< Pointer to the instructions themselves
} S_ANIMATION;


/***************************************< Constants >**************************************/
//! \brief Retro animation
CODE const S_ANIMATION_INSTRUCTION gasRetroVersion[ 8u ] = 
  {
  {133u, {15,  0, 15,  0,  0, 15, 15,  0, 15,  0,  0, 15}, {15,  0,  0}, LOAD, 0u },
  {133u, { 0, 15,  0, 15, 15,  0,  0, 15,  0, 15, 15,  0}, { 0,  0,  0}, LOAD, 0u },
  {133u, {15,  0,  0,  0,  0,  0, 15,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD, 0u },
  {133u, { 0, 15,  0, 15, 15,  0,  0, 15,  0, 15, 15,  0}, { 0,  0,  0}, LOAD, 0u },
  {133u, {15,  0,  0,  0,  0,  0, 15,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD, 0u },
  {133u, { 0,  0,  0, 15,  0,  0,  0,  0,  0, 15,  0,  0}, { 0,  0,  0}, LOAD, 0u },
  {133u, {15,  0, 15,  0,  0, 15, 15,  0,  0, 15,  0, 15}, {15,  0,  0}, LOAD, 0u },
  {133u, { 0,  0,  0, 15,  0,  0,  0,  0,  0, 15,  0,  0}, { 0,  0,  0}, LOAD, 0u },
};

//! \brief "Sine" wave flasher animation
CODE const S_ANIMATION_INSTRUCTION gasSoftFlashing[ 4u ] = 
{
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD,          0u },
  { 50u, { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1}, { 1,  0,  0}, ADD | REPEAT, 14u },
  {100u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, {15,  0,  0}, LOAD,          0u }, 
  { 50u, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, {-1,  0,  0}, ADD | REPEAT, 14u },
};

//! \brief Shooting star anticlockwise animation
CODE const S_ANIMATION_INSTRUCTION gasShootingStar[ 7u ] = 
{ 
  {100u, { 5, 10, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD,            0u },
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, RSHIFT | REPEAT, 2u },
  {100u, { 0,  0,  0,  0,  5, 10,  0,  0,  0,  0,  0,  0}, {15,  0,  0}, LOAD,            0u },
  {100u, { 0,  0,  0,  0,  0,  5, 15,  0,  0,  0,  0,  0}, {10,  0,  0}, LOAD,            0u },
  {100u, { 0,  0,  0,  0,  0,  0, 10, 15,  0,  0,  0,  0}, { 5,  0,  0}, LOAD,            0u },
  {100u, { 0,  0,  0,  0,  0,  0,  5, 10, 15,  0,  0,  0}, { 0,  0,  0}, LOAD,                  0u },
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, RSHIFT | REPEAT, 4u },
};

//! \brief Star launch animation
CODE const S_ANIMATION_INSTRUCTION gasStarLaunch[ 39u ] = 
{
  {400u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0} },
  {200u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0} },
  {200u, {10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, { 0,  0,  0} },
  {200u, {15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10}, { 0,  0,  0} },
  {200u, {15,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0, 15}, { 0,  0,  0} },
  {200u, {15, 10,  0,  0,  0,  0,  0,  0,  0,  0,  5, 15}, { 0,  0,  0} },
  {200u, {15, 15,  0,  0,  0,  0,  0,  0,  0,  0, 10, 15}, { 0,  0,  0} },
  {200u, {15, 15,  5,  0,  0,  0,  0,  0,  0,  0, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 10,  0,  0,  0,  0,  0,  0,  5, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15,  0,  0,  0,  0,  0,  0, 10, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15,  5,  0,  0,  0,  0,  0, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 10,  0,  0,  0,  0,  5, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15,  0,  0,  0,  0, 10, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15,  5,  0,  0,  0, 15, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15, 10,  0,  0,  5, 15, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15, 15,  0,  0, 10, 15, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15, 15,  5,  0, 15, 15, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15, 15, 10,  5, 15, 15, 15, 15, 15}, { 0,  0,  0} },
  {200u, {15, 15, 15, 15, 15, 15, 10, 15, 15, 15, 15, 15}, { 0,  0,  0} },
  {400u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, {15, 15,  0} },
  {200u, {15, 15, 15, 15, 15, 15, 10, 15, 15, 15, 15, 15}, {15, 15,  0} },
  {200u, {15, 15, 15, 15, 15, 10,  5, 15, 15, 15, 15, 15}, {15, 15,  0} },
  {200u, {15, 15, 15, 15, 15,  5,  0, 15, 15, 15, 15, 15}, {15, 14,  0} },
  {200u, {15, 15, 15, 15, 15,  0,  0, 10, 15, 15, 15, 15}, {15, 13,  0} },
  {200u, {15, 15, 15, 15, 10,  0,  0,  5, 15, 15, 15, 15}, {15, 12,  0} },
  {200u, {15, 15, 15, 15,  5,  0,  0,  0, 15, 15, 15, 15}, {15, 11,  0} },
  {200u, {15, 15, 15, 15,  0,  0,  0,  0, 10, 15, 15, 15}, {15, 10,  0} },
  {200u, {15, 15, 15, 10,  0,  0,  0,  0,  5, 15, 15, 15}, {15,  9,  0} },
  {200u, {15, 15, 15,  5,  0,  0,  0,  0,  0, 15, 15, 15}, {15,  8,  0} },
  {200u, {15, 15, 15,  0,  0,  0,  0,  0,  0, 10, 15, 15}, {15,  7,  0} },
  {200u, {15, 15, 10,  0,  0,  0,  0,  0,  0,  5, 15, 15}, {15,  6,  0} },
  {200u, {15, 15,  5,  0,  0,  0,  0,  0,  0,  0, 15, 15}, {15,  5,  0} },
  {200u, {15, 15,  0,  0,  0,  0,  0,  0,  0,  0, 10, 15}, {12,  4,  0} },
  {200u, {15, 10,  0,  0,  0,  0,  0,  0,  0,  0,  5, 15}, { 9,  3,  0} },
  {200u, {15,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0, 15}, { 6,  2,  0} },
  {200u, {15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10}, { 3,  1,  0} },
  {200u, {10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, { 0,  0,  0} },
  {200u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0} },
};

//! \brief Generic flasher animation
CODE const S_ANIMATION_INSTRUCTION gasGenericFlasher[ 2u ] = 
{
  {500u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, { 7,  7,  7}, LOAD, 0u }, 
  {500u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD, 0u },
};

//! \brief KITT animation
CODE const S_ANIMATION_INSTRUCTION gasKITT[ 19u ] = 
{
  {200u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD,                  0u },
  {100u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, { 0,  0,  0}, LOAD,                  0u },
  {100u, {10,  5,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, { 0,  0,  0}, LOAD, 0u },
  {100u, {15, 10,  5,  0,  0,  0,  0,  0,  0,  5, 10, 15}, { 0,  0,  0}, LOAD, 0u },
  {100u, {10, 15, 10,  5,  0,  0,  0,  0,  5, 10, 15, 10}, { 0,  0,  0}, LOAD, 0u },
  {100u, { 5, 10, 15, 10,  5,  0,  0,  5, 10, 15, 10,  5}, { 0,  0,  0}, LOAD, 0u },
  {100u, { 0,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5,  0}, { 0,  0,  0}, LOAD, 0u },
  {100u, { 0,  0,  0,  0,  0,-15,-15,  0,  0,  0,  0,  0}, { 5,  0,  0}, ADD | USHIFT | REPEAT, 2u },
  {100u, { 0,  0,  0,  0,  0,-15,-15,  0,  0,  0,  0,  0}, {-5,  0,  0}, ADD | USHIFT | REPEAT, 2u },
  {100u, { 0,  0,  0,  0,  0,  5,  5,  0,  0,  0,  0,  0}, { 0,  0,  0} },
  {100u, { 0,  0,  0,  0,  5, 10, 10,  5,  0,  0,  0,  0}, { 0,  0,  0} },
  {100u, { 0,  0,  0,  5, 10, 15, 15, 10,  5,  0,  0,  0}, { 0,  0,  0} },
  {100u, { 0,  0,  5, 10, 15, 10, 10, 15, 10,  5,  0,  0}, { 0,  0,  0} },
  {100u, { 0,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5,  0}, { 0,  0,  0} },
  {100u, { 5, 10, 15, 10,  5,  0,  0,  5, 10, 15, 10,  5}, { 0,  0,  0} },
  {100u, {10, 15, 10,  5,  0,  0,  0,  0,  5, 10, 15, 10}, { 0,  0,  0} },
  {100u, {15, 10,  5,  0,  0,  0,  0,  0,  0,  5, 10, 15}, { 0,  0,  0} },
  {100u, {10,  5,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, { 0,  0,  0} },
  {100u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, { 0,  0,  0} },
  
  /*
  {200u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, { 0,  0,  0}, LOAD,  0u },
  {100u, {10,  5,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, { 0,  0,  0}, LOAD,  0u },
  {100u, {15, 10,  5,  0,  0,  0,  0,  0,  0,  5, 10, 15}, { 0,  0,  0}, LOAD,  0u },
  {100u, {10, 15, 10,  5,  0,  0,  0,  0,  5, 10, 15, 10}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 5, 10, 15, 10,  5,  0,  0,  5, 10, 15, 10,  5}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 0,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  5, 10, 15, 10, 10, 15, 10,  5,  0,  0}, { 5,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  5, 10, 15, 15, 10,  5,  0,  0,  0}, {10,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  5, 10, 10,  5,  0,  0,  0,  0}, {15,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  0,  5,  5,  0,  0,  0,  0,  0}, {10,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 5,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  0,  5,  5,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  5, 10, 10,  5,  0,  0,  0,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  5, 10, 15, 15, 10,  5,  0,  0,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  5, 10, 15, 10, 10, 15, 10,  5,  0,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 0,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5,  0}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 5, 10, 15, 10,  5,  0,  0,  5, 10, 15, 10,  5}, { 0,  0,  0}, LOAD,  0u },
  {100u, {10, 15, 10,  5,  0,  0,  0,  0,  5, 10, 15, 10}, { 0,  0,  0}, LOAD,  0u },
  {100u, {15, 10,  5,  0,  0,  0,  0,  0,  0,  5, 10, 15}, { 0,  0,  0}, LOAD,  0u },
  {100u, {10,  5,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, { 0,  0,  0}, LOAD,  0u },
  {100u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, { 0,  0,  0}, LOAD,  0u },
  */
};

//! \brief Disco animation
CODE const S_ANIMATION_INSTRUCTION gasDisco[ 12u ] = 
{
  {40u, {  0, 15,  0, 15,  0, 15,  0, 15,  0, 15,  0, 15}, {15,  0, 15}, LOAD, 0u },
  {40u, {  0,  7,  0,  7,  0,  7,  0,  7,  0,  7,  0,  7}, { 7,  0,  7}, LOAD, 0u },
  {40u, {  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4}, { 4,  0,  4}, LOAD, 0u },
  {40u, {  0,  2,  0,  2,  0,  2,  0,  2,  0,  2,  0,  2}, { 2,  0,  2}, LOAD, 0u },
  {40u, {  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1}, { 1,  0,  1}, LOAD, 0u },
  {100u,{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD, 0u },
  {40u, { 15,  0, 15,  0, 15,  0, 15,  0, 15,  0, 15,  0}, { 0, 15,  0}, LOAD, 0u },
  {40u, {  7,  0,  7,  0,  7,  0,  7,  0,  7,  0,  7,  0}, { 0,  7,  0}, LOAD, 0u },
  {40u, {  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0}, { 0,  4,  0}, LOAD, 0u },
  {40u, {  2,  0,  2,  0,  2,  0,  2,  0,  2,  0,  2,  0}, { 0,  2,  0}, LOAD, 0u },
  {40u, {  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0}, { 0,  1,  0}, LOAD, 0u },
  {100u,{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD, 0u },
};


//! \brief All blackness, reached right before going to power down mode
CODE const S_ANIMATION_INSTRUCTION gasBlackness[ 1u ] =
{
  {0xFFFFu, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, { 0,  0,  0}, LOAD, 0u },
};

//! \brief Table of animations
CODE const S_ANIMATION gasAnimations[ NUM_ANIMATIONS ] = 
{
  {sizeof(gasRetroVersion)/sizeof(S_ANIMATION_INSTRUCTION),        gasRetroVersion},
  {sizeof(gasSoftFlashing)/sizeof(S_ANIMATION_INSTRUCTION),        gasSoftFlashing},
  {sizeof(gasShootingStar)/sizeof(S_ANIMATION_INSTRUCTION),        gasShootingStar},
  {sizeof(gasStarLaunch)/sizeof(S_ANIMATION_INSTRUCTION),          gasStarLaunch},
  {sizeof(gasGenericFlasher)/sizeof(S_ANIMATION_INSTRUCTION),      gasGenericFlasher},
  {sizeof(gasKITT)/sizeof(S_ANIMATION_INSTRUCTION),                gasKITT},
  {sizeof(gasDisco)/sizeof(S_ANIMATION_INSTRUCTION),               gasDisco},
  // Last animation, don't change its location
  {sizeof(gasBlackness)/sizeof(S_ANIMATION_INSTRUCTION),           gasBlackness}
};


/***************************************< Global variables >**************************************/
//! \brief Ms resolution timer that is synchronized between paired devices
//! \note  Depending on the animation, its maximum value can be anything
IDATA U16 gu16SynchronizedTimer;
IDATA U16 gu16LastCall;       //!< The last time the main cycle was called
// Local variables
static IDATA U8 u8LastState = 0xFFu;          //!< Previously executed instruction index
static IDATA U8 u8RepetitionCounter = 0u;  //!< Instruction repetition counter


/***************************************< Static function definitions >**************************************/


/***************************************< Private functions >**************************************/


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initialize layer
//! \param  -
//! \return -
//! \global All globals in this layer.
//! \note   Should be called in the init block of the firmware.
//-----------------------------------------------------------------------------
void Animation_Init( void )
{
  gu16SynchronizedTimer = 0u;
  gu16LastCall = Util_GetTimerMs();
}

//----------------------------------------------------------------------------
//! \brief  Check timer and update LED brightnesses based on the animation.
//! \param  -
//! \return -
//! \global -
//! \note   Should be called from main cycle.
//-----------------------------------------------------------------------------
void Animation_Cycle( void )
{
  U8  u8AnimationState;
  U16 u16StateTimer = 0u;
  U16 u16TimeNow = Util_GetTimerMs();
  U8  u8Index;
  U8  u8Temp;
  
  // Check if time has elapsed since last call
  if( u16TimeNow != gu16LastCall )
  {
    // Increase the synchronized timer with the difference
    DISABLE_IT;
    gu16SynchronizedTimer += ( u16TimeNow - gu16LastCall );
    ENABLE_IT;

    // Make sure not to overindex arrays
    if( gsPersistentData.u8AnimationIndex >= NUM_ANIMATIONS )
    {
      gsPersistentData.u8AnimationIndex = 0u;
    }
    
    // Calculate the state of the animation
    for( u8AnimationState = 0u; u8AnimationState < gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLength; u8AnimationState++ )
    {
      u16StateTimer += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u16TimingMs;
      if( u16StateTimer > gu16SynchronizedTimer )
      {
        break;
      }
    }
    if( u8AnimationState >= gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLength )
    {
      // restart animation
      u8AnimationState = 0u;
      DISABLE_IT;
      gu16SynchronizedTimer = 0;
      ENABLE_IT;
    }
    if( u8LastState != u8AnimationState )  // next instruction
    {
      // Just a load instruction, nothing more
      if( LOAD == gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
      {
        memcpy( gau8LEDBrightness, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].au8LEDBrightness, LEDS_NUM );
        memcpy( gau8RGBLEDs, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].au8RGBLEDBrightness, NUM_RGBLED_COLORS );
        u8LastState = u8AnimationState;
      }
      else  // Other opcodes -- IMPORTANT: the order of operations are fixed!
      {
        // Add operation
        if( ADD & gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
        {
          for( u8Index = 0u; u8Index < LEDS_NUM; u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( gau8LEDBrightness[ u8Index ] > 15u )  // overflow/underflow happened
            {
              gau8LEDBrightness[ u8Index ] = 0u;
            }
          }
          for( u8Index = 0u; u8Index < NUM_RGBLED_COLORS; u8Index++ )
          {
            gau8RGBLEDs[ u8Index ] += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].au8RGBLEDBrightness[ u8Index ];
            if( gau8RGBLEDs[ u8Index ] > 15u )  // overflow/underflow happened
            {
              gau8RGBLEDs[ u8Index ] = 0u;
            }
          }
        }
        // Right shift operation
        if( RSHIFT & gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
        {
          u8Temp = gau8LEDBrightness[ LEDS_NUM - 1u ];
          for( u8Index = LEDS_NUM - 1u; u8Index > 0u; u8Index-- )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index - 1u ];
          }
          gau8LEDBrightness[ 0u ] = u8Temp;
        }
        // Left shift operation
        if( LSHIFT & gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
        {
          u8Temp = gau8LEDBrightness[ 0u ];
          for( u8Index = 0u; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index + 1u ];
          }
          gau8LEDBrightness[ LEDS_NUM - 1u ] = u8Temp;
        }
        // Upward shift operation
        if( USHIFT & gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
        {
          // Left side
          u8Temp = gau8LEDBrightness[ RIGHT_LEDS_START - 1u ];
          for( u8Index = RIGHT_LEDS_START - 1u; u8Index > 0u; u8Index-- )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index - 1u ];
          }
          gau8LEDBrightness[ 0u ] = u8Temp;
          // Right side
          u8Temp = gau8LEDBrightness[ RIGHT_LEDS_START ];
          for( u8Index = RIGHT_LEDS_START; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index + 1u ];
          }
          gau8LEDBrightness[ LEDS_NUM - 1u ] = u8Temp;
        }
        // Downward shift operation
        if( DSHIFT & gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
        {
        #warning TODO
        }
        // Repeat instruction
        if( REPEAT & gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOpcode )
        {
          // If we're here the first time
          if( 0u == u8RepetitionCounter )
          {
            u8RepetitionCounter = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u8AnimationOperand;
            // Step back in time
            gu16SynchronizedTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u16TimingMs;
          }
          else  // We're already repeating...
          {
            u8RepetitionCounter--;
            if( 0u != u8RepetitionCounter )
            {
              // Step back in time
              gu16SynchronizedTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].u16TimingMs;
            }
            else  // No more repeating
            {
             u8LastState = u8AnimationState;
            }
          }
        }
        else  // if there's no repeat opcode
        {
          u8LastState = u8AnimationState;  // save that this operation is finished
        }
      }
    }
    // Store the timestamp
    gu16LastCall = u16TimeNow;
  }
}

//----------------------------------------------------------------------------
//! \brief  Set the new animation
//! \param  -
//! \return -
//! \global -
//! \note   Should be called from main cycle only!
//-----------------------------------------------------------------------------
void Animation_Set( U8 u8AnimationIndex )
{
  if( u8AnimationIndex < NUM_ANIMATIONS )
  {
    gsPersistentData.u8AnimationIndex = u8AnimationIndex;
    DISABLE_IT;
    gu16SynchronizedTimer = 0u;
    ENABLE_IT;
    u8LastState = 0xFFu;
    u8RepetitionCounter = 0u;
  }
}


/***************************************< End of file >**************************************/
