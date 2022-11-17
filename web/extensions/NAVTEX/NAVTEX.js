// Copyright (c) 2017 John Seamons, ZL/KF6VO

var nt = {
   ext_name: 'NAVTEX',     // NB: must match navtex.c:navtex_ext.name
   first_time: true,
   test_location: false,
   
   //dataH: 300,
   dataH: 445,
   ctrlW: 550,
   ctrlH: 175,
   splitH: 100,
   lhs: 130,
   tw: 1024,
   x: 0,
   last_y: [],

   freqs: null,
   menu_s: [ ],
   menus: [ ],
   sfmt: 'w3-text-red w3-ext-retain-input-focus',
   
   SHOW_MSGS: 0,
   SHOW_MAP: 1,
   SHOW_SPLIT: 2,
   show: 0,
   show_s: [ 'messages', 'map', 'split' ],

   navtex_mode: 0,
   navtex_mode_s:    [ 'normal', 'DX' ],
   dsc_mode: 0,
   dsc_mode_s:       [ 'normal', '+errs' ],
   selcall_mode: 0,
   selcall_mode_s:   [ 'normal', '+errs', '+raw', '+both' ],
   MODE_NORMAL: 0,
   MODE_DX: 1,
   MODE_SHOW_ERRS: 1,
   MODE_SHOW_RAW: 2,
   MODE_SHOW_BOTH: 3,
   show_errs: 0,
   auto_zoom: 1,

   type: 0,
   TYPE_NAVTEX: 0,
   TYPE_DSC: 1,
   TYPE_SELCALL: 2,
   freq: 0,
   freq_s: '',
   cf_navtex: 500,         // menu freq is cf, so low cf can be used
   cf_dsc: 500,            // menu freq is cf, so low cf can be used
   cf_selcall: 1785,       // so cf/dial matches published freqs
   cf_selcall_low: 785,    // signals where lower freq (easier to listen to) can be used
   pb_skirt_width: 50,
   cf: 500,
   shift: 170,
   baud: 100,
   framing: '4/7',
   inverted: 0,
   encoding: 'CCIR476',

   dx: 0,
   dxn: 80,
   fifo: [],

   log_mins: 0,
   log_interval: null,
   log_txt: '',

   locations: {},
   too_old_min: 30,
   locations_visible: true,

   last_last: 0
};

function NAVTEX_main()
{
	ext_switch_to_client(nt.ext_name, nt.first_time, navtex_recv);		// tell server to use us (again)
	if (!nt.first_time)
		navtex_controls_setup();
	nt.first_time = false;
}

