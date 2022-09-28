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

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "web.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "rx.h"
#include "services.h"

#include <types.h>
#include <unistd.h>
#include <sys/stat.h>

bool update_pending = false, update_task_running = false, update_in_progress = false;
int pending_maj = -1, pending_min = -1;

enum fail_reason_e {
    FAIL_NONE = 0,
    FAIL_FS_FULL = 1, FAIL_NO_INET = 2, FAIL_NO_GITHUB = 3, FAIL_GIT = 4,
    FAIL_MAKEFILE = 5, FAIL_BUILD = 6
};
fail_reason_e fail_reason;

static void update_build_ctask(void *param)
{
    int status;
	bool build_normal = true;
	
    //#define BUILD_SHORT_MF
    //#define BUILD_SHORT
    #if defined(BUILD_SHORT_MF) || defined(BUILD_SHORT)
        bool force_build = (bool) FROM_VOID_PARAM(param);
        if (force_build) {
            #if defined(BUILD_SHORT_MF)
                status = system("cd /root/" REPO_NAME "; mv Makefile.1 Makefile; rm -f /root/build/obj/r*.o; make >>/root/build.log 2>&1");
                build_normal = false;
            #elif defined(BUILD_SHORT)
                status = system("cd /root/" REPO_NAME "; rm -f /root/build/obj_O3/u*.o; make >>/root/build.log 2>&1");
                build_normal = false;
            #endif
            child_status_exit(status);
	        child_exit(EXIT_SUCCESS);
        }
    #endif

	if (build_normal) {
	
	    // run git directly rather than depending on the Makefile to be intact
	    // (for the failure case when the Makefile has become zero length)
	    char *cmd_p;
	    asprintf(&cmd_p, "cd /root/" REPO_NAME "; echo ======== building.. >>/root/build.log; date >>/root/build.log; " \
		    "git clean -fd >>/root/build.log 2>&1; " \
		    "git checkout . >>/root/build.log 2>&1; " \
		);
		status = system(cmd_p);
		kiwi_ifree(cmd_p);
        child_status_exit(status);

        struct stat st;
        bool use_git_proto = kiwi_file_exists(DIR_CFG "/opt.git_no_https");
	    asprintf(&cmd_p, "cd /root/" REPO_NAME "; " \
	        "git pull -v %s://github.com/flydog-sdr/FlyDog_SDR_GPS.git >>/root/build.log 2>&1; ", \
		    use_git_proto? "git" : "https" \
		);
		status = system(cmd_p);
		kiwi_ifree(cmd_p);
        status = child_status_exit(status, NO_ERROR_EXIT);
        
        // try again using github.com well-known public ip address (failure mode when ISP messes with github.com DNS)
        // must use git: protocol otherwise https: cert mismatch error will occur
        if (status != 0) {
            asprintf(&cmd_p, "cd /root/" REPO_NAME "; " \
                "git pull -v git://" GITHUB_COM_PUBLIC_IP "/flydog-sdr/FlyDog_SDR_GPS.git >>/root/build.log 2>&1; "
            );
            status = system(cmd_p);
            kiwi_ifree(cmd_p);
            child_status_exit(status);
        }

        // starting with v1.365 the "make clean" below replaced the former "make clean_dist"
        // so that $(BUILD_DIR)/obj_keep stays intact across updates
        status = system("cd /root/" REPO_NAME "; make clean >>/root/build.log 2>&1; make >>/root/build.log 2>&1; make install >>/root/build.log 2>&1;");
        child_status_exit(status);
        system("cd /root/" REPO_NAME "; date >>/root/build.log; echo ======== build complete >>/root/build.log");
	}
	
	child_exit(EXIT_SUCCESS);
}

static void fetch_makefile_ctask(void *param)
{
    system("echo ======== checking for update >/root/build.log; date >>/root/build.log");

	int status = system("cd /root/" REPO_NAME " ; git fetch origin >>/root/build.log 2>&1");
	if (status != 0)
        printf("UPDATE: fetch origin status=0x%08x\n", status);
	child_status_exit(status);

	status = system("cd /root/" REPO_NAME " ; git show origin:Makefile >Makefile.1 2>>/root/build.log");
	if (status != 0)
        printf("UPDATE: show origin:Makefile status=0x%08x\n", status);
	child_status_exit(status);

    system("cd /root/" REPO_NAME " ; diff Makefile Makefile.1 >>/root/build.log; echo ======== version check complete >>/root/build.log");
	child_exit(EXIT_SUCCESS);
}

