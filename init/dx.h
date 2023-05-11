/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"

// DX list

#define DX_HIDDEN_SLOT 1

typedef struct {
	double freq;        // must be first for qsort_doublecomp()
    u2_t time_begin, time_end;
	u4_t flags;
    u2_t ident_idx;
    char country[4], lang[4], target[4];

	int idx;
	const char *ident, *ident_s;
	char *notes, *notes_s;
	const char *params;
	int low_cut;
	int high_cut;
	int offset;
} dx_t;

typedef enum { DB_STORED = 0, DB_EiBi } dx_db_e;

typedef enum { DX_JSON = 0, DX_CSV = 1 } dx_upload_e;

typedef struct {
    dx_db_e db;
} dx_rx_t;

typedef struct {
	int masked_lo, masked_hi;   // Hz
} dx_mask_t;

typedef struct {
    dx_rx_t dx_rx[MAX_RX_CHANS + 1];    // +1 for STREAM_ADMIN use at end
	dx_t *stored_list, *eibi_list;
	int stored_len, eibi_len;
	#define DX_LIST_ALLOC_CHUNK 256
	int stored_alloc_size;
	bool eibi_init;
	bool hidden_used;
	bool json_up_to_date;
	dx_mask_t *masked_list;
	int masked_len;
	u4_t update_seq;
	char *json;
	int lines;
	int json_parse_errors, dx_format_errors;
	#define N_DX_FILE_HASH_BYTES 4      // 4 bytes = 8 chars
    char file_hash[N_DX_FILE_HASH_BYTES*2 + SPACE_FOR_NULL];
    int file_size;
} dxlist_t;

extern dxlist_t dx;

#define	DX_MODE	    0x0000000f  // 32 modes
#define DX_DECODE_MODE(flags)   (  (((flags) & DX_MODE_16)? 16:0) | ((flags) & DX_MODE) )
#define DX_ENCODE_MODE(mode)    (  (((mode) >= 16)? DX_MODE_16:0) | ((mode) & DX_MODE) )

#define	DX_TYPE	    0x000001f0  // 32 types
#define DX_TYPE_SFT 4
#define DX_STORED_FLAGS_TYPEIDX(flags)  (  ((flags) & DX_TYPE)            >> DX_TYPE_SFT )
#define DX_EiBi_FLAGS_TYPEIDX(flags)    ( (((flags) & DX_TYPE) - DX_EiBi) >> DX_TYPE_SFT )

#define DX_N_STORED 16
#define DX_STORED   0x00000000  // stored: 0x000, 0x010, ... 0x0f0 (16)
#define	DX_MASKED   0x000000f0	// masked

// legacy values
#define	DX_ACTIVE   0x00000000  // signal is actively heard
#define	DX_WL		0x00000010	// on watchlist, i.e. not actually heard yet, marked as a signal to watch for
#define	DX_SB		0x00000020	// a sub-band, not a station
#define	DX_DG		0x00000030	// DGPS
#define	DX_SE		0x00000040	// special event
#define	DX_XX		0x00000050	// interference

#define DX_N_EiBi   12
#define	DX_EiBi 	0x00000100  // EiBi: 0x100, 0x110, ... 0x1f0 (16)
#define	DX_BCAST	0x00000100
#define	DX_UTIL		0x00000110
#define	DX_TIME		0x00000120
#define	DX_ALE		0x00000130
#define	DX_HFDL		0x00000140
#define	DX_MILCOM   0x00000150
#define	DX_CW		0x00000160
#define	DX_FSK      0x00000170
#define	DX_FAX      0x00000180
#define	DX_AERO		0x00000190
#define	DX_MARINE   0x000001a0
#define	DX_SPY      0x000001b0

#define DX_DOW_SFT  9
#define	DX_DOW      0x0000fe00
#define DX_MON      0x00008000
#define DX_TUE      0x00004000
#define DX_WED      0x00002000
#define DX_THU      0x00001000
#define DX_FRI      0x00000800
#define DX_SAT      0x00000400
#define DX_SUN      0x00000200

#define DX_FLAGS    0xffff0000
#define DX_FILTERED 0x00010000
#define DX_HAS_EXT  0x00020000
#define DX_MODE_16  0x00040000

extern const int eibi_counts[DX_N_EiBi];

void dx_reload();
void update_masked_freqs(dx_t *_dx_list = NULL, int _dx_list_len = 0);
void dx_prep_list(bool need_sort, dx_t *_dx_list, int _dx_list_len_prev, int _dx_list_len_new);
void dx_eibi_init();

#define DX_LABEL_FOFF_CONVERT true
void dx_save_as_json(bool dx_label_foff_convert = false);


// AJAX_DX support

#define CSV_FLT 0
#define CSV_STR 1
#define CSV_DEC 2       // NB: if type == CSV_DEC, caller must kiwi_ifree(val)

#define TYPE_JSON 0
#define TYPE_CSV 1

typedef struct {
    int type;
    const char *data;
    int data_len;
    char **s_a;
    int idx;
} dx_param_t;

bool _dx_parse_csv_field(int type, char *field, void *val, bool *empty = NULL);
void _dx_write_file(void *param);
