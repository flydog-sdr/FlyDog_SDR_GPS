// Copyright (c) 2019-2020 John Seamons, ZL/KF6VO

var drm = {
   ext_name: 'DRM',     // NB: must match DRM.cpp:DRM_ext.name
   first_time: true,
   active: false,
   special_passband: null,
   dseq: 0,
   interval: null,
   interval2: null,

   locked: 0,
   wrong_srate: false,
   hacked: false,

   run: 0,
   is_stopped: 0,
   is_monitor: 0,
   freq_s: '',
   freq: 0,
   
   // ui
   w_graph_nom: 675,
   w_graph_min: 400,

   panel_0:       [  'info',     'info',     'info',     'info',  'info',  'info',  'info',  'info' ],
   panel_1:       [  'by-svc',   'by-time',  'by-freq',  'graph', 'IQ',    'IQ',    'IQ',    'IQ' ],
   BY_SVC: 0,
   BY_TIME: 1,
   BY_FREQ: 2,
   GRAPH: 3,
   IQ_ALL: 4,
   FAC: 5,
   SDC: 6,
   MSC: 7,
   IQ_END: 7,
   
   display_idx: 1,
   display_idx_s:    [ 'schedule', 'graph', 'iq' ],
   display_idx_si:   [ 1, 5, 7 ],   // index into display_s below (enumerated object keys and values)
   
   display_s: {
      'Schedule': [
         { m:'by service' },        // display_idx = 1
         { m:'by time' },
         { m:'by freq' }
      ],
      'Display': [
         { m:'IF/SNR', c:'graph' },
      ],
      'IQ': [
         { m:'all', c:'IQ' },
         { m:'FAC', c:'IQ' },
         { m:'SDC', c:'IQ' },
         { m:'MSC', c:'IQ' }
      ]
   },
   
   stations: null,
   using_default: false,
   double_fault: false,
   loading_msg: '&nbsp;loading data from kiwisdr.com ...',
   SINGLE: 0,
   MULTI: 1,
   REGION: 2,
   SERVICE: 3,
   
   monitor: 0,
   
   database: 0,
   database_s: [ 'drmrx.org', 'kiwisdr.com' ],
   database_url: [
      kiwi_SSL() +'drm.kiwisdr.com/drm/drmrx.cjson',
      kiwi_SSL() +'drm.kiwisdr.com/drm/stations2.cjson'
   ],
   
   last_occ: -1,
   last_ilv: -1,
   last_sdc: -1,
   last_msc: -1,
   
   DRM_DAT_IQ: 0,
   
   EAudCod: [ 'AAC', 'OPUS', 'RESERVED', 'xHE_AAC', '' ],
   AAC: 0,
   xHE_AAC: 3,
   
   // extra wide band edges are intentional
   bands: [
      { n:'LW', b:140, e:300 },
      { n:'MW', b:500, e:1700 },
      { n:'120m', b:22500, e:2500 },
      { n:'90m', b:3100, e:3500 },
      { n:'75m', b:3800, e:4100 },
      { n:'60m', b:4700, e:5100 },
      { n:'49m', b:5800, e:6300 },
      { n:'41m', b:7200, e:7600 },
      { n:'31m', b:9300, e:10000 },
      { n:'25m', b:11500, e:12200 },
      { n:'22m', b:13500, e:14000 },
      { n:'19m', b:15000, e:15900 },
      { n:'16m', b:17400, e:18000 },
      { n:'15m', b:18800, e:19100 },
      { n:'13m', b:21400, e:22000 },
      { n:'11m', b:25500, e:26200 }
   ],

   last_last: 0
};

function DRM_main()
{
	ext_switch_to_client(drm.ext_name, drm.first_time, drm_recv);		// tell server to use us (again)
	if (!drm.first_time)
		drm_lock_setup();
	drm.first_time = false;
}

