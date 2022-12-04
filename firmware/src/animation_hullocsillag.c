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
//  UMOVE     = 0x04u,  //!< Moves some of the values upwards. Uses saturation logic. Doesn't roll over.
//  DMOVE     = 0x08u,  //!< Moves some of the values downwards. Uses saturation logic. Doesn't roll over.
  DIV       = 0x10u,  //!< Divides the the current LED brightness levels by the given number
  USOURCE   = 0x20u,  //!< Add values to the brightness and if it overflows/underflows then it will be added to the upwards next value. If it overflows/underflows then it will do the same until it reaches the uppper or lower end.
  DSOURCE   = 0x40u,  //!< Add values to the brightness and if it overflows/underflows then it will be added to the downwards next value. If it overflows/underflows then it will do the same until it reaches the uppper or lower end.
  REPEAT    = 0x80u   //!< Do the instruction and repeat by (operand)-times
} E_ANIMATION_OPCODE;

//! \brief Instruction used by the animation state machine -- for normal LEDs
typedef struct
{
  U16 u16TimingMs;                               //!< How long the machine should stay in this state
  U8  au8LEDBrightness[ LEDS_NUM ];              //!< Brightness of each LED
  U8  u8AnimationOpcode;                         //!< Opcode (E_ANIMATION_OPCODE)
  U8  u8AnimationOperand;                        //!< Opcode-specific operand
} S_ANIMATION_INSTRUCTION_NORMAL;

//! \brief Instruction used by the animation state machine -- for the RGB LED
typedef struct
{
  U16 u16TimingMs;                               //!< How long the machine should stay in this state
  U8  au8RGBLEDBrightness[ NUM_RGBLED_COLORS ];  //!< Brightness of each color
  U8  u8AnimationOpcode;                         //!< Opcode (E_ANIMATION_OPCODE)
  U8  u8AnimationOperand;                        //!< Opcode-specific operand
} S_ANIMATION_INSTRUCTION_RGB;

//! \brief Animation structure
typedef struct
{
  U8                                         u8AnimationLengthNormal;  //!< How many instructions this animation has for the normal LEDs
  const S_ANIMATION_INSTRUCTION_NORMAL CODE* psInstructionsNormal;     //!< Pointer to the instructions themselves -- normal LEDs
  U8                                         u8AnimationLengthRGB;     //!< How many instructions this animation has for the RGB LED
  const S_ANIMATION_INSTRUCTION_RGB CODE*    psInstructionsRGB;        //!< Pointer to the instructions themselves -- RGB LED
} S_ANIMATION;


/***************************************< Constants >**************************************/
//--------------------------------------------------------
//! \brief "Sine" wave flasher animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasSoftFlashing[ 4u ] = 
{
  {125u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,          0u },
  {125u, { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1}, ADD | REPEAT, 14u },
  {125u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, LOAD,          0u }, 
  {125u, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, ADD | REPEAT, 14u },
};

//--------------------------------------------------------
//! \brief "Fade ring" animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasFadeRing[ 3u ] =
{
  { 40u, {15,  1, 15,  1, 15,  1,  1, 15,  1, 15,  1, 15}, LOAD,          0u },
  { 40u, {-1,  1, -1,  1, -1,  1,  1, -1,  1, -1,  1, -1}, ADD | REPEAT, 13u },
  { 40u, { 1, -1,  1, -1,  1, -1, -1,  1, -1,  1, -1,  1}, ADD | REPEAT, 13u },
};

