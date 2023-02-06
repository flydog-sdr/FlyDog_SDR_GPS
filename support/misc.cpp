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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "valgrind.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "web.h"
#include "spi.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "debug.h"
#include "mongoose.h"

#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sched.h>
#include <sys/stat.h>


void misc_init()
{
    //printf("sed -z -f " DIR_CFG "/rsyslog.sed -i /etc/rsyslog.conf\n");
    system("sed -z -f " DIR_CFG "/rsyslog.sed -i /etc/rsyslog.conf");
}


// used by qsort
// NB: assumes first element in struct is sort field
int qsort_floatcomp(const void *elem1, const void *elem2)
{
	const float f1 = *(const float *) elem1, f2 = *(const float *) elem2;
    if (f1 < f2)
        return -1;
    return f1 > f2;
}

int qsort_doublecomp(const void *elem1, const void *elem2)
{
	const double f1 = *(const double *) elem1, f2 = *(const double *) elem2;
    if (f1 < f2)
        return -1;
    return f1 > f2;
}

int qsort_intcomp(const void *elem1, const void *elem2)
{
	const int i1 = *(const int *) elem1, i2 = *(const int *) elem2;
	return i1 - i2;
}

float median_f(float *f, int len, float *pct_1, float *pct_2)
{
     qsort(f, len, sizeof *f, qsort_floatcomp);
     if (pct_1) *pct_1 = f[(int) (len * (*pct_1 / 100.0))];
     if (pct_2) *pct_2 = f[(int) (len * (*pct_2 / 100.0))];
     return f[len/2];
}

int median_i(int *i, int len, int *pct_1, int *pct_2)
{
     qsort(i, len, sizeof *i, qsort_intcomp);
     if (pct_1) *pct_1 = i[(int) (len * ((float) *pct_1 / 100.0))];
     if (pct_2) *pct_2 = i[(int) (len * ((float) *pct_2 / 100.0))];
     return i[len/2];
}

static int misc_miso_busy[2];

SPI_MISO *get_misc_miso(int which)
{
    assert(misc_miso_busy[which] == 0);
    misc_miso_busy[which]++;
    return &SPI_SHMEM->misc_miso[which];
}

void release_misc_miso(int which)
{
    misc_miso_busy[which]--;
}

static int misc_mosi_busy;

SPI_MOSI *get_misc_mosi()
{
    assert(misc_mosi_busy == 0);
    misc_mosi_busy++;
    return &SPI_SHMEM->misc_mosi;
}

void release_misc_mosi()
{
    misc_mosi_busy--;
}

u2_t ctrl_get()
{
	SPI_MISO *ctrl = get_misc_miso();
	
	spi_get_noduplex(CmdCtrlGet, ctrl, sizeof(ctrl->word[0]));
	u2_t rv = ctrl->word[0];
	release_misc_miso();
	return rv;
}

void ctrl_clr_set(u2_t clr, u2_t set)
{
	spi_set_noduplex(CmdCtrlClrSet, clr, set);
	//printf("ctrl_clr_set(0x%04x, 0x%04x) ctrl_get=0x%04x\n", clr, set, ctrl_get());
}

void ctrl_positive_pulse(u2_t bits)
{
	spi_set_noduplex(CmdCtrlClrSet, bits, bits);
	spi_set_noduplex(CmdCtrlClrSet, bits, 0);
}

stat_reg_t stat_get()
{
    SPI_MISO *status = get_misc_miso();
    stat_reg_t stat;
    
    spi_get_noduplex(CmdGetStatus, status, sizeof(stat));
	release_misc_miso();
    stat.word = status->word[0];

    return stat;
}

u2_t getmem(u2_t addr)
{
	SPI_MISO *mem = get_misc_miso();
	
	memset(mem->word, 0x55, sizeof(mem->word));
	spi_get_noduplex(CmdGetMem, mem, 4, addr);
	release_misc_miso();
	assert(addr == mem->word[1]);
	
	return mem->word[0];
}

