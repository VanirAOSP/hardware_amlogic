/******************** (C) COPYRIGHT 2009 STMicroelectronics ********************
* File Name          : MEMSAlgLib_AirMouse.h
* Department         : STMicroelectronics APM MEMS G.C. FAE TEAM 
* Author             : Travis Tu <travis.tu@st.com>
* Date First Issued  : 2009.11.12
********************************************************************************
* History:
* 2010.3.22  V1.0
********************************************************************************
* THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR REFERENCE OR 
* EDUCATION. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY 
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING 
* FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE 
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

#ifndef __MEMSALGLIB_ECOMPASS_H
#define __MEMSALGLIB_ECOMPASS_H

typedef struct
{
  signed short ax;  //x value
  signed short ay;  //y value
  signed short az;  //z value
  signed short mx;  //x value
  signed short my;  //y value
  signed short mz;  //z value
  signed long time; //system time
}ECOM_ValueTypeDef;

/***********************************************************************/
/**                             e Compass                             **/
/***********************************************************************/
//init the eCompass Algorithm 
//Only invoked once
void          MEMSAlgLib_eCompass_Init(signed short oneGravityValue, signed short oneGaussValue);

//Hard Iron Calibration Part
unsigned char MEMSAlgLib_eCompass_IsCalibrated(void);
void          MEMSAlgLib_eCompass_GetCalibration(signed short * mxOff, signed short * myOff, signed short * mzOff);
void          MEMSAlgLib_eCompass_SetCalibration(signed short mx,signed short my,signed short mz);
void          MEMSAlgLib_eCompass_ForceCalibration(void);

//Update the Algorithm
//invoked once every loop
void          MEMSAlgLib_eCompass_Update(ECOM_ValueTypeDef* v);

//Get the Azimuth back from algorithm
signed  short MEMSAlgLib_eCompass_Get_Azimuth(void);

//Get Pitch -180 ~ +180
signed  short MEMSAlgLib_eCompass_Get_Pitch(void);

//Get Roll -90 ~ +90
signed short MEMSAlgLib_eCompass_Get_Roll(void);

#endif