//--------------------------------------------------------
//! \brief Shooting star anticlockwise animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasShootingStar[ 4u ] = 
{ 
  {100u, { 5, 10, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,            0u },
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, RSHIFT | REPEAT, 8u },
  {100u, {15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, LOAD,            0u },
  {100u, {10, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, LOAD,            0u },
};

//--------------------------------------------------------
//! \brief Generic flasher animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasGenericFlasher[ 2u ] = 
{
  {500u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, LOAD, 0u }, 
  {500u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD, 0u },
};

//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasKITT[ 22u ] = 
{
  {200u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,  0u },
  {100u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, LOAD,  0u },
  {100u, {10,  5,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, LOAD,  0u },
  {100u, {15, 10,  5,  0,  0,  0,  0,  0,  0,  5, 10, 15}, LOAD,  0u },
  {100u, {10, 15, 10,  5,  0,  0,  0,  0,  5, 10, 15, 10}, LOAD,  0u },
  {100u, { 5, 10, 15, 10,  5,  0,  0,  5, 10, 15, 10,  5}, LOAD,  0u },
  {100u, { 0,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5,  0}, LOAD,  0u },
  {100u, { 0,  0,  5, 10, 15, 10, 10, 15, 10,  5,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  5, 10, 15, 15, 10,  5,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  5, 10, 10,  5,  0,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  0,  5,  5,  0,  0,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  0,  5,  5,  0,  0,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  0,  5, 10, 10,  5,  0,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  0,  5, 10, 15, 15, 10,  5,  0,  0,  0}, LOAD,  0u },
  {100u, { 0,  0,  5, 10, 15, 10, 10, 15, 10,  5,  0,  0}, LOAD,  0u },
  {100u, { 0,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5,  0}, LOAD,  0u },
  {100u, { 5, 10, 15, 10,  5,  0,  0,  5, 10, 15, 10,  5}, LOAD,  0u },
  {100u, {10, 15, 10,  5,  0,  0,  0,  0,  5, 10, 15, 10}, LOAD,  0u },
  {100u, {15, 10,  5,  0,  0,  0,  0,  0,  0,  5, 10, 15}, LOAD,  0u },
  {100u, {10,  5,  0,  0,  0,  0,  0,  0,  0,  0,  5, 10}, LOAD,  0u },
  {100u, { 5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5}, LOAD,  0u },
};

//--------------------------------------------------------
//! \brief Disco animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasDisco[ 6u ] = 
{
  {40u, {  0, 15,  0, 15,  0, 15,  0, 15,  0, 15,  0, 15}, LOAD,         0u },
  {40u, {  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2}, DIV | REPEAT, 3u },
  {100u,{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,         0u },
  {40u, { 15,  0, 15,  0, 15,  0, 15,  0, 15,  0, 15,  0}, LOAD,         0u },
  {40u, {  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1}, DIV | REPEAT, 3u },
  {100u,{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,         0u },
};

//--------------------------------------------------------
//! \brief Pseudo-random fade animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasPseudoRandomFade[ 15u ] = 
{
  { 66u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD,  0u },
  { 66u, { 0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  1,  0,  0,  0,  0, -1,  0,  0,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0, -1,  0,  0,  0,  0,  0,  0,  0,  1,  0}, ADD | REPEAT, 14u },
  { 66u, { 1,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,  0}, ADD | REPEAT, 14u },
  { 66u, {-1,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  0,  0,  0, -1,  0,  1,  0,  0,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  0,  0,  0,  0,  0, -1,  0,  0,  0,  1}, ADD | REPEAT, 14u },
  { 66u, { 0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1}, ADD | REPEAT, 14u },
  { 66u, { 0, -1,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  0, -1,  0,  0,  1,  0,  0,  0,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  0,  0,  0,  0, -1,  0,  0,  0,  0,  0}, ADD | REPEAT, 14u },  //RGB lights up here
  { 66u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  0,  0,  1,  0,  0,  0,  0, -1,  0,  0}, ADD | REPEAT, 14u },
  { 66u, { 0,  0,  0,  0, -1,  0,  0,  0,  0,  0,  0,  0}, ADD | REPEAT, 14u },
};

//--------------------------------------------------------
//! \brief All blackness, reached right before going to power down mode -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasBlackness[ 1u ] =
{
  {0xFFFFu, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, LOAD, 0u },
};
//! \brief All blackness, reached right before going to power down mode -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasBlacknessRGB[ 1u ] =
{
  {0xFFFFu, { 0,  0,  0}, LOAD, 0u },
};

// *******************************************************
//! \brief Table of animations
CODE const S_ANIMATION gasAnimations[ NUM_ANIMATIONS ] = 
{
  {sizeof(gasSoftFlashing)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),     gasSoftFlashing,     sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  {sizeof(gasShootingStar)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),     gasShootingStar,     sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  {sizeof(gasGenericFlasher)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),   gasGenericFlasher,   sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  {sizeof(gasKITT)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasKITT,             sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  {sizeof(gasDisco)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),            gasDisco,            sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  {sizeof(gasFadeRing)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),         gasFadeRing,         sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  {sizeof(gasPseudoRandomFade)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL), gasPseudoRandomFade, sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness },
  // Last animation, don't change its location
  {sizeof(gasBlackness)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),        gasBlackness,        sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),  gasBlackness }
};


/***************************************< Global variables >**************************************/
IDATA U16 gu16NormalTimer;                    //!< Ms resolution timer for normal LED animation
IDATA U16 gu16RGBTimer;                       //!< Ms resolution timer for the RGB LED animation
IDATA U16 gu16LastCall;                       //!< The last time the main cycle was called
// Local variables
static IDATA U8 u8LastState = 0xFFu;          //!< Previously executed instruction index for normal LEDs
static IDATA U8 u8RepetitionCounter = 0u;     //!< Instruction repetition counter for normal LEDs
static IDATA U8 u8LastStateRGB = 0xFFu;       //!< Previously executed instruction index for RGB LED
static IDATA U8 u8RepetitionCounterRGB = 0u;  //!< Instruction repetition counter for RGB LED


/***************************************< Static function definitions >**************************************/
static I8 SaturateBrightness( U8* pu8BrightnessVariable );


/***************************************< Private functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Saturates the given brightness variable
//! \param  *pu8BrightnessVariable: pointer to the variable
//! \return How much the variable needed to change
//! \global -
//! \note   Changes the parameter too.
//-----------------------------------------------------------------------------
static I8 SaturateBrightness( U8* pu8BrightnessVariable )
{
  I8 i8Return = 0;
  if( (I8)*pu8BrightnessVariable < 0 )
  {
    i8Return = (I8)*pu8BrightnessVariable;
    *pu8BrightnessVariable = 0u;
  }
  else if( (I8)*pu8BrightnessVariable > 15u )
  {
    i8Return = (I8)*pu8BrightnessVariable - 15;
    *pu8BrightnessVariable = 15u;
  }
  return i8Return;
}


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
  gu16NormalTimer = 0u;
  gu16RGBTimer = 0u;
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
  U8  u8Index, u8InnerIndex;
  U8  u8OpCode;
  U8  u8Temp;
  I8  i8Change;
  
  // Check if time has elapsed since last call
  if( u16TimeNow != gu16LastCall )
  {
    // Increase the synchronized timer with the difference
    DISABLE_IT;
    gu16NormalTimer += ( u16TimeNow - gu16LastCall );
    gu16RGBTimer += ( u16TimeNow - gu16LastCall );
    ENABLE_IT;

    // Make sure not to overindex arrays
    if( gsPersistentData.u8AnimationIndex >= NUM_ANIMATIONS )
    {
      gsPersistentData.u8AnimationIndex = 0u;
    }
    
    // --------------------------------------< For the normal LEDs
    // Calculate the state of the animation
    for( u8AnimationState = 0u; u8AnimationState < gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthNormal; u8AnimationState++ )
    {
      u16StateTimer += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u16TimingMs;
      if( u16StateTimer > gu16NormalTimer )
      {
        break;
      }
    }
    if( u8AnimationState >= gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthNormal )
    {
      // restart animation
      u8AnimationState = 0u;
      DISABLE_IT;
      gu16NormalTimer = 0u;
      gu16RGBTimer = 0u;
      ENABLE_IT;
    }
    if( u8LastState != u8AnimationState )  // next instruction
    {
      u8OpCode = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u8AnimationOpcode;
      // Just a load instruction, nothing more
      if( LOAD == u8OpCode )
      {
        memcpy( gau8LEDBrightness, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness, LEDS_NUM );
        u8LastState = u8AnimationState;
      }
      else  // Other opcodes -- IMPORTANT: the order of operations are fixed!
      {
        // Add operation
        if( ADD & u8OpCode )
        {
          for( u8Index = 0u; u8Index < LEDS_NUM; u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( gau8LEDBrightness[ u8Index ] > 15u )  // overflow/underflow happened
            {
              gau8LEDBrightness[ u8Index ] = 0u;
            }
          }
        }
        // Right shift operation
        if( RSHIFT & u8OpCode )
        {
          u8Temp = gau8LEDBrightness[ LEDS_NUM - 1u ];
          for( u8Index = LEDS_NUM - 1u; u8Index > 0u; u8Index-- )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index - 1u ];
          }
          gau8LEDBrightness[ 0u ] = u8Temp;
        }
        // Left shift operation
        if( LSHIFT & u8OpCode )
        {
          u8Temp = gau8LEDBrightness[ 0u ];
          for( u8Index = 0u; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index + 1u ];
          }
          gau8LEDBrightness[ LEDS_NUM - 1u ] = u8Temp;
        }
/*
        // Upward move operation
        if( UMOVE & u8OpCode )
        {
          // Left side
          for( u8Index = 0u; u8Index < (RIGHT_LEDS_START - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] -= i8Change;
            for( u8InnerIndex = u8Index; u8InnerIndex < (RIGHT_LEDS_START - 1u); u8InnerIndex++ )
            {
              i8Change += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
              gau8LEDBrightness[ u8InnerIndex + 1u ] += i8Change;
              i8Change = SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex + 1u ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START - 1u ];
          gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] );
          // Right side
          for( u8Index = LEDS_NUM - 1u; u8Index > RIGHT_LEDS_START; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( (I8)gau8LEDBrightness[ u8Index ] - i8Change < 0u )  // saturation downwards
            {
              gau8LEDBrightness[ u8Index - 1u ] += gau8LEDBrightness[ u8Index ];
            }
            else  // no saturation
            {
              gau8LEDBrightness[ u8Index - 1u ] += i8Change;
            }
            gau8LEDBrightness[ u8Index ] -= i8Change;
            SaturateBrightness( &gau8LEDBrightness[ u8Index ] );
            SaturateBrightness( &gau8LEDBrightness[ u8Index - 1u ] );  // saturate the next LED too
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START ];
          gau8LEDBrightness[ RIGHT_LEDS_START ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START ] );
        }
        // Downward move operation
        if( DMOVE & u8OpCode )
        {
          //TODO: this works for positive move values only!
          // Left side
          for( u8Index = (RIGHT_LEDS_START - 1u); u8Index > 0u ; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( (I8)gau8LEDBrightness[ u8Index ] - i8Change < 0u )  // saturation downwards
            {
              gau8LEDBrightness[ u8Index - 1u ] += gau8LEDBrightness[ u8Index ];
            }
            else  // no saturation
            {
              gau8LEDBrightness[ u8Index - 1u ] += i8Change;
            }
            gau8LEDBrightness[ u8Index ] -= i8Change;
            SaturateBrightness( &gau8LEDBrightness[ u8Index ] );
            SaturateBrightness( &gau8LEDBrightness[ u8Index - 1u ] );  // saturate the next LED too
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ 0u ];
          gau8LEDBrightness[ 0u ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ 0u ] );
          // Right side
          for( u8Index = RIGHT_LEDS_START; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( (I8)gau8LEDBrightness[ u8Index ] - i8Change < 0u )  // saturation downwards
            {
              gau8LEDBrightness[ u8Index + 1u ] += gau8LEDBrightness[ u8Index ];
            }
            else  // no saturation
            {
              gau8LEDBrightness[ u8Index + 1u ] += i8Change;
            }
            gau8LEDBrightness[ u8Index ] -= i8Change;
            SaturateBrightness( &gau8LEDBrightness[ u8Index ] );
            SaturateBrightness( &gau8LEDBrightness[ u8Index + 1u ] );  // saturate the next LED too
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ LEDS_NUM - 1u ];
          gau8LEDBrightness[ LEDS_NUM - 1u ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ LEDS_NUM - 1u ] );
        }
*/
        // Upward source instruction
        if( USOURCE & u8OpCode )
        {
          // Left side
          for( u8Index = 0u; u8Index < (RIGHT_LEDS_START - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = u8Index; u8InnerIndex < (RIGHT_LEDS_START - 1u); u8InnerIndex++ )
            {
              gau8LEDBrightness[ u8InnerIndex + 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START - 1u ];
          gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] );
          // Right side
          for( u8Index = LEDS_NUM - 1u; u8Index > RIGHT_LEDS_START; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = LEDS_NUM - 1u; u8InnerIndex > RIGHT_LEDS_START; u8InnerIndex-- )
            {
              gau8LEDBrightness[ u8InnerIndex - 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START ];
          gau8LEDBrightness[ RIGHT_LEDS_START ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START ] );
        }
        // Downward source instruction
        if( DSOURCE & u8OpCode )
        {
          // Left side
          for( u8Index = (RIGHT_LEDS_START - 1u); u8Index > 0u; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = u8Index; u8InnerIndex > 0u; u8InnerIndex-- )
            {
              gau8LEDBrightness[ u8InnerIndex - 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ 0u ];
          gau8LEDBrightness[ 0u ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ 0u ] );
          // Right side
          for( u8Index = RIGHT_LEDS_START; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = RIGHT_LEDS_START; u8InnerIndex < (LEDS_NUM - 1u); u8InnerIndex++ )
            {
              gau8LEDBrightness[ u8InnerIndex + 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ LEDS_NUM - 1u ];
          gau8LEDBrightness[ LEDS_NUM - 1u ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ LEDS_NUM - 1u ] );
        }
        // Divide instruction
        if( DIV & u8OpCode )
        {
          for( u8Index = 0u; u8Index < LEDS_NUM; u8Index++ )
          {
            u8Temp = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( u8Temp != 0u )
            {
              gau8LEDBrightness[ u8Index ] /= u8Temp;
            }
          }
        }
        // Repeat instruction
        if( REPEAT & u8OpCode )
        {
          // If we're here the first time
          if( 0u == u8RepetitionCounter )
          {
            u8RepetitionCounter = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u8AnimationOperand;
            // Step back in time
            gu16NormalTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u16TimingMs;
          }
          else  // We're already repeating...
          {
            u8RepetitionCounter--;
            if( 0u != u8RepetitionCounter )
            {
              // Step back in time
              gu16NormalTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u16TimingMs;
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
    
    // --------------------------------------< For the RGB LED
    // Calculate the state of the animation
    u16StateTimer = 0u;
    for( u8AnimationState = 0u; u8AnimationState < gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthRGB; u8AnimationState++ )
    {
      u16StateTimer += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u16TimingMs;
      if( u16StateTimer > gu16RGBTimer )
      {
        break;
      }
    }
/*
    if( u8AnimationState >= gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthRGB )
    {
      // restart animation
      u8AnimationState = 0u;
      DISABLE_IT;
      gu16SynchronizedTimer = 0;
      ENABLE_IT;
    }
*/
    if( u8LastStateRGB != u8AnimationState )  // next instruction
    {
      u8OpCode = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u8AnimationOpcode;
      // Just a load instruction, nothing more
      if( LOAD == u8OpCode )
      {
        memcpy( gau8RGBLEDs, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].au8RGBLEDBrightness, NUM_RGBLED_COLORS );
        u8LastStateRGB = u8AnimationState;
      }
      else  // Other opcodes -- IMPORTANT: the order of operations are fixed!
      {
        // Add operation
        if( ADD & u8OpCode )
        {
          for( u8Index = 0u; u8Index < NUM_RGBLED_COLORS; u8Index++ )
          {
            gau8RGBLEDs[ u8Index ] += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].au8RGBLEDBrightness[ u8Index ];
            if( gau8RGBLEDs[ u8Index ] > 15u )  // overflow/underflow happened
            {
              gau8RGBLEDs[ u8Index ] = 0u;
            }
          }
        }
        // Right shift operation
        if( RSHIFT & u8OpCode )
        {
          // Not implemented
        }
        // Left shift operation
        if( LSHIFT & u8OpCode )
        {
          // Not implemented
        }
/*
        // Upward move operation
        if( UMOVE & u8OpCode )
        {
          // Not implemented
        }
        // Downward move operation
        if( DMOVE & u8OpCode )
        {
          // Not implemented
        }
*/
        if( USOURCE & u8OpCode )
        {
          // Not implemented
        }
        if( DSOURCE & u8OpCode )
        {
          // Not implemented
        }
        if( DIV & u8OpCode )
        {
          for( u8Index = 0u; u8Index < NUM_RGBLED_COLORS; u8Index++ )
          {
            u8Temp = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].au8RGBLEDBrightness[ u8Index ];
            if( u8Temp != 0u )
            {
              gau8RGBLEDs[ u8Index ] /= u8Temp;
            }
          }
        }
        // Repeat instruction
        if( REPEAT & u8OpCode )
        {
          // If we're here the first time
          if( 0u == u8RepetitionCounterRGB )
          {
            u8RepetitionCounterRGB = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u8AnimationOperand;
            // Step back in time
            gu16RGBTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u16TimingMs;
          }
          else  // We're already repeating...
          {
            u8RepetitionCounterRGB--;
            if( 0u != u8RepetitionCounterRGB )
            {
              // Step back in time
              gu16RGBTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u16TimingMs;
            }
            else  // No more repeating
            {
             u8LastStateRGB = u8AnimationState;
            }
          }
        }
        else  // if there's no repeat opcode
        {
          u8LastStateRGB = u8AnimationState;  // save that this operation is finished
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
    gu16NormalTimer = 0u;
    gu16RGBTimer = 0u;
    ENABLE_IT;
    u8LastState = 0xFFu;
    u8RepetitionCounter = 0u;
    u8LastStateRGB = 0xFFu;
    u8RepetitionCounterRGB = 0u;
  }
}


/***************************************< End of file >**************************************/