void printmem(const char *str, u2_t addr)
{
	printf("%s %04x: %04x\n", str, addr, (int) getmem(addr));
}

void cmd_debug_print(conn_t *c, char *s, int slen, bool tx)
{
    int sl = slen - 4;
    printf("%c %s %.3s %.4s%s%d ", tx? 'T':'<', Task_s(-1), c? rx_streams[c->type].uri : "nil",
        s, (s[3] != ' ')? " " : "", sl);
    if (sl > 0) {
        char *s2 = kiwi_str_encode(&s[4], true);
        sl = strlen(s2);
        cprintf(c, "\"%.80s\" %s%s\n", s2, (sl > 80)? "... " : "", (sl > 80)? &s2[sl-8] : "");
        kiwi_ifree(s2);
    } else {
        cprintf(c, "\n");
    }
}

void send_msg_buf(conn_t *c, char *s, int slen)
{
    if (c->internal_connection) {
        //clprintf(c, "send_msg_buf: internal_connection <%s>\n", s);
    } else {
        if (c->mc == NULL) {
            #if 0
                clprintf(c, "send_msg_buf: c->mc is NULL\n");
                clprintf(c, "send_msg_buf: CONN-%d %p valid=%d type=%d [%s] auth=%d KA=%d KC=%d mc=%p rx=%d magic=0x%x ip=%s:%d other=%s%d %s\n",
                    c->self_idx, c, c->valid, c->type, rx_streams[c->type].uri, c->auth, c->keep_alive, c->keepalive_count, c->mc, c->rx_channel,
                    c->magic, c->remote_ip, c->remote_port, c->other? "CONN-":"", c->other? c->other-conns:0, c->stop_data? "STOP":"");
            #endif
            return;
        }

        if (cmd_debug) cmd_debug_print(c, s, slen, true);
        mg_websocket_write(c->mc, WEBSOCKET_OPCODE_BINARY, s, slen);
    }
}

void send_msg(conn_t *c, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) cprintf(c, "send_msg: %p <%s>\n", c->mc, s);
	send_msg_buf(c, s, strlen(s));
	kiwi_ifree(s, "send_msg");
}

#define N_MSG_HDR 4
void send_msg_data(conn_t *c, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	int size = N_MSG_HDR + sizeof(cmd) + nbytes;
	char *buf = (char *) kiwi_imalloc("send_msg_data", size);
	char *s = buf;
	int n = kiwi_snprintf_ptr(s, N_MSG_HDR + SPACE_FOR_NULL, "DAT ");
	if (debug) cprintf(c, "send_msg_data: cmd=%d nbytes=%d size=%d\n", cmd, nbytes, size);
	s += n;
	*s++ = cmd;
	if (nbytes)
		memcpy(s, bytes, nbytes);
	send_msg_buf(c, buf, size);
	kiwi_ifree(buf, "send_msg_data");
}

void send_msg_data2(conn_t *c, bool debug, u1_t cmd, u1_t data2, u1_t *bytes, int nbytes)
{
	int size = N_MSG_HDR + sizeof(cmd)+ sizeof(data2) + nbytes;
	char *buf = (char *) kiwi_imalloc("send_msg_data2", size);
	char *s = buf;
	int n = kiwi_snprintf_ptr(s, N_MSG_HDR + SPACE_FOR_NULL, "DAT ");
	if (debug) cprintf(c, "send_msg_data2: cmd=%d data2=%d nbytes=%d size=%d\n", cmd, data2, nbytes, size);
	s += n;
	*s++ = cmd;
	*s++ = data2;
	if (nbytes)
		memcpy(s, bytes, nbytes);
	send_msg_buf(c, buf, size);
	kiwi_ifree(buf, "send_msg_data2");
}

