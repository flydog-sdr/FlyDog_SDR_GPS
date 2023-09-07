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

// Copyright (c) 2014-2022 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "valgrind.h"
#include "rx.h"
#include "clk.h"
#include "misc.h"
#include "str.h"
#include "web.h"
#include "peri.h"
#include "eeprom.h"
#include "spi.h"
#include "spi_dev.h"
#include "gps.h"
#include "coroutines.h"
#include "cfg.h"
#include "net.h"
#include "ext_int.h"
#include "sanitizer.h"
#include "shmem.h"      // shmem_init()
#include "debug.h"
#include "fpga.h"

#include "other.gen.h"

#ifdef EV_MEAS
    #warning NB: EV_MEAS is enabled
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/resource.h>
#include <sched.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

kiwi_t kiwi;

int version_maj, version_min;
int fw_sel, fpga_id, rx_chans, wf_chans, nrx_bufs, nrx_samps, nrx_samps_loop, nrx_samps_rem,
    snd_rate, rx_decim;

int p0=0, p1=0, p2=0, wf_sim, wf_real, wf_time, ev_dump=0, wf_flip, wf_start=1, tone, down,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, do_slice=-1,
	rx_yield=1000, gps_chans=GPS_CHANS, spi_clkg, spi_speed=SPI_48M, wf_max, rx_num, wf_num,
	do_gps, do_sdr=1, navg=1, wf_olap, meas, spi_delay=100, do_fft, debian_ver, monitors_max,
	noisePwr=-160, unwrap=0, rev_iq, ineg, qneg, fft_file, fftsize=1024, fftuse=1024, bg,
	print_stats, ecpu_cmds, ecpu_tcmds, use_spidev, debian_maj, debian_min, test_flag, dx_print,
	gps_debug, gps_var, gps_lo_gain, gps_cg_gain, use_foptim, is_locked, drm_nreg_chans;

u4_t ov_mask, snd_intr_usec;

bool create_eeprom, need_hardware, kiwi_reg_debug, have_ant_switch_ext, gps_e1b_only,
    disable_led_task, is_multi_core, debug_printfs, cmd_debug, anti_aliased;

int main_argc;
char **main_argv;
char *fpga_file;
static bool _kiwi_restart;

void kiwi_restart()
{
    #ifdef USE_ASAN
        // leak detector needs exit while running on main() stack
        _kiwi_restart = true;
        TaskWakeupF(TID_MAIN, TWF_CANCEL_DEADLINE);
        while (1)
            TaskSleepSec(1);
    #else
        kiwi_exit(0);
    #endif
}

#ifdef USE_OTHER
void other_task(void *param)
{
    void other_main(int argc, char *argv[]);
    other_main(main_argc, main_argv);
    kiwi_exit(0);
}
#endif

