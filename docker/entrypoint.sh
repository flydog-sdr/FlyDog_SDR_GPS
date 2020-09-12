#!/bin/bash

if [ -z "$ADMIN_PASSWORD" ]; then
	echo >&2 'error: missing required ADMIN_PASSWORD environment variable'
	echo >&2 '  Did you forget to -e ADMIN_PASSWORD=... ?'
	exit 1
fi

cat << EOF > /root/kiwi.config/admin.json
{
	"cfg": "pwd",
	"user_password": "",
	"user_auto_login": true,
	"admin_password": "${ADMIN_PASSWORD}",
	"admin_auto_login": true,
	"port": 8073,
	"enable_gps": true,
	"update_check": false,
	"update_install": false,
	"sdr_hu_register": false,
	"api_key": "replaced with API key from sdr.hu/register",
	"port_ext": 8073,
	"firmware_sel": 1,
	"tlimit_exempt_pwd": "",
	"server_enabled": true,
	"auto_add_nat": false,
	"duc_enable": false,
	"duc_user": "",
	"duc_pass": "",
	"duc_host": "",
	"duc_update": 3,
	"daily_restart": false,
	"update_restart": 0,
	"ip_address.dns1": "8.8.8.8",
	"ip_address.dns2": "8.8.4.4",
	"url_redirect": "",
	"ip_blacklist": "47.88.219.24/24",
	"no_dup_ip": false,
	"my_kiwi": true,
	"onetime_password_check": true,
	"kiwisdr_com_register": false,
	"options": 0,
	"GPS_tstamp": true,
	"use_kalman_position_solver": true,
	"rssi_azel_iq": 0,
	"always_acq_gps": true,
	"include_alert_gps": true,
	"include_E1B": true,
	"survey": 181,
	"E1B_offset": 4,
	"acq_Navstar": true,
	"acq_QZSS": true,
	"QZSS_prio": false,
	"acq_Galileo": true,
	"plot_E1B": true,
	"rev_user": "",
	"rev_host": "",
	"ip_address": {
		"use_static": false,
		"ip": "",
		"netmask": "",
		"gateway": "",
		"dns1": "",
		"dns2": ""
	},
	"proxy_server": "proxy.kiwisdr.com"
}
EOF

/usr/local/bin/kiwid -debian 10 -use_spidev 1 -bg -raspsdr