// sent direct to mg_connection -- only directly called in a few places where conn_t isn't available
// caution: never use an mprint() here as this will result in a loop
void send_msg_mc(struct mg_connection *mc, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	size_t slen = strlen(s);
	if (debug) printf("send_msg_mc: %d <%s>\n", slen, s);
	mg_websocket_write(mc, WEBSOCKET_OPCODE_BINARY, s, slen);
	kiwi_ifree(s, "send_msg_mc");
}

void send_msg_encoded(conn_t *conn, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = kiwi_str_encode(s);
	kiwi_ifree(s, "send_msg_encoded");
	send_msg(conn, FALSE, "%s %s=%s", dst, cmd, buf);
	kiwi_ifree(buf, "send_msg_encoded");
}

// sent direct to mg_connection -- only directly called in a few places where conn_t isn't available
// caution: never use an mprint() here as this will result in a loop
void send_msg_mc_encoded(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = kiwi_str_encode(s);
	kiwi_ifree(s, "send_msg_mc_encoded");
	send_msg_mc(mc, FALSE, "%s %s=%s", dst, cmd, buf);
	kiwi_ifree(buf, "send_msg_mc_encoded");
}

void input_msg_internal(conn_t *conn, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (conn == NULL || fmt == NULL || !conn->valid) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
    //assert(conn->internal_connection);
	nbuf_allocq(&conn->c2s, s, strlen(s));
	kiwi_ifree(s, "input_msg_internal");
}

float ecpu_use()
{
	typedef struct {
		u1_t f0, g0, f1, g1, f2, g2, f3, g3;
	} ctr_t;
	ctr_t *c;
	
	if (down || do_fft) return 0;

	SPI_MISO *cpu = get_misc_miso();
	spi_get_noduplex(CmdGetCPUCtr, cpu, sizeof(u2_t[3]));
	release_misc_miso();
	c = (ctr_t*) &cpu->word[0];
	u4_t gated = (c->g3 << 24) | (c->g2 << 16) | (c->g1 << 8) | c->g0;
	u4_t free_run = (c->f3 << 24) | (c->f2 << 16) | (c->f1 << 8) | c->f0;

	spi_set(CmdCPUCtrClr);
	if (free_run == 0) return 0;
	return ((float) gated / (float) free_run * 100);
}

struct print_max_min_int_t {
	int min_i, max_i;
	double min_f, max_f;
	int min_idx, max_idx;
};

static print_max_min_int_t *print_max_min_init(void **state)
{
	print_max_min_int_t **pp = (print_max_min_int_t **) state;
	if (*pp == NULL) {
		*pp = (print_max_min_int_t *) kiwi_imalloc("print_max_min_init", sizeof(print_max_min_int_t));
		print_max_min_int_t *p = *pp;
		memset(p, 0, sizeof(*p));
		p->min_i = 0x7fffffff; p->max_i = 0x80000000;
		p->min_f = 1e38; p->max_f = -1e38;
		p->min_idx = p->max_idx = -1;
	}
	return *pp;
}

void print_max_min_stream_i(void **state, int flags, const char *name, int index, int nargs, ...)
{
	va_list ap;
	va_start(ap, nargs);
	print_max_min_int_t *p = print_max_min_init(state);
	bool update = false;

	if (flags & P_MAX_MIN_RESET) {
		memset(p, 0, sizeof(*p));
		p->min_i = 0x7fffffff; p->max_i = 0x80000000;
		p->min_f = 1e38; p->max_f = -1e38;
		p->min_idx = p->max_idx = -1;
	}

	for (int i=0; i < nargs; i++) {
		int arg_i = va_arg(ap, int);
		if (arg_i > p->max_i) {
			p->max_i = arg_i;
			p->max_idx = index;
			update = true;
		}
		if (arg_i < p->min_i) {
			p->min_i = arg_i;
			p->min_idx = index;
			update = true;
		}
	}
	
	if ((flags & P_MAX_MIN_DUMP) || ((flags & P_MAX_MIN_RANGE) && update)) {
		printf("min/max %s: %d(%d)..%d(%d)\n", name, p->min_i, p->min_idx, p->max_i, p->max_idx);
	}

	va_end(ap);
}

