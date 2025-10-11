/******************************************************************************
*
* Copyright (C) 2017 CAEN SpA - www.caen.it - support.computing@caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This file is subject to the terms and conditions defined in file
* 'CAEN_License_Agreement.txt', which is part of this source code package.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/


#ifndef _WDCONFIG__H
#define _WDCONFIG__H

#include "WaveDemo.h"

#define streq(a,b) (strcmp((a),(b))==0)

/* ###########################################################################
*  Functions
*  ########################################################################### */

/*!
* \fn	void SetDefaultConfiguration(WaveDemoConfig_t *WDcfg)
* \brief	Fill the Configuration Structure with Default Values.
*
* \param [in,out]	WDcfg	Pointer to the WaveDumpConfig data structure.
*/
void SetDefaultConfiguration(WaveDemoConfig_t *WDcfg);

/*! \fn      int ParseConfigFile(FILE *f_ini, WaveDemoConfig_t *WDcfg)
*   \brief   Read the configuration file and set the WaveDump paremeters
*
*   \param   f_ini        Pointer to the config file
*   \param   WDcfg:   Pointer to the WaveDumpConfig data structure
*   \return  0 = Success; negative numbers are error codes
*/
int ParseConfigFile(FILE* f_ini, WaveDemoConfig_t* WDcfg);

#endif // _WDCONFIG__H