function drm_recv(data)
{
   var i, s;
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var d = new Uint8Array(data, 5);
		var len = d.length-1;
		if (isUndefined(drm.canvas_IQ)) return;
      var ct = drm.canvas_IQ.ctx;
      var w = drm.w_IQ;
      var h = drm.h_IQ;
		var color = [];
		
		switch (cmd) {
		
		/*
		case drm.DRM_DAT_FAC:
         ct.fillStyle = 'white';
         ct.fillRect(0,0, w,h);
         ct.fillStyle = 'grey';
         ct.fillRect(0,h/2, w,1);
         ct.fillRect(w/2,0, 1,h);
         color.push('green');
         // fall through

		case drm.DRM_DAT_SDC:
         color.push('red');
         // fall through

		case drm.DRM_DAT_MSC:
         color.push('blue');
         ct.fillStyle = color.shift();
      */
      
		case drm.DRM_DAT_IQ:
         ct.fillStyle = 'white';
         ct.fillRect(0,0, w,h);
         ct.fillStyle = 'grey';
         ct.fillRect(0,h/2, w,1);
         ct.fillRect(w/2,0, 1,h);
         
         if (drm.display == drm.IQ_ALL || drm.display == drm.FAC) {
            ct.fillStyle = 'green';
            for (i = 0; i < 64; i++) {
               var x = d[i*2]   / 255.0 * w;
               var y = d[i*2+1] / 255.0 * h;
               ct.fillRect(x,y, 2,2);
            }
         }
         i = 64;
         
         if (drm.display == drm.IQ_ALL || drm.display == drm.SDC) {
            ct.fillStyle = 'red';
            for (; i < 64+256; i++) {
               var x = d[i*2]   / 255.0 * w;
               var y = d[i*2+1] / 255.0 * h;
               ct.fillRect(x,y, 2,2);
            }
         }
         i = 64+256;

         if (drm.display == drm.IQ_ALL || drm.display == drm.MSC) {
            ct.fillStyle = 'blue';
            for (; i < 64+256+2048; i++) {
               var x = d[i*2]   / 255.0 * w;
               var y = d[i*2+1] / 255.0 * h;
               ct.fillRect(x,y, 2,2);
            }
         }
         
		   return;
		
		}

		console.log('drm_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('drm_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('drm_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				kiwi_load_js(['pkgs/js/graph.js', 'pkgs/js/sprintf/sprintf.js'], 'drm_lock_setup');
				break;
			
			case "inuse":
			   drm.inuse = +param[1];
				break;
			
			case "heavy":
			   drm.heavy = +param[1];
				break;
			
			case "locked":
			   var p = +param[1];
			   if (p == -1) {
			      drm.wrong_srate = true;
			      drm.locked = 0;
			   } else
			   if (p == -2) {
			      drm.hacked = true;
			      drm.locked = 0;
			   } else
			   if (p == 1) {
			      drm.locked = 1;
			   } else {
			      drm.locked = 0;
			   }
            drm_controls_setup();
				break;
			
			case "drm_status_cb":
			   if (!drm.run || !drm.desktop)
			      break;

			   var deco = kiwi_decodeURIComponent('DRM', param[1]);
            if (!deco) break;

            var o = kiwi_JSON_parse('drm_status_cb', deco);
            if (!o) break;

            drm_all_status(o.io, o.time, o.frame, o.FAC, o.SDC, o.MSC);

			   w3_innerHTML('id-drm-if_level', 'IF Level: '+ o.if.toFixed(1) +' dB');
			   if (o.if < 0 && o.if > -100) {
			      if (isUndefined(drm.last_if)) drm.last_if = o.if;
			      drm.last_if = graph_plot(drm.gr_if, o.if, { line: drm.last_if, color: 'green' });
			   }

			   w3_innerHTML('id-drm-snr', o.snr? ('SNR: '+ o.snr.toFixed(1) +' dB') : '');
			   if (o.snr > 0) {
			      if (isUndefined(drm.last_snr)) drm.last_snr = o.snr;
			      drm.last_snr = graph_plot(drm.gr_snr, o.snr, { line: drm.last_snr });
			   }

			   if (isDefined(o.mod)) {
			      i = o.mod;
			      if (i >= 0 && i <= 3)
			         w3_color('id-drm-mod-'+ ['A','B','C','D'][i], 'black', 'lime');

			      i = o.occ;
			      if (i >= 0 && i <= 5) {
			         if (drm.last_occ != i) {
			            w3_color('id-drm-occ-'+ ['4.5','5','9','10','18','20'][drm.last_occ], '', '');
			         }
			         w3_color('id-drm-occ-'+ ['4.5','5','9','10','18','20'][i], 'black', 'lime');
			         drm.last_occ = i;
			         
			         /*
			         if (i != drm.OOC_10_kHz && ) {
			            drm.special_passband = ;
			         }
			         */
			      }

			      i = o.ilv;
			      if (i >= 0 && i <= 1) {
			         if (drm.last_ilv != i) {
			            w3_color('id-drm-ilv-'+ ['L','S'][drm.last_ilv], '', '');
			         }
			         w3_color('id-drm-ilv-'+ ['L','S'][i], 'black', 'lime');
			         drm.last_ilv = i;
			      }

			      i = o.sdc;
			      if (i >= 0 && i <= 1) {
			         if (drm.last_sdc != i) {
			            w3_color('id-drm-sdc-'+ ['4','16'][drm.last_sdc], '', '');
			         }
			         w3_color('id-drm-sdc-'+ ['4','16'][i], 'black', 'lime');
			         drm.last_sdc = i;
			      }

			      i = o.msc;
			      if (i > 2) i = 2;
			      if (i >= 1 && i <= 2) {
			         if (drm.last_msc != i) {
			            w3_color('id-drm-msc-'+ ['16','64'][drm.last_msc-1], '', '');
			         }
			         w3_color('id-drm-msc-'+ ['16','64'][i-1], 'black', 'lime');
			         drm.last_msc = i;
			      }

			      w3_innerHTML('id-drm-prot', sprintf('Protect: A=%d B=%d', o.pla, o.plb));

			      w3_innerHTML('id-drm-nsvc', sprintf('Services: A=%d D=%d', o.nas, o.nds));

			      if (o.mod >= 0 && o.mod <= 3)
                  w3_innerHTML('id-drm-desc',
                     [
                        'Local/regional use',
                        'Medium range use, multipath resistant',
                        'Long distance use, multipath/doppler resistant',
                        'Resistance to large delay/doppler spread'
                     ][o.mod]
                  );
			   } else {
               drm_reset_status();
            }
			   
			   w3_show_hide('id-drm-mode', isDefined(o.mod));
			   
			   var codec = drm.AAC;
			   o.svc.forEach(function(ao, i) {
			      i++;
               //if (ao.cur && i == 2) ao.ac = drm.xHE_AAC;
			      s = i;
			      if (ao.id) {
			         var label = kiwi_clean_html(kiwi_clean_newline(decodeURIComponent(ao.lbl)));
                  s += ' '+ drm.EAudCod[ao.ac] +' ('+ ao.id +') '+ label + (ao.ep? (' UEP ('+ ao.ep.toFixed(1) +'%) ') : ' EEP ') +
                     (ao.ad? 'Audio ':'Data ') + ao.br.toFixed(2) +' kbps';
               }
               var el = w3_el('id-drm-svc-'+ i);
			      el.innerHTML = s;
			      w3_color(el, null, ao.cur? 'mediumSlateBlue' : '');
			      if (ao.cur && ao.ac < 4)   // glitches to = 4 during acquisition
			         codec = ao.ac;
			   });
            w3_show('id-drm-svcs');
            if (codec != drm.AAC && codec != drm.xHE_AAC) console.log('codec='+ codec);
            w3_innerHTML('id-drm-error',
               (codec != drm.AAC && codec != drm.xHE_AAC)? 'WARNING: codec not supported -- audio will be bad' : '<br>');
            w3_color('id-drm-error', 'white', 'red', (codec != drm.AAC && codec != drm.xHE_AAC));

			   w3_innerHTML('id-drm-msgs', o.msg? kiwi_clean_html(kiwi_clean_newline(decodeURIComponent(o.msg))) : '');
			   break;

			case "drm_bar_pct":
			   if (!drm.run)
			      break;

			   var pct = w3_clamp(parseInt(param[1]), 0, 100);
			   //if (pct > 0 && pct < 3) pct = 3;    // 0% and 1% look weird on bar
			   var el = w3_el('id-drm-bar');
			   if (el) el.style.width = pct +'%';
			   break;

			case "annotate":
			   switch (+param[1]) {
               case 0: drm_annotate('lime'); break;
               case 1: drm_annotate('gold'); break;
               case 2: drm_annotate('red'); break;
               case 3: drm_annotate('blue'); break;
            }
			   break;
			
			default:
				console.log('drm_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function drm_annotate(color)
{
   graph_annotate(drm.gr_if, color);
   graph_annotate(drm.gr_snr, color);
}

function drm_reset_status()
{
   //console.log('drm_reset_status');
   drm_all_status(4,4,4,4,4,4);
   w3_innerHTML('id-drm-if_level', '');
   w3_innerHTML('id-drm-snr', '');
   w3_hide('id-drm-mode');
   for (var i = 0; i <= 3; i++)
      w3_color('id-drm-mod-'+ ['A','B','C','D'][i], '', '');
   w3_innerHTML('id-drm-error', '');
   w3_hide('id-drm-svcs');
   w3_innerHTML('id-drm-msgs', '');
}

function drm_status(id, v)
{
   var color;
   switch (v) {
      case 0: color = 'lime'; break;
      case 1: color = 'yellow'; break;
      case 2: color = 'red'; break;
      case 3: default: color = 'red'; break;
      case 4: color = 'grey'; break;
   }
   w3_color('id-drm-status-'+ id, color);
}

function drm_all_status(io, time, frame, FAC, SDC, MSC)
{
   drm_status('io', io);
   drm_status('time', time);
   drm_status('frame', frame);
   drm_status('fac', FAC);
   drm_status('sdc', SDC);
   drm_status('msc', MSC);
}

function drm_lock_setup()
{
   ext_send('SET lock_set');
   //alert('# DRM SET lock_set');
}

function drm_saved_mode()
{
   drm.saved_mode = ext_get_mode();
   console.log('drm_save_mode FIRST saved_mode='+ drm.saved_mode);
   if (drm.saved_mode == 'drm')
      drm.saved_mode = ext_get_prev_mode();     // if 'DRM" button clicked use previously set mode
   console.log('drm_save_mode FINAL saved_mode='+ drm.saved_mode);
}

function drm_tscale(utc)
{
   /*
   var pct = drm.mobile? 0.30 : 0.35;
   var factor = drm.mobile? 0.026 : 0.025;
   return ((pct * drm.w_sched) + (utc * factor * drm.w_sched)).toFixed(0);
   
   var pct = drm.mobile? 0.03 : 0.04;
   var factor = drm.mobile? 0.0385 : 0.037;
   return ((pct * drm.w_sched) + (utc * factor * drm.w_sched)).toFixed(0);
   */

   var Lmargin = 27, Rmargin = drm.mobile? 0:20, scrollBar = 15;
   return (Lmargin + utc * (drm.w_sched - Lmargin - Rmargin - scrollBar) / 24 /* hrs */).toFixed(0);
}

function drm_schedule_static()
{
   var s = '';
   
   // using "title=" here doesn't work because of the "pointer-events: none" required for proper scrolling
   for (var hour = 0; hour <= 24; hour++) {
      //s += w3_div(sprintf('id-drm-sched-tscale|left:%spx;|title="%02d:00"', drm_tscale(hour), hour));
      s += w3_div(sprintf('id-drm-sched-tscale|left:%spx;', drm_tscale(hour)));
   }

   s += w3_div(sprintf('id-drm-sched-now|left:%spx;', drm_tscale(kiwi_UTC_minutes()/60)));
   
   if (drm.interval == null) {
      drm.interval = setInterval(function() {
         w3_el('id-drm-sched-now').style.left = px(drm_tscale(kiwi_UTC_minutes()/60));
      }, 60000);
   }
   
   return s;
}

function drm_pre_set_freq(freq, station)
{
   var no_clear_special = isArg(station);

   if (no_clear_special != true) {
      console.log('drm_pre_select_cb  RESET special_passband ########');
      //kiwi_trace();
      drm.special_passband = null;
   }

   if (station) {
      drm_station(station);
   } else {
      drm_station('');
      // FIXME: search for schedule entry based on freq/time
   }
   
   drm_set_freq(freq);
   drm_start();
}

function drm_click(idx)
{
   var o = drm.stations[idx];
   //console.log('drm_click idx='+ idx +' '+ o.f +' '+ dq(o.s.toLowerCase()) +' '+ (o.u? o.u:''));
   
   // it's a hack to add passband to the broadcaster's URL link, but didn't want to change the cjson file format again
   // i.e. "<url>?f=/<pb_lo>,<pb_hi>"     e.g. pb value could 2300 or 2.3k
   drm.special_passband = null;
   if (o.u) {
		var p = new RegExp('^.*\?f=\/([-0-9.k]*)?,?([-0-9.k]*)?.*$').exec(o.u);
		console.log(p);
		if (p && p.length == 3)
         drm.special_passband = { low: p[1].parseFloatWithUnits('k'), high: p[2].parseFloatWithUnits('k') };
   }
   /**/
      console.log('special_passband:');
      if (drm.special_passband)
         console.log(drm.special_passband);
      else
         console.log('(none)');
   /**/
   
   w3_hide('id-drm-bar-container');
   drm_pre_set_freq(o.f, o.s);
}

function drm_schedule_svc()
{
   if (!drm.stations) return '';

   var i;
   var s = drm.using_default? w3_div('w3-yellow w3-padding w3-show-inline-block', 'can\'t contact kiwisdr.com<br>using default data') : '';
   var narrow = drm.narrow_listing;
   var toff = drm_tscale(narrow? 1.0 : 0.25);

   for (i = 0; i < drm.stations.length; i++) {
      var o = drm.stations[i];
      if (o.t == drm.REGION) continue;
      var station = o.s;
      var freq = o.f;
      var url = o.u;
      var si = '';

      //var station_name = station.replace('_', narrow? '<br>':' ');
      //if (narrow) station_name = station_name.replace(',', '<br>');
      var station_name = station.replace('_', ' ');
      station_name += '&nbsp;&nbsp;&nbsp;'+ (narrow? '<br>':'') + freq;
      var count = (station_name.match(/<br>/g) || [1]).length;
      var em =  count + (narrow? 2:1);
      var time_h = Math.max(20 * (em-1), 30);
      //console.log(narrow +'|'+ count +'|'+ em +' '+ station_name);

      while (i < drm.stations.length && o.s == station && o.f == freq) {
         var b_px = drm_tscale(o.b);
         var e_px = drm_tscale(o.e);
         si += w3_div(sprintf('id-drm-sched-time %s|left:%spx; width:%spx; height:%dpx|title="%s" onclick="drm_click(%d);"',
            o.v? 'w3-light-green':'', b_px, (e_px - b_px + 2).toFixed(0), time_h, freq.toFixed(0), i));
         i++;
         o = drm.stations[i];
      }
      i--;

      var info = '';
      if (url) info = w3_link('w3-valign', url, w3_icon('w3-link-darker-color cl-drm-sched-info', 'fa-info-circle', 24));

      s += w3_inline('cl-drm-sched-station cl-drm-sched-striped/w3-valign',
         w3_div(sprintf('|font-size:%dem', em), '&nbsp;'),  // sets the parent height since other divs absolutely positioned
         info, si,
         w3_div(sprintf('cl-drm-station-name|left:%spx', toff), station_name)
      );
      if (o && o.t == drm.SERVICE) {
         s += w3_div('cl-drm-sched-hr-div cl-drm-sched-striped', '<hr class="cl-drm-sched-hr">');
         i++;
      }
   }
   
   return s;
}

function drm_schedule_time_freq(sort_by_freq)
{
   if (!drm.stations) return '';

   var i, j;
   var s = drm.using_default? w3_div('w3-yellow w3-padding w3-show-inline-block', 'can\'t contact kiwisdr.com<br>using default data') : '';
   var narrow = drm.narrow_listing;
   drm.stations_freq = [];
   var now = kiwi_UTC_minutes()/60.0;
   
   for (i = j = 0; i < drm.stations.length; i++) {
      var o = drm.stations[i];
      if (o.t != drm.SINGLE && o.t != drm.MULTI) continue;
      drm.stations_freq[j++] = o;
   }

   drm.stations_freq.sort(function(a,b) {
      var a_cmp = sort_by_freq? a.f : a.b;
      var b_cmp = sort_by_freq? b.f : b.b;
      var a_India = a.s.startsWith('India,');
      var b_India = b.s.startsWith('India,');
      
      // always put India MW at bottom of schedules
      if ( a_India && !b_India) return  1;
      if (!a_India &&  b_India) return -1;

      //if (sort_by_freq)
         return ((a_cmp < b_cmp)? -1 : ((a_cmp > b_cmp)? 1:0));
   });
   console.log('sort_by_freq='+ sort_by_freq +' ...');
   console.log(drm.stations_freq);
   var toff = drm_tscale(narrow? 1.0 : 0.25);
   var last_band = '';
   
   for (i = 0; i < drm.stations_freq.length; i++) {
      var o = drm.stations_freq[i];
      var station = o.s;
      var freq = o.f;
      var url = o.u;
      var si = '';
      
      var station_name = station.replace('_', narrow? '<br>':' ');
      if (narrow) station_name = station_name.replace(',', '<br>');
      station_name = freq +'&nbsp;&nbsp;&nbsp;'+ (narrow? '<br>':'') + station_name;
      var count = (station_name.match(/<br>/g) || [1]).length;
      var em =  count + (narrow? 2:1);
      var time_h = Math.max(20 * (em-1), 30);

      while (i < drm.stations_freq.length && o.s == station && o.f == freq) {
         var b_px = drm_tscale(o.b);
         var e_px = drm_tscale(o.e);
         si += w3_div(sprintf('id-drm-sched-time %s|left:%spx; width:%spx; height:%dpx|title="%s" onclick="drm_click(%d);"',
            o.v? 'w3-light-green':'', b_px, (e_px - b_px + 2).toFixed(0), time_h, freq.toFixed(0), o.i));
         i++;
         o = drm.stations_freq[i];
      }
      i--;

      if (sort_by_freq) {
         var band = null;
      
         for (j = 0; j < drm.bands.length; j++) {
            var b = drm.bands[j];
            if (freq >= b.b && freq <= b.e) {
               band = b.n;
               break;
            }
         }
         
         if (band && band != last_band) {
            s += w3_inline_percent('cl-drm-sched-hr-div cl-drm-sched-striped/',
               '<hr class="cl-drm-sched-hr">', 10, '<b>'+ band +'</b>', 5, '<hr class="cl-drm-sched-hr">');
            last_band = band;
         }
      }

      var info = '';
      if (url) info = w3_link('w3-valign', url, w3_icon('w3-link-darker-color cl-drm-sched-info', 'fa-info-circle', 24));

      s += w3_inline('cl-drm-sched-station cl-drm-sched-striped/w3-valign',
         w3_div(sprintf('|font-size:%dem', em), '&nbsp;'),  // sets the parent height since other divs absolutely positioned
         info, si,
         w3_div(sprintf('cl-drm-station-name|left:%spx', toff), station_name)
      );
   }
   
   return s;
}

// stations.cjson format:
//
//    [
//       { "region0_name": null, "svc0_name": [ freq/times ], "svc1_name": [ freq/times ] ... },
//       { "region1_name": null, "svc0_name": [ freq/times ], "svc1_name": [ freq/times ] ... },
//       {}
//    ]
//
// freq/times format:
//
//    [ "optional service URL", freq0, start_time, stop_time, freq1, start_time, stop_time ... ]
//
//       "start_time, stop_time" can also be an array [ start0, stop0, start1, stop1 ... ]
//
// Underscores in station names are converted to line breaks in schedule entries.
// Negative start or end time means entry should be marked as verified.

function drm_get_stations_done_cb(stations)
{
   var fault = false;
   
   if (!stations) {
      console.log('drm_get_stations_done_cb: stations='+ stations);
      fault = true;
   } else
   
   if (stations.AJAX_error && stations.AJAX_error == 'timeout') {
      console.log('drm_get_stations_done_cb: TIMEOUT');
      drm.using_default = true;
      fault = true;
   } else
   if (stations.AJAX_error && stations.AJAX_error == 'status') {
      console.log('drm_get_stations_done_cb: status='+ stations.status);
      drm.using_default = true;
      fault = true;
   } else
   if (!isArray(stations)) {
      console.log('drm_get_stations_done_cb: not array');
      fault = true;
   }
   
   if (fault) {
      if (drm.double_fault) {
         console.log('drm_get_stations_done_cb: default station list fetch FAILED');
         return;
      }
      console.log(stations);
      
      // load the default station list from a file embedded with the extension
      var url = kiwi_url_origin() +'/extensions/DRM/stations.cjson';
      console.log('drm_get_stations_done_cb: using default station list '+ url);
      drm.using_default = true;
      drm.double_fault = true;
      kiwi_ajax_progress(url, 'drm_get_stations_done_cb', 0, /* timeout */ 10000);
      return;
   }
   
   console.log('drm_get_stations_done_cb: from '+ drm.database_url[drm.database]);
   
   try {
      drm.stations = [];
      var idx = 0;
      var region, station, freq, begin, end, wrap, prefix, verified, url;
      var is_India_MW = false;
      stations.forEach(function(obj, i) {    // each object of outer array
         prefix = '';
         w3_obj_enum(obj, function(key, i1, ar1) {   // each object
            if (i1 == 0) {
               region = key;
               if (region == 'India MW') {
                  prefix = 'India, ';
                  is_India_MW = true;
               }
               drm.stations.push( { t:drm.REGION, f:0, s:'', r:region } );
               idx++;
               return;
            } else {
               if (!isArray(ar1)) return;
               station = prefix + key;
               url = null;
               for (i1 = 0; i1 < ar1.length; i1++) {
                  var ae = ar1[i1];
                  if (i1 == 0 && isString(ae)) { url = ae; continue; }
                  freq = ae;
                  i1++;
               
                  ae = ar1[i1];
                  if (isArray(ae)) {
                     for (var i2 = 0; i2 < ae.length; i2++) {
                        begin = kiwi_hh_mm(ae[i2++]);
                        end = kiwi_hh_mm(ae[i2]);
                        verified = (begin < 0 || end < 0);
                        if (drm.database != 0) verified = !verified;
                        begin = Math.abs(begin); end = Math.abs(end);
                        wrap = (end < begin);
                        if (wrap) {
                           drm.stations.push( { t:drm.MULTI, f:freq, s:station, r:region, b:begin, e:24, v:verified, u:url, i:idx } );
                           idx++;
                           drm.stations.push( { t:drm.MULTI, f:freq, s:station, r:region, b:0, e:end, v:verified, u:url, i:idx } );
                        } else
                           drm.stations.push( { t:drm.MULTI, f:freq, s:station, r:region, b:begin, e:end, v:verified, u:url, i:idx } );
                        idx++;
                     }
                  } else {
                     begin = kiwi_hh_mm(ar1[i1++]);
                     end = kiwi_hh_mm(ar1[i1]);
                     verified = (begin < 0 || end < 0);
                     if (drm.database != 0) verified = !verified;
                     begin = Math.abs(begin); end = Math.abs(end);
                     wrap = (end < begin);
                     if (wrap) {
                        drm.stations.push( { t:drm.SINGLE, f:freq, s:station, r:region, b:begin, e:24, v:verified, u:url, i:idx } );
                        idx++;
                        drm.stations.push( { t:drm.SINGLE, f:freq, s:station, r:region, b:0, e:end, v:verified, u:url, i:idx } );
                     } else
                        drm.stations.push( { t:drm.SINGLE, f:freq, s:station, r:region, b:begin, e:end, v:verified, u:url, i:idx } );
                     idx++;
                  }
               }
               if (!is_India_MW) {     // make all India MW appear as a single service
                  drm.stations.push( { t:drm.SERVICE, f:0, s:station, r:region } );
                  idx++;
               }
            }
         });
      });
      console.log(drm.stations);
   } catch(ex) {
      console.log('drm_get_stations_done_cb: catch');
      console.log(ex);
   }

   w3_innerHTML('id-drm-panel-by-svc', drm_schedule_svc());
   w3_innerHTML('id-drm-panel-by-time', drm_schedule_time_freq(0));
   w3_innerHTML('id-drm-panel-by-freq', drm_schedule_time_freq(1));
}

function drm_panel_show(controls_inner, data_html)
{
	var controls_html =
		w3_div('id-drm-controls w3-text-white',
			w3_divs('',
				w3_div('w3-medium w3-text-aqua', '<b>Digital Radio Mondiale (DRM30) decoder</b>'),
				
            w3_col_percent('w3-margin-T-4/',
               w3_div('id-drm-station w3-text-css-yellow', '&nbsp;'), 70,
					w3_div('', 'Based on <b><a href="https://sourceforge.net/projects/drm/" target="_blank">Dream 2.2.1</a></b>')
               /*
               w3_div('', 'Schedules: ' +
                  '<a href="http://ab27.bplaced.net/drm.pdf" target="_blank">ab27(pdf)</a> ' +
                  '<a href="https://www.drm.org/what-can-i-hear/broadcast-schedule-2" target="_blank">drm.org</a> ' +
                  '<a href="http://www.hfcc.org/drm" target="_blank">hfcc.org</a>'), 40
               */
            ),
            
            controls_inner
         )
      );
   
	ext_panel_show(controls_html, data_html, null);
	ext_set_controls_width_height(600, 185);
}

function drm_mobile_controls_setup(mobile)
{
	drm.mobile = drm.mobile || 1;
	console.log('$ mobile drm mobile_laptop_test='+ mobile_laptop_test);
	drm.w_nom = drm.w_sched = 300;
   drm.h_sched = mobile_laptop_test? 100:230;
   drm.cpanel_margin = 20;

	var controls_html =
      w3_div(sprintf('id-drm-controls w3-absolute|width:%dpx; height:%dpx;', drm.w_sched, drm.h_sched),
         w3_div(sprintf('id-drm-panel-container cl-drm-sched|width:100%%; height:100%%;'),
            w3_div('id-drm-panel-by-svc-static', drm_schedule_static()),
            w3_div('id-drm-panel-by-svc w3-iphone-scroll w3-absolute|width:100%; height:100%;', drm.loading_msg)
         )
      )

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(drm.w_sched + drm.cpanel_margin, drm.h_sched + drm.cpanel_margin);
	drm_database_cb('drm.database', 0, true);

   // in mobile mode close button just closes panel but keeps DRM running
	var el = w3_el('id-ext-controls-close');
      console.log('DRM mobile setup panelShown='+ w3_el('id-ext-controls').panelShown);

	el.onclick = function() {
	   toggle_panel("ext-controls", 0);
	   //extint_panel_hide();
      console.log('DRM mobile ext-controls-close panelShown='+ w3_el('id-ext-controls').panelShown);
	};

   w3_create_attribute('id-ext-controls-close-img', 'src', 'icons/close.black.24.png');
   drm.last_mobile = {};   // force rescale first time
   drm.rescale_cnt = drm.rescale_cnt2 = 0;
   drm.fit = '';

	if (drm.interval2 == null) drm.interval2 = setInterval(function() {
      mobile = ext_mobile_info(drm.last_mobile);
      drm.last_mobile = mobile;

      //extint_news('Dwh='+ mobile.width +','+ mobile.height +' '+ mobile.orient_unchanged +
      //   '<br>r='+ drm.rescale_cnt  +','+ drm.rescale_cnt2 +' '+ drm.fit +' #'+ drm.dseq);
      //drm.dseq++;

      if (mobile.orient_unchanged) return;
      drm.rescale_cnt++;

      var cwidth = w3_el('id-control').uiWidth;    // typ 365
      if (drm.w_nom + cwidth <= mobile.width) {    // can fit side-by-side
         drm.w_sched = mobile.width - cwidth - 60;
	      w3_el('id-ext-controls').style.zIndex = 125;       // so pinch zoom of id-control takes priority
         drm.fit = 'sbs'+ drm.w_sched;
      } else {
         drm.w_sched = mobile.width - drm.cpanel_margin*2;  // fit width
         w3_el('id-ext-controls').style.zIndex = 150;       // restore original priority when overlapped
         drm.fit = 'fw'+ drm.w_sched;
      }

      // don't need to scale using code below, just redraw
	   ext_set_controls_width_height(drm.w_sched + drm.cpanel_margin, drm.h_sched + drm.cpanel_margin);
      w3_el('id-drm-controls').style.width = px(drm.w_sched);
      w3_innerHTML('id-drm-panel-by-svc-static', drm_schedule_static());
      w3_innerHTML('id-drm-panel-by-svc', drm_schedule_svc());
      w3_innerHTML('id-drm-panel-by-time', drm_schedule_time_freq(0));
      w3_innerHTML('id-drm-panel-by-freq', drm_schedule_time_freq(1));

      /*
      var el = w3_el('id-ext-controls');
      console.log('$ id-ext-controls wh='+ mobile.width +','+ mobile.height +' uiw='+ el.uiWidth);
   
      if (mobile.narrow) {
         // scale control panel up or down to fit width of all narrow screens
         var scale = mobile.width / el.uiWidth * 0.95;
         //alert('scnW='+ mobile.width +' cpW='+ el.uiWidth +' sc='+ scale.toFixed(2));
         el.style.transform = 'scale('+ scale.toFixed(2) +')';
         el.style.transformOrigin = 'bottom left';    // panel has left:0 by default
         console.log('$ id-ext-controls scale='+ scale.toFixed(3) +' wh='+ mobile.width +','+ mobile.height);
         drm.rescale_cnt2++;
      } else {
         el.style.transform = 'none';
      }
      */
	}, 500);
}

function drm_desktop_controls_setup(w_graph)
{
   var s;
   var controls_inner, data_html = null;
   var h = 250;
   var w_lhs = 25;
   var w_msg = 500;
   var w_msg2 = 450;
   var pad = 10;

   if (drm.wrong_srate) {
      controls_inner =
            w3_text('w3-medium w3-text-css-yellow',
               'Currently, DRM does not support Kiwis configured for 20 kHz wide channels.'
            );
   } else
   
   if (drm.hacked) {
      controls_inner = w3_text('w3-medium w3-text-css-yellow', 'Yeah, nah..');
   } else
   
   if (drm.locked == 0) {
      if (kiwi.is_multi_core) {
         s = 'DRM not supported on this channel.';
      } else {
         // DRM_config_html() will have set cfg.DRM.nreg_chans before use here
         var drm_nreg_chans = cfg.DRM.nreg_chans;
         console_log('drm_nreg_chans', drm_nreg_chans);
         if (drm_nreg_chans == 0)
            s = 'Requires exclusive use of the Kiwi. There can be no other connections.';
         else {
            if (drm_nreg_chans == 1)
               s = 'Can only run DRM with one other Kiwi connection.<br>' +
                   'And the other connection is not using any extensions.';
            else
               s = 'Can only run DRM with '+ drm_nreg_chans +' or fewer other Kiwi connections.<br>' +
                   'And the other connections are not using any extensions.';
         }
         s += '<br>Please try again when these conditions are met.';
      }
      controls_inner = w3_text('w3-medium w3-text-css-yellow', s);
   } else {
      var svcs = 'Services:<br>';
      for (var i=1; i <= 4; i++) {
         svcs +=
            w3_inline('',
               w3_checkbox('w3-margin-R-8', '', 'drm.svc'+ i, (i == 1), 'drm_svcs_cbox_cb'),
               w3_div('id-drm-svc-'+ i +'|width:100%')
            );
      }
   
      var sblk = function(id)    { return id +' '+ w3_icon('id-drm-status-'+ id.toLowerCase(), 'fa-square', 16, 'grey') +' &nbsp;&nbsp;'; };
      var tblk = function(id,t)  { return w3_span('id-drm-'+ id +' cl-drm-blk', t); };
      var mblk = function(t)     { return tblk('mod-'+ t, t); };
      var oblk = function(t)     { return tblk('occ-'+ t, t); };
      var iblk = function(t)     { return tblk('ilv-'+ t, t); };
      var qsdc = function(t)     { return tblk('sdc-'+ t, t); };
      var qmsc = function(t)     { return tblk('msc-'+ t, t); };

      var width =  w_msg + w_graph;
      drm.w_sched = w_graph;
      var twidth = w_lhs + width;
      var margin = 10;

      data_html =
         time_display_html('drm') +
         
         w3_div(sprintf('id-drm-panel-0-info w3-relative w3-no-scroll|width:%dpx; height:%dpx; background-color:black;', twidth, h),
            w3_div(sprintf('id-drm-console-msg w3-margin-T-8 w3-small w3-text-white w3-absolute|left:%dpx; width:%dpx; height:%dpx; overflow-x:hidden;',
               w_lhs, w_msg, h),
               w3_div('id-drm-status', sblk('IO') + sblk('Time') + sblk('Frame') + sblk('FAC') + sblk('SDC') + sblk('MSC')),
               w3_inline('w3-halign-space-between|width:250px/',
                  w3_div('id-drm-if_level'),
                  w3_div('id-drm-snr')
               ),
               w3_divs('id-drm-mode w3-hide/w3-margin-T-6',
                  w3_inline('/w3-show-inline',
                     'DRM mode ', mblk('A'), mblk('B'), mblk('C'), mblk('D'),
                     '&nbsp;&nbsp;&nbsp; Chan ', oblk('4.5'), oblk('5'), oblk('9'), oblk('10'), oblk('18'), oblk('20'), ' kHz',
                     '&nbsp;&nbsp;&nbsp; ILV ', iblk('L'), iblk('S')
                  ),
                  w3_inline('/w3-show-inline',
                     'SDC ', qsdc('4'), qsdc('16'), ' QAM', '&nbsp;&nbsp;&nbsp; MSC ', qmsc('16'), qmsc('64'), ' QAM',
                     w3_div('id-drm-prot w3-margin-left'),
                     w3_div('id-drm-nsvc w3-margin-left')
                  ),
                  w3_div('id-drm-desc')
               ),
               w3_div('id-drm-error|width:'+ px(w_msg2)),
               w3_div('id-drm-svcs w3-hide|width:'+ px(w_msg2), svcs),
               '<br><br>',
               w3_div(sprintf('id-drm-msgs|width:%dpx', w_msg))
            ),

            w3_div(sprintf('id-drm-panel-graph w3-absolute|width:%dpx; height:%dpx; left:%dpx;', w_graph, h, w_lhs + w_msg),
               w3_div(sprintf('id-drm-panel-1-by-svc cl-drm-sched|width:%dpx; height:100%%;', w_graph),
                  w3_div('id-drm-tscale', drm_schedule_static()),
                  w3_div('id-drm-panel-by-svc w3-scroll-y w3-absolute|width:100%; height:100%;', drm.loading_msg)
               ),
               w3_div(sprintf('id-drm-panel-1-by-time cl-drm-sched|width:%dpx; height:100%%;', w_graph),
                  w3_div('id-drm-tscale', drm_schedule_static()),
                  w3_div('id-drm-panel-by-time w3-scroll-y w3-absolute|width:100%; height:100%;', drm.loading_msg)
               ),
               w3_div(sprintf('id-drm-panel-1-by-freq cl-drm-sched|width:%dpx; height:100%%;', w_graph),
                  w3_div('id-drm-tscale', drm_schedule_static()),
                  w3_div('id-drm-panel-by-freq w3-scroll-y w3-absolute|width:100%; height:100%;', drm.loading_msg)
               ),

               w3_div('id-drm-panel-1-graph w3-show-block',
                  //w3_canvas('id-drm-graph-if', w_graph, h/2, { pad_top:pad, pad_bottom:pad/2 }),
                  //w3_canvas('id-drm-graph-snr', w_graph, h/2, { pad_top:pad, pad_bottom:pad/2, top:h/2 })
                  w3_canvas('id-drm-graph-if', w_graph, h/2 - pad*2 , { top:pad }),
                  w3_canvas('id-drm-graph-snr', w_graph, h/2 - pad*2, { top:h/2 + pad })
               ),

               w3_canvas('id-drm-panel-1-IQ', w_graph, h, { pad:pad })
            )
         ) +

         w3_div('id-drm-options w3-display-right w3-text-white|top:230px; right:0px; width:200px; height:200px',
            w3_select_hier('w3-text-red w3-width-auto', '', '', 'drm.display_idx', drm.display_idx, drm.display_s, 'drm_display_cb'),
            w3_div('w3-margin-T-8',
               w3_divs('id-drm-options-by-svc id-drm-options-by-time id-drm-options-by-freq/w3-tspace-4',
                  w3_div('cl-drm-sched-options-time w3-light-green', 'verified'),
                  w3_div('cl-drm-sched-options-time', 'not verified'),
                  w3_link('w3-link-color', 'http://forum.kiwisdr.com/discussion/1865/drm-heard#latest', 'Please report<br>schedule changes')
               )
            )
         );

      controls_inner =
         w3_inline('w3-halign-space-between w3-margin-T-8/',
            w3_text('w3-text-white',
               'Schedules in top panel: Click on green/pink bars to tune a station. <br>' +
               'Use menu to sort schedules by service, time or frequency. <br>' +
               'Gray vertical lines are spaced 1 hour apart beginning at 00:00 UTC on the left. <br>' +
               'Red line shows current UTC time and updates while the extension is running. <br>' +
               '<span class="w3-text-yellow-highlight">New</span> ' +
               'Database menu below selects source of schedule information including ' +
               w3_link('w3-link-color', 'https://www.drmrx.org', 'drmrx.org')
            )
         ) +

         w3_inline('w3-margin-T-8/w3-margin-between-16',
            w3_select('w3-text-red', '', 'database', 'drm.database', drm.database, drm.database_s, 'drm_database_cb'),
            w3_button('id-drm-stop-button w3-padding-smaller w3-pink', 'Stop', 'drm_stop_start_cb'),
            w3_button('w3-padding-smaller w3-pink', 'Monitor IQ', 'drm_monitor_IQ_cb'),
            //w3_button('w3-padding-smaller w3-css-yellow', 'Reset', 'drm_reset_cb'),
            w3_button('w3-padding-smaller w3-aqua', 'Test 1', 'drm_test_cb', 1),
            w3_button('w3-padding-smaller w3-aqua', 'Test 2', 'drm_test_cb', 2),
            w3_div('id-drm-bar-container w3-progress-container w3-round-large w3-white w3-hide|width:160px; height:16px',
               w3_div('id-drm-bar w3-progressbar w3-round-large w3-light-green|width:'+ 50 +'%', '&nbsp;')
            )
         );
   }

   drm_panel_show(controls_inner, data_html);
	ext_set_data_height(h);
   
	if (drm.locked == 0) return;

	time_display_setup('drm');

	drm.canvas_if = w3_el('id-drm-graph-if');
	drm.canvas_if.ctx = drm.canvas_if.getContext("2d");
   drm.gr_if = graph_init(drm.canvas_if, { dBm:0, speed:1, averaging:false });
	graph_mode(drm.gr_if, 'auto');
	//graph_mode(drm.gr_if, 'fixed', 30, -30);
	graph_clear(drm.gr_if);
	//graph_marker(drm.gr_if, 30);
	
	drm.canvas_snr = w3_el('id-drm-graph-snr');
	drm.canvas_snr.ctx = drm.canvas_snr.getContext("2d");
   drm.gr_snr = graph_init(drm.canvas_snr, { dBm:0, speed:1, averaging:false });
	graph_mode(drm.gr_snr, 'auto');
	//graph_mode(drm.gr_snr, 'fixed', 30, -30);
	graph_clear(drm.gr_snr);
	//graph_marker(drm.gr_snr, 30);
	
	drm.canvas_IQ = w3_el('id-drm-panel-1-IQ');
	drm.canvas_IQ.ctx = drm.canvas_IQ.getContext("2d");
	drm.w_IQ = w_graph - pad*2;
	drm.h_IQ = h - pad*2;
	
	drm.desktop = 1;
}

function drm_controls_setup()
{
   drm_saved_mode();
   console.log('drm_controls_setup saved_mode='+ drm.saved_mode);
   drm.saved_passband = ext_get_passband();
   
   drm.is_stopped = 0;
   console.log('drm_controls_setup is_stopped='+ drm.is_stopped);

   drm.is_monitor = 0;
   console.log('drm_controls_setup is_monitor='+ drm.is_monitor);
   ext_send('SET monitor=0');

   // URL params that need to be setup before controls instantiated
	var p = drm.url_params = ext_param();
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         console.log('DRM param1 <'+ a +'>');
         var a1 = a.split(':');
         a1 = a1[a1.length-1].toLowerCase();
         w3_ext_param_array_match_str(drm.display_idx_s, a, function(i) { drm.display_idx = drm.display_idx_si[i]; });
         var r;
         if ((r = w3_ext_param('lo', a)).match) {
            drm.pb_lo = r.num;
         } else
         if ((r = w3_ext_param('hi', a)).match) {
            drm.pb_hi = r.num;
         } else
         if ((r = w3_ext_param('mobile', a)).match) {
            drm.mobile = r.has_value? r.num : 1;
         } else
         if ((r = w3_ext_param('debug', a)).match) {
            var debug = r.has_value? r.num : 0;
            ext_send('SET debug='+ debug);
         }
      });
   }
   
	var mobile = ext_mobile_info();
   var w_graph = drm.w_graph_nom - (Math.max(0, 1440 - mobile.width));
   drm.narrow_listing = (w_graph < drm.w_graph_nom) || (drm.mobile == 2);
   //console.log('$ whg='+ mobile.width +','+ mobile.height +','+ w_graph);
	
	//alert('mw='+ mobile.width +' w_graph='+ w_graph);
	if (drm.mobile || w_graph <= drm.w_graph_min) {
	   drm_mobile_controls_setup(mobile);
	} else {
	   drm_desktop_controls_setup(w_graph);
	}

	if (drm.locked == 0) return;
	drm.active = true;

	if (drm.url_params) {
      var freq = parseFloat(drm.url_params);
      if (freq) drm_pre_set_freq(freq);
   }

   // URL params that need to be setup after controls instantiated
	var p = drm.url_params;
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         //console.log('DRM param1 <'+ a +'>');
         var a1 = a.split(':');
         a1 = a1[a1.length-1].toLowerCase();
         var r;
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         } else
         if ((r = w3_ext_param('test', a)).match) {
            var test = r.has_value? r.num : 1;
            test = w3_clamp(test, 1, 2, 1);
            drm_station('Test Recording '+ test);
            w3_show('id-drm-bar-container');
            drm_test(test);
           
         }
      });
   }

   drm_run(1);
   // done after drm_run() so correct drm.saved_{mode,passband} is set
   drm_set_freq(ext_get_freq_kHz());
}