function navtex_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('navtex_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('navtex_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('navtex_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				var f = 'extensions/FSK/';
            kiwi_load_js(['pkgs_maps/pkgs_maps.js', 'pkgs_maps/pkgs_maps.css',
               f+'JNX.js', f+'BiQuadraticFilter.js', f+'CCIR476.js', f+'DSC.js', f+'Selcall.js'],
               'navtex_controls_setup');
				break;

			case "test_done":
				break;

			default:
				console.log('navtex_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function navtex_baud_error_init()
{
   var hh = nt.th/2;
   var cv = navtex_canvas;
   var ct = navtex_canvas.ctx;

   ct.fillStyle = 'white';
   ct.font = '14px Verdana';
   ct.fillText('Baud', nt.lhs/2-25, hh);
   ct.fillText('Error', nt.lhs/2-25, hh+14);
}

function navtex_baud_error(err)
{
   var max = 8;
   if (err > max) err = max;
   if (err < -max) err = -max;
   var h = Math.round(nt.th*0.4/2 * err/max);
   //console.log('err='+ err +' h='+ h);

   var bw = 20;
   var bx = nt.lhs - bw*2;
   var hh = nt.th/2;
   var cv = navtex_canvas;
   var ct = navtex_canvas.ctx;
   
   ct.fillStyle = 'black';
   ct.fillRect(bx,0, bw,nt.th);

   if (h > 0) {
      ct.fillStyle = 'lime';
      ct.fillRect(bx,hh-h, bw,h);
   } else {
      ct.fillStyle = 'red';
      ct.fillRect(bx,hh, bw,-h);
   }
}

// must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
var navtex_console_status_msg_p = { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, cols: 135 };

function navtex_output(s)
{
   navtex_console_status_msg_p.s = encodeURIComponent(s);

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-navtex-console-msgs', 'id-navtex-console-msg', navtex_console_status_msg_p);
}

function navtex_output_char(c)
{
   if (nt.type == nt.TYPE_NAVTEX && nt.dx) {    // ZCZC_STnn
      if (c == '\r' || c == '\n') c = ' ';
      nt.fifo.push(c);
      var s = nt.fifo.join('');
      //console.log('DX ['+ s +']');
      if (!s.startsWith('ZCZC')) {
         while (nt.fifo.length > 9) nt.fifo.shift();
      
         if (nt.dx_tail) {
            if (nt.dx_tail == nt.dxn && c == ' ') return;
            nt.dx_tail--;
            if (nt.dx_tail == 0) c += ' ...\n';
         } else {
            return;
         }
      } else {
         c = '';
         if (nt.dx_tail) c += ' ...\n';
         c += (new Date()).toUTCString().substr(5,20) +' UTC | ';
         if (nt.freq_s != '') c += nt.freq_s + ' | ';
         c += s.substr(0,9) +' | ';
         nt.fifo = [];
         nt.dx_tail = nt.dxn;
      }
   }
   
   navtex_output(c);
   nt.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('NAVTEX', c));
}

function navtex_audio_data_cb(samps, nsamps)
{
   nt.jnx.process_data(samps, nsamps);
}

var navtex_canvas;

function navtex_controls_setup()
{
   nt.th = nt.dataH;
	nt.saved_mode = ext_get_mode();

	nt.jnx = new JNX();
	nt.freq = ext_get_freq()/1e3;
	//w3_console.log(nt.jnx, 'nt.jnx');
	nt.jnx.set_baud_error_cb(navtex_baud_error);
	nt.jnx.set_output_char_cb(navtex_output_char);

   // URL params that need to be setup before controls instantiated
   var p = nt.url_params = ext_param();
	console.log('NAVTEX url_params='+ p);
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         var r;
         if ((r = w3_ext_param('zoom', a)).match) {
            if (isNumber(r.num)) {
               nt.auto_zoom = (r.num == 0)? 0:1;
            }
         }
      });
   }

   var wh = 'width:'+ px(nt.lhs+1024) +'; height:'+ px(nt.dataH) +';';
   var cbox = 'w3-label-inline w3-label-not-bold';

   var data_html =
      time_display_html('navtex') +
      
      w3_div('id-navtex-data w3-display-container|left:0px; '+ wh,

         // re w3-hide: for map to initialize properly it must be initially selected, then it will be hidden if
         // the 'map' URL param was not given.
         w3_div('id-navtex-msgs w3-hide|'+ wh +'; z-index:1; overflow:hidden; position:absolute;',
            '<canvas id="id-navtex-canvas" width='+ dq(nt.lhs+1024) +' height='+ dq(nt.dataH) +' style="left:0; position:absolute"></canvas>',
            w3_div('id-navtex-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left:'+ px(nt.lhs) +'; width:1024px; position:absolute; overflow-x:hidden;',
               '<pre><code id="id-navtex-console-msgs"></code></pre>'
            )
         ),
         w3_div('|left:'+ px(nt.lhs) +'; width:1024px; height:'+ px(nt.dataH) +'; position:absolute; z-index:0|id="id-navtex-map"')
      ) +

      w3_div('id-navtex-options w3-display-right w3-text-white|top:230px; right:0px; width:250px; height:200px',
         w3_text('w3-text-aqua w3-bold', 'Display options'),
         w3_select('w3-margin-T-4 w3-width-auto '+ nt.sfmt, '', 'show', 'nt.show', nt.show, nt.show_s, 'navtex_show_cb'),
         
         w3_checkbox('w3-margin-T-10//'+ cbox, 'Show day/night', 'nt.day_night_visible', true, 'navtex_day_night_visible_cb'),
         w3_checkbox(cbox, 'Show graticule', 'nt.graticule_visible', true, 'navtex_graticule_visible_cb'),

         w3_inline('w3-margin-T-10 w3-valign/', 
            w3_checkbox('//'+ cbox, 'Show locations', 'nt.locations_visible', true, 'navtex_locations_visible_cb'),
            w3_button('id-navtex-show-locations w3-margin-left class-button w3-small w3-grey w3-momentary', 'Clear old', 'navtex_clear_old_cb', 1)
         )
      );

	var controls_html =
		w3_div('id-navtex-controls w3-text-white',
			w3_divs('/w3-tspace-8',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b><a href="https://en.wikipedia.org/wiki/Navtex" target="_blank">NAVTEX</a> / ' +
				      '<a href="https://en.wikipedia.org/wiki/Digital_selective_calling" target="_blank">DSC</a> / ' +
				      '<a href="http://hflink.com/selcall" target="_blank">Selcall</a></b>'), 45,
					w3_div('', 'FSK from <b><a href="https://arachnoid.com/JNX/index.html" target="_blank">JNX</a></b>, P. Lutus &copy; 2011'), 55
				),
				
            w3_col_percent('',
               w3_div('id-navtex-station w3-text-css-yellow', '&nbsp;'), 50,
               w3_div('', 'NAVTEX schedules: ' +
                  '<a href="http://www.dxinfocentre.com/navtex.htm" target="_blank">MF</a>, ' +
                  '<a href="http://www.dxinfocentre.com/maritimesafetyinfo.htm" target="_blank">HF</a>'), 50
            ),

            w3_inline('id-navtex-menus/'),

            w3_inline('/w3-margin-between-16',
               w3_button('w3-padding-smaller', 'Next', 'w3_select_next_prev_cb', { dir:w3_MENU_NEXT, id:'nt.menu', func:'navtex_pre_select_cb' }),
               w3_button('w3-padding-smaller', 'Prev', 'w3_select_next_prev_cb', { dir:w3_MENU_PREV, id:'nt.menu', func:'navtex_pre_select_cb' }),

               w3_inline('',     // because of /w3-margin-between-16 above
                  w3_inline('id-navtex-mode/w3-margin-between-16',
                     w3_select('w3-text-red', '', 'display', 'nt.navtex_mode', 0, nt.navtex_mode_s, 'navtex_mode_cb')
                  ),
               
                  w3_inline('id-dsc-mode w3-hide/w3-margin-between-16',
                     w3_select('w3-text-red', '', 'display', 'nt.dsc_mode', 0, nt.dsc_mode_s, 'navtex_dsc_mode_cb')
                  ),
               
                  w3_inline('id-selcall-mode w3-hide/w3-margin-between-16',
                     w3_select('w3-text-red', '', 'display', 'nt.selcall_mode', 0, nt.selcall_mode_s, 'navtex_selcall_mode_cb')
                  )
               ),
               
               w3_checkbox('w3-label-inline w3-label-not-bold/', 'auto<br>zoom', 'nt.auto_zoom', nt.auto_zoom, 'navtex_auto_zoom_cb'),
               w3_button('w3-padding-smaller w3-css-yellow', 'Clear', 'navtex_clear_button_cb', 0),
               w3_button('id-navtex-log w3-padding-smaller w3-purple', 'Log', 'navtex_log_cb'),

               cfg.navtex.test_file? w3_button('w3-padding-smaller w3-aqua', 'Test', 'navtex_test_cb') : '',

               w3_input('id-navtex-log-mins/w3-label-not-bold/w3-ext-retain-input-focus|padding:0;width:auto|size=4',
                  'log min', 'nt.log_mins', nt.log_mins, 'navtex_log_mins_cb')
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('navtex');
	var el = w3_el('navtex-time-display');
	el.style.top = px(10);
	navtex_canvas = w3_el('id-navtex-canvas');
	navtex_canvas.ctx = navtex_canvas.getContext("2d");

   // URL params that need to be setup after controls instantiated
	if (nt.url_params) {
      p = nt.url_params.split(',');
      p.forEach(function(a, i) {
         //console.log('NAVTEX param2 <'+ a +'>');
         var r;
         if (w3_ext_param('dx', a).match) {
            navtex_mode_cb('id-nt.mode', nt.MODE_DX);
         } else
         if (w3_ext_param('errors', a).match) {
            navtex_dsc_mode_cb('id-nt.dsc_mode', nt.MODE_SHOW_ERRS);
            navtex_selcall_mode_cb('id-nt.selcall_mode', nt.MODE_SHOW_ERRS);
         } else
         if (w3_ext_param('raw', a).match) {
            navtex_selcall_mode_cb('id-nt.selcall_mode', nt.MODE_SHOW_RAW);
         } else
         if (w3_ext_param('both', a).match) {
            navtex_selcall_mode_cb('id-nt.selcall_mode', nt.MODE_SHOW_BOTH);
         } else
         if ((r = w3_ext_param('log_time', a)).match) {
            if (isNumber(r.num)) {
               navtex_log_mins_cb('id-nt.log_mins', r.num);
            }
         } else
         if (w3_ext_param('map', a).match) {
            nt.show = nt.SHOW_MAP;
         } else
         if (w3_ext_param('split', a).match) {
            nt.show = nt.SHOW_SPLIT;
         } else
         if (cfg.navtex.test_file && w3_ext_param('test', a).match) {
            ext_send('SET test');
         } else
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         }
      });
   }

	navtex_setup();
	navtex_baud_error_init();

   ext_set_data_height(nt.dataH);
	ext_set_controls_width_height(nt.ctrlW, nt.ctrlH);
	
	nt.kmap = kiwi_map_init('navtex', [12.5, 112.5], 5, 17);

	w3_do_when_rendered('id-navtex-menus', function() {
	   nt.ext_url = kiwi_SSL() +'files.kiwisdr.com/navtex/NAVTEX_freq_menus.cjson';
	   nt.int_url = kiwi_url_origin() +'/extensions/NAVTEX/NAVTEX_freq_menus.cjson';
	   nt.using_default = false;
	   nt.double_fault = false;
	   if (0 && dbgUs) {
         kiwi_ajax(nt.ext_url +'.xxx', 'navtex_get_menus_cb', 0, -500);
	   } else {
         kiwi_ajax(nt.ext_url, 'navtex_get_menus_cb', 0, 10000);
      }
   });

	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(navtex_audio_data_cb);

   // age locations
   nt.locations_age_interval = setInterval(function() {
      var old = Date.now() - nt.too_old_min*60*1000;
      w3_obj_enum(nt.locations, function(key, i, o) {
         if (o.upd < old) {
            console.log('LOC-OLD '+ o.loc_name +' > '+ nt.too_old_min +'m');
            o.el.style.background = 'grey';
         }
      });
   }, 60000);

   setTimeout(function() { navtex_show_cb('id-nt.show', nt.show); }, 1000);

   if (nt.test_location) setTimeout(function() {
         navtex_location_update('666666', 15, 115);
         navtex_location_update('4444', 15, 115);
         nt.test_location = false;
      }, 5000);
}

