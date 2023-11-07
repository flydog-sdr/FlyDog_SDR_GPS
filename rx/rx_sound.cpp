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

// Copyright (c) 2014-2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "options.h"
#include "config.h"
#include "kiwi.h"
#include "printf.h"
#include "rx.h"
#include "rx_util.h"
#include "clk.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "nbuf.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "cuteSDR.h"
#include "rx_noise.h"
#include "teensy.h"
#include "agc.h"
#include "fir.h"
#include "iir.h"
#include "squelch.h"
#include "debug.h"
#include "data_pump.h"
#include "cfg.h"
#include "mongoose.h"
#include "ima_adpcm.h"
#include "ext_int.h"
#include "rx.h"
#include "fastfir.h"
#include "noiseproc.h"
#include "lms.h"
#include "dx.h"
#include "noise_blank.h"
#include "rx_sound.h"
#include "rx_sound_cmd.h"
#include "rx_waterfall.h"
#include "rx_filter.h"
#include "wdsp.h"
#include "fpga.h"
#include "rf_attn.h"

#ifdef DRM
 #include "DRM.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>

#include <algorithm>


// current tests & workarounds

#define SND_FREQ_SET_IQ_ROTATION_BUG_WORKAROUND

snd_t snd_inst[MAX_RX_CHANS];

#ifdef USE_SDR

// 1st estimate of processing delay
const double gps_delay    = 28926.838e-6;
const double gps_week_sec = 7*24*3600.0;

struct gps_timestamp_t {
    bool   init;
	double gpssec;       // current gps timestamp
	double last_gpssec;  // last gps timestamp
};

gps_timestamp_t gps_ts[MAX_RX_CHANS];

float g_genfreq, g_genampl, g_mixfreq;

// if entries here are ordered by snd_cmd_key_e then the reverse lookup (str_hash_t *)->hashes[key].name
// will work as a debugging aid
static str_hashes_t snd_cmd_hashes[] = {
    { "~~~~~~~~~", STR_HASH_MISS },
    { "SET dbgA", CMD_AUDIO_START },
    { "SET mod=", CMD_TUNE },
    { "SET comp", CMD_COMPRESSION },
    { "SET rein", CMD_REINIT },
    { "SET litt", CMD_LITTLE_ENDIAN },
    { "SET gen=", CMD_GEN_FREQ },
    { "SET gena", CMD_GEN_ATTN },
    { "SET agc=", CMD_SET_AGC },
    { "SET sque", CMD_SQUELCH },
    { "SET nb a", CMD_NB_ALGO },
    { "SET nr a", CMD_NR_ALGO },
    { "SET nb t", CMD_NB_TYPE },
    { "SET nr t", CMD_NR_TYPE },
    { "SET mute", CMD_MUTE },
    { "SET ovld", CMD_OVLD_MUTE },
    { "SET de_e", CMD_DE_EMP },
    { "SET test", CMD_TEST },
    { "SET UAR ", CMD_UAR },
    { "SET AR O", CMD_AR_OKAY },
    { "SET unde", CMD_UNDERRUN },
    { "SET seq=", CMD_SEQ },
    { "SET lms_", CMD_LMS_AUTONOTCH },
    { "SET sam_", CMD_SAM_PLL },
    { "SET wind", CMD_SND_WINDOW_FUNC },
    { "SET spc_", CMD_SPEC },
    { "SET rf_a", CMD_RF_ATTN },
    { 0 }
};

str_hash_t snd_cmd_hash;

void c2s_sound_init()
{
    str_hash_init("snd", &snd_cmd_hash, snd_cmd_hashes);

	//evSnd(EC_DUMP, EV_SND, 10000, "rx task", "overrun");
	
	if (do_sdr) {
		spi_set(CmdSetGenFreq, 0, 0);
		spi_set(CmdSetGenAttn, 0, 0);
	}
}

CAgc m_Agc[MAX_RX_CHANS];

CNoiseProc m_NoiseProc_snd[MAX_RX_CHANS];

CSquelch m_Squelch[MAX_RX_CHANS];

CFastFIR m_PassbandFIR[MAX_RX_CHANS];
CFastFIR m_chan_null_FIR[MAX_RX_CHANS];

CFir m_AM_FIR[MAX_RX_CHANS];

#ifdef OPTION_EXPERIMENT_CICF
    CFir m_CICF_FIR[MAX_RX_CHANS];
#endif

CFir m_nfm_deemp_FIR[MAX_RX_CHANS];     // see: tools/FIR.m
CFir m_am_ssb_deemp_FIR[MAX_RX_CHANS];

CIir m_deemp_Biquad[MAX_RX_CHANS];      // see: tools/biquad.MZT.m

void c2s_sound_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	double frate = ext_update_get_sample_rateHz(-1);
	wdsp_SAM_demod_init();

    //cprintf(conn, "rx%d c2s_sound_setup\n", conn->rx_channel);
	send_msg(conn, SM_SND_DEBUG, "MSG freq_offset=%.3f", freq_offset_kHz);
	send_msg(conn, SM_SND_DEBUG, "MSG center_freq=%d bandwidth=%d adc_clk_nom=%.0f", (int) ui_srate/2, (int) ui_srate, ADC_CLOCK_NOM);
	send_msg(conn, SM_SND_DEBUG, "MSG audio_init=%d audio_rate=%d sample_rate=%.6f", conn->isLocal, snd_rate, frate);

    dx_last_community_download();
}

bool specAF_FFT(int rx_chan, int instance, int flags, int ratio, int ns_out, TYPECPX *samps)
{
    int i;
	snd_t *s = &snd_inst[rx_chan];
    //printf("specAF_FFT %d\n", ns_out);
    #define FFT_WIDTH CONV_FFT_SIZE
    float pwr[FFT_WIDTH];
    u1_t fft[FFT_WIDTH];
    check(ns_out == FFT_WIDTH);

    // limit update rate
    u4_t ms = timer_ms();
    #define UPDATE_MS 125
    if (ms > s->specAF_last_ms + UPDATE_MS) {
        if (s->specAF_last_ms)
            s->specAF_last_ms += UPDATE_MS;
        else
            s->specAF_last_ms = ms;
    } else {
        return false;
    }

    for (i=0; i < FFT_WIDTH; i++) {
        pwr[i] = samps[i].re * samps[i].re;
    }

    float scale = 10.0f * 2.0f / (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL * FFT_WIDTH * FFT_WIDTH);
    scale *= s->isChanNull? 0.0004f : 1e6f;
    //#define PN_SCALE_DEBUG
    #ifdef PN_SCALE_DEBUG
        if (p_f[1]) scale *= p_f[1];
    #endif
	
	for (i=0; i < FFT_WIDTH; i++) {
		float dB = 10.0 * log10f(pwr[i] * scale + (float) 1e-30);
		if (dB > 0) dB = 0;
		if (dB < -200.0) dB = -200.0;
		dB--;

		int unwrap = (i < FFT_WIDTH/2)? FFT_WIDTH/2 : -FFT_WIDTH/2;
		fft[i+unwrap] = (u1_t) (int) dB;
	}

    snd_send_msg_data(rx_chan, false, 0x00, fft, FFT_WIDTH);
    return true;
}