static void report_result(conn_t *conn)
{
	// let admin interface know result
	assert(conn != NULL);
	char *date_m = kiwi_str_encode((char *) __DATE__);
	char *time_m = kiwi_str_encode((char *) __TIME__);
	send_msg(conn, false, "MSG update_cb="
	    "{\"f\":%d,\"p\":%d,\"i\":%d,\"r\":%d,\"g\":%d,"
	    "\"v1\":%d,\"v2\":%d,\"p1\":%d,\"p2\":%d,\"d\":\"%s\",\"t\":\"%s\"}",
		fail_reason, update_pending, update_in_progress, rx_chans, GPS_CHANS,
		version_maj, version_min, pending_maj, pending_min, date_m, time_m);
	kiwi_ifree(date_m);
	kiwi_ifree(time_m);
}

static bool daily_restart = false;
static bool ip_auto_download_check = false;
static bool ip_auto_download_oneshot = false;

/*
    // task
    _update_task()
        status = child_task(fetch_makefile_ctask)
	    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	        error ...
        status = child_task(update_build_ctask)
	    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	        error ...

    child_task(func)
        if (fork())
            // child
            func() -> fetch_makefile_ctask() / update_build_ctask()
                status = system(...)
                if (status < 0)
                    child_exit(EXIT_FAILURE);
                if (WIFEXITED(status))
                    child_exit(WEXITSTATUS(status));
                child_exit(EXIT_FAILURE);
        // parent
        while
            waitpid(&status)
        return status
*/

