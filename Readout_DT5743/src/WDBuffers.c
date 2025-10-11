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

#include "WDBuffers.h"

int WDBuff_get_start(WaveDemoBuffers_t *buff, int bd) {
	return buff->head[bd];
}

int WDBuff_get_end(WaveDemoBuffers_t *buff, int bd) {
	return buff->tail[bd];
}

int WDBuff_reset(WaveDemoBuffers_t *buff, int bd) {
	if (!buff->buffer[bd])
		return -1;
	buff->head[bd] = 0;
	buff->tail[bd] = 0;
	buff->tmp_pos[bd] = 0;
	return 0;
}

int WDBuff_empty(WaveDemoBuffers_t *buff, int bd) {
	// We define empty as head == tail
	return (buff->head[bd] == buff->tail[bd]) ? 1 : 0;
}

int WDBuff_full(WaveDemoBuffers_t *buff, int bd) {
	// We determine "full" case by head being one position behind the tail
	// Note that this means we are wasting one space in the buffer!
	// Instead, you could have an "empty" flag and determine buffer full that way
	return ((buff->head[bd] + 1) % EVT_BUF_SIZE) == buff->tail[bd] ? 1  : 0;
}

int WDBuff_free_space(WaveDemoBuffers_t *buff, int bd) {
	if (WDBuff_full(buff, bd))
		return 0;
	if (buff->head[bd] >= buff->tail[bd])
		return EVT_BUF_SIZE - 1 - (buff->head[bd] - buff->tail[bd]);
	else
		return buff->tail[bd] - buff->head[bd] - 1;
}

int WDBuff_used_space(WaveDemoBuffers_t *buff, int bd) {
	if (buff->head[bd] >= buff->tail[bd])
		return buff->head[bd] - buff->tail[bd];
	else
		return EVT_BUF_SIZE - buff->tail[bd] + buff->head[bd];
}

float WDBuff_occupancy(WaveDemoBuffers_t *buff, int bd) {
	int used = WDBuff_used_space(buff, bd);
	return (float)(100.0 * used / (EVT_BUF_SIZE - 1));
}

int WDBuff_remove(WaveDemoBuffers_t *buff, int bd, int num) {
	int i = 0;
	if (!buff->buffer[bd] || num < 0)
		return -1;
	for (i = 0; i < num; i++) {
		if (WDBuff_empty(buff, bd))
			break;
		buff->tail[bd] = (buff->tail[bd] + 1) % EVT_BUF_SIZE;
	}
	return i;
}

int WDBuff_get_write_pointer(WaveDemoBuffers_t *buff, int bd, WaveDemoEvent_t** event) {
	if (!buff->buffer[bd] || WDBuff_full(buff, bd))
		return -1;
	int head = buff->head[bd];
	*event = &(buff->buffer[bd][head]);
	return 0;
}

int WDBuff_added(WaveDemoBuffers_t *buff, int bd, int num) {
	int i = 0;
	if (!buff->buffer[bd] || num < 0)
		return -1;
	for (i = 0; i < num; i++) {
		if (WDBuff_full(buff, bd))
			break;
		buff->head[bd] = (buff->head[bd] + 1) % EVT_BUF_SIZE;
	}
	return i;
}

int WDBuff_peak(WaveDemoBuffers_t *buff, int bd, WaveDemoEvent_t **event) {
	if (!buff->buffer[bd] || WDBuff_empty(buff, bd))
		return -1;
	int tail = buff->tail[bd];
	*event = &(buff->buffer[bd][tail]);
	return 0;
}

int WDBuff_set_position(WaveDemoBuffers_t *buff, int bd, int pos) {
	if (!buff->buffer[bd] || WDBuff_empty(buff, bd))
		return -1;
	if (pos < 0 || pos >= EVT_BUF_SIZE)
		return -1;
	if (pos >= buff->head[bd] || pos < buff->tail[bd])
		return -1;
	buff->tmp_pos[bd] = pos;
	return 0;
}

int WDBuff_get_next(WaveDemoBuffers_t *buff, int bd, WaveDemoEvent_t **event) {
	if (!buff->buffer[bd] || WDBuff_empty(buff, bd))
		return -1;
	if (buff->tmp_pos[bd] == buff->head[bd])
		return -1;
	int pos = buff->tmp_pos[bd];
	*event = &(buff->buffer[bd][pos]);
	buff->tmp_pos[bd] = (buff->tmp_pos[bd] + 1) % EVT_BUF_SIZE;
	return 0;
}