function navtex_get_menus_cb(freqs)
{
   nt.freqs = freqs;
   
   ext_get_menus_cb(nt, freqs,
      'navtex_get_menus_cb',  // retry_cb

      function(cb_param) {    // done_cb
         ext_render_menus(nt, 'navtex', 'nt');
   
         // first URL param can be a match in the preset menus
         if (nt.url_params) {
            var freq = parseFloat(nt.url_params);
            if (!isNaN(freq)) {
               // select matching menu item frequency
               var found = false;
               for (var i = 0; i < nt.n_menu; i++) {
                  var menu = 'nt.menu'+ i;
                  w3_select_enum(menu, function(option) {
                     //console.log('CONSIDER '+ parseFloat(option.innerHTML));
                     if (parseFloat(option.innerHTML) == freq) {
                        navtex_pre_select_cb(menu, option.value, false);
                        found = true;
                     }
                  });
                  if (found) break;
               }
            }
         }
      }, null
   );
}

function navtex_auto_zoom(shift)
{
   if (shift < 170) return 14;
   if (shift <= 200) return 13;
   if (shift <= 450) return 12;
   return 11;
}

function navtex_setup()
{
	nt.freq = ext_get_freq()/1e3;
   console.log('NAVTEX/DSC SETUP freq='+ nt.freq +' cf='+ nt.cf +' shift='+ nt.shift  +' baud='+ nt.baud +' framing='+ nt.framing +
      ' enc='+ nt.encoding +' inv='+ nt.inverted +' show_raw='+ nt.show_errs +' show_errs='+ nt.show_errs);
	nt.jnx.setup_values(ext_sample_rate(), nt.cf, nt.shift, nt.baud, nt.framing, nt.inverted, nt.encoding, nt.show_raw, nt.show_errs);
   navtex_crosshairs(1);
}