static void _update_task(void *param)
{
	conn_t *conn = (conn_t *) FROM_VOID_PARAM(param);
	bool force_check = (conn && conn->update_check == FORCE_CHECK);
	bool force_build = (conn && conn->update_check == FORCE_BUILD);
	bool report = (force_check || force_build);
	bool ver_changed, update_install;
	int status;
	fail_reason = FAIL_NONE;
	
	lprintf("UPDATE: checking for updates\n");
	if (force_check) update_pending = false;    // don't let pending status override version reporting when a forced check
	
	// NB debug mode: use of "cd /root/" REPO_NAME "; " very important here as a potential git re-clone causes
	// the current directory of the kiwid process to be deleted! So a subsequent forced build via the
	// update tab "build now" button would otherwise fail.
	
    #define FS_USE "cd /root/" REPO_NAME "; df . | tail -1 | /usr/bin/tr -s ' ' | cut -d' ' -f 5 | grep '100%'"
    //#define FS_USE "cd /root/" REPO_NAME "; df . | tail -1 | /usr/bin/tr -s ' ' | cut -d' ' -f 5 | grep '100%' | true"
    status = non_blocking_cmd_system_child("kiwi.ck_fs", FS_USE, POLL_MSEC(250));
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        lprintf("UPDATE: Filesystem is FULL!\n");
        fail_reason = FAIL_FS_FULL;
		if (report) report_result(conn);
		goto common_return;
    }

    #define PING_INET "cd /root/" REPO_NAME "; ping -qc2 1.1.1.1 >/dev/null 2>&1"
    //#define PING_INET "cd /root/" REPO_NAME "; ping -qc2 192.0.2.1"     // will never answer (RFC 5737)
    status = non_blocking_cmd_system_child("kiwi.ck_inet", PING_INET, POLL_MSEC(250));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        lprintf("UPDATE: No Internet connection? (can't ping 1.1.1.1)\n");
        fail_reason = FAIL_NO_INET;
		if (report) report_result(conn);
		goto common_return;
    }

    #define PING_GITHUB "cd /root/" REPO_NAME "; git show origin:Makefile >/dev/null 2>&1"
    //#define PING_GITHUB "cd /root/" REPO_NAME "; git show origin:MakefileXXX"
    status = non_blocking_cmd_system_child("kiwi.ck_ghub", PING_GITHUB, POLL_MSEC(250));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        lprintf("UPDATE: No connection to github.com?\n");
        fail_reason = FAIL_NO_GITHUB;
		if (report) report_result(conn);
		goto common_return;
    }

    #define CHECK_GIT "cd /root/" REPO_NAME "; git fetch origin >/dev/null 2>&1"
    //#define CHECK_GIT "false"
    status = non_blocking_cmd_system_child("kiwi.ck_git", CHECK_GIT, POLL_MSEC(250));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        lprintf("UPDATE: Git clone damaged!\n");
        fail_reason = FAIL_GIT;
		if (report) report_result(conn);
		goto common_return;
    }

	// Run fetch in a Linux child process otherwise this thread will block and cause trouble
	// if the check is invoked from the admin page while there are active user connections.
	status = child_task("kiwi.update", fetch_makefile_ctask, POLL_MSEC(1000));

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		lprintf("UPDATE: Makefile update failed -- check /root/build.log file\n");
        fail_reason = FAIL_MAKEFILE;
		if (report) report_result(conn);
		goto common_return;
	}
	
	FILE *fp;
	scallz("fopen Makefile.1", (fp = fopen("/root/" REPO_NAME "/Makefile.1", "r")));
		int n1, n2;
		n1 = fscanf(fp, "VERSION_MAJ = %d\n", &pending_maj);
		n2 = fscanf(fp, "VERSION_MIN = %d\n", &pending_min);
	fclose(fp);
	
	ver_changed = (n1 == 1 && n2 == 1 && (pending_maj > version_maj  || (pending_maj == version_maj && pending_min > version_min)));
	update_install = (admcfg_bool("update_install", NULL, CFG_REQUIRED) == true);
	
	if (force_check) {
		if (ver_changed)
			lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but check only\n",
				version_maj, version_min, pending_maj, pending_min);
		else
			lprintf("UPDATE: running most current version\n");
		
		report_result(conn);
		goto common_return;
	} else

	if (ver_changed && !update_install && !force_build) {
		lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but update install not enabled\n",
			version_maj, version_min, pending_maj, pending_min);
	} else
	
	if (ver_changed || force_build) {
		lprintf("UPDATE: version changed%s, current %d.%d, new %d.%d\n",
			force_build? " (forced)":"",
			version_maj, version_min, pending_maj, pending_min);
		lprintf("UPDATE: building new version..\n");

        #ifndef PLATFORM_raspberrypi
            update_in_progress = true;  // NB: must be before rx_server_user_kick(-1) to prevent new connections
            rx_server_user_kick(-1);    // kick everyone off to speed up build
            TaskSleepReasonSec("kick delay", 5);
        #endif

		// Run build in a Linux child process so the server can continue to respond to connection requests
		// and display a "software update in progress" message.
		// This is because the calls to system() in update_build_ctask() block for the duration of the build.
		u4_t build_time = timer_sec();
		status = child_task("kiwi.build", update_build_ctask, POLL_MSEC(1000), TO_VOID_PARAM(force_build));
		
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            lprintf("UPDATE: Build failed, check /root/build.log file\n");
            fail_reason = FAIL_BUILD;
		    if (force_build) report_result(conn);
		    goto common_return;
		}
		
		lprintf("UPDATE: build took %d secs\n", timer_sec() - build_time);
		lprintf("UPDATE: switching to new version %d.%d\n", pending_maj, pending_min);
		if (admcfg_int("update_restart", NULL, CFG_REQUIRED) == 0) {
		    kiwi_exit(0);
		} else {
		    lprintf("UPDATE: rebooting Beagle..\n");
		    system("sleep 3; reboot");
		}
	} else {
		lprintf("UPDATE: version %d.%d is current\n", version_maj, version_min);
	}
	
	if (daily_restart) {
	    lprintf("UPDATE: daily restart..\n");
	    kiwi_exit(0);
	}

common_return:
	if (ip_auto_download_oneshot) {
	    ip_auto_download_oneshot = false;
        //printf("bl_GET: update check normal\n");
	    bl_GET(TO_VOID_PARAM(1));
	}

	if (conn) conn->update_check = WAIT_UNTIL_NO_USERS;     // restore default
	update_pending = update_task_running = update_in_progress = false;
}

// called at update check TOD, on each user logout in case update is pending or on demand by admin UI
void check_for_update(update_check_e type, conn_t *conn)
{
	bool force = (type != WAIT_UNTIL_NO_USERS);
	
	if (!force && admcfg_bool("update_check", NULL, CFG_REQUIRED) == false) {
		//printf("UPDATE: exiting because admin update check not enabled\n");
	
        if (ip_auto_download_check) {
            ip_auto_download_check = false;
            //printf("bl_GET: update check false\n");
            bl_GET(TO_VOID_PARAM(1));
        }

		return;
	}
	
	if (force) {
		lprintf("UPDATE: force %s by admin\n", (type == FORCE_CHECK)? "update check" : "build");
		assert(conn != NULL);
		if (update_task_running) {
			lprintf("UPDATE: update task already running\n");
			report_result(conn);
			return;
		} else {
			conn->update_check = type;
		}
	}
	
	if (ip_auto_download_check) {
	    ip_auto_download_oneshot = true;
	    ip_auto_download_check = false;
	}

	if ((force || (update_pending && rx_count_server_conns(EXTERNAL_ONLY) == 0)) && !update_task_running) {
		update_task_running = true;
		CreateTask(_update_task, TO_VOID_PARAM(conn), ADMIN_PRIORITY);
	}
}