int main(int argc, char *argv[])
{
	int i;
	int p_gps = 0, gpio_test_pin = 0;
	bool ext_clk = false, err;

	#define FW_CONFIGURED   -2  // -2 because -1 means "other" firmware and 0-N is Kiwi firmware
	#define FW_OTHER        -1
	int fw_sel_override = FW_CONFIGURED;   
	
	version_maj = VERSION_MAJ;
	version_min = VERSION_MIN;
	
	#ifdef MULTI_CORE
	    is_multi_core = true;
	#endif
	
	main_argc = argc;
	main_argv = argv;
	
	#ifdef DEVSYS
		do_sdr = 0;
		p_gps = -1;
	#else
		// enable generation of core file in /tmp
		//scall("core_pattern", system("echo /tmp/core-%e-%s-%p-%t > /proc/sys/kernel/core_pattern"));
		
		// use same filename to prevent looping dumps from filling up filesystem
		scall("core_pattern", system("echo /tmp/core-%e > /proc/sys/kernel/core_pattern"));
		const struct rlimit unlim = { RLIM_INFINITY, RLIM_INFINITY };
		scall("setrlimit", setrlimit(RLIMIT_CORE, &unlim));
		system("rm -f /tmp/core-kiwi.*-*");     // remove old core files
		set_cpu_affinity(0);
	#endif
	
	#ifdef PLATFORM_beaglebone_black
	    kiwi.platform = PLATFORM_BBG_BBB;
	#endif
	
	#ifdef PLATFORM_beaglebone_ai
	    kiwi.platform = PLATFORM_BB_AI;
	#endif
	
	#ifdef PLATFORM_beaglebone_ai64
	    kiwi.platform = PLATFORM_BB_AI64;
	#endif
	
	#ifdef PLATFORM_raspberrypi
	    kiwi.platform = PLATFORM_RPI;
	#endif
	
	kstr_init();
	shmem_init();
	printf_init();
	
	system("mkdir -p " DIR_DATA);

    #define ARG(s) (strcmp(argv[ai], s) == 0)
    #define ARGP() argv[++ai]
    #define ARGL(v) if (ai+1 < argc) (v) = strtol(argv[++ai], 0, 0);
    
	for (int ai = 1; ai < argc; ) {
	    if (strncmp(argv[ai], "-o_", 3) == 0) {
	        fw_sel_override = FW_OTHER;
	        ai++;
		    while (ai < argc && ((argv[ai][0] != '+') && (argv[ai][0] != '-'))) ai++;
	        continue;
	    }
	
		if (ARG("-fw")) { ARGL(fw_sel_override); printf("firmware select override: %d\n", fw_sel_override); } else

		if (ARG("-kiwi_reg")) kiwi_reg_debug = TRUE; else
		if (ARG("-cmd_debug")) cmd_debug = TRUE; else
		if (ARG("-bg")) { background_mode = TRUE; bg=1; } else
		if (ARG("-fopt")) use_foptim = 1; else   // in EDATA_DEVEL mode use foptim version of files
		if (ARG("-down")) down = 1; else
		if (ARG("+gps")) p_gps = 1; else
		if (ARG("-gps")) p_gps = -1; else
		if (ARG("+sdr")) do_sdr = 1; else
		if (ARG("-sdr")) do_sdr = 0; else
		if (ARG("+fft")) do_fft = 1; else
		if (ARG("-debug")) debug_printfs = true; else
		if (ARG("-gps_debug")) { gps_debug = -1; ARGL(gps_debug); } else
		if (ARG("-stats") || ARG("+stats")) { print_stats = STATS_TASK; ARGL(print_stats); } else
		if (ARG("-gpio")) { ARGL(gpio_test_pin); } else
		if (ARG("-v")) {} else      // dummy arg so Kiwi version can appear in e.g. htop
		
		if (ARG("-test")) { ARGL(test_flag); printf("test_flag %d(0x%x)\n", test_flag, test_flag); } else
		if (ARG("-dx")) { ARGL(dx_print); printf("dx %d(0x%x)\n", dx_print, dx_print); } else
		if (ARG("-led") || ARG("-leds")) disable_led_task = true; else
		if (ARG("-gps_e1b")) gps_e1b_only = true; else
		if (ARG("-gps_var")) { ARGL(gps_var); printf("gps_var %d\n", gps_var); } else
		if (ARG("-e1b_lo_gain")) { ARGL(gps_lo_gain); printf("e1b_lo_gain %d\n", gps_lo_gain); } else
		if (ARG("-e1b_cg_gain")) { ARGL(gps_cg_gain); printf("e1b_cg_gain %d\n", gps_cg_gain); } else

		if (ARG("-debian")) {} else     // dummy arg so Kiwi version can appear in e.g. htop
		if (ARG("-ctrace")) { ARGL(web_caching_debug); } else
		if (ARG("-ext")) ext_clk = true; else
		if (ARG("-use_spidev")) { ARGL(use_spidev); } else
		if (ARG("-eeprom")) create_eeprom = true; else
		if (ARG("-sim")) wf_sim = 1; else
		if (ARG("-real")) wf_real = 1; else
		if (ARG("-time")) wf_time = 1; else
		if (ARG("-dump") || ARG("+dump")) { ARGL(ev_dump); } else
		if (ARG("-flip")) wf_flip = 1; else
		if (ARG("-start")) wf_start = 1; else
		if (ARG("-mult")) wf_mult = 1; else
		if (ARG("-multgen")) wf_mult_gen = 1; else
		if (ARG("-wmax")) wf_max = 1; else
		if (ARG("-olap")) wf_olap = 1; else
		if (ARG("-meas")) meas = 1; else
		
		// do_fft
		if (ARG("-none")) unwrap = 0; else
		if (ARG("-norm")) unwrap = 1; else
		if (ARG("-rev")) unwrap = 2; else
		if (ARG("-qi")) rev_iq = 1; else
		if (ARG("-ineg")) ineg = 1; else
		if (ARG("-qneg")) qneg = 1; else
		if (ARG("-file")) fft_file = 1; else
		if (ARG("-fftsize")) { ARGL(fftsize); } else
		if (ARG("-fftuse")) { ARGL(fftuse); } else
		if (ARG("-np")) { ARGL(noisePwr); } else

		if (ARG("-rcordic")) rx_cordic = 1; else
		if (ARG("-rcic")) rx_cic = 1; else
		if (ARG("-rcic2")) rx_cic2 = 1; else
		if (ARG("-rdump")) rx_dump = 1; else
		if (ARG("-wcordic")) wf_cordic = 1; else
		if (ARG("-wcic")) wf_cic = 1; else
		if (ARG("-clkg")) spi_clkg = 1; else
		
		if (ARG("-avg")) { ARGL(navg); } else
		if (ARG("-tone")) { ARGL(tone); } else
		if (ARG("-slc")) { ARGL(do_slice); } else
		if (ARG("-rx")) { ARGL(rx_num); } else
		if (ARG("-wf")) { ARGL(wf_num); } else
		if (ARG("-spispeed")) { ARGL(spi_speed); } else
		if (ARG("-spi")) { ARGL(spi_delay); } else
		if (ARG("-ch")) { ARGL(gps_chans); } else
		if (ARG("-y")) { ARGL(rx_yield); } else
		if (ARG("-p0")) { ARGL(p0); printf("-p0 = %d\n", p0); } else
		if (ARG("-p1")) { ARGL(p1); printf("-p1 = %d\n", p1); } else
		if (ARG("-p2")) { ARGL(p2); printf("-p2 = %d\n", p2); } else
		
		lprintf("unknown arg: \"%s\"\n", argv[ai]);

		ai++;
		while (ai < argc && ((argv[ai][0] != '+') && (argv[ai][0] != '-'))) ai++;
	}
	
	lprintf("KiwiSDR v%d.%d --------------------------------------------------------------------\n",
		version_maj, version_min);
    lprintf("compiled: %s %s on %s\n", __DATE__, __TIME__, COMPILE_HOST);

    #if (defined(DEVSYS) && 0)
        printf("%6s %6s %6s %6s\n", toUnits(1234), toUnits(999800, 1), toUnits(999800777, 2), toUnits(1800777666, 3));
        printf("______ ______ ______ ______\n");
        _exit(0);
    #endif
    
    #if (defined(DEVSYS) && 0)
        for (int i = 0; i < 0xfffd; i++) kstr_wrap((char *) malloc(1));
        printf("kstr_wrap() test WORKED\n");
        _exit(0);
    #endif
    
    #if (defined(DEVSYS) && 0)
        void ale_2g_test();
        ale_2g_test();
        _exit(0);
    #endif

    #if (defined(DEVSYS) && 0)
        printf("kiwi_strnlen(\"sss\"(n),3): 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d\n", kiwi_strnlen(NULL, 3), kiwi_strnlen("a", 3),
            kiwi_strnlen("ab", 3), kiwi_strnlen("abc", 3), kiwi_strnlen("abcd", 3), kiwi_strnlen("abcde", 3));
        _exit(0);
    #endif

    #if (defined(DEVSYS))
        printf("DEVSYS: nothing to do, exiting..\n");
        _exit(0);
    #endif

    char *reply = read_file_string_reply("/etc/debian_version");
    if (reply == NULL) panic("debian_version");
    if (sscanf(kstr_sp(reply), "%d.%d", &debian_maj, &debian_min) != 2) panic("debian_version");
    kstr_free(reply);
    lprintf("/etc/debian_version %d.%d\n", debian_maj, debian_min);
    debian_ver = debian_maj;
    
    #if defined(USE_ASAN)
    	lprintf("### compiled with USE_ASAN\n");
    #endif
    
    #if defined(HOST) && defined(USE_VALGRIND)
    	lprintf("### compiled with USE_VALGRIND\n");
    #endif
    
    assert(TRUE != FALSE);
    assert(true != false);
    assert(TRUE == true);
    assert(FALSE == false);
    assert(NOT_FOUND != TRUE);
    assert(NOT_FOUND != FALSE);
    
    debug_printfs |= kiwi_file_exists(DIR_CFG "/opt.debug")? 1:0;

    #ifndef PLATFORM_raspberrypi
        // on reboot let ntpd and other stuff settle first
        if (background_mode && !kiwi_file_exists("/tmp/.kiwi_no_restart_delay")) {
            lprintf("background mode: delaying start 30 secs...\n");
            system("touch /tmp/.kiwi_no_restart_delay");    // removed on reboot
            sleep(30);
        }
    #endif

	TaskInit();
	misc_init();
    cfg_reload();
    clock_init();

    if (fw_sel_override != FW_CONFIGURED) {
        fw_sel = fw_sel_override;
    } else {
        fw_sel = admcfg_int("firmware_sel", &err, CFG_OPTIONAL);
        if (err) fw_sel = FW_SEL_SDR_RX4_WF4;
    }
    
    bool update_admcfg = false;
    anti_aliased = admcfg_default_bool("anti_aliased", false, &update_admcfg);
    if (update_admcfg) admcfg_save_json(cfg_adm.json);      // during init doesn't conflict with admin cfg
    
    if (fw_sel == FW_SEL_SDR_RX4_WF4) {
        fpga_id = FPGA_ID_RX4_WF4;
        rx_chans = 4;
        wf_chans = 4;
        snd_rate = SND_RATE_4CH;
        rx_decim = RX_DECIM_4CH;
        nrx_bufs = RXBUF_SIZE_4CH / NRX_SPI;
        lprintf("firmware: SDR_RX4_WF4\n");
    } else
    if (fw_sel == FW_SEL_SDR_RX8_WF2) {
        fpga_id = FPGA_ID_RX8_WF2;
        rx_chans = 8;
        wf_chans = 2;
        snd_rate = SND_RATE_8CH;
        rx_decim = RX_DECIM_8CH;
        nrx_bufs = RXBUF_SIZE_8CH / NRX_SPI;
        lprintf("firmware: SDR_RX8_WF2\n");
    } else
    if (fw_sel == FW_SEL_SDR_RX3_WF3) {
        fpga_id = FPGA_ID_RX3_WF3;
        rx_chans = 3;
        wf_chans = 3;
        snd_rate = SND_RATE_3CH;
        rx_decim = RX_DECIM_3CH;
        nrx_bufs = RXBUF_SIZE_3CH / NRX_SPI;
        lprintf("firmware: SDR_RX3_WF3\n");
    } else
    if (fw_sel == FW_SEL_SDR_RX14_WF0) {
        fpga_id = FPGA_ID_RX14_WF0;
        rx_chans = 14;
        wf_chans = 0;
        snd_rate = SND_RATE_14CH;
        rx_decim = RX_DECIM_14CH;
        nrx_bufs = RXBUF_SIZE_14CH / NRX_SPI;
        lprintf("firmware: SDR_RX14_WF0\n");
    } else {
        fpga_id = FPGA_ID_OTHER;
        lprintf("firmware: OTHER\n");
    }
    
    if (fpga_id == FPGA_ID_OTHER) {
        fpga_file = strdup((char *) "other");
    } else {
        asprintf(&fpga_file, "rx%d.wf%d", rx_chans, wf_chans);
    
        bool no_wf = cfg_bool("no_wf", &err, CFG_OPTIONAL);
        if (err) no_wf = false;
        if (no_wf) wf_chans = 0;

        lprintf("firmware: rx_chans=%d wf_chans=%d\n", rx_chans, wf_chans);

        assert(rx_chans <= MAX_RX_CHANS);
        assert(wf_chans <= MAX_WF_CHANS);

        nrx_samps = NRX_SAMPS_CHANS(rx_chans);
        nrx_samps_loop = nrx_samps * rx_chans / NRX_SAMPS_RPT;
        nrx_samps_rem = (nrx_samps * rx_chans) - (nrx_samps_loop * NRX_SAMPS_RPT);
        snd_intr_usec = 1e6 / ((float) snd_rate/nrx_samps);
        lprintf("firmware: RX rx_decim=%d RX1_STD_DECIM=%d RX2_STD_DECIM=%d USE_RX_CICF=%d\n",
            rx_decim, RX1_STD_DECIM, RX2_STD_DECIM, VAL_USE_RX_CICF);
        lprintf("firmware: RX srate=%.3f(%d) bufs=%d samps=%d loop=%d rem=%d intr_usec=%d\n",
            ext_update_get_sample_rateHz(-2), snd_rate, nrx_bufs, nrx_samps, nrx_samps_loop, nrx_samps_rem, snd_intr_usec);

        assert(nrx_bufs <= MAX_NRX_BUFS);
        assert(nrx_samps <= MAX_NRX_SAMPS);
        assert(nrx_samps < FASTFIR_OUTBUF_SIZE);    // see data_pump.h

        lprintf("firmware: WF xfer=%d samps=%d rpt=%d loop=%d rem=%d\n",
            NWF_NXFER, NWF_SAMPS, NWF_SAMPS_RPT, NWF_SAMPS_LOOP, NWF_SAMPS_REM);

        rx_num = rx_chans, wf_num = wf_chans;
        monitors_max = (rx_chans * N_CAMP) + N_QUEUERS;
    }
    
	TaskInitCfg();

    // force enable_gps true because there is no longer an option switch in the admin interface (now uses acquisition checkboxes)
    do_gps = admcfg_default_bool("enable_gps", true, NULL);
    if (!do_gps) {
	    admcfg_set_bool("enable_gps", true);
		admcfg_save_json(cfg_adm.json);     // during init doesn't conflict with admin cfg
		do_gps = 1;
    }
    
    if (p_gps != 0) do_gps = (p_gps == 1)? 1:0;
    
	if (down) do_sdr = do_gps = 0;
	need_hardware = (do_gps || do_sdr);

	// called early, in case another server already running so we can detect the busy socket and bail
	web_server_init(WS_INIT_CREATE);

	if (need_hardware) {
		peri_init();
		fpga_init();
		if (gpio_test_pin) gpio_test(gpio_test_pin);
		//pru_start();
		eeprom_update();
		
		bool ext_ADC_clk = cfg_bool("ext_ADC_clk", &err, CFG_OPTIONAL);
		if (err) ext_ADC_clk = false;
		
		u2_t ctrl = CTRL_EEPROM_WP;
		ctrl_clr_set(0xffff, ctrl);
		if (!(ext_clk || ext_ADC_clk)) ctrl |= CTRL_OSC_EN;
		ctrl_clr_set(0, ctrl);

		net.dna = fpga_dna();
		printf("device DNA %08x|%08x\n", PRINTF_U64_ARG(net.dna));
	}
	
	if (do_fft) {
		printf("==== IQ %s\n", rev_iq? "reverse":"normal");
		if (ineg) printf("==== I neg\n");
		if (qneg) printf("==== Q neg\n");
		printf("==== unwrap %s\n", (unwrap==0)? "none" : ((unwrap==1)? "normal":"reverse"));
	}
	
	rx_server_init();

    #ifdef USE_SDR
        extint_setup();
    #endif

	web_server_init(WS_INIT_START);

	if (do_gps) {
		if (!GPS_CHANS) panic("no GPS_CHANS configured");
        #ifdef USE_GPS
		    gps_main(argc, argv);
		#endif
	}
	
	CreateTask(stat_task, NULL, MAIN_PRIORITY);

    #ifdef USE_OTHER
        if (fw_sel == FW_OTHER) {
	        CreateTask(other_task, NULL, MAIN_PRIORITY);
	    }
    #endif
    
	// run periodic housekeeping functions
	while (TRUE) {
	
		TaskCollect();
		TaskCheckStacks(false);

		TaskSleepReasonSec("main loop", 10);
		
        if (_kiwi_restart) kiwi_exit(0);
	}
}