function navtex_crosshairs(vis)
{
   var ct = canvas_annotation.ctx;
   ct.clearRect(0,0, window.innerWidth,24);
   
   if (vis && ext_get_zoom() >= 10) {
      var f = ext_get_freq();
      if (nt.mode == 'usb') f += nt.cf;
      var f_range = get_visible_freq_range();
      //console.log(f_range);
      var Lpx = scale_px_from_freq(f - nt.shift/2, f_range);
      var Rpx = scale_px_from_freq(f + nt.shift/2, f_range);
      //console.log('NAVTEX crosshairs f='+ f +' Lpx='+ Lpx +' Rpx='+ Rpx);
      var d = 3;
      for (var i = 0; i < 6; i++) {
         var y = i*d;
         ct.fillStyle = (i&1)? 'black' : 'white';
         ct.fillRect(Lpx-d,y, d,d);
         ct.fillRect(Rpx-d,y, d,d);
         ct.fillStyle = (i&1)? 'white' : 'black';
         ct.fillRect(Lpx,y, d,d);
         ct.fillRect(Rpx,y, d,d);
      }
   }

   w3_show_hide_inline('id-navtex-mode', nt.type == nt.TYPE_NAVTEX);
   w3_show_hide_inline('id-dsc-mode', nt.type == nt.TYPE_DSC);
   w3_show_hide_inline('id-selcall-mode', nt.type == nt.TYPE_SELCALL);
}

