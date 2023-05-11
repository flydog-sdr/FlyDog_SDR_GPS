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

// Copyright (c) 2022-2023 John Seamons, ZL/KF6VO

#pragma once

#define N_IP_BLACKLIST 512
#define N_IP_BLACKLIST_HASH_BYTES 4     // 4 bytes = 8 chars

typedef struct {
    u4_t dropped;
    u4_t ip;        // ipv4
    u1_t a,b,c,d;   // ipv4
    u4_t nm;        // ipv4
    u1_t cidr;
    bool whitelist;
    u4_t last_dropped;
} ip_blacklist_t;

int ip_blacklist_add_iptables(char *ip_s);
void ip_blacklist_init();
bool check_ip_blacklist(char *remote_ip, bool log=false);
void ip_blacklist_dump();
void bl_GET(void *param);
