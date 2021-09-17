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

// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "printf.h"
#include "timer.h"
#include "web.h"
#include "spi.h"
#include "clk.h"
#include "gps.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "dx.h"
#include "rx.h"
#include "security.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

// process non-websocket connections
char *rx_server_ajax(struct mg_connection *mc)
{
	int i, j, n;
	char *sb, *sb2;
	rx_stream_t *st;
	char *uri = (char *) mc->uri;
	
	if (*uri == '/') uri++;
	
	for (st = rx_streams; st->uri; st++) {
		if (strcmp(uri, st->uri) == 0)
			break;
	}

	if (!st->uri) return NULL;

	// these are okay to process while we're down or updating
	if ((down || update_in_progress || backup_in_progress)
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_DISCOVERY
		)
			return NULL;

	//printf("rx_server_ajax: uri=<%s> qs=<%s>\n", uri, mc->query_string);
	
	// these don't require a query string
	if (mc->query_string == NULL
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_USERS
		&& st->type != AJAX_SNR
		&& st->type != AJAX_DISCOVERY
		&& st->type != AJAX_PHOTO
		) {
		lprintf("rx_server_ajax: missing query string! uri=<%s>\n", uri);
		return NULL;
	}
	
	// iptables will stop regular connection attempts from a blacklisted ip.
	// But when proxied we need to check the forwarded ip address.
	// Note that this code always sets remote_ip[] as a side-effect for later use (the real client ip).
	char remote_ip[NET_ADDRSTRLEN];
    if (check_if_forwarded("AJAX", mc, remote_ip) && check_ip_blacklist(remote_ip)) {
		lprintf("AJAX: IP BLACKLISTED: url=<%s> qs=<%s>\n", uri, mc->query_string);
    	return NULL;
    }

	switch (st->type) {
	
	// SECURITY:
	//	Returns JSON
	//	Done as an AJAX because needed for .js file version checking long before any websocket available
	case AJAX_VERSION:
		asprintf(&sb, "{\"maj\":%d,\"min\":%d}", version_maj, version_min);
		break;

	// SECURITY:
	//	Okay, requires a matching auth key generated from a previously authenticated admin web socket connection
	//	MITM vulnerable
	//	Returns JSON
	case AJAX_PHOTO: {
		char vname[64], fname[64];		// mg_parse_multipart() checks size of these
		const char *data;
		int data_len, rc = 0;
		
		printf("PHOTO UPLOAD REQUESTED from %s len=%d\n", remote_ip, mc->content_len);
		//printf("PHOTO UPLOAD REQUESTED key=%s ckey=%s\n", mc->query_string, current_authkey);
		
		int key_cmp = -1;
		if (mc->query_string && current_authkey) {
			key_cmp = strcmp(mc->query_string, current_authkey);
			kiwi_ifree(current_authkey);
			current_authkey = NULL;
		}
		if (key_cmp != 0)
			rc = 1;
		
		if (rc == 0) {
			mg_parse_multipart(mc->content, mc->content_len,
				vname, sizeof(vname), fname, sizeof(fname), &data, &data_len);
			
			if (data_len < PHOTO_UPLOAD_MAX_SIZE) {
				FILE *fp;
				scallz("fopen photo", (fp = fopen(DIR_CFG "/photo.upload.tmp", "w")));
				scall("fwrite photo", (n = fwrite(data, 1, data_len, fp)));
				fclose(fp);
				
				// do some server-side checking
				char *reply;
				int status;
				reply = non_blocking_cmd("file " DIR_CFG "/photo.upload.tmp", &status);
				if (reply != NULL) {
					if (strstr(kstr_sp(reply), "image data") == 0)
						rc = 2;
					kstr_free(reply);
				} else {
					rc = 3;
				}
			} else {
				rc = 4;
			}
		}
		
		// only clobber the old file if the checks pass
		if (rc == 0)
			system("mv " DIR_CFG "/photo.upload.tmp " DIR_CFG "/photo.upload");
		
		printf("AJAX_PHOTO: data=%p data_len=%d \"%s\" rc=%d\n", data, data_len, fname, rc);
		asprintf(&sb, "{\"r\":%d}", rc);
		break;
	}

	// SECURITY:
	//	Delivery restricted to the local network.
	//	Used by kiwisdr.com/scan -- the KiwiSDR auto-discovery scanner.
	case AJAX_DISCOVERY:
		if (!isLocal_ip(remote_ip)) return (char *) -1;
		asprintf(&sb, "%d %s %s %d %d %s",
			net.serno, net.ip_pub, net.ip_pvt, net.port, net.nm_bits, net.mac);
		printf("/DIS REQUESTED from %s: <%s>\n", remote_ip, sb);
		break;

	// SECURITY:
	//	Delivery restricted to the local network.
	case AJAX_USERS:
		if (!isLocal_ip(remote_ip)) {
			printf("/users NON_LOCAL FETCH ATTEMPT from %s\n", remote_ip);
			return (char *) -1;
		}
		sb = rx_users(true);
		printf("/users REQUESTED from %s\n", remote_ip);
		return sb;		// NB: return here because sb is already a kstr_t (don't want to do kstr_wrap() below)
		break;

	// SECURITY:
	//	Can be requested by anyone.
	//	Returns JSON
	case AJAX_SNR: {
    	bool need_comma1 = false;
    	char *sb = (char *) "[", *sb2;
		for (i = 0; i < SNR_MEAS_MAX; i++) {
            SNR_meas_t *meas = &SNR_meas_data[i];
            if (!meas->valid) continue;
            asprintf(&sb2, "%s{\"ts\":\"%s\",\"seq\":%u,\"utc\":%d,\"ihr\":%d,\"snr\":[",
				need_comma1? ",":"", meas->tstamp, meas->seq, meas->is_local_time? 0:1, snr_meas_interval_hrs);
        	need_comma1 = true;
        	sb = kstr_cat(sb, kstr_wrap(sb2));
        	
        	bool need_comma2 = false;
			for (j = 0; j < SNR_MEAS_NDATA; j++) {
        		SNR_data_t *data = &meas->data[j];
        		if (data->f_lo == 0 && data->f_hi == 0) continue;
				asprintf(&sb2, "%s{\"lo\":%d,\"hi\":%d,\"min\":%d,\"max\":%d,\"p50\":%d,\"p95\":%d,\"snr\":%d}",
					need_comma2? ",":"", data->f_lo, data->f_hi,
					data->min, data->max, data->pct_50, data->pct_95, data->snr);
        		need_comma2 = true;
				sb = kstr_cat(sb, kstr_wrap(sb2));
			}
    		sb = kstr_cat(sb, "]}");
		}
		
    	sb = kstr_cat(sb, "]\n");
		printf("/snr REQUESTED from %s\n", remote_ip);
		return sb;		// NB: return here because sb is already a kstr_t (don't want to do kstr_wrap() below)
		break;
	}

	// SECURITY:
	//	OKAY, used by kiwisdr.com and Priyom Pavlova at the moment
	//	Returns '\n' delimited keyword=value pairs
	case AJAX_STATUS: {
		const char *s1, *s3, *s4, *s5, *s6, *s7;
		
		// if location hasn't been changed from the default try using ipinfo lat/log
		// or, failing that, put us in Antarctica to be noticed
		s4 = cfg_string("rx_gps", NULL, CFG_OPTIONAL);
		const char *gps_loc;
		char *ipinfo_lat_lon = NULL;
		if (strcmp(s4, "(-37.631120, 176.172210)") == 0) {
			if (gps.ipinfo_ll_valid) {
				asprintf(&ipinfo_lat_lon, "(%f, %f)", gps.ipinfo_lat, gps.ipinfo_lon);
				gps_loc = ipinfo_lat_lon;
			} else {
				gps_loc = "(-69.0, 90.0)";		// Antarctica
			}
		} else {
			gps_loc = s4;
		}
		
		// append location to name if none of the keywords in location appear in name
		s1 = cfg_string("rx_name", NULL, CFG_OPTIONAL);
		char *name;
		name = strdup(s1);
		cfg_string_free(s1);

		s5 = cfg_string("rx_location", NULL, CFG_OPTIONAL);
		if (name && s5) {
		
			// hack to include location description in name
			#define NKWDS 8
			char *kwds[NKWDS], *loc, *r_loc;
			loc = strdup(s5);
			n = kiwi_split((char *) loc, &r_loc, ",;-:/()[]{}<>| \t\n", kwds, NKWDS);
			for (i=0; i < n; i++) {
				//printf("KW%d: <%s>\n", i, kwds[i]);
				if (strcasestr(name, kwds[i]))
					break;
			}
			kiwi_ifree(loc); kiwi_ifree(r_loc);
			if (i == n) {
				char *name2;
				asprintf(&name2, "%s | %s", name, s5);
				kiwi_ifree(name);
				name = name2;
				//printf("KW <%s>\n", name);
			}
		}
		
		// If this Kiwi doesn't have any open access (no password required)
		// prevent it from being listed (no_open_access == true and send "auth=password"
		// below which will prevent listing.
		const char *pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
		bool has_pwd = (pwd_s != NULL && *pwd_s != '\0');
		cfg_string_free(pwd_s);
		int chan_no_pwd = rx_chan_no_pwd();
		int users_max = has_pwd? chan_no_pwd : rx_chans;
		int users = MIN(current_nusers, users_max);
		bool no_open_access = (has_pwd && chan_no_pwd == 0);
        bool kiwisdr_com_reg = (admcfg_bool("kiwisdr_com_register", NULL, CFG_OPTIONAL) == 1)? 1:0;
		//printf("STATUS current_nusers=%d users_max=%d users=%d\n", current_nusers, users_max, users);
		//printf("STATUS has_pwd=%d chan_no_pwd=%d no_open_access=%d reg=%d\n", has_pwd, chan_no_pwd, no_open_access, kiwisdr_com_reg);


		// Advertise whether Kiwi can be publicly listed,
		// and is available for use
		//
		// kiwisdr_com_reg:	returned status values:
		//		no		private
		//		yes		active, offline
		
		bool offline = (down || update_in_progress || backup_in_progress);
		const char *status;

		if (!kiwisdr_com_reg) {
			// Make sure to always keep set to private when private
			status = "private";
			users_max = rx_chans;
			users = current_nusers;
		} else
		if (offline)
			status = "offline";
		else
			status = "active";

		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		u4_t avatar_ctime = timer_server_build_unix_time();
		
		int tdoa_ch = cfg_int("tdoa_nchans", NULL, CFG_OPTIONAL);
		if (tdoa_ch == -1) tdoa_ch = rx_chans;		// has never been set
		if (!admcfg_bool("GPS_tstamp", NULL, CFG_REQUIRED)) tdoa_ch = -1;
		
		bool has_20kHz = (snd_rate == SND_RATE_3CH);
		bool has_GPS = (clk.adc_gps_clk_corrections > 8);
		bool has_tlimit = (inactivity_timeout_mins || ip_limit_mins);
		bool has_masked = (dx.masked_len > 0);
		bool has_limits = (has_tlimit || has_masked);
		bool have_DRM_ext = (DRM_enable && (snd_rate == SND_RATE_4CH));
		
		asprintf(&sb,
			"status=%s\n%soffline=%s\n"
			"name=%s\nsdr_hw=KiwiSDR v%d.%d%s%s%s%s%s%s%s%s ⁣\n"
			"op_email=%s\nbands=%.0f-%.0f\nusers=%d\nusers_max=%d\navatar_ctime=%u\n"
			"gps=%s\ngps_good=%d\nfixes=%d\nfixes_min=%d\nfixes_hour=%d\n"
			"tdoa_id=%s\ntdoa_ch=%d\n"
			"asl=%d\nloc=%s\n"
			"sw_version=%s%d.%d\nantenna=%s\nsnr=%d,%d\n"
			"uptime=%d\n"
			"gps_date=%d,%d\ndate=%s\n",
			status, no_open_access? "auth=password\n" : "", offline? "yes":"no",
			name, version_maj, version_min,

			// "nbsp;nbsp;" can't be used here because HTML can't be sent.
			// So a Unicode "invisible separator" #x2063 surrounded by spaces gets the desired double spacing.
			// Edit this by selecting the following lines in BBEdit and doing:
			//		Markup > Utilities > Translate Text to HTML (first menu entry)
			//		CLICK ON "selection only" SO ENTIRE FILE DOESN'T GET EFFECTED
			// This will produce "&#xHHHH;" hex UTF-16 surrogates.
			// Re-encode by doing reverse (second menu entry, "selection only" should still be set).
			
			// To determine UTF-16 surrogates, find desired icon at www.endmemo.com/unicode/index.php
			// Then enter 4 hex UTF-8 bytes into www.ltg.ed.ac.uk/~richard/utf-8.cgi?input=📶&mode=char
			// Resulting hex UTF-16 field can be entered below.

			has_20kHz?						" ⁣ 🎵 20 kHz" : "",
			has_GPS?						" ⁣ 📡 GPS" : "",
			has_limits?						" ⁣ " : "",
			has_tlimit?						"⏳" : "",
			has_masked?						"🚫" : "",
			has_limits?						" LIMITS" : "",
			have_DRM_ext?					" ⁣ 📻 DRM" : "",
			have_ant_switch_ext?			" ⁣ 📶 ANT-SWITCH" : "",

			(s3 = cfg_string("admin_email", NULL, CFG_OPTIONAL)),
			(float) kiwi_reg_lo_kHz * kHz, (float) kiwi_reg_hi_kHz * kHz,
			users, users_max, avatar_ctime,
			gps_loc, gps.good, gps.fixes, gps.fixes_min, gps.fixes_hour,
			(s7 = cfg_string("tdoa_id", NULL, CFG_OPTIONAL)), tdoa_ch,
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			s5,
			"KiwiSDR_v", version_maj, version_min,
			(s6 = cfg_string("rx_antenna", NULL, CFG_OPTIONAL)),
			snr_all, snr_HF,
			timer_sec(),
			gps.set_date? 1:0, gps.date_set? 1:0, utc_ctime_static()
			);

		kiwi_ifree(name);
		kiwi_ifree(ipinfo_lat_lon);
		cfg_string_free(s3);
		cfg_string_free(s4);
		cfg_string_free(s5);
		cfg_string_free(s6);
		cfg_string_free(s7);

		//printf("STATUS REQUESTED from %s: <%s>\n", remote_ip, sb);
		break;
	}

	default:
		return NULL;
		break;
	}

	sb = kstr_wrap(sb);
	//printf("AJAX: RTN <%s>\n", kstr_sp(sb));
	return sb;		// NB: sb is kiwi_ifree()'d by caller
}
