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

#ifndef _WDBUFFERS_H
#define _WDBUFFERS_H

#include "WaveDemo.h"


//****************************************************************************
// Function prototypes
//****************************************************************************
int WDBuff_get_start(WaveDemoBuffers_t *buff, int bd);
int WDBuff_get_end(WaveDemoBuffers_t *buff, int bd);
int WDBuff_reset(WaveDemoBuffers_t *buff, int bd);
int WDBuff_empty(WaveDemoBuffers_t *buff, int bd);
int WDBuff_full(WaveDemoBuffers_t *buff, int bd);
int WDBuff_free_space(WaveDemoBuffers_t *buff, int bd);
int WDBuff_used_space(WaveDemoBuffers_t *buff, int bd);
float WDBuff_occupancy(WaveDemoBuffers_t *buff, int bd);

int WDBuff_remove(WaveDemoBuffers_t *buff, int bd, int num);
int WDBuff_added(WaveDemoBuffers_t *buff, int bd, int num);

int WDBuff_get_write_pointer(WaveDemoBuffers_t *buff, int bd, WaveDemoEvent_t** event);

int WDBuff_peak(WaveDemoBuffers_t *buff, int bd, WaveDemoEvent_t **event);

int WDBuff_set_position(WaveDemoBuffers_t *buff, int bd, int pos);
int WDBuff_get_next(WaveDemoBuffers_t *buff, int bd, WaveDemoEvent_t **event);

#endif