function navtex_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('nt.menu')[1]);
   console.log('navtex_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);
   var header;
   var cont = 0;
   var found = false;

	w3_select_enum(path, function(option) {
	   if (found) return;
	   //console.log('navtex_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled && option.value != -1) {
	      if (cont)
	         header = header +' '+ option.innerHTML;
	      else
	         header = option.innerHTML;
	      cont = 1;
	   } else {
	      cont = 0;
	   }

	   if (option.value != idx || option.disabled) return;
	   found = true;

      nt.mode = 'cw';
      if (nt.menu_s[menu_n].includes('DSC')) {
         nt.cf = nt.cf_dsc;
         nt.type = nt.TYPE_DSC;
         nt.framing = '7/3';
         nt.encoding = 'DSC';
         nt.inverted = 1;
      } else
      if (nt.menu_s[menu_n].includes('Selcall')) {
         nt.mode = 'usb';
         nt.cf = header.includes('Beacons')? nt.cf_selcall_low : nt.cf_selcall;
         nt.type = nt.TYPE_SELCALL;
         nt.framing = '7/3';
         nt.encoding = 'Selcall';
         nt.inverted = 0;
      } else {
         nt.cf = nt.cf_navtex;
         nt.type = nt.TYPE_NAVTEX;
         nt.framing = '4/7';
         nt.encoding = 'CCIR476';
         nt.inverted = 0;
      }

      nt.freq_s = option.innerHTML;
      console.log('navtex_pre_select_cb opt.val='+ option.value +' freq_s='+ nt.freq_s);
      nt.freq = parseFloat(nt.freq_s);
      ext_tune(nt.freq, nt.mode, nt.auto_zoom? ext_zoom.ABS : ext_zoom.CUR, navtex_auto_zoom(nt.shift));
      navtex_setup();

      var pb_half = nt.shift/2 + nt.pb_skirt_width;
      console.log('navtex_pre_select_cb cf='+ nt.cf +' pb_half='+ pb_half);
      ext_set_passband(nt.cf - pb_half, nt.cf + pb_half);

      // set again to get correct freq given new passband
      ext_tune(nt.freq, nt.mode, nt.auto_zoom? ext_zoom.ABS : ext_zoom.CUR, navtex_auto_zoom(nt.shift));

      // if called directly instead of from menu callback, select menu item
      console.log('navtex_pre_select_cb path='+ path +' idx='+ idx);
      w3_select_value(path, idx);

      w3_el('id-navtex-station').innerHTML =
         '<b>'+ nt.menu_s[menu_n] +', '+ header +'</b>';
      
      if (header.includes('Beacons'))
         navtex_show_cb('id-nt.show', nt.SHOW_SPLIT);
	});

   // reset other menus
   navtex_clear_menus(menu_n);
}