function drm_run(run)
{
   if (run == drm.run) return;

   if (run) {
      drm_saved_mode();
      console.log('drm_run saved_mode='+ drm.saved_mode);
      //kiwi_trace();
      drm.saved_passband = ext_get_passband();
   }

   //console.log('drm_run run='+ run);
   //kiwi_trace();
   drm.run = run;
   ext_send('SET run='+ drm.run);
}

function drm_test(test)
{
   //console.log('drm_test test='+ test);
   ext_send('SET test='+ test);
}

function drm_stop(from_stop_button)
{
   drm_test(0);
   drm_run(0);
   drm_reset_status();
   drm_station('');
   w3_hide('id-drm-bar-container');
   
   if (from_stop_button) {
      drm_set_mode('iq');
   } else {
      if (isDefined(drm.saved_passband)) {
         console_log_fqn('drm_stop RESTORE', 'drm.saved_passband.low', 'drm.saved_passband.high');
         ext_set_passband(drm.saved_passband.low, drm.saved_passband.high);
      }
      if (isDefined(drm.saved_mode)) {
         console_log_fqn('drm_stop RESTORE', 'drm.saved_mode');
         drm_set_mode(drm.saved_mode);
      }
   }
}

function drm_start()
{
   drm_test(0);
   drm_run(1);
   drm_set_mode('drm');
   drm.is_stopped = 0;     // for when called by drm_pre_select_cb()
   w3_button_text('id-drm-stop-button', 'Stop', 'w3-pink', 'w3-green');
}

