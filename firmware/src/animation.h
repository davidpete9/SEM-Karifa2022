/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file animation.h
*
* \brief Implementation of LED animations
*
* \author Hekk_Elek
*
**********************************************************************************************************/
#ifndef ANIMATION_H
#define ANIMATION_H

/***************************************< Includes >**************************************/


/***************************************< Definitions >**************************************/
#define NUM_ANIMATIONS        (15u)  //!< Number of animations implemented


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
extern IDATA U16 gu16SynchronizedTimer;


/***************************************< Public functions >**************************************/
void Animation_Init( void );
void Animation_Cycle( void );
void Animation_Set( U8 u8AnimationIndex );


#endif /* ANIMATION_H */

/***************************************< End of file >**************************************/