function navtex_clear_menus(except)
{
   // reset frequency menus
   console.log('navtex_clear_menus except='+ except);
   for (var i = 0; i < nt.n_menu; i++) {
      if (!isArg(except) || i != except)
         w3_select_value('nt.menu'+ i, -1);
   }
}

function navtex_log_mins_cb(path, val)
{
   nt.log_mins = w3_clamp(+val, 0, 24*60, 0);
   console.log('navtex_log_mins_cb path='+ path +' val='+ val +' log_mins='+ nt.log_mins);
	w3_set_value(path, nt.log_mins);

   kiwi_clearInterval(nt.log_interval);
   if (nt.log_mins != 0) {
      console.log('NAVTEX logging..');
      nt.log_interval = setInterval(function() { navtex_log_cb(); }, nt.log_mins * 60000);
   }
}

function navtex_log_cb()
{
   var ts = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode;
   var txt = new Blob([nt.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = ((nt.type == nt.TYPE_DSC)? 'DSC.' : 'NAVTEX.') + ts +'.log.txt';
   document.body.appendChild(a);
   console.log('navtex_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

function NAVTEX_environment_changed(changed)
{
   //w3_console.log(changed, 'NAVTEX_environment_changed');
   if (!changed.passband_screen_location) return;
   navtex_crosshairs(1);

   if (changed.freq || changed.mode) {

      // reset all frequency menus when frequency etc. is changed by some other means (direct entry, WF click, etc.)
      // but not for changed.zoom, changed.resize etc.
      var dsp_freq = ext_get_freq()/1e3;
      var mode = ext_get_mode();
      //console.log('Navtex ENV nt.freq='+ nt.freq +' dsp_freq='+ dsp_freq);
      
      // remove menu selection if freq/mode changed by action outside of extension
      // but allow for +/- 1 kHz fine tuning
      var delta_fkHz = Math.abs(nt.freq - dsp_freq);
      if (delta_fkHz > 1 || mode != ((nt.type == nt.TYPE_SELCALL)? 'usb' : 'cw')) {
         navtex_clear_menus();
         w3_el('id-navtex-station').innerHTML = '&nbsp;';
      }
   }

   if (0 && changed.resize) {
      var el = w3_el('id-navtex-data');
      if (!el) return;
      var left = Math.max(0, (window.innerWidth - nt.dataW - time_display_width()) / 2);
      //console.log('navtex_resize wiw='+ window.innerWidth +' nt.dataW='+ nt.dataW +' time_display_width='+ time_display_width() +' left='+ left);
      el.style.left = px(left);
      return;
   }
}

function navtex_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   w3_select_value(path, idx);
   nt.dx = nt.show_raw = nt.show_errs = 0;

   switch (idx) {
      case nt.MODE_DX:
         nt.dx = 1;
         break;
   }
   
   navtex_setup();
}

function navtex_dsc_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   w3_select_value(path, idx);
   nt.show_raw = nt.show_errs = 0;

   switch (idx) {
      case nt.MODE_SHOW_ERRS:
         nt.show_errs = 1;
         break;
   }
   
   navtex_setup();
}

function navtex_selcall_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   w3_select_value(path, idx);
   nt.show_raw = nt.show_errs = 0;

   switch (idx) {
      case nt.MODE_SHOW_ERRS:
         nt.show_errs = 1;
         break;

      case nt.MODE_SHOW_RAW:
         nt.show_raw = 1;
         break;

      case nt.MODE_SHOW_BOTH:
         nt.show_errs = nt.show_raw = 1;
         break;
   }
   
   navtex_setup();
}

function navtex_auto_zoom_cb(path, checked, first)
{
   if (first) return;
   checked = checked? 1:0;
   //console.log('navtex_auto_zoom_cb checked='+ checked);
   nt.auto_zoom = checked;
   w3_checkbox_set(path, checked);
}

function navtex_clear_button_cb(path, idx, first)
{
   if (first) return;
   navtex_output('\f');
   nt.log_txt = '';
   
   // if the map is showing clear all the markers as well
   if (nt.show == nt.SHOW_MAP || nt.show == nt.SHOW_SPLIT) navtex_clear_old_cb('', 2);
}

function navtex_show_cb(path, idx, first)
{
	//console.log('navtex_show_cb: idx='+ idx +' first='+ first);
	if (first) return;
   idx = +idx;
   nt.show = idx;
	w3_set_value(path, idx);
	w3_hide2('id-navtex-msgs', idx == nt.SHOW_MAP);
	w3_hide2('id-navtex-map', idx == nt.SHOW_MSGS);
	w3_el('id-navtex-msgs').style.top = px((idx == nt.SHOW_SPLIT)? (nt.dataH - nt.splitH) : 0);
	w3_el('id-navtex-msgs').style.height = px((idx == nt.SHOW_SPLIT)? nt.splitH : nt.dataH);
	if (idx == nt.SHOW_SPLIT)
	   w3_scrollDown('id-navtex-console-msg');
	if (idx != nt.SHOW_MSGS)
      navtex_output("\nMap only displays Selcall beacon locations (e.g. not DSC)");
}

function navtex_day_night_visible_cb(path, checked, first)
{
   if (first) return;
   if (!nt.kmap.day_night) return;
   var checked = w3_checkbox_get(path);
   kiwi_map_day_night_visible(nt.kmap, checked);
}

function navtex_graticule_visible_cb(path, checked, first)
{
   //console.log('navtex_graticule_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   if (!nt.kmap.graticule) return;
   checked = w3_checkbox_get(path);
   kiwi_map_graticule_visible(nt.kmap, checked);
}

function navtex_locations_visible_cb(path, checked, first)
{
   //console.log('navtex_locations_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   nt.locations_visible = checked;
	kiwi_map_markers_visible('id-navtex-location', checked);
}

function navtex_clear_old_cb(path, idx, first)
{
   //console.log('navtex_clear_old_cb idx='+ idx +' first='+ first);
   if (!(+idx)) return;
   var all = (+idx == 2);
   var old = Date.now() - nt.too_old_min*60*1000;
   w3_obj_enum(nt.locations, function(key, i, o) {
      if (!all && o.upd > old) return;
      console.log('LOC-CLR '+ o.loc_name);
      o.mkr.remove();
      delete nt.locations[o.loc_name];
   });
}

function navtex_location_update(loc_name, lat, lon, url, color)
{
   var dup;
   
   if (!nt.locations[loc_name]) {
      console.log('LOC-NEW '+ loc_name +' '+ lat.toFixed(2) +' '+ lon.toFixed(2));

      var marker = kiwi_map_add_marker_div(nt.kmap, kmap.NO_ADD_TO_MAP,
         [lat, lon], '', [12, 12], [0, 0], 1.0);
      var loc_o = { loc_name: loc_name, mkr: marker, upd: Date.now(), pos: [] };
      if (nt.test_location && loc_name.startsWith('ABC'))
         loc_o.upd -= (nt.too_old_min+10)*60*1000;
      loc_o.pos.push([lat, lon]);
      nt.locations[loc_name] = loc_o;
      
      kiwi_style_marker(nt.kmap, kmap.ADD_TO_MAP, marker, loc_name,
         'id-navtex-location id-navtex-location-'+ loc_name + (nt.locations_visible? '' : ' w3-hide'),
         kmap.DIR_RIGHT,
         function(ev) {
            // sometimes multiple instances exist, so iterate to catch them all
            w3_iterate_classname('id-navtex-location-'+ loc_name,
               function(el) {
                  if (!el) return;
                  //console.log(el);
                  loc_o.el = el;
                  var fg = color? color[0] : 'white';
                  var bg = color? color[1] : 'blue';
                  w3_color(el, fg, bg);
                  loc_o.orig_bg = bg;

                  el.addEventListener('mouseenter', function(ev) {
                     var mkr = ev.target;
                     nt.saved_color = mkr.style.color;
                     nt.saved_bkg = mkr.style.background;
                     mkr.style.color = 'black';
                     mkr.style.background = 'yellow';
                     mkr.style.zIndex = 9001;
                  });
                  el.addEventListener('mouseleave', function(ev) {
                     var mkr = ev.target;
                     mkr.style.color = nt.saved_color;
                     mkr.style.background = nt.saved_bkg;
                     mkr.style.zIndex = 9000;
                  });
                  el.addEventListener('click', function(ev) {
                     console.log('*click*');
                     if (!url) return;
                     var a = document.createElement('a');
                     a.setAttribute('href', url);
                     a.setAttribute('target', '_blank');
                     document.body.appendChild(a);
                     console.log(a);
                     a.click();
                     document.body.removeChild(a);
                  });
               }
            );
         }
      );

      dup = false;
   } else {
      var loc_o = nt.locations[loc_name];
      var marker = loc_o.mkr;
      marker.setLatLng([lat, lon]);
      var n = loc_o.pos.push([lat, lon]);
      
      // might have re-appeared after previously going grey
      if (loc_o.el)
         loc_o.el.style.background = loc_o.orig_bg;     

      var now = Date.now();
      var dt = Math.floor(((now - loc_o.upd) / 60000) % 60);
      loc_o.upd = now;
      console.log('LOC-UPD '+ loc_name +' '+ lat.toFixed(2) +' '+ lon.toFixed(2) +' #'+ n +' '+ dt +'m');
      dup = true;
   }
   
   return dup;
}

function navtex_test_cb(path, idx, first)
{
   ext_send('SET test');
}

function NAVTEX_blur()
{
	ext_unregister_audio_data_cb();
	ext_set_mode(nt.saved_mode);
   navtex_crosshairs(0);
   kiwi_clearInterval(nt.log_interval);
   kiwi_clearInterval(nt.locations_age_interval);
   kiwi_map_blur(nt.kmap);
}

// called to display HTML for configuration parameters in admin interface
function NAVTEX_config_html()
{
   var s =
      w3_inline_percent('w3-container',
         w3_div('w3-restart',
            w3_input_get('', 'Test filename', 'navtex.test_file', 'w3_string_set_cfg_cb', 'NAVTEX.test.12k.au')
         ), 40
      );

   ext_config_html(nt, 'navtex', 'NAVTEX', 'NAVTEX configuration', s);
}