function drm_stop_start_cb(path, cb_param)
{
   console.log('drm_stop_start_cb ENTRY is_stopped='+ drm.is_stopped);
   drm.is_stopped ^= 1;
   
   if (drm.is_stopped) {
      console.log('$drm_stop_start_cb: do stop, show start');
      drm_stop(1);
      w3_button_text(path, 'Start', 'w3-green', 'w3-pink');
   } else {
      console.log('$drm_stop_start_cb: do start, show stop');
      drm_start();
   }
}

function drm_monitor_IQ_cb(path, cb_param)
{
   console.log('drm_monitor_IQ_cb ENTRY is_monitor='+ drm.is_monitor +' cb_param='+ cb_param);
   drm.is_monitor ^= 1;
   
   if (drm.is_monitor) {
      console.log('$drm_monitor_IQ_cb: do monitor start');
      w3_remove_then_add(path, 'w3-pink', 'w3-green');
      ext_send('SET monitor=1');
   } else {
      console.log('$drm_monitor_IQ_cb: do monitor stop');
      w3_remove_then_add(path, 'w3-green', 'w3-pink');
      ext_send('SET monitor=0');
   }
}

function drm_set_passband()
{
   //console.log('### drm_set_passband override_pbw='+ override_pbw +' extint.override_pb='+ extint.override_pb); 
   
   // respect pb set in dx labels and in URL
   if (drm.pb_lo || drm.pb_hi || extint.override_pb) {
      console.log('drm_set_passband pb_lo,hi='+ drm.pb_lo +','+ drm.pb_hi +' override_pb='+ extint.override_pb);
   } else
   if (drm.special_passband) {   // can't simply clear on first use because special pb needs to get set several times
      console_log_fqn('drm_set_passband SPECIAL PB', 'drm.special_passband.low', 'drm.special_passband.high');
      ext_set_passband(drm.special_passband.low, drm.special_passband.high);
   } else {
      console.log('drm_set_passband DEFAULT PB');
      ext_set_passband(-5000, 5000);
   }
}

