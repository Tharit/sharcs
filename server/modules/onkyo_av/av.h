/*
 * Copyright (c) 2012 Martin Kleinhans <mail@mkleinhans.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _AV_H_
#define _AV_H_

#define AV_VERSION "0.1"

/*
 * available commands
 */
#define AV_CMD_ALL -1
enum {
	AV_CMD_POWER,
	AV_CMD_MUTE,
	AV_CMD_INPUT,
	AV_CMD_MODE,
	AV_CMD_VOLUME,
	AV_CMD_LATENIGHT,
	AV_CMD_DIMMER,
	AV_CMD_PRESET,
	
	/* @TODO other supported commands?
	 * SLZ, ZVL, = zone2
	 * RAS, - re-EQ
	 * TUN, - tuning
	 * DIF, - display mode, not available on tx-sr875
	 * OSD, - setup control
	 *
	 * many additional commands for onkyo RI devices
	 */
	
	/* helper definition */
	AV_NUM_COMMANDS
};

/*
 * available inputs
 */
enum {
	AV_INPUT_DVD 		= 0x10,
	AV_INPUT_VCR_DVR 	= 0x00,
	AV_INPUT_CBL_SAT 	= 0x01,
	AV_INPUT_GAME_TV 	= 0x02,
	AV_INPUT_AUX1 		= 0x03,
	AV_INPUT_AUX2 		= 0x04,
	AV_INPUT_TAPE 		= 0x20,
	AV_INPUT_TUNER 		= 0x24,
	AV_INPUT_TUNER_FM 	= 0x24,
	AV_INPUT_TUNER_AM 	= 0x25,
	AV_INPUT_CD 		= 0x23,
	AV_INPUT_PHONO 		= 0x22,
};

/*
 * available listening modes
 */
enum {
	AV_MODE_STEREO			= 0x00,
	AV_MODE_DIRECT			= 0x01,
	AV_MODE_SURROUND		= 0x02,
	AV_MODE_ALL_CH_STEREO	= 0x0C,
	AV_MODE_FILM			= 0x03,
	AV_MODE_THX				= 0x04,
	AV_MODE_ACTION			= 0x05,
	AV_MODE_MUSICAL			= 0x06,
	AV_MODE_MONO_MOVIE		= 0x07,
	AV_MODE_ORCHESTRA		= 0x08,
	AV_MODE_UNPLUGGED		= 0x09,
	AV_MODE_PURE_AUDIO		= 0x11,
	AV_MODE_STUDIO_MIX		= 0x0A,
	AV_MODE_TV_LOGIC		= 0x0B,
	AV_MODE_THEATER			= 0x0D,
	AV_MODE_ENHANCED7		= 0x0E,
	AV_MODE_MONO			= 0x0F,
	
	/* different modes activated either by AV_MODE_SURROUND or directly */
	AV_MODE_PL_MOVIE		= 0x80,
	AV_MODE_PL_MUSIC		= 0x81,
	AV_MODE_NEO_CINEMA		= 0x82,
	AV_MODE_NEO_MUSIC		= 0x83,
	AV_MODE_NEO_THX_CINEMA	= 0x84,	
	AV_MODE_PL_THX_CINEMA	= 0x85,	
	AV_MODE_PL_GAME			= 0x86,
	AV_MODE_NEURAL_THX		= 0x88,
};

/*
 * available dimmer modes
 */
enum {
	AV_DIMMER_BRIGHTEST = 0x0,
	AV_DIMMER_BRIGHT	= 0x8,
	AV_DIMMER_DIM		= 0x1,
	AV_DIMMER_DARK		= 0x2,
};

/*
 * opens and initializes the connection to the device
 */
int av_init_tty(const char *devicename,void (*cb)(int,int));
int av_init_libftdi(int vendor,int product,const char *description,const char *serial,unsigned int index,void (*cb)(int,int));

/*
 * requests specified value from device
 */
void av_req(int cmd);

/*
 * sets the value of the specified command
 */
int av_seti(int cmd, int v);
int av_sets(int cmd, const char* v);

/*
 * returns current value of specified command
 * if value is not available, returns -1
 */
int av_state(int cmd);

/*
 * checks actions are pending, e.g. call to av_main is required
 */
int av_busy();

/*
 * closes the connection
 */
int av_stop();

/*
 * main routine, handles communication with device
 * errors occuring in this function are critical
 */
extern int (*av_main)();

/*
 * returns last error, or NULL
 */
const char* av_error();

/*
 * validate a value for a specific command
 */
int av_validvs(int cmd,const char* v);
int av_validvi(int cmd,int v);

/*
 * conversion functions for commands
 */
const char* av_cmd2str(int cmd);
int av_str2cmd(const char* cmd);

/*
 * conversion functions for values
 */
const char* av_v2str(int cmd,int v);
int av_str2v(int cmd,const char* v);

#endif