void c2s_sound(void *param)
{
	conn_t *conn = (conn_t *) param;
	rx_common_init(conn);
	conn->snd_cmd_recv_ok = false;
	int rx_chan = conn->rx_channel;
	snd_t *s = &snd_inst[rx_chan];
	wf_inst_t *wf = &WF_SHMEM->wf_inst[rx_chan];
	rx_chan_t *rxc = &rx_channels[rx_chan];
	rx_dpump_t *rx = &rx_dpump[rx_chan];
    iq_buf_t *iq = &RX_SHMEM->iq_buf[rx_chan];
	
	int j, k, n;

	memset(s, 0, sizeof(snd_t));
	s->freq = s->mode = s->gen = s->squelch_on_seq = -1;
	s->agc = 1; s->thresh = -90; s->decay = 50;
	s->compression = 1;
	s->nb_algo = NB_OFF; s->nr_algo = NR_OFF_;
	
	double z1 = 0;

	double frate = ext_update_get_sample_rateHz(rx_chan);      // FIXME: do this in loop to get incremental changes
	//printf("### frate %f snd_rate %d\n", frate, snd_rate);
	#define ATTACK_TIMECONST .01	// attack time in seconds
	float sMeterAlpha = 1.0 - expf(-1.0/((float) frate * ATTACK_TIMECONST));
	float sMeterAvg_dB = 0, sMeter_dBm;
	
    strncpy(s->out_pkt_real.h.id, "SND", 3);
    strncpy(s->out_pkt_iq.h.id,   "SND", 3);
		
	s->seq = 0;
	
    #ifdef SND_SEQ_CHECK
        s->snd_seq_ck_init = false;
    #endif
    
	m_Squelch[rx_chan].SetupParameters(rx_chan, frate);
	m_Squelch[rx_chan].SetSquelch(0, 0);
	
	// don't start data pump until first connection so GPS search can run at full speed on startup
	static bool data_pump_started;
	if (do_sdr && !data_pump_started) {
		data_pump_init();
		data_pump_started = true;
	}
	
	if (do_sdr) {
		//printf("SOUND ENABLE channel %d\n", rx_chan);
		rx_enable(rx_chan, RX_CHAN_ENABLE);
	}

	double adc_clock_corrected = 0;
	s->spectral_inversion = kiwi.spectral_inversion;

    #ifdef SND_FREQ_SET_IQ_ROTATION_BUG_WORKAROUND
        bool first_freq_trig = true, first_freq_set = false;
        u4_t first_freq_time;
    #endif
	
	u4_t dx_update_seq = 0;
	bool masked = false, masked_area = false, check_masked = false;
	bool allow_gps_tstamp = admcfg_bool("GPS_tstamp", NULL, CFG_REQUIRED);	
	int noise_pulse_last = 0;

	u4_t rssi_p = 0;
	bool rssi_filled = false;
	#define N_RSSI 65
	float rssi_q[N_RSSI];

	// Overload muting stuff
	bool squelched_overload = false; // squelch flag specific for the overloading
	bool overload_before = false; // were we overloaded in the previous instant?
	bool overload_flag = false; // are we overloaded now?
	int overload_timer = -1; // keep track of when we stopped being overloaded to allow a decay
	//float max_thr = -35; // this is the maximum signal in dBm before muting (now a global config parameter)
	float last_max_thr = max_thr;
	
	wdsp_SAM_PLL(rx_chan, PLL_MED);
	wdsp_SAM_PLL(rx_chan, PLL_RESET);

	gps_timestamp_t *gps_tsp = &gps_ts[rx_chan];
	memset(gps_tsp, 0, sizeof(gps_timestamp_t));

    // Compensate for audio sample buffer size in FPGA. Normalize to buffer size used for FW_SEL_SDR_RX4_WF4 mode.
	int ref_nrx_samps = NRX_SAMPS_CHANS(8);     // 8-ch mode has the smallest FPGA buffer size
    int norm_nrx_samps;
    double gps_delay2 = 0;
	switch (fw_sel) {
	    case FW_SEL_SDR_RX4_WF4: norm_nrx_samps = nrx_samps - ref_nrx_samps; break;
	    case FW_SEL_SDR_RX8_WF2: norm_nrx_samps = nrx_samps; break;
	    case FW_SEL_SDR_RX14_WF0: norm_nrx_samps = nrx_samps; break;    // FIXME: this is now the smallest buffer size
	    case FW_SEL_SDR_RX3_WF3: const double target = 15960.828e-6;      // empirically measured using GPS 1 PPS input
	                             norm_nrx_samps = (int) (target * SND_RATE_3CH);
	                             gps_delay2 = target - (double) norm_nrx_samps / SND_RATE_3CH; // fractional part of target delay
	                             break;
	}
	//printf("rx_chans=%d norm_nrx_samps=%d nrx_samps=%d ref_nrx_samps=%d gps_delay2=%e\n",
	//    rx_chans, norm_nrx_samps, nrx_samps, ref_nrx_samps, gps_delay2);
	
	//clprintf(conn, "SND INIT conn: %p mc: %p %s:%d %s\n",
	//	conn, conn->mc, conn->remote_ip, conn->remote_port, conn->mc->uri);
	
	#if 0
        if (strcmp(conn->remote_ip, "") == 0)
            cprintf(conn, "SND INIT conn: %p mc: %p %s:%d %s\n",
                conn, conn->mc, conn->remote_ip, conn->remote_port, conn->mc->uri);
    #endif

	nbuf_t *nb = NULL;

	while (TRUE) {
		double f_phase;
		u64_t i_phase;
		
		// reload freq NCO if adc clock has been corrected
		// reload freq NCO if spectral inversion changed
		if (s->freq >= 0 && (adc_clock_corrected != conn->adc_clock_corrected || s->spectral_inversion != kiwi.spectral_inversion)) {
		    //cprintf(conn, "SND UPD adc_clock_corrected=%lf conn->adc_clock_corrected=%lf\n", adc_clock_corrected, conn->adc_clock_corrected);
			adc_clock_corrected = conn->adc_clock_corrected;
            s->spectral_inversion = kiwi.spectral_inversion;

			double freq_kHz = s->freq * kHz;
			double freq_inv_kHz = ui_srate - freq_kHz;
			f_phase = (s->spectral_inversion? freq_inv_kHz : freq_kHz) / conn->adc_clock_corrected;
            i_phase = (u64_t) round(f_phase * pow(2,48));
            //cprintf(conn, "SND UPD freq %.3f kHz i_phase 0x%08x|%08x clk %.6f(%d)\n",
            //    s->freq, PRINTF_U64_ARG(i_phase), conn->adc_clock_corrected, clk.adc_clk_corrections);
            if (do_sdr) spi_set3(CmdSetRXFreq, rx_chan, (u4_t) ((i_phase >> 16) & 0xffffffff), (u2_t) (i_phase & 0xffff));

		    //cprintf(conn, "SND freq updated due to ADC clock correction\n");
		}

        #ifdef SND_FREQ_SET_IQ_ROTATION_BUG_WORKAROUND
            if (first_freq_set) {
                //#define DOUBLE_SET_DELAY 500        // only 90% reliable!
                #define DOUBLE_SET_DELAY 1500
                if (timer_ms() > first_freq_time + DOUBLE_SET_DELAY) {
                    double freq_kHz = s->freq * kHz;
                    double freq_inv_kHz = ui_srate - freq_kHz;
                    f_phase = (s->spectral_inversion? freq_inv_kHz : freq_kHz) / conn->adc_clock_corrected;
                    i_phase = (u64_t) round(f_phase * pow(2,48));
                    //cprintf(conn, "SND DOUBLE-SET freq %.3f kHz i_phase 0x%08x|%08x clk %.6f rx_chan=%d\n",
                    //    s->freq, PRINTF_U64_ARG(i_phase), conn->adc_clock_corrected, rx_chan);
                    spi_set3(CmdSetRXFreq, rx_chan, (u4_t) ((i_phase >> 16) & 0xffffffff), (u2_t) (i_phase & 0xffff));
                    first_freq_set = false;
                }
            }
        #endif

		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
		if (n) {
		    rx_sound_cmd(conn, frate, n, nb->buf);
		    continue;
		}
        check(nb == NULL);

		if (!do_sdr) {
			NextTask("SND skip");
			continue;
		}

		if (rx_chan >= rx_num) {
			TaskSleepMsec(1000);
			continue;
		}
		
		if (conn->stop_data) {
			//clprintf(conn, "SND stop_data rx_server_remove()\n");
			rx_enable(rx_chan, RX_CHAN_FREE);
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		// no keep-alive seen for a while or the bug where the initial cmds are not received and the connection hangs open
		// and locks-up a receiver channel
		conn->keep_alive = conn->internal_connection? 0 : (timer_sec() - conn->keepalive_time);
		bool keepalive_expired = (conn->keep_alive > (conn->auth? KEEPALIVE_SEC : KEEPALIVE_SEC_NO_AUTH));
		bool connection_hang = (conn->keepalive_count > 4 && s->cmd_recv != CMD_ALL);
		if (keepalive_expired || connection_hang || conn->inactivity_timeout || conn->kick) {
			//if (keepalive_expired) clprintf(conn, "SND KEEP-ALIVE EXPIRED\n");
			//if (connection_hang) clprintf(conn, "SND CONNECTION HANG\n");
			//if (conn->inactivity_timeout) clprintf(conn, "SND INACTIVITY T/O\n");
			//if (conn->kick) clprintf(conn, "SND KICK\n");
			
			if (keepalive_expired && !conn->auth) {
			    cprintf(conn, "PWD entry timeout\n");
			    send_msg(conn, SM_NO_DEBUG, "MSG password_timeout");
			}
		
			// Ask waterfall task to stop (must not do while, for example, holding a lock).
			// We've seen cases where the sound connects, then times out. But the w/f has never connected.
			// So have to check for conn->other being valid.
			conn_t *cwf = conn->other;
			if (cwf && cwf->type == STREAM_WATERFALL && cwf->rx_channel == conn->rx_channel) {
				cwf->stop_data = TRUE;
				
				// do this only in sound task: disable data pump channel
				rx_enable(rx_chan, RX_CHAN_DISABLE);	// W/F will free rx_chan[]
			} else {
				rx_enable(rx_chan, RX_CHAN_FREE);		// there is no W/F, so free rx_chan[] now
			}
			
			//clprintf(conn, "SND rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

        // set arrived when "ident_user=" received or if too much time has passed without it being received
        if (!conn->arrived && (conn->ident || ((s->cmd_recv & CMD_FREQ) && timer_sec() > (conn->arrival + 15)))) {
            if (!conn->ident)
			    kiwi_str_redup(&conn->ident_user, "user", (char *) "(no identity)");
            rx_loguser(conn, LOG_ARRIVED);
            conn->arrived = TRUE;
        }

		// don't process any audio data until we've received all necessary commands
		if (s->cmd_recv != CMD_ALL) {
			TaskSleepMsec(100);
			continue;
		}
		
		if (!conn->snd_cmd_recv_ok) {
			#ifdef TR_SND_CMDS
				clprintf(conn, "SND cmd_recv ALL 0x%x/0x%x\n", s->cmd_recv, CMD_ALL);
			#endif
            rx_enable(rx_chan, RX_DATA_ENABLE);
			conn->snd_cmd_recv_ok = true;
		}
		
		if (s->change_freq_mode || dx_update_seq != dx.update_seq) {

            // apply masked frequencies
            masked = masked_area = false;
            if (dx.masked_len != 0) {
                int f = round(s->freq*kHz);
                int pb_lo = f + s->locut;
                int pb_hi = f + s->hicut;
                //printf("SND f=%d lo=%.0f|%d hi=%.0f|%d ", f, s->locut, pb_lo, s->hicut, pb_hi);
                for (j=0; j < dx.masked_len; j++) {
                    dx_mask_t *dmp = &dx.masked_list[j];
                    if (!((pb_hi < dmp->masked_lo || pb_lo > dmp->masked_hi))) {
                        masked_area = true;     // needed by c2s_sound_camp()
                        masked = conn->tlimit_exempt_by_pwd? false : true;
                        //printf("MASKED");
                        break;
                    }
                }
                //printf("\n");
            }
		    dx_update_seq = dx.update_seq;
        }

        #ifdef OPTION_EXPERIMENT_CICF
            if (s->cicf_setup) {
                // defaults should give ntaps=17
                float pass = 1800;
                float stop = 6000;
                float attn = 90;
                int ntaps = m_CICF_FIR[rx_chan].InitLPFilter(0, 1.0, attn, pass, stop, frate, true);
                printf("m_CICF_FIR pass=%.0f stop=%.0f attn=%.0f, ntaps=%d\n", pass, stop, attn, ntaps);
                s->cicf_run = true;
                s->cicf_setup = false;
            }
        #endif
		
		#define	SND_FLAG_LPF		    0x01
		#define	SND_FLAG_ADC_OVFL	    0x02
		#define	SND_FLAG_NEW_FREQ	    0x04
		#define	SND_FLAG_MODE_IQ	    0x08
		#define SND_FLAG_COMPRESSED     0x10
		#define SND_FLAG_RESTART        0x20
		#define SND_FLAG_SQUELCH_UI     0x40
		#define SND_FLAG_LITTLE_ENDIAN  0x80
		
		bool isNBFM = (s->mode == MODE_NBFM || s->mode == MODE_NNFM);
		bool IQ_or_DRM_or_stereo = (s->mode == MODE_IQ || s->mode == MODE_DRM || s->mode == MODE_SAS || s->mode == MODE_QAM);

		u1_t *bp_real_u1  = s->out_pkt_real.u1;
		s2_t *bp_real_s2  = s->out_pkt_real.s2;
		u1_t *bp_iq_u1    = s->out_pkt_iq.u1;
		s2_t *bp_iq_s2    = s->out_pkt_iq.s2;
		u1_t *flags    = (IQ_or_DRM_or_stereo? &s->out_pkt_iq.h.flags : &s->out_pkt_real.h.flags);
		u1_t *seq      = (IQ_or_DRM_or_stereo? s->out_pkt_iq.h.seq    : s->out_pkt_real.h.seq);
		u1_t *smeter   = (IQ_or_DRM_or_stereo? s->out_pkt_iq.h.smeter : s->out_pkt_real.h.smeter);

		bool do_de_emp = (!IQ_or_DRM_or_stereo && ((isNBFM && s->deemp_nfm) || (!isNBFM && s->deemp)));
		
		#ifdef DRM
            drm_t *drm = &DRM_SHMEM->drm[rx_chan];
        #endif

		u2_t bc = 0;
    
        ext_receive_iq_samps_t receive_iq_pre_fir   = ext_users[rx_chan].receive_iq_pre_fir;
		ext_receive_S_meter_t receive_S_meter       = ext_users[rx_chan].receive_S_meter;
		ext_receive_iq_samps_t receive_iq_pre_agc   = isNBFM? NULL : ext_users[rx_chan].receive_iq_pre_agc;
		tid_t receive_iq_pre_agc_tid                = isNBFM? (tid_t) NULL : ext_users[rx_chan].receive_iq_pre_agc_tid;
		ext_receive_iq_samps_t receive_iq_post_agc  = isNBFM? NULL : ext_users[rx_chan].receive_iq_post_agc;
		tid_t receive_iq_post_agc_tid               = isNBFM? (tid_t) NULL : ext_users[rx_chan].receive_iq_post_agc_tid;
		ext_receive_real_samps_t receive_real       = ext_users[rx_chan].receive_real;
		tid_t receive_real_tid                      = ext_users[rx_chan].receive_real_tid;
		
		int ns_out;
		int fir_pos;

        do {    // while (bc < LOOP_BC)
			while (rx->wr_pos == rx->rd_pos) {
				evSnd(EC_EVENT, EV_SND, -1, "rx_snd", "sleeping");

                //#define MEAS_SND_LOOP
                #ifdef MEAS_SND_LOOP
                    u4_t quanta = FROM_VOID_PARAM(TaskSleepReason("check pointers"));
                    static u4_t last, cps, max_quanta, sum_quanta;
                    u4_t now = timer_sec();
                    if (last != now) {
                        for (; last < now; last++) {
                            if (last < (now-1))
                                real_printf("s- ");
                            else
                                real_printf("s%d|%d/%d ", cps, sum_quanta/(cps? cps:1), max_quanta);
                            fflush(stdout);
                        }
                        max_quanta = sum_quanta = 0;
                        cps = 0;
                    } else {
                        if (quanta > max_quanta) max_quanta = quanta;
                        sum_quanta += quanta;
                        cps++;
                    }
                #else
				    TaskSleepReason("check pointers");
                #endif
			}
			
        	TaskStat2(TSTAT_INCR|TSTAT_ZERO, 0, "aud");

			TYPECPX *i_samps_c = rx->in_samps[rx->rd_pos];

			// check 48-bit ticks counter timestamp in audio IQ stream
			const u64_t ticks   = rx->ticks[rx->rd_pos];
			const u64_t dt      = time_diff48(ticks, clk.ticks);  // time difference to last GPS solution
#if 0
			static u64_t last_ticks[MAX_RX_CHANS] = {0};
			static u4_t  tick_seq[MAX_RX_CHANS]   = {0};
			const u64_t diff_ticks = time_diff48(ticks, last_ticks[rx_chan]); // time difference to last buffer
			
			if ((tick_seq[rx_chan] % 32) == 0) printf("ticks %08x|%08x %08x|%08x // %08x|%08x %08x|%08x #%d,%d GPST %f\n",
											 PRINTF_U64_ARG(ticks), PRINTF_U64_ARG(diff_ticks),
											 PRINTF_U64_ARG(dt), PRINTF_U64_ARG(clk.ticks),
											 clk.adc_gps_clk_corrections, clk.adc_clk_corrections, clk.gps_secs);
			if (diff_ticks != rx_decim * nrx_samps)
				printf("ticks %08x|%08x %08x|%08x // %08x|%08x %08x|%08x #%d,%d GPST %f (%d) *****\n",
					   PRINTF_U64_ARG(ticks), PRINTF_U64_ARG(diff_ticks),
					   PRINTF_U64_ARG(dt), PRINTF_U64_ARG(clk.ticks),
					   clk.adc_gps_clk_corrections, clk.adc_clk_corrections, clk.gps_secs,
					   tick_seq[rx_chan]);
			last_ticks[rx_chan] = ticks;
			tick_seq[rx_chan]++;
#endif
			gps_tsp->gpssec = fmod(gps_week_sec + clk.gps_secs + dt/clk.adc_clock_base - gps_delay + gps_delay2, gps_week_sec);

		    #ifdef SND_SEQ_CHECK
		        if (rx->in_seq[rx->rd_pos] != s->snd_seq_ck) {
		            if (!s->snd_seq_ck_init) {
		                s->snd_seq_ck_init = true;
		            } else {
		                real_printf("rx%d: got %d expecting %d\n", rx_chan, rx->in_seq[rx->rd_pos], s->snd_seq_ck);
		            }
		            s->snd_seq_ck = rx->in_seq[rx->rd_pos];
		        }
		        s->snd_seq_ck++;
		    #endif
		    
			rx->rd_pos = (rx->rd_pos+1) & (N_DPBUF-1);
			
			const int ns_in = nrx_samps;
			
            // Forward IQ samples if requested.
            // Remember that receive_iq_*() is used by some extensions to pushback test data, e.g. DRM
            if (receive_iq_pre_fir != NULL)
                receive_iq_pre_fir(rx_chan, 0, ns_in, i_samps_c);

			
            if (s->nb_enable[NB_CLICK] == NB_PRE_FILTER) {
                u4_t now = timer_sec();
                if (now != noise_pulse_last) {
                    noise_pulse_last = now;
                    TYPEREAL pulse = s->nb_param[NB_CLICK][NB_PULSE_GAIN] * (K_AMPMAX - 16);
                    for (int i=0; i < s->nb_param[NB_CLICK][NB_PULSE_SAMPLES]; i++) {
                        i_samps_c[i].re = pulse;
                        i_samps_c[i].im = 0;
                    }
                    //real_printf("[CLICK-PRE]"); fflush(stdout);
                }
            }

            //#define NB_STD_POST_FILTER
            #ifdef NB_STD_POST_FILTER
            #else
                if (s->nb_enable[NB_BLANKER] && s->nb_algo == NB_STD)
		            m_NoiseProc_snd[rx_chan].ProcessBlanker(ns_in, i_samps_c, i_samps_c);
		    #endif
		    
		    #ifdef OPTION_EXPERIMENT_CICF
		        if (s->cicf_run) {
                    m_CICF_FIR[rx_chan].ProcessFilter(ns_in, i_samps_c, i_samps_c);
                    //real_printf("."); fflush(stdout);
		        }
		    #endif

            TYPECPX *f_samps_c = &iq->iq_samples[iq->iq_wr_pos][0];
            ns_out  = m_PassbandFIR[rx_chan].ProcessData(rx_chan, ns_in, i_samps_c, f_samps_c);
            fir_pos = m_PassbandFIR[rx_chan].FirPos();

			// FIR has a pipeline delay:
			//   gpssec=          t_0     t_1   t_2   t_3   t_4     t_5   t_6   t_7
			//                     v       v     v     v     v       v     v     v
			//   ns_in|ns_out = 170|512 170|0 170|0 170|0 170|512 170|0 170|0 170|512 ... (170*3 = 510)
			//                    ^                          ^                   ^
			//                   (a) fir_pos=0              (b) fir_pos=168     (c) fir_pos=166
			// GPS start times of 512 sample buffers:
			//  * @a : t_0 +  170     (no samples in the FIR buffer)
			//  * @b : t_4 +  170-168 (there are already 168 samples in the FIR buffer)
			// NB: 4 processing chunks when fir_pos=0, 3 otherwise
			
		    //real_printf("ns_in,out=%2d|%3d fir_pos=%d\n", ns_in, ns_out, fir_pos); fflush(stdout);

            #if 0
                for (int i=0; i < ns_in; i++) {
                    TYPECPX *in = &i_samps_c[i];
                    if (in->re > 32767.0f) { real_printf("FIR-in %.1f ", in->re); fflush(stdout); }
                    static TYPEREAL max_in;
                    if (in->re > max_in) { max_in = in->re; real_printf("MAX-IN %.1f ", max_in); fflush(stdout); }
                }
                if (ns_out) for (int i=0; i < ns_out; i++) {
                    TYPECPX *out = &f_samps_c[i];
                    if (out->re > 32767.0f) { real_printf("FIR-o%d re %.1f ", i, out->re); fflush(stdout); }
                    if (out->im > 32767.0f) { real_printf("FIR-o%d im %.1f ", i, out->im); fflush(stdout); }
                    static TYPEREAL max_out;
                    if (out->re > max_out) { max_out = out->re; real_printf("MAX-OUT %.1f ", max_out); fflush(stdout); }
                }
            #endif
            
            if (ns_out == 0)
                continue;
			
            // correct GPS timestamp for offset in the FIR filter
            //  (1) delay in FIR filter
            int sample_filter_delays = norm_nrx_samps - fir_pos;
            //  (2) delay in AGC (if on)
            if (s->agc)
                sample_filter_delays -= m_Agc[rx_chan].GetDelaySamples();
            gps_tsp->gpssec = fmod(gps_week_sec + gps_tsp->gpssec + rx_decim * sample_filter_delays / clk.adc_clock_base,
                                          gps_week_sec);
    
            // FIXME: could this block be moved outside the loop since it only effects the header?
            s->out_pkt_iq.h.gpssec  = u4_t(gps_tsp->last_gpssec);
            s->out_pkt_iq.h.gpsnsec = gps_tsp->init? u4_t(1e9*(gps_tsp->last_gpssec - s->out_pkt_iq.h.gpssec)) : 0;
            // real_printf("__GPS__ gpssec=%.9f diff=%.9f\n",  gps_tsp->gpssec, gps_tsp->gpssec - gps_tsp->last_gpssec);
            const double dt_to_pos_sol = gps_tsp->last_gpssec - clk.gps_secs;
            s->out_pkt_iq.h.last_gps_solution = gps_tsp->init? ((clk.ticks == 0)? 255 : u1_t(std::min(254.0, dt_to_pos_sol))) : 0;
            if (!gps_tsp->init) gps_tsp->init = true;
            s->out_pkt_iq.h.dummy = 0;
            gps_tsp->last_gpssec = gps_tsp->gpssec;
    
			iq->iq_seqnum[iq->iq_wr_pos] = iq->iq_seq;
			iq->iq_seq++;

            // Forward IQ samples if requested.
            // Remember that receive_iq_*() is used by some extensions to pushback test data, e.g. DRM
            if (receive_iq_pre_agc != NULL)
                receive_iq_pre_agc(rx_chan, 0, ns_out, f_samps_c);
            
            if (receive_iq_pre_agc_tid != (tid_t) NULL)
                TaskWakeupFP(receive_iq_pre_agc_tid, TWF_CHECK_WAKING, TO_VOID_PARAM(rx_chan));
    
            // delay updating iq_wr_pos until after AGC applied below
            
            TYPECPX *s_samps_c = f_samps_c;
            for (j=0; j<ns_out; j++) {
    
                // S-meter from CuteSDR
                // FIXME: Why is SND_MAX_VAL less than CUTESDR_MAX_VAL again?
                // And does this explain the need for SMETER_CALIBRATION?
                // Can't remember how this evolved..
                #define SND_MAX_VAL ((float) ((1 << (CUTESDR_SCALE-2)) - 1))
                #define SND_MAX_PWR (SND_MAX_VAL * SND_MAX_VAL)
                float re = (float) s_samps_c->re, im = (float) s_samps_c->im;
                float pwr = re*re + im*im;
                float pwr_dB = 10.0 * log10f((pwr / SND_MAX_PWR) + 1e-30);
                sMeterAvg_dB = (1.0 - sMeterAlpha)*sMeterAvg_dB + sMeterAlpha*pwr_dB;
                s_samps_c++;
            
                // forward S-meter samples if requested
                // S-meter value in audio packet is sent less often than if we send it from here
                if (receive_S_meter != NULL && (j == 0 || j == ns_out/2))
                    receive_S_meter(rx_chan, sMeterAvg_dB + S_meter_cal);
            }
            sMeter_dBm = sMeterAvg_dB + S_meter_cal;
            
            TYPEMONO16 *r_samps_r;
            
            if (!IQ_or_DRM_or_stereo) {
                r_samps_r = &rx->real_samples[rx->real_wr_pos][0];
                rx->freqHz[rx->real_wr_pos] = conn->freqHz;
                rx->real_seqnum[rx->real_wr_pos] = rx->real_seq;
                rx->real_seq++;
            }
            
            /*  Signal path:
            
                rx->in_samps => TYPECPX *i_samps_c
                m_PassbandFIR(i_samps_c, => TYPECPX *f_samps_c)
                s_meter(f_samps_c(s_samps_c))
            
                if (!IQ_or_DRM_or_stereo) rx->real_samples => TYPEMONO16 *r_samps_r     // output buf
            
                switch (mode) {     // f_samps_c => r_samps_r
                    MODE_AMx
                        m_Agc(f_samps_c, => TYPECPX *a_samps_c)
                        demod(a_samps_c) => TYPEREAL *d_samps_r
                        m_AM_FIR(d_samps_r, => r_samps_r)
            
                    MODE_SAMx/QAM
                        m_Agc(f_samps_c, => TYPECPX *a_samps_c)
                        wdsp_SAM_demod(a_samps_c, => r_samps_r)
                            m_chan_null_FIR(a_samps_c, NULL)        // has side-effect internal output
            
                    MODE NxFM
                        m_Agc(f_samps_c, => TYPECPX *a_samps_c)
                        demod(a_samps_c) => TYPEREAL *d_samps_r
                        m_Squelch(d_samps_r, => r_samps_r)
            
                    MODE_IQ/DRM
                        (f_samps_c used later)
            
                    MODE_SSB
                        m_Agc(f_samps_c, => TYPEREAL *r_samps_r)
                }
            
                do_de_emp: m_nfm_deemp_FIR/m_am_ssb_deemp_FIR(r_samps_r, => r_samps_r)
                !IQ_or_DRM_or_stereo: <noise_blankers>(r_samps_r, => r_samps_r)     // algos are non-IQ only
                non_NBFM_squelch()
            
                switch (output_mode) {
                    IQ SAS QAM DRM-monitor-mode     // i.e. 2ch modes
                        SAS QAM
                            r_samps_r(rx->agc_samples) => TYPECPX *cs_c
                        IQ DRM-monitor-mode
                            f_samps_c => TYPECPX *cs_c
                            m_Agc(cs_c, cs_c)
                        cs_c => OUT
                
                    other-non-DRM
                        r_samps_r => OUT
                
                    DRM
                        drm_buf->out_samples => OUT
                }
            
            */
            
            switch (s->mode) {
            
            case MODE_AM:
            case MODE_AMN: {
                // AM detector from CuteSDR
                TYPECPX *a_samps_c = rx->agc_samples;
                m_Agc[rx_chan].ProcessData(ns_out, f_samps_c, a_samps_c);
    
                TYPEREAL *d_samps_r = rx->demod_samples;
    
                for (j=0; j<ns_out; j++) {
                    float pwr = a_samps_c->re*a_samps_c->re + a_samps_c->im*a_samps_c->im;
                    float mag = sqrt(pwr);

                    // high pass filter (DC removal) with IIR filter
                    // H(z) = (1 - z^-1) / (1 - DC_ALPHA*z^-1)
                    #define DC_ALPHA 0.99f
                    float z0 = mag + (z1 * DC_ALPHA);
                    *d_samps_r = z0-z1;
                    z1 = z0;
                    d_samps_r++;
                    a_samps_c++;
                }
                
                // clean up residual noise left by detector
                // the non-FFT FIR has no pipeline delay issues
                d_samps_r = rx->demod_samples;
                m_AM_FIR[rx_chan].ProcessFilter(ns_out, d_samps_r, r_samps_r);
                break;
            }
            
            case MODE_SAM:
            case MODE_SAL:
            case MODE_SAU:
            case MODE_SAS:
            case MODE_QAM: {
                TYPECPX *a_samps_c = rx->agc_samples;
                m_Agc[rx_chan].ProcessData(ns_out, f_samps_c, a_samps_c);

                // NB:
                //      MODE_SAS/QAM stereo mode: output samples put back into a_samps_c
                //      chan null mode: in addition to r_samps_r output, compute FFT of nulled a_samps_c
                s->isChanNull = wdsp_SAM_demod(rx_chan, s->mode, s->SAM_mparam, ns_out, a_samps_c, r_samps_r);
                s->specAF_instance = s->isChanNull? SND_INSTANCE_FFT_CHAN_NULL : SND_INSTANCE_FFT_PASSBAND;
                if (s->isChanNull) m_chan_null_FIR[rx_chan].ProcessData(rx_chan, ns_out, a_samps_c, NULL);
                break;
            }
            
            case MODE_NBFM:
            case MODE_NNFM: {
                TYPEREAL *d_samps_r = rx->demod_samples;
                TYPECPX *a_samps_c = rx->agc_samples;
                m_Agc[rx_chan].ProcessData(ns_out, f_samps_c, a_samps_c);
                
                //#define PN_F_DEBUG
                #ifdef PN_F_DEBUG
                    if (p_f[0]) {
                        TYPEREAL fm_scale = p_f[0];
                        for (j=0; j < ns_out; j++) {
                            a_samps_c[j].re /= fm_scale; a_samps_c[j].im /= fm_scale;
                            #define SHOW_MAX_MIN_FM_IN
                            #ifdef SHOW_MAX_MIN_FM_IN
                                static void *FM_in_state;
                                print_max_min_stream_f(&FM_in_state, P_MAX_MIN_RANGE, "FM_IN", j, 2, (double) a_samps_c[j].re, (double) a_samps_c[j].im);
                            #endif
                        }
                    }
                #endif
                
                // FM demod from CSDR: github.com/simonyiszk/csdr
                // Output clipper is mandatory to prevent artifacts that make the audio "rough" sounding.
                // See NFM fmdemod_quadri_cf command pipeline example.
                // See also:
                //      www.embedded.com/design/configurable-systems/4212086/DSP-Tricks--Frequency-demodulation-algorithms-
                //      www.pa3fwm.nl/technotes/tn24-fm-noise.html
                #define MAX_NBFM_VAL 32767
                //#define PN_F_DEBUG
                #ifdef PN_F_DEBUG
                    float max_val = p_f[1]? fabsf(p_f[1]) : MAX_NBFM_VAL;
                    float clipper_val = p_f[2]? p_f[2] : CLIPPER_NBFM_VAL;
                #else
                    const float max_val = MAX_NBFM_VAL;
                    const float clipper_val = CLIPPER_NBFM_VAL;
                #endif

                #define FMDEMOD_QUADRI_K 0.340447550238101026565118445432744920253753662109375
                float i = a_samps_c->re, q = a_samps_c->im, out;
                float iL = conn->last_sample.re, qL = conn->last_sample.im;
                float pwr = i*i + q*q;
                out = pwr? (max_val * FMDEMOD_QUADRI_K * (i*(q-qL) - q*(i-iL)) / pwr) : 0;
                if (clipper_val > 0) out = PN_CLAMP(out, clipper_val);
                *d_samps_r = out;
                a_samps_c++; d_samps_r++;
                
                for (j=1; j < ns_out; j++) {
                    i = a_samps_c->re, q = a_samps_c->im;
                    iL = a_samps_c[-1].re, qL = a_samps_c[-1].im;
                    pwr = i*i + q*q;
                    out = pwr? (max_val * FMDEMOD_QUADRI_K * (i*(q-qL) - q*(i-iL)) / pwr) : 0;
                    if (clipper_val > 0) out = PN_CLAMP(out, clipper_val);

                    //#define SHOW_MAX_MIN_FM_OUT
                    #ifdef SHOW_MAX_MIN_FM_OUT
                        static void *FM_out_state;
                        print_max_min_stream_f(&FM_out_state, (p_f[1] < 0)? P_MAX_MIN_RESET : P_MAX_MIN_RANGE, "FM_OUT", j-1, 1, (double) out);
                    #endif

                    *d_samps_r = out;
                    a_samps_c++; d_samps_r++;
                }
                
                conn->last_sample = a_samps_c[-1];
                d_samps_r = rx->demod_samples;
    
                // use the noise squelch from CuteSDR
                // nsq_nc_sq = -1_0_+1
                int nsq_nc_sq = m_Squelch[rx_chan].PerformFMSquelch(ns_out, d_samps_r, r_samps_r);
                if (nsq_nc_sq != 0) s->squelched = (nsq_nc_sq == 1)? true:false;
                break;
            }
            
            case MODE_IQ:
            case MODE_DRM:
                break;
            
            case MODE_USB:
            case MODE_USN:
            case MODE_LSB:
            case MODE_LSN:
            case MODE_CW:
            case MODE_CWN:
                m_Agc[rx_chan].ProcessData(ns_out, f_samps_c, r_samps_r);
                break;
    
            default:
                panic("mode");
            }
    
            if (do_de_emp) {    // !IQ_or_DRM_or_stereo
                if (isNBFM) {
                    m_nfm_deemp_FIR[rx_chan].ProcessFilter(ns_out, r_samps_r, r_samps_r);
                } else {
                    #ifdef TEST_AM_SSB_BIQUAD
                        m_deemp_Biquad[rx_chan].ProcessFilter(ns_out, r_samps_r, r_samps_r);
                    #else
                        m_am_ssb_deemp_FIR[rx_chan].ProcessFilter(ns_out, r_samps_r, r_samps_r);
                    #endif
                }
            }
            
            if (s->nb_enable[NB_CLICK] == NB_POST_FILTER) {
                u4_t now = timer_sec();
                if (now != noise_pulse_last) {
                    noise_pulse_last = now;
                    TYPEMONO16 pulse = s->nb_param[NB_CLICK][NB_PULSE_GAIN] * (K_AMPMAX - 16);
                    for (int i=0; i < s->nb_param[NB_CLICK][NB_PULSE_SAMPLES]; i++) {
                        r_samps_r[i] = pulse;
                    }
                    //real_printf("[CLICK-POST]"); fflush(stdout);
                }
            }

            // noise & autonotch processors that only operate on real samples (i.e. non-IQ)
            if (!IQ_or_DRM_or_stereo) {
                if (s->nb_enable[NB_BLANKER]) {
                    switch (s->nb_algo) {
                        #ifdef NB_STD_POST_FILTER
                            case NB_STD: m_NoiseProc_snd[rx_chan].ProcessBlanker(ns_out, r_samps_r, r_samps_r); break;
                        #endif
                        case NB_WILD: nb_Wild_process(rx_chan, ns_out, r_samps_r, r_samps_r); break;
                    }
                }
                
                // ordered so denoiser can cleanup residual noise from autonotch
                switch (s->nr_algo) {
                    case NR_WDSP:
                        if (s->nr_enable[NR_AUTONOTCH]) wdsp_ANR_filter(rx_chan, NR_AUTONOTCH, ns_out, r_samps_r, r_samps_r);
                        if (s->nr_enable[NR_DENOISE]) wdsp_ANR_filter(rx_chan, NR_DENOISE, ns_out, r_samps_r, r_samps_r);
                        break;

                    case NR_ORIG:
                        if (s->nr_enable[NR_AUTONOTCH]) m_LMS[rx_chan][NR_AUTONOTCH].ProcessFilter(ns_out, r_samps_r, r_samps_r);
                        if (s->nr_enable[NR_DENOISE]) m_LMS[rx_chan][NR_DENOISE].ProcessFilter(ns_out, r_samps_r, r_samps_r);
                        break;

                    case NR_SPECTRAL:
                        nr_spectral_process(rx_chan, ns_out, r_samps_r, r_samps_r);
                        break;
                }
            }
            
            // non-NBFM squelch
            if ((s->squelch || s->sq_changed) && !isNBFM && s->mode != MODE_DRM) {
                if (!rssi_filled || s->squelch_on_seq == -1) {
                    rssi_q[rssi_p++] = sMeter_dBm;
                    if (rssi_p >= N_RSSI) { rssi_p = 0; rssi_filled = true; }
                }

                bool squelch_off = (s->squelch == 0);
                bool rtn_is_open = squelch_off? true:false;
                if (!squelch_off && rssi_filled) {
                    float median_nf = median_f(rssi_q, N_RSSI);
                    float rssi_thresh = median_nf + s->squelch;
                    bool is_open = (s->squelch_on_seq != -1);
                    if (is_open) rssi_thresh -= 6;      // hysteresis
                    bool rssi_green = (sMeter_dBm >= rssi_thresh);
                    if (rssi_green) {
                        s->squelch_on_seq = s->seq;
                        is_open = true;
                    }
                    
                    rtn_is_open = is_open;
                    if (!is_open) rtn_is_open = false; 
                    if (s->seq > s->squelch_on_seq + s->tail_delay) {
                        s->squelch_on_seq = -1;
                        rtn_is_open = false; 
                    }

                    //cprintf(conn, "squelch=%d rssi_p=%02d rssi_filled=%d median_nf=%.0f mute=%d sMeter_dBm=%.0f >= rssi_thresh=%.0f(%d) rssi_green=%d squelch_on_seq=%d squelched=%d\n",
                    //    s->squelch, rssi_p, rssi_filled, median_nf, s->mute, sMeter_dBm, rssi_thresh, is_open, rssi_green, s->squelch_on_seq, s->squelched);
                } else {
                    //cprintf(conn, "squelch=%d rssi_p=%02d rssi_filled=%d mute=%d sMeter_dBm=%.0f squelch_on_seq=%d squelched=%d\n",
                    //    s->squelch, rssi_p, rssi_filled, s->mute, sMeter_dBm, s->squelch_on_seq, s->squelched);
                }

                s->squelched = (!rtn_is_open);
                if (s->sq_changed) s->sq_changed = false;
            }

            // mute receiver if overload is detected
            // use the same tail_delay parameter used for the squelch
            if (s->mute_overload) {
                //#define TEST_OVLD_MUTE
                #ifdef TEST_OVLD_MUTE
                    max_thr = -90;
                #endif
            	overload_flag = (sMeter_dBm >= max_thr)? true:false;
            	if (overload_before && !overload_flag) {
            		if (s->seq > overload_timer + s->tail_delay+1) {
            			overload_timer = -1;
            			squelched_overload = false;
            		}
            		else { 
            			squelched_overload = true;
            		}
            	}
                if (overload_flag) {
                	squelched_overload = true;
                	overload_before = true;
                	overload_timer = s->seq;
                }
            } else {
                squelched_overload = overload_before = overload_flag = false;
                overload_timer = -1;
            }
            
            // update UI with admin changes to max_thr
            float max_thr_t = s->mute_overload? max_thr : +90;
            if (max_thr_t != last_max_thr) {
                send_msg(conn, false, "MSG max_thr=%.0f", roundf(max_thr_t));
                last_max_thr = max_thr_t;
            }

            ////////////////////////////////
            // copy to output buffer and send to client
            ////////////////////////////////
            
            #define SILENCE_VALUE 1     // non-zero to prevent triggering the Firefox "goes silent" watchdog
            //bool send_silence = (masked || s->squelched || squelched_overload);
            bool send_silence = masked;     // enforced on server side to prevent clients from cheating

            // IQ/stereo output modes (except non-monitor mode DRM)
            if (s->mode == MODE_IQ || s->mode == MODE_SAS || s->mode == MODE_QAM
            #ifdef DRM
                // DRM monitor mode is effectively the same as MODE_IQ
                || (s->mode == MODE_DRM && (drm->monitor || rx_chan >= DRM_MAX_RX))
            #endif
            ) {
                TYPECPX *cs_c;
                if (s->mode == MODE_SAS || s->mode == MODE_QAM) {
                    cs_c = rx->agc_samples;
                } else {
                    cs_c = f_samps_c;
                    if (!send_silence)
                        m_Agc[rx_chan].ProcessData(ns_out, cs_c, cs_c);

                    // Forward IQ samples if requested.
                    // Remember that receive_iq_*() is used by some extensions to pushback test data, e.g. DRM
                    if (receive_iq_post_agc != NULL)
                        receive_iq_post_agc(rx_chan, 0, ns_out, cs_c);
            
                    if (receive_iq_post_agc_tid != (tid_t) NULL)
                        TaskWakeupFP(receive_iq_post_agc_tid, TWF_CHECK_WAKING, TO_VOID_PARAM(rx_chan));

                    iq->iq_wr_pos = (iq->iq_wr_pos+1) & (N_DPBUF-1);    // after AGC above
                }

                #if 0
                    if (ns_out) for (int i=0; i < ns_out; i++) {
                        TYPECPX *out = &cs_c[i];
                        if (out->re > 32767.0) real_printf("IQ-out %.1f\n", out->re);
                    }
                #endif
                
                if (send_silence) {
                    TYPECPX *sp = cs_c;
                    for (int i = 0; i < ns_out; i++) { sp->re = sp->im = SILENCE_VALUE; sp++; }
                }
                
                if (s->little_endian) {
                    bc = ns_out * NIQ * sizeof(s2_t);
                    for (j=0; j < ns_out; j++) {
                        // can cast TYPEREAL directly to s2_t due to choice of CUTESDR_SCALE
                        s2_t re = (s2_t) cs_c->re, im = (s2_t) cs_c->im;
                        *bp_iq_s2++ = re;      // arm native little-endian (put any swap burden on client)
                        *bp_iq_s2++ = im;
                        cs_c++;
                    }
                } else {
                    for (j=0; j < ns_out; j++) {
                        // can cast TYPEREAL directly to s2_t due to choice of CUTESDR_SCALE
                        s2_t re = (s2_t) cs_c->re, im = (s2_t) cs_c->im;
                        *bp_iq_u1++ = (re >> 8) & 0xff; bc++;  // choose a network byte-order (big-endian)
                        *bp_iq_u1++ = (re >> 0) & 0xff; bc++;
                        *bp_iq_u1++ = (im >> 8) & 0xff; bc++;
                        *bp_iq_u1++ = (im >> 0) & 0xff; bc++;
                        cs_c++;
                    }
                }
            } else
            
            // all other modes (except DRM)
            if (s->mode != MODE_DRM) {
                iq->iq_wr_pos = (iq->iq_wr_pos+1) & (N_DPBUF-1);
                int freqHz = rx->freqHz[rx->real_wr_pos];
                rx->real_wr_pos = (rx->real_wr_pos+1) & (N_DPBUF-1);
    
                // Forward real samples if requested.
                // Remember that receive_real() is used by some extensions to pushback test data.
                if (receive_real != NULL)
                    receive_real(rx_chan, 0, ns_out, r_samps_r, freqHz);
                
                if (receive_real_tid != (tid_t) NULL)
                    TaskWakeupFP(receive_real_tid, TWF_CHECK_WAKING, TO_VOID_PARAM(rx_chan));
    
                if (send_silence) {
                    TYPEMONO16 *rs_r = r_samps_r;
                    for (int i = 0; i < ns_out; i++) *rs_r++ = SILENCE_VALUE;
                }
                
                if (s->compression) {
                    encode_ima_adpcm_i16_e8(r_samps_r, bp_real_u1, ns_out, &s->adpcm_snd);
                    bp_real_u1 += ns_out/2;		// fixed 4:1 compression
                    bc += ns_out/2;
                } else {
                    // can cast TYPEREAL directly to s2_t due to choice of CUTESDR_SCALE
                    if (s->little_endian) {
                        bc += ns_out * sizeof(s2_t);
                        for (j=0; j < ns_out; j++) {
                            *bp_real_s2++ = *r_samps_r++;    // arm native little-endian (put any swap burden on client)
                        }
                    } else {
                        for (j=0; j < ns_out; j++) {
                            *bp_real_u1++ = (*r_samps_r >> 8) & 0xff; bc++;	// choose a network byte-order (big-endian)
                            *bp_real_u1++ = (*r_samps_r >> 0) & 0xff; bc++;
                            r_samps_r++;
                        }
                    }
                }
            }
            
            #ifdef DRM
            
                // Data comes from DRM output routine writing directly to drm_buf[] and updating out_wr_pos.
                // Send silence if buffers are not updated in time.
                
                else
                if (s->mode == MODE_DRM) {
                    m_Agc[rx_chan].ProcessData(ns_out, f_samps_c, f_samps_c);
                    iq->iq_wr_pos = (iq->iq_wr_pos+1) & (N_DPBUF-1);    // after AGC above

                    drm_buf_t *drm_buf = &DRM_SHMEM->drm_buf[rx_chan];
                    int pkt_remain = FASTFIR_OUTBUF_SIZE;
                    
                    int bufs = pos_wrap_diff(drm_buf->out_wr_pos, drm_buf->out_rd_pos, N_DRM_OBUF);
                    int remain = drm_buf->out_samps - drm_buf->out_pos;
                    int avail_samples = bufs? (remain + (bufs-1) * drm_buf->out_samps) : 0;
                    //{ real_printf("d%d as%d ", bufs, avail_samples); fflush(stdout); }
                    //{ real_printf("d%d ", bufs); fflush(stdout); }
                    
                    if (avail_samples < FASTFIR_OUTBUF_SIZE) {
                        drm_t *drm = &DRM_SHMEM->drm[0];
                        drm->sent_silence++;
                        send_silence = true;
                    }
                    
                    if (send_silence) {     // so waterfall keeps going (stays synced)
                        // non-zero to keep FF silence detector from being tripped
                        if (s->little_endian) {
                            bc += pkt_remain * NIQ * sizeof(s2_t);
                            for (j=0; j < pkt_remain; j++) {
                                *bp_iq_s2++ = SILENCE_VALUE;     // arm native little-endian (put any swap burden on client)
                                *bp_iq_s2++ = SILENCE_VALUE;
                            }
                        } else {
                            for (j=0; j < pkt_remain; j++) {
                                *bp_iq_u1++ = 0; bc++;     // choose a network byte-order (big-endian)
                                *bp_iq_u1++ = SILENCE_VALUE; bc++;
                                *bp_iq_u1++ = 0; bc++;
                                *bp_iq_u1++ = SILENCE_VALUE; bc++;
                            }
                        }
                    } else {
                        while (pkt_remain) {
                            TYPESTEREO16 *o_samps_s = &drm_buf->out_samples[drm_buf->out_rd_pos][drm_buf->out_pos];
                            int samps = MIN(pkt_remain, remain);
    
                            if (s->little_endian) {
                                bc += samps * NIQ * sizeof(s2_t);
                                for (j=0; j < samps; j++) {
                                    *bp_iq_s2++ = o_samps_s->left;   // arm native little-endian (put any swap burden on client)
                                    *bp_iq_s2++ = o_samps_s->right;
                                    o_samps_s++;
                                    pkt_remain--;
                                }
                            } else {
                                for (j=0; j < samps; j++) {
                                    *bp_iq_u1++ = (o_samps_s->left >> 8) & 0xff; bc++;	// choose a network byte-order (big-endian)
                                    *bp_iq_u1++ = (o_samps_s->left >> 0) & 0xff; bc++;
                                    *bp_iq_u1++ = (o_samps_s->right >> 8) & 0xff; bc++;
                                    *bp_iq_u1++ = (o_samps_s->right >> 0) & 0xff; bc++;
                                    o_samps_s++;
                                    pkt_remain--;
                                }
                            }
                            drm_buf->out_pos += samps;
                            if (drm_buf->out_pos == drm_buf->out_samps) {
                                drm_buf->out_pos = 0;
                                drm_buf->out_rd_pos = (drm_buf->out_rd_pos + 1) & (N_DRM_OBUF-1);
                            }
                        }
                    }
                }
            #endif

        } while (bc < LOOP_BC);     // multiple loops when compressing

        NextTask("s2c begin");
                
        // send s-meter data with each audio packet
        // -127 -126 .. 3.4 dBm => 0 1 .. 130.4 (+127) => 0 10 .. 1304 (*10)
        #define SMETER_BIAS 127.0
        if (sMeter_dBm < -127.0) sMeter_dBm = -127.0; else
        if (sMeter_dBm >    3.4) sMeter_dBm =    3.4;
        u2_t sMeter = (u2_t) ((sMeter_dBm + SMETER_BIAS) * 10);
        SET_BE_U16(smeter, sMeter);

        *flags = 0;
        if (dpump.rx_adc_ovfl) *flags |= SND_FLAG_ADC_OVFL;
        if (IQ_or_DRM_or_stereo) *flags |= SND_FLAG_MODE_IQ;
        if (s->compression && !IQ_or_DRM_or_stereo) *flags |= SND_FLAG_COMPRESSED;
        if (s->squelched || squelched_overload) *flags |= SND_FLAG_SQUELCH_UI;
        if (s->little_endian) *flags |= SND_FLAG_LITTLE_ENDIAN;

        if (s->change_LPF) {
            *flags |= SND_FLAG_LPF;
            s->change_LPF = false;
        }

        if (s->change_freq_mode) {
            *flags |= SND_FLAG_NEW_FREQ;
            s->change_freq_mode = false;
        }

        if (s->restart) {
            *flags |= SND_FLAG_RESTART;
            s->restart = false;
        }

        // send sequence number that waterfall syncs to on client-side
        s->seq++;
        SET_LE_U32(seq, s->seq);
        wf->snd_seq = s->seq;
        //{ real_printf("%d ", s->seq & 1); fflush(stdout); }
        //{ real_printf("q%d ", s->seq); fflush(stdout); }

        //printf("hdr %d S%d\n", sizeof(out_pkt.h), bc); fflush(stdout);
        int aud_bytes;
        int c2s_sound_camp(rx_chan_t *rxc, conn_t *conn, u1_t flags, char *bp, int bytes, int aud_bytes, bool masked_area);

        if (IQ_or_DRM_or_stereo) {
            // allow GPS timestamps to be seen by internal extensions
            // but selectively remove from external connections (see admin page security tab)
            if (!allow_gps_tstamp) {
                s->out_pkt_iq.h.last_gps_solution = 0;
                s->out_pkt_iq.h.gpssec = 0;
                s->out_pkt_iq.h.gpsnsec = 0;
            }
            const int bytes = sizeof(s->out_pkt_iq.h) + bc;
            app_to_web(conn, (char*) &s->out_pkt_iq, bytes);
            aud_bytes = sizeof(s->out_pkt_iq.h.smeter) + bc;
            if (rxc->n_camp)
                aud_bytes += c2s_sound_camp(rxc, conn, *flags, (char*) &s->out_pkt_iq, bytes, aud_bytes, masked_area);
        } else {
            const int bytes = sizeof(s->out_pkt_real.h) + bc;
            app_to_web(conn, (char*) &s->out_pkt_real, bytes);
            aud_bytes = sizeof(s->out_pkt_real.h.smeter) + bc;
            if (rxc->n_camp)
                aud_bytes += c2s_sound_camp(rxc, conn, *flags, (char*) &s->out_pkt_real, bytes, aud_bytes, masked_area);
        }

        audio_bytes[rx_chan] += aud_bytes;
        audio_bytes[rx_chans] += aud_bytes;     // [rx_chans] is the sum of all audio channels

        NextTask("s2c end");
	}
}

int c2s_sound_camp(rx_chan_t *rxc, conn_t *conn, u1_t flags, char *bp, int bytes, int aud_bytes, bool masked_area)
{
    int i, n;
    int rx_chan = conn->rx_channel;
	snd_t *s = &snd_inst[rx_chan];
    int additional_bytes = 0;
    
    for (i = 0, n = 0; i < n_camp; i++) {
        conn_t *conn_mon = rxc->camp_conn[i];
        if (conn_mon == NULL) continue;
        
        // detect camping connection has gone away
        if (!conn_mon->valid || conn_mon->type != STREAM_MONITOR || conn_mon->remote_port != rxc->camp_id[i]) {
            //cprintf(conn, ">>> CAMPER gone rx%d type=%d id=%d/%d slot=%d/%d\n",
            //    rx_chan, conn_mon->type, conn_mon->remote_port, rxc->camp_id[i], i+1, n_camp);
            rxc->camp_conn[i] = NULL;
            rxc->n_camp--;
            continue;
        }

        if (!conn_mon->camp_init) {
            //cprintf(conn_mon, ">>> CAMP init rx%d slot=%d/%d\n", rx_chan, i+1, n_camp);
            double frate = ext_update_get_sample_rateHz(-1);
            send_msg(conn_mon, SM_SND_DEBUG, "MSG center_freq=%d bandwidth=%d adc_clk_nom=%.0f", (int) ui_srate/2, (int) ui_srate, ADC_CLOCK_NOM);
            send_msg(conn_mon, SM_SND_DEBUG, "MSG audio_camp=0,%d audio_rate=%d sample_rate=%.6f", conn->isLocal, snd_rate, frate);
            send_msg(conn_mon, SM_SND_DEBUG, "MSG audio_adpcm_state=%d,%d", s->adpcm_snd.index, s->adpcm_snd.previousValue);
            //cprintf(c, "MSG audio_adpcm_state=%d,%d seq=%d\n", s->adpcm_snd.index, s->adpcm_snd.previousValue, s->seq);
            conn_mon->isMasked = false;
            conn_mon->camp_init = conn_mon->camp_passband = true;
        } else {
            if (conn_mon->camp_passband || (flags & SND_FLAG_LPF)) {
                send_msg(conn_mon, SM_SND_DEBUG, "MSG audio_passband=%.0f,%.0f", s->locut, s->hicut);
                //cprintf(conn_mon, "MSG audio_passband=%.0f,%.0f\n", s->locut, s->hicut);
                conn_mon->camp_passband = false;
            }
            
            // adpcm state has to be sent *before* generation/transmission of new compressed data
            //cprintf(conn_mon, "rx%d masked_area=%d isMasked=%d\n", rx_chan, masked_area, conn_mon->isMasked);
            if (masked_area && !conn_mon->isMasked && !conn_mon->tlimit_exempt_by_pwd) {
                send_msg(conn_mon, false, "MSG isMasked=1");
                conn_mon->isMasked = true;
            } else
            if (!masked_area && conn_mon->isMasked) {
                send_msg(conn_mon, false, "MSG isMasked=0");
                conn_mon->isMasked = false;
            }
            
            if (!conn_mon->isMasked) {
                app_to_web(conn_mon, bp, bytes);
            }
            additional_bytes += aud_bytes;
        }
        
        n++;
    }
    
    if (n != rxc->n_camp) {
        cprintf(conn, ">>> WARNING n(%d) != n_camp(%d)\n", n, rxc->n_camp);
        rxc->n_camp = n;
    }
    
    return additional_bytes;
}

void c2s_sound_shutdown(void *param)
{
    conn_t *c = (conn_t*)(param);
    //cprintf(c, "rx%d c2s_sound_shutdown mc=0x%x\n", c->rx_channel, c->mc);
    if (c && c->mc) {
        rx_server_websocket(WS_MODE_CLOSE, c->mc);
    }
}

#endif