function drm_set_mode(mode)
{
   console_log('drm_set_mode', mode);
   //kiwi_trace();
   ext_set_mode(mode, null, { no_drm_proc:true });
   if (mode == 'drm') drm_set_passband();
}

function drm_set_freq(freq)
{
   drm.freq = freq;
   var zoom = ext_get_zoom();
   if (zoom < 5) zoom = 5; else
   if (zoom > 9) zoom = 9;

   // respect pb set in dx labels and in URL
   var pb_specified = (drm.pb_lo || drm.pb_hi || extint.override_pb);
   var saved_pb = pb_specified? ext_get_passband() : null;
   ext_tune(drm.freq, 'drm', ext_zoom.ABS, zoom);

   if (pb_specified) {
      //console.log('drm_set_freq PB FROM PARAMS '+ drm.pb_lo +','+ drm.pb_hi +' override_pb='+ extint.override_pb);
      //console.log('drm_set_freq PB FROM PARAMS saved: '+ saved_pb.low +','+ saved_pb.high);
      ext_set_passband(saved_pb.low, saved_pb.high);
   }
   
   drm_set_passband();
}

function drm_reset_cb(path, val, first)
{
   console.log('drm_reset_cb');
   ext_send('SET reset');
}

function drm_test_cb(path, val, first)
{
   console.log('drm_test_cb '+ val);
   drm_station('Test Recording '+ val);
   drm_run(0);
   drm_start();
   drm_test(val);
   drm_set_mode('drm');
   w3_show('id-drm-bar-container');
   drm_annotate('magenta');
}

