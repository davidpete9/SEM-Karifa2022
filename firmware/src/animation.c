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

/***************************************< Includes >**************************************/
// Standard C libraries
#include <string.h>

// Own includes
#include "types.h"
#include "led.h"
#include "util.h"
#include "animation.h"
#include "persist.h"


/***************************************< Definitions >**************************************/


/***************************************< Types >**************************************/
//! \brief Instruction used by the animation state machine
typedef struct
{
  U16 u16TimingMs;                   //!< How long the machine should stay in this state
  U8  au8LEDBrightness[ LEDS_NUM ];  //!< Brightness of each LEDs
} S_ANIMATION_INSTRUCTION;

//! \brief Animation structure
typedef struct
{
  U8                                  u8AnimationLength;  //!< How many instructions this animation has
  const S_ANIMATION_INSTRUCTION CODE* psInstructions;     //!< Pointer to the instructions themselves
} S_ANIMATION;


/***************************************< Constants >**************************************/
//! \brief Heartbeat animation
CODE const S_ANIMATION_INSTRUCTION gasHeartBeat[ 4u ] = 
{ 
  {150u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15} }, 
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {150u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15} }, 
  {450u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief Shooting star anticlockwise animation
CODE const S_ANIMATION_INSTRUCTION gasShootingStar[ 10u ] = 
{ 
  {100u, {15,  0,  0,  0,  0,  0,  0,  0,  5, 10} }, 
  {100u, {10, 15,  0,  0,  0,  0,  0,  0,  0,  5} },
  {100u, { 5, 10, 15,  0,  0,  0,  0,  0,  0,  0} },
  {100u, { 0,  5, 10, 15,  0,  0,  0,  0,  0,  0} },
  {100u, { 0,  0,  5, 10, 15,  0,  0,  0,  0,  0} },
  {100u, { 0,  0,  0,  5, 10, 15,  0,  0,  0,  0} },
  {100u, { 0,  0,  0,  0,  5, 10, 15,  0,  0,  0} },
  {100u, { 0,  0,  0,  0,  0,  5, 10, 15,  0,  0} },
  {100u, { 0,  0,  0,  0,  0,  0,  5, 10, 15,  0} },
  {100u, { 0,  0,  0,  0,  0,  0,  0,  5, 10, 15} },
};

//! \brief Generic flasher animation
CODE const S_ANIMATION_INSTRUCTION gasGenericFlasher[ 2u ] = 
{
  {500u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15} }, 
  {500u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief Shooting stars chasing animation
CODE const S_ANIMATION_INSTRUCTION gasShootingStarChasing[ 10u ] = 
{
  {100u, {15,  0,  0,  5, 10, 15,  0,  0,  5, 10} }, 
  {100u, {10, 15,  0,  0,  5, 10, 15,  0,  0,  5} },
  {100u, { 5, 10, 15,  0,  0,  5, 10, 15,  0,  0} },
  {100u, { 0,  5, 10, 15,  0,  0,  5, 10, 15,  0} },
  {100u, { 0,  0,  5, 10, 15,  0,  0,  5, 10, 15} },
  {100u, {15,  0,  0,  5, 10, 15,  0,  0,  5, 10} },
  {100u, {10, 15,  0,  0,  5, 10, 15,  0,  0,  5} },
  {100u, { 5, 10, 15,  0,  0,  5, 10, 15,  0,  0} },
  {100u, { 0,  5, 10, 15,  0,  0,  5, 10, 15,  0} },
  {100u, { 0,  0,  5, 10, 15,  0,  0,  5, 10, 15} },
};

//! \brief Shooting stars both direction animation
CODE const S_ANIMATION_INSTRUCTION gasShootingStarBoth[ 10u ] = 
{
  {100u, {15,  0,  0, 15, 10,  5,  0,  0,  5, 10} }, 
  {100u, {10, 15, 15, 10,  5,  0,  0,  0,  0,  5} },
  {100u, { 5, 15, 15,  5,  0,  0,  0,  0,  0,  0} },
  {100u, {15, 10, 10, 15,  0,  0,  0,  0,  0,  0} },
  {100u, {10,  5,  5, 10, 15,  0,  0,  0,  0, 15} },
  {100u, { 5,  0,  0,  5, 10, 15,  0,  0, 15, 10} },
  {100u, { 0,  0,  0,  0,  5, 10, 15, 15, 10,  5} },
  {100u, { 0,  0,  0,  0,  0,  5, 15, 15,  5,  0} },
  {100u, { 0,  0,  0,  0,  0, 15, 10, 10, 15,  0} },
  {100u, { 0,  0,  0,  0, 15, 10,  5,  5, 10, 15} },
};

//! \brief "Sine" wave flasher animation
CODE const S_ANIMATION_INSTRUCTION gasSoftFlashing[ 30u ] = 
{
  {100u, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  { 50u, { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1} },
  { 50u, { 2,  2,  2,  2,  2,  2,  2,  2,  2,  2} },
  { 50u, { 3,  3,  3,  3,  3,  3,  3,  3,  3,  3} },
  { 50u, { 4,  4,  4,  4,  4,  4,  4,  4,  4,  4} },
  { 50u, { 5,  5,  5,  5,  5,  5,  5,  5,  5,  5} },
  { 50u, { 6,  6,  6,  6,  6,  6,  6,  6,  6,  6} },
  { 50u, { 7,  7,  7,  7,  7,  7,  7,  7,  7,  7} },
  { 50u, { 8,  8,  8,  8,  8,  8,  8,  8,  8,  8} },
  { 50u, { 9,  9,  9,  9,  9,  9,  9,  9,  9,  9} },
  { 50u, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10} },
  { 50u, {11, 11, 11, 11, 11, 11, 11, 11, 11, 11} },
  { 50u, {12, 12, 12, 12, 12, 12, 12, 12, 12, 12} },
  { 50u, {13, 13, 13, 13, 13, 13, 13, 13, 13, 13} },
  { 50u, {14, 14, 14, 14, 14, 14, 14, 14, 14, 14} },
  {100u, {15, 15, 15, 15, 15, 15, 15, 15, 15, 15} }, 
  { 50u, {14, 14, 14, 14, 14, 14, 14, 14, 14, 14} },
  { 50u, {13, 13, 13, 13, 13, 13, 13, 13, 13, 13} },
  { 50u, {12, 12, 12, 12, 12, 12, 12, 12, 12, 12} },
  { 50u, {11, 11, 11, 11, 11, 11, 11, 11, 11, 11} },
  { 50u, {10, 10, 10, 10, 10, 10, 10, 10, 10, 10} },
  { 50u, { 9,  9,  9,  9,  9,  9,  9,  9,  9,  9} },
  { 50u, { 8,  8,  8,  8,  8,  8,  8,  8,  8,  8} },
  { 50u, { 7,  7,  7,  7,  7,  7,  7,  7,  7,  7} },
  { 50u, { 6,  6,  6,  6,  6,  6,  6,  6,  6,  6} },
  { 50u, { 5,  5,  5,  5,  5,  5,  5,  5,  5,  5} },
  { 50u, { 4,  4,  4,  4,  4,  4,  4,  4,  4,  4} },
  { 50u, { 3,  3,  3,  3,  3,  3,  3,  3,  3,  3} },
  { 50u, { 2,  2,  2,  2,  2,  2,  2,  2,  2,  2} },
  { 50u, { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1} },
};

//! \brief Butterfly animation
CODE const S_ANIMATION_INSTRUCTION gasButterfly[ 6u ] = 
{
  {150u, {  0,  0,  0,  0,  0,  0,  0,  0,  0, 15} },
  {150u, { 15,  0,  0,  0,  0,  0,  0,  0, 15, 15} },
  {150u, { 15, 15,  0,  0,  0,  0,  0, 15, 15, 15} },
  {150u, { 15, 15, 15,  0,  0,  0, 15, 15, 15, 15} },
  {150u, { 15, 15, 15, 15,  0, 15, 15, 15, 15, 15} },
  {150u, { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15} },
};

//! \brief Chase 2 fading animation
CODE const S_ANIMATION_INSTRUCTION gasChase2Fading[ 5u ] = 
{
  {150u, {  0,  0,  2,  6, 15,  0,  0,  2,  6, 15} },
  {150u, { 15,  0,  0,  2,  6, 15,  0,  0,  2,  6} },
  {150u, {  6, 15,  0,  0,  2,  6, 15,  0,  0,  2} },
  {150u, {  2,  6, 15,  0,  0,  2,  6, 15,  0,  0} },
  {150u, {  0,  2,  6, 15,  0,  0,  2,  6, 15,  0} },
};

//! \brief Fading 2 section animation
CODE const S_ANIMATION_INSTRUCTION gasFading2Section[ 12u ] = 
{
  {40u, {  0, 15,  0, 15,  0, 15,  0, 15,  0, 15} },
  {40u, {  0,  7,  0,  7,  0,  7,  0,  7,  0,  7} },
  {40u, {  0,  4,  0,  4,  0,  4,  0,  4,  0,  4} },
  {40u, {  0,  2,  0,  2,  0,  2,  0,  2,  0,  2} },
  {40u, {  0,  1,  0,  1,  0,  1,  0,  1,  0,  1} },
  {100u,{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {40u, { 15,  0, 15,  0, 15,  0, 15,  0, 15,  0} },
  {40u, {  7,  0,  7,  0,  7,  0,  7,  0,  7,  0} },
  {40u, {  4,  0,  4,  0,  4,  0,  4,  0,  4,  0} },
  {40u, {  2,  0,  2,  0,  2,  0,  2,  0,  2,  0} },
  {40u, {  1,  0,  1,  0,  1,  0,  1,  0,  1,  0} },
  {100u,{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief Heartbeat fading animation
CODE const S_ANIMATION_INSTRUCTION gasHeartbeatFading[ 8u ] = 
{
  {150u, { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15} },
  {100u, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {100u, { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15} },
  {33u, {  7,  7,  7,  7,  7,  7,  7,  7,  7,  7} },
  {33u, {  4,  4,  4,  4,  4,  4,  4,  4,  4,  4} },
  {33u, {  2,  2,  2,  2,  2,  2,  2,  2,  2,  2} },
  {33u, {  1,  1,  1,  1,  1,  1,  1,  1,  1,  1} },
  {300u, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief KITT animation
CODE const S_ANIMATION_INSTRUCTION gasKITT[ 12u ] = 
{
  {100u, {  0, 15,  0,  0,  0,  0,  0,  0,  0,  0} },
  {100u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
  {100u, { 15,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {100u, {  0,  0,  0,  0, 15,  0,  0,  0,  0, 15} },
  {100u, {  0,  0,  0,  0,  0, 15,  0,  0, 15,  0} },
  {100u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {100u, {  0,  0,  0,  0,  0,  0,  0, 15,  0,  0} },
  {100u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {100u, {  0,  0,  0,  0,  0, 15,  0,  0, 15,  0} },
  {100u, {  0,  0,  0,  0, 15,  0,  0,  0,  0, 15} },
  {100u, { 15,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {100u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief Level Up animation
CODE const S_ANIMATION_INSTRUCTION gasLevelUp[ 12u ] = 
{
  {160u, {  0,  0,  0,  0, 15,  0,  0,  0,  0,  0} },
  {140u, {  0,  0,  0, 15,  0, 15,  0,  0,  0,  0} },
  {120u, {  0,  0, 15,  0,  0,  0, 15,  0,  0,  0} },
  {100u, {  0, 15,  0,  0,  0,  0,  0, 15,  0, 15} },
  {80u, { 15,  0,  0,  0,  0,  0,  0,  0, 15,  0} },
  {20u, { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15} },
  {80u, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {20u, { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15} },
  {80u, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {20u, { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15} },
  {200u, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief Ping-pong animation
CODE const S_ANIMATION_INSTRUCTION gasPingPong[ 20u ] = 
{
  {60u, {  0,  0,  0,  0,  0,  0,  0,  0,  0, 15} },
  {60u, {  0,  0,  0,  0,  0,  0,  0,  0, 15,  0} },
  {60u, {  0,  0,  0,  0,  0,  0,  0, 15,  0,  0} },
  {60u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {60u, {  0,  0,  0,  0,  0, 15,  0,  0,  0,  0} },
  {60u, {  0,  0,  0,  0, 15,  0,  0,  0,  0,  0} },
  {60u, {  0,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {60u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
  {60u, {  0, 15,  0,  0,  0,  0,  0,  0,  0,  0} },
  {60u, { 15,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {60u, {  0,  0,  0,  0,  0,  0,  0,  0,  0, 15} },
  {60u, { 15,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {60u, {  0, 15,  0,  0,  0,  0,  0,  0,  0,  0} },
  {60u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
  {60u, {  0,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {60u, {  0,  0,  0,  0, 15,  0,  0,  0,  0,  0} },
  {60u, {  0,  0,  0,  0,  0, 15,  0,  0,  0,  0} },
  {60u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {60u, {  0,  0,  0,  0,  0,  0,  0, 15,  0,  0} },
  {60u, {  0,  0,  0,  0,  0,  0,  0,  0, 15,  0} },
};

//! \brief Race animation
CODE const S_ANIMATION_INSTRUCTION gasRace[ 30u ] = 
{
  {250u, {  0,  0,  0,  0,  0,  0,  0,  0,  0, 15} },
  {250u, {  0,  0,  0,  0,  0,  0,  0,  0, 15,  0} },
  {250u, {  0,  0,  0,  0,  0,  0,  0, 15,  0,  0} },
  {250u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {250u, {  0,  0,  0,  0,  0, 15,  0,  0,  0,  0} },
  {250u, {  0,  0,  0,  0, 15,  0,  0,  0,  0,  0} },
  {250u, {  0,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {250u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
  {250u, {  0, 15,  0,  0,  0,  0,  0,  0,  0,  0} },
  {250u, { 15,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {120u, {  0,  0,  0,  0,  0,  0,  0,  0,  0, 15} },
  {120u, {  0,  0,  0,  0,  0,  0,  0,  0, 15,  0} },
  {120u, {  0,  0,  0,  0,  0,  0,  0, 15,  0,  0} },
  {120u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {120u, {  0,  0,  0,  0,  0, 15,  0,  0,  0,  0} },
  {120u, {  0,  0,  0,  0, 15,  0,  0,  0,  0,  0} },
  {120u, {  0,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {120u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
  {120u, {  0, 15,  0,  0,  0,  0,  0,  0,  0,  0} },
  {120u, { 15,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
  {70u, {  0,  0,  0,  0,  0,  0,  0,  0,  0, 15} },
  {70u, {  0,  0,  0,  0,  0,  0,  0,  0, 15,  0} },
  {70u, {  0,  0,  0,  0,  0,  0,  0, 15,  0,  0} },
  {70u, {  0,  0,  0,  0,  0,  0, 15,  0,  0,  0} },
  {70u, {  0,  0,  0,  0,  0, 15,  0,  0,  0,  0} },
  {70u, {  0,  0,  0,  0, 15,  0,  0,  0,  0,  0} },
  {70u, {  0,  0,  0, 15,  0,  0,  0,  0,  0,  0} },
  {70u, {  0,  0, 15,  0,  0,  0,  0,  0,  0,  0} },
  {70u, {  0, 15,  0,  0,  0,  0,  0,  0,  0,  0} },
  {70u, { 15,  0,  0,  0,  0,  0,  0,  0,  0,  0} },
};

//! \brief Stepping 3 section animation
CODE const S_ANIMATION_INSTRUCTION gasStepping3Section[ 3u ] = 
{
  {200u, {  0,  0, 15,  0,  0, 15,  0,  0, 15,  0} },
  {200u, {  0, 15,  0,  0, 15,  0,  0, 15,  0,  0} },
  {200u, { 15,  0,  0, 15,  0,  0, 15,  0,  0, 15} },
};

//! \brief Ying-yang bounce animation
CODE const S_ANIMATION_INSTRUCTION gasYingYangBounce[ 10u ] = 
{
  {100u, {  0,  0,  2,  5, 15,  0,  0,  2,  5, 15} },
  {100u, { 15,  0,  0,  2,  5, 15,  0,  0,  2,  5} },
  {100u, {  5, 15,  0,  0,  2,  5, 15,  0,  0,  2} },
  {100u, {  2,  5, 15,  0,  0,  2,  5, 15,  0,  0} },
  {100u, {  0,  2,  5, 15,  0,  0,  2,  5, 15,  0} },
  {100u, {  0,  0,  2,  5, 15,  0,  0,  2,  5, 15} },
  {100u, {  0,  2,  5, 15,  0,  0,  2,  5, 15,  0} },
  {100u, {  2,  5, 15,  0,  0,  2,  5, 15,  0,  0} },
  {100u, {  5, 15,  0,  0,  2,  5, 15,  0,  0,  2} },
  {100u, { 15,  0,  0,  2,  5, 15,  0,  0,  2,  5} },
};

//! \brief Pairing animation (fast flashing)
CODE const S_ANIMATION_INSTRUCTION gasPairing[ 2u ] = 
{
  { 50u, {15,  0, 15,  0, 15,  0, 15,  0, 15,  0} }, 
  { 50u, { 0, 15,  0, 15,  0, 15,  0, 15,  0, 15} },
};

//! \brief Table of animations
CODE const S_ANIMATION gasAnimations[ NUM_ANIMATIONS ] = 
{
//  {sizeof(gasHeartBeat)/sizeof(S_ANIMATION_INSTRUCTION),           gasHeartBeat},
  {sizeof(gasHeartbeatFading)/sizeof(S_ANIMATION_INSTRUCTION),     gasHeartbeatFading},
  {sizeof(gasShootingStar)/sizeof(S_ANIMATION_INSTRUCTION),        gasShootingStar},
  {sizeof(gasButterfly)/sizeof(S_ANIMATION_INSTRUCTION),           gasButterfly},
  {sizeof(gasSoftFlashing)/sizeof(S_ANIMATION_INSTRUCTION),        gasSoftFlashing},
  {sizeof(gasKITT)/sizeof(S_ANIMATION_INSTRUCTION),                gasKITT},
  {sizeof(gasChase2Fading)/sizeof(S_ANIMATION_INSTRUCTION),        gasChase2Fading},
//  {sizeof(gasShootingStarChasing)/sizeof(S_ANIMATION_INSTRUCTION), gasShootingStarChasing},
  {sizeof(gasFading2Section)/sizeof(S_ANIMATION_INSTRUCTION),      gasFading2Section},
  {sizeof(gasLevelUp)/sizeof(S_ANIMATION_INSTRUCTION),             gasLevelUp},
  {sizeof(gasShootingStarBoth)/sizeof(S_ANIMATION_INSTRUCTION),    gasShootingStarBoth},
  {sizeof(gasStepping3Section)/sizeof(S_ANIMATION_INSTRUCTION),    gasStepping3Section},
  {sizeof(gasGenericFlasher)/sizeof(S_ANIMATION_INSTRUCTION),      gasGenericFlasher},
  {sizeof(gasPingPong)/sizeof(S_ANIMATION_INSTRUCTION),            gasPingPong},
  {sizeof(gasYingYangBounce)/sizeof(S_ANIMATION_INSTRUCTION),      gasYingYangBounce},
  {sizeof(gasRace)/sizeof(S_ANIMATION_INSTRUCTION),                gasRace},
  // Pairing animation
  {sizeof(gasPairing)/sizeof(S_ANIMATION_INSTRUCTION),             gasPairing}
};


/***************************************< Global variables >**************************************/
//! \brief Ms resolution timer that is synchronized between paired devices
//! \note  Depending on the animation, its maximum value can be anything
IDATA U16 gu16SynchronizedTimer;
IDATA U16 gu16LastCall;       //!< The last time the main cycle was called


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
    
    // Update LED brightnesses based on the synchronized timer
    memcpy( gau8LEDBrightness, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructions[ u8AnimationState ].au8LEDBrightness, LEDS_NUM );
    
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
  }
}


/***************************************< End of file >**************************************/