//#define TEST_UPDATE     // enables printf()s and simulates local time entering update window

static bool update_on_startup = true;
static int 	prev_update_window = -1;

// called at the top of each minute
void schedule_update(int min)
{
	#define UPDATE_SPREAD_HOURS	5	// # hours to spread updates over
	#define UPDATE_SPREAD_MIN	(UPDATE_SPREAD_HOURS * 60)

	#define UPDATE_START_HOUR	1	// 1 AM local time
	#define UPDATE_END_HOUR		(UPDATE_START_HOUR + UPDATE_SPREAD_HOURS)
	
	// relative to local time if timezone has been determined, utc otherwise
    int local_hour;
    (void) local_hour_min_sec(&local_hour);

	#ifdef TEST_UPDATE
        int utc_hour;
        time_hour_min_sec(utc_time(), &utc_hour);
	    printf("UPDATE: UTC=%02d:%02d Local=%02d:%02d update_window=[%02d:00,%02d:00]\n",
	        utc_hour, min, local_hour, min, UPDATE_START_HOUR, UPDATE_END_HOUR);
	    local_hour = 1;
	#endif

	bool update_window = (local_hour >= UPDATE_START_HOUR && local_hour < UPDATE_END_HOUR);
	bool first_update_window = false;
	
	// don't let Kiwis hit github.com all at once!
	if (update_window) {
		int mins_now = min + (local_hour - UPDATE_START_HOUR) * 60;
		int serno = serial_number;
		
		#ifdef TEST_UPDATE
            #define SERNO_MIN_TRIG 1
		    serno = SERNO_MIN_TRIG;
		    int mins_trig = serno % UPDATE_SPREAD_MIN;
		    int hr_trig = UPDATE_START_HOUR + mins_trig/60;
		    int min_trig = mins_trig % 60;
            printf("TEST_UPDATE: %02d:%02d mins_now=%d mins_trig=%d (%02d:%02d sn=%d)\n",
                local_hour, min, mins_now, mins_trig, hr_trig, min_trig, serno);
        #endif

        update_window = update_window && (mins_now == (serno % UPDATE_SPREAD_MIN));

        if (prev_update_window == -1) prev_update_window = update_window? 1:0;
        first_update_window = ((prev_update_window == 0) && update_window);
		#ifdef TEST_UPDATE
            printf("TEST_UPDATE: update_window=%d prev_update_window=%d first_update_window=%d\n",
                update_window, prev_update_window, first_update_window);
        #endif
        prev_update_window = update_window? 1:0;

		if (update_window) {
		    printf("TLIMIT-IP 24hr cache cleared\n");
            json_init(&cfg_ipl, (char *) "{}");     // clear 24hr ip address connect time limit cache
        }
	}
	
    //#define TRIG_UPDATE
    #ifdef TRIG_UPDATE
        static bool trig_update;
        if (timer_sec() >= 60 && !trig_update) {
            update_window = true;
            trig_update = true;
        }
    #endif
    
    daily_restart = first_update_window && !update_on_startup && (admcfg_bool("daily_restart", NULL, CFG_REQUIRED) == true);
    ip_auto_download_check = first_update_window && !update_on_startup && (admcfg_bool("ip_blacklist_auto_download", NULL, CFG_REQUIRED) == true);

    //printf("min=%d ip_auto_download_check=%d update_window=%d update_on_startup=%d auto=%d\n",
    //    timer_sec()/60, ip_auto_download_check, update_window, update_on_startup,
    //    (admcfg_bool("ip_blacklist_auto_download", NULL, CFG_REQUIRED) == true));
    
    if (update_on_startup && admcfg_int("restart_update", NULL, CFG_REQUIRED) != 0) {
		lprintf("UPDATE: update on restart delayed until update window\n");
		update_on_startup = false;
    }

	if (update_window || update_on_startup) {
		lprintf("UPDATE: check scheduled %s\n", update_on_startup? "(startup)":"");
		update_on_startup = false;
		update_pending = true;
		check_for_update(WAIT_UNTIL_NO_USERS, NULL);
	}
}