function drm_svcs_cbox_cb(path, checked, first)
{
   var which = path.substr(-1,1);
   //console.log('drm_svcs_cbox_cb path='+ path +' checked='+ checked +' which='+ which);
   if (first) return;
   for (var i = 1; i <= 4; i++)
      w3_checkbox_set('drm.svc'+ i, false);
   w3_checkbox_set(path, true);
   ext_send('SET svc='+ which);
}

function drm_database_cb(path, idx, first)
{
   if (first)
      idx = readCookie('last_drm', 0);
   idx = +idx;
   writeCookie('last_drm', idx);
   w3_select_value(path, idx);
   drm.database = idx;
   console.log('drm_database_cb database='+ drm.database +' '+ drm.database_url[drm.database]);

   // Request json file with DRM station schedules.
   // Can't use file w/ .json extension since our file contains comments and
   // Firefox improperly caches json files with errors!
   // FIXME: rate limit this
   drm.using_default = drm.double_fault = false;
   var url, timeout;
   
   if (0 && !drm.timeout_tested) {
      drm.timeout_tested = true;
      url = drm.database_url[drm.database] +'.xxx';
      timeout = -3000;
   } else {
      url = drm.database_url[drm.database];
      timeout = 10000;
   }

   kiwi_ajax_progress(url, 'drm_get_stations_done_cb', 0, timeout);
}