void print_max_min_stream_f(void **state, int flags, const char *name, int index, int nargs, ...)
{
	va_list ap;
	va_start(ap, nargs);
	print_max_min_int_t *p = print_max_min_init(state);
	bool dump = (flags & P_MAX_MIN_DUMP);
	bool update = false;
	
	if (flags & P_MAX_MIN_RESET) {
		memset(p, 0, sizeof(*p));
		p->min_i = 0x7fffffff; p->max_i = 0x80000000;
		p->min_f = 1e38; p->max_f = -1e38;
		p->min_idx = p->max_idx = -1;
	}

	if (!dump) for (int i=0; i < nargs; i++) {
		double arg_f = va_arg(ap, double);
		if (arg_f > p->max_f) {
			p->max_f = arg_f;
			p->max_idx = index;
			update = true;
		}
		if (arg_f < p->min_f) {
			p->min_f = arg_f;
			p->min_idx = index;
			update = true;
		}
	}
	
	if (dump || ((flags & P_MAX_MIN_RANGE) && update)) {
		//printf("min/max %s: %e(%d)..%e(%d)\n", name, p->min_f, p->min_idx, p->max_f, p->max_idx);
		printf("min/max %s: %f(%d)..%f(%d)\n", name, p->min_f, p->min_idx, p->max_f, p->max_idx);
	}

	va_end(ap);
}

