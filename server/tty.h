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

#ifndef _TTY_H_
#define _TTY_H_

typedef struct tty_context* TTYCTX;
	 
const char* tty_error(TTYCTX c);
TTYCTX tty_init_libftdi(int vendor,int product,const char *description,const char *serial,unsigned int index,void (*cb)(const char*,int));
TTYCTX tty_init_tty(const char *devicename,void (*cb)(const char*,int));
int tty_main(TTYCTX c);
int tty_busy(TTYCTX c);
int tty_stop(TTYCTX c);
int tty_send(TTYCTX c,const char *s);
int tty_main(TTYCTX c);
int tty_set_event_char(TTYCTX c,char e);

#endif