function drm_display_cb(path, idx, first)
{
   idx = +idx;
   //console.log('$ drm_display_cb idx='+ idx);
   w3_select_value(path, idx);   // for benefit of non-default startup drm.display_idx values

   var i = 0, idx_actual = -1;
   var found = false;
	w3_select_enum(path, function(option) {
	   //console.log('drm_display_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML +' i='+ i);
	   
	   if (!option.disabled) {
         if (!found && option.value == idx) {
            idx_actual = i;
            found = true;
         }
         i++;
	   }
	   
	});

   var dsp = drm.display = idx_actual;
   w3_color('id-drm-panel-graph', null, (drm.panel_1[dsp] != 'graph')? 'black' : 'mediumBlue');
   
   console.log('$ drm_display_cb idx='+ idx +' show='+ dsp +' '+ drm.panel_0[dsp] +'/'+ drm.panel_1[dsp]);
   drm.panel_0.forEach(function(el) { w3_hide('id-drm-panel-0-'+ el); });
   drm.panel_1.forEach(function(el) { w3_hide('id-drm-panel-1-'+ el); });
   drm.panel_1.forEach(function(el) { w3_hide('id-drm-options-'+ el); });
   w3_show('id-drm-panel-0-'+ drm.panel_0[dsp]);
   w3_show('id-drm-panel-1-'+ drm.panel_1[dsp]);
   w3_show('id-drm-options-'+ drm.panel_1[dsp]);

   ext_send('SET send_iq='+ ((dsp >= drm.IQ_ALL && dsp <= drm.IQ_END)? 1:0));
}