void print_max_min_u1(const char *name, u1_t *data, int len)
{
	int i;
	int max = (int) 0x80000000U, min = (int) 0x7fffffffU;
	int max_idx = -1, min_idx = -1;
	
	for (i=0; i < len; i++) {
		int s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	printf("min/max %s: %d(%d)..%d(%d)\n", name, min, min_idx, max, max_idx);
}

void print_max_min_u2(const char *name, u2_t *data, int len)
{
	int i;
	int max = (int) 0x80000000U, min = (int) 0x7fffffffU;
	int max_idx = -1, min_idx = -1;
	
	for (i=0; i < len; i++) {
		int s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	printf("min/max %s: %d(%d)..%d(%d)\n", name, min, min_idx, max, max_idx);
}

void print_max_min_s2(const char *name, s2_t *data, int len)
{
	int i;
	int max = (int) 0x80000000U, min = (int) 0x7fffffffU;
	int max_idx = -1, min_idx = -1;
	
	for (i=0; i < len; i++) {
		int s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	printf("min/max %s: %d(%d)..%d(%d)\n", name, min, min_idx, max, max_idx);
}

void print_max_min_i(const char *name, int *data, int len)
{
	int i;
	int max = (int) 0x80000000U, min = (int) 0x7fffffffU;
	int max_idx = -1, min_idx = -1;
	
	for (i=0; i < len; i++) {
		int s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	printf("min/max %s(%d): %d(%d)..%d(%d)\n", name, len, min, min_idx, max, max_idx);
}

void print_max_min_f(const char *name, float *data, int len)
{
	int i;
	float max = -1e38, min = 1e38;
	int max_idx = -1, min_idx = -1;
	
	for (i=0; i < len; i++) {
		float s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	//printf("min/max %s: %e(%d)..%e(%d)\n", name, min, min_idx, max, max_idx);
	printf("min/max %s: %f(%d)..%f(%d)\n", name, min, min_idx, max, max_idx);
}

void print_max_min_c(const char *name, TYPECPX *data, int len)
{
	int i;
	float max = -1e38, min = 1e38;
	
	for (i=0; i < len; i++) {
		float s;
		s = data[i].re;
		if (s > max) max = s;
		if (s < min) min = s;
		s = data[i].im;
		if (s > max) max = s;
		if (s < min) min = s;
	}
	
	printf("min/max %s: %e..%e\n", name, min, max);
}

int bits_required(u4_t v)
{
	if (v == 0) return 1;
	int bits;
    for (bits = 31; bits >= 0; bits--)
        if (v & (1 << bits)) break;
    bits++;
	return bits;
}

u4_t snd_hdr[8];

int snd_file_open(const char *fn, int nchans, double srate)
{
    int fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) return fd;
    int srate_i = round(srate);
    snd_hdr[0] = FLIP32(0x2e736e64);    // ".snd"
    snd_hdr[1] = FLIP32(sizeof(snd_hdr));   // header size
    snd_hdr[2] = FLIP32(0xffffffff);    // unknown filesize
    snd_hdr[3] = FLIP32(3);             // 16-bit linear PCM
    snd_hdr[4] = FLIP32(srate_i);       // sample rate
    snd_hdr[5] = FLIP32(nchans*NIQ);    // number of channels
    snd_hdr[6] = FLIP32(0);
    snd_hdr[7] = FLIP32(0);
    write(fd, snd_hdr, sizeof(snd_hdr));
    return fd;
}

FILE *pgm_file_open(const char *fn, int *offset, int width, int height, int depth)
{
    FILE *fp = fopen(fn, "w");
    if (fp != NULL) {
        *offset = fprintf(fp, "P5 %d ", width);
        // reserve space for height (written later with pgm_file_height()) using a fixed-length field
        fprintf(fp, "%6d %d\n", height, depth);
    }
    return fp;
}

void pgm_file_height(FILE *fp, int offset, int height)
{
    fflush(fp);
    fseek(fp, offset, SEEK_SET);
    if (height > 999999) {
        height = 999999;
        lprintf("pgm_file_height: height limited to 999999!\n");
    }
    fprintf(fp, "%6d", height);
}

off_t kiwi_file_size(const char *fn)
{
    struct stat st;
    int n = stat(fn, &st);
    if (n < 0) return -1;
    return st.st_size;
}

bool kiwi_file_exists(const char *fn)
{
    return (kiwi_file_size(fn) >= 0);
}

int kiwi_file_read(const char *id, const char *fn, char *s, int len, bool rem_nl)
{
    int fd = open(fn, O_RDONLY);
    if (fd < 0) {
        printf("%s kiwi_file_read: \"%s\" open %s\n", id, fn, strerror(errno));
        return -1;
    }
    int n = read(fd, s, len);
    if (n < 0) {
        printf("%s kiwi_file_read: \"%s\" read %s\n", id, fn, strerror(errno));
        close(fd);
        return -1;
    }
    
    if (rem_nl && s[n-1] == '\n')
        s[n-1] = '\0';
    
    close(fd);
    return n;
}

int kiwi_file_write(const char *id, const char *fn, char *s, int len, bool add_nl)
{
    int fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        printf("%s kiwi_file_write: \"%s\" open %s\n", id, fn, strerror(errno));
        return -1;
    }

    int n = write(fd, s, len);
    if (n < 0) {
        printf("%s kiwi_file_write: \"%s\" write %s\n", id, fn, strerror(errno));
        close(fd);
        return -1;
    }

    if (add_nl) {
        int nn = write(fd, "\n", 1);
        if (nn < 0) {
            printf("%s kiwi_file_write: \"%s\" write %s\n", id, fn, strerror(errno));
            close(fd);
            return -1;
        }
    }

    close(fd);
    return n;
}

static const char *field = "ABCDEFGHIJKLMNOPQR";
static const char *square = "0123456789";
static const char *subsquare = "abcdefghijklmnopqrstuvwx";

void grid_to_latLon(char *grid, latLon_t *loc)
{
	double lat, lon;
	char c;
	int slen = strlen(grid);
	
	loc->lat = loc->lon = 999.0;
	if (slen < 4) return;
	
	c = tolower(grid[0]);
	if (c < 'a' || c > 'r') return;
	lon = (c-'a')*20 - 180;

	c = tolower(grid[1]);
	if (c < 'a' || c > 'r') return;
	lat = (c-'a')*10 - 90;

	c = grid[2];
	if (c < '0' || c > '9') return;
	lon += (c-'0') * SQ_LON_DEG;

	c = grid[3];
	if (c < '0' || c > '9') return;
	lat += (c-'0') * SQ_LAT_DEG;

	if (slen != 6) {	// assume center of square (i.e. "....ll")
		lon += SQ_LON_DEG /2.0;
		lat += SQ_LAT_DEG /2.0;
	} else {
		c = tolower(grid[4]);
		if (c < 'a' || c > 'x') return;
		lon += (c-'a') * SUBSQ_LON_DEG;

		c = tolower(grid[5]);
		if (c < 'a' || c > 'x') return;
		lat += (c-'a') * SUBSQ_LAT_DEG;

		lon += SUBSQ_LON_DEG /2.0;	// assume center of sub-square (i.e. "......44")
		lat += SUBSQ_LAT_DEG /2.0;
	}

	loc->lat = lat;
	loc->lon = lon;
	//printf("GRID %s%s = (%f, %f)\n", grid, (slen != 6)? "[ll]":"", lat, lon);
}

int latLon_to_grid6(latLon_t *loc, char *grid6)
{
	int i;
	double r, lat, lon;
	
	// longitude
	lon = loc->lon + 180.0;
	if (lon < 0 || lon >= 360.0) return -1;
	i = (int) lon / FLD_DEG_LON;
	grid6[0] = field[i];
	r = lon - (i * FLD_DEG_LON);
	
	i = (int) floor(r / SQ_LON_DEG);
	grid6[2] = square[i];
	r = r - (i * SQ_LON_DEG);
	
	i = (int) floor(r * (SUBSQ_PER_SQ / SQ_LON_DEG));
	grid6[4] = subsquare[i];
	
	// latitude
	lat = loc->lat + 90.0;
	if (lat < 0 || lat >= 180.0) return -1;
	i = (int) lat / FLD_DEG_LAT;
	grid6[1] = field[i];
	r = lat - (i * FLD_DEG_LAT);
	
	i = (int) floor(r / SQ_LAT_DEG);
	grid6[3] = square[i];
	r = r - (i * SQ_LAT_DEG);
	
	i = (int) floor(r * (SUBSQ_PER_SQ / SQ_LAT_DEG));
	grid6[5] = subsquare[i];
	
	return 0;
}

void set_cpu_affinity(int cpu)
{
#if defined(HOST) && defined(MULTI_CORE) && !defined(PLATFORM_raspberrypi)
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu, &cpu_set);
    scall("set_affinity", sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpu_set));
#endif
}

u4_t pos_wrap_diff(u4_t next, u4_t prev, u4_t size)
{
	u4_t diff;
	
	if (next >= prev)
		diff = next - prev;
	else
		diff = ((size-1) - prev) + next;	// i.e. amount outside prev - next
	
	return diff;
}


#define N_TO_UNITS 16
static char toUnits_buf[4][N_TO_UNITS];

char *toUnits(int num, int instance)
{
    char *cp = toUnits_buf[instance];
    float nf;
    
    if (num < 1000) {
        kiwi_snprintf_ptr(cp, N_TO_UNITS, "%d", num);
    } else
    if (num < 1000000) {
        nf = num / 1e3;
        kiwi_snprintf_ptr(cp, N_TO_UNITS, "%.1fk", nf);
    } else
    if (num < 1000000000) {
        nf = num / 1e6;
        kiwi_snprintf_ptr(cp,  N_TO_UNITS,"%.1fM", nf);
    } else {
        nf = num / 1e9;
        kiwi_snprintf_ptr(cp, N_TO_UNITS, "%.1fG", nf);
    }

    return cp;
}