function drm_station(s)
{
   if (!drm.desktop) return;
   var el = w3_el('id-drm-station');
   drm.last_station = s;
   if (s == '') s = '&nbsp;';
   if (!el) return;
   el.innerHTML = '<b>'+ (s.replace('_', ' ')) +'</b>';
   drm_annotate('magenta');
}

function DRM_environment_changed(changed)
{
   var dsp_freq = ext_get_freq()/1e3;
   var mode = ext_get_mode();
   //console.log('DRM ENV drm.freq='+ drm.freq +' dsp_freq='+ dsp_freq);
   if (drm.freq != dsp_freq || mode != 'drm') {
      drm_station('');
   }
}

function DRM_blur()
{
   console.log('DRM_blur saved_mode='+ drm.saved_mode);
   kiwi_clearInterval(drm.interval); drm.interval = null;
   kiwi_clearInterval(drm.interval2); drm.interval2 = null;
	drm.active = false;
   
   drm_stop(0);
   if (isUndefined(drm.saved_mode) || drm.saved_mode == 'drm') {
      console.log('DRM_blur FORCE iq');
      drm_set_mode('iq');
   }
   drm.locked = 0;
   ext_send('SET lock_clear');
   //alert('# DRM SET lock_clear');
   ext_set_data_height();     // restore default height
}

function DRM_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Digital Radio Mondiale (DRM30) decoder help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'Schedules in top panel: Click on green/pink bars to tune a station. <br>' +
               'Use menu to sort schedules by service, time or frequency. <br>' +
               'Gray vertical lines are spaced 1 hour apart beginning at 00:00 UTC on the left. <br>' +
               'Red line shows current UTC time and updates while the extension is running. <br>' +
               '<span class="w3-text-yellow-highlight">New</span> ' +
               'A database menu allows selection of the source of schedule information. <br>' +
               '<br>' +
            
               'With DRM selective fading can prevent even the strongest signals from being received properly. ' +
               'To see if signal fading is occurring adjust the waterfall "WF max/min" controls carefully so the ' +
               'waterfall colors are not saturated. The image below shows fading (dark areas) that might cause problems. See the ' +
               '<a href="http://forum.kiwisdr.com/discussion/1842/v1-360-drm-extension-now-available/p1" target="_blank">' +
               'Kiwi forum</a> for more information. ' +
               '<br><br><img src="gfx/DRM.sel.fade.png" /><br><br>' +
            
               'Custom passbands set before invoking the DRM extension will be respected. <br>' +
               'For example the passband field of a DX label that has mode DRM, or a passband specification in ' +
               'the URL e.g. "my_kiwi.com:8073/?pb=0,5k&ext=drm" ' +
               '<hr>' +

               'DRM code from <b>Dream 2.2.1</b> <br>' +
               'Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik <br>' +
               'Copyright (c) 2001-2020 &nbsp;&nbsp;&nbsp;&nbsp;' +
               '<a href="https://sourceforge.net/projects/drm" target="_blank">sourceforge.net/projects/drm</a> <br>' +
               'License: GNU General Public License version 2.0 (GPLv2) <br>' +
               '<hr>' +

               '<b>Fraunhofer FDK AAC Codec Library for Android</b> <br>' +
               '© Copyright  1995 - 2018 Fraunhofer-Gesellschaft zur Förderung der angewandten <br>' +
               'Forschung e.V. All rights reserved. <br>' +
               'For more information visit <a href="http://www.iis.fraunhofer.de/amm" target="_blank">www.iis.fraunhofer.de/amm</a> <br>' +
               '<hr>' +

               '<b>OpenCORE-AMR modifications to Fraunhofer FDK AAC Codec</b> <br>' +
               'Copyright (C) 2009-2011 Martin Storsjo &nbsp;&nbsp;&nbsp;&nbsp;' +
               '<a href="https://sourceforge.net/projects/opencore-amr" target="_blank">sourceforge.net/projects/opencore-amr</a> <br>' +
               'License: Apache License V2.0 <br>' +
               '<hr>' +

               'Features <b>NewsService Journaline(R)</b> decoder technology by <br>' +
               'Fraunhofer IIS, Erlangen, Germany. <br>' +
               'Copyright (c) 2003, 2004 <br>' +
               'For more information visit <a href="http://www.iis.fhg.de/dab" target="_blank">www.iis.fhg.de/dab</a> <br>' +
               'License: GNU General Public License version 2.0 (GPLv2) <br>' +
               '<hr>'
            )
         );
      confirmation_show_content(s, 610, 350);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   } else {
      if (drm.mobile) {
         return 'off';
         //return true;
      }
   }
   return true;
}

// called to display HTML for configuration parameters in admin interface
function DRM_config_html()
{
   // Let cfg.DRM.nreg_chans retain values > rx_chans if it was set when another configuration was used.
   // Just clamp the menu value to the current rx_chans;
   var default_nreg_chans = 3;      // FIXME: should be config param?
	var nreg_chans = ext_get_cfg_param('DRM.nreg_chans', default_nreg_chans);
	if (nreg_chans == -1) nreg_chans = default_nreg_chans;   // has never been set
	//console_log('nreg_chans/rx_chans', nreg_chans, rx_chans);
	drm.nreg_chans = Math.min(nreg_chans, rx_chans-1);
	var max_chans = Math.max(4, rx_chans);    // FIXME: "4" should be config param?
   drm.nreg_chans_u = { 0:'none' };
   for (var i = 1; i < max_chans; i++)
      drm.nreg_chans_u[i] = i.toFixed(0);
   
   var s =
      w3_inline_percent('w3-container',
         w3_div('w3-center',
            w3_select('w3-width-auto', 'Number of non-DRM connections allowed<br>when DRM in use',
               '', 'DRM.nreg_chans', drm.nreg_chans, drm.nreg_chans_u, 'admin_select_cb')
         ), 40
      );

   ext_config_html(drm, 'DRM', 'DRM', 'DRM configuration', s);
}
