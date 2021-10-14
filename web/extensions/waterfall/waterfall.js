// Copyright (c) 2021 John Seamons, ZL/KF6VO


var wfext = {    // "wf" is already in openwebrx.js
   ext_name: 'waterfall',     // NB: must match waterfall.cpp:waterfall_ext.name
   first_time: true,
   sfmt: 'w3-text-red w3-ext-retain-input-focus',

   aper_algo: 3,
   aper_algo_s: [ 'IIR', 'MMA', 'EMA', 'off' ],
   aper_algo_e: { IIR:0, MMA:1, EMA:2, OFF:3 },
   aper_param: 0,
   aper_param_s: [ 'gain', 'averages', 'averages', '' ],
   aper_params: {
      IIR_min:0, IIR_max:2, IIR_step:0.1, IIR_def:0.8, IIR_val:undefined,
      MMA_min:1, MMA_max:16, MMA_step:1, MMA_def:2, MMA_val:undefined,
      EMA_min:1, EMA_max:16, EMA_step:1, EMA_def:2, EMA_val:undefined
   },
   
   tstamp: 0,
   tstamp_f: '',
   tstamp_i: 0,
   tstamp_s: [ 'off', '2s', '5s', '10s', '30s', '1m', '5m', '10m', '30m', '60m', 'custom' ],
   tstamp_v: [ 0, 2, 5, 10, 30, 60, 300, 600, 1800, 3600, -1 ],

   tstamp_tz_s: [ 'UTC', 'local' ],
   
   winf_i: 2,
   winf_s: [ 'Hanning', 'Hamming', 'Blackman-Harris', 'none' ],
   
   interp_i: 3,
   interp_s: [ 'max', 'min', 'last', 'drop samp', 'CMA' ],
   
   cic_comp: true
};

function waterfall_main()
{
	ext_switch_to_client(wfext.ext_name, wfext.first_time, waterfall_recv);		// tell server to use us (again)
	if (!wfext.first_time)
		waterfall_controls_setup();
	wfext.first_time = false;
}

function waterfall_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('waterfall_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('waterfall_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('waterfall_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				waterfall_controls_setup();
				break;

			default:
				console.log('waterfall_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function waterfall_controls_setup()
{
	var controls_html =
		w3_div('id-waterfall-controls w3-text-white',
		   w3_divs('',
            w3_div('w3-medium w3-text-aqua w3-margin-B-16', '<b>Waterfall control</b>'),

            w3_col_percent('w3-valign w3-margin-T-8/',
               w3_text('w3-text-css-orange', '<b>Aperture<br>auto<br>mode</b>'), 17,
               w3_select('id-wfext-aper-algo w3-text-red', 'Averaging', '', 'wfext.aper_algo', wfext.aper_algo, wfext.aper_algo_s, 'waterfall_aper_algo_cb'), 20,
               w3_div('id-wfext-aper-param',
                  w3_slider('id-wfext-aper-param-slider', 'Parameter', 'wfext.aper_param', wfext.aper_param, 0, 10, 1, 'waterfall_aper_param_cb')
               ), 40,
               '&nbsp;', 3, w3_div('id-wfext-aper-param-field')
            ),
            
            w3_inline('id-wfext-maxmin w3-background-fade w3-margin-T-8 w3-hide w3-text-white w3-small|background:#575757/',
               'Min/max:&nbsp;', w3_div('id-wfext-min'), '/', w3_div('id-wfext-max'), '&nbsp;=&nbsp;',
               w3_div('id-wfext-min-comp'), '/', w3_div('id-wfext-max-comp'), '&nbsp;(computed) +&nbsp;',
               w3_div('id-wfext-min-floor'), '/', w3_div('id-wfext-max-ceil'), '&nbsp;(floor/ceil)'
            ),
            w3_div('id-wfext-maxmin-spacer w3-margin-T-8 w3-small', '&nbsp;'),
            
            //w3_hr('w3-margin-10'),
            w3_inline('w3-margin-T-8/w3-hspace-16',
               w3_text('w3-text-css-orange', '<b>Timestamps</b>'),
               w3_select('id-wfext-tstamp '+ wfext.sfmt, '', '', 'wfext.tstamp_i', wfext.tstamp_i, wfext.tstamp_s, 'wfext_tstamp_cb'),
               w3_input('id-wfext-tstamp-custom w3-ext-retain-input-focus w3-hide|padding:0;width:auto|size=4',
                  '', 'wfext.tstamp_f', wfext.tstamp_f, 'wfext_tstamp_custom_cb'),
               w3_select(wfext.sfmt, '', '', 'wf.ts_tz', wf.ts_tz, wfext.tstamp_tz_s, 'w3_num_cb')
            ),
            
            //w3_hr('w3-margin-10'),
            w3_inline('w3-margin-T-8/w3-hspace-16',
               w3_text('w3-text-css-orange', '<b>FFT</b>'),
               w3_select(wfext.sfmt, '', 'window function', 'wfext.winf_i', wfext.winf_i, wfext.winf_s, 'wfext_winf_cb'),
               w3_select(wfext.sfmt, '', 'interpolation', 'wfext.interp_i', wfext.interp_i, wfext.interp_s, 'wfext_interp_cb'),
               w3_checkbox('w3-label-inline w3-label-not-bold', 'CIC<br>comp', 'wfext.cic_comp', wfext.cic_comp, 'wfext_cic_comp_cb')
            )
         )
		);

	ext_panel_show(controls_html, null);
	ext_set_controls_width_height(440, 250);
	
	if (wf.aper == kiwi.aper_e.auto) {
      w3_show_inline('id-wfext-maxmin');
      waterfall_maxmin_cb();
      w3_hide('id-wfext-maxmin-spacer');
   }
}

function waterfall_init()
{
   var init_aper = +ext_get_cfg_param('init.aperture', -1, EXT_NO_SAVE);
   //console.log('waterfall_init: init_aper='+ init_aper +' url_tstamp='+ wf.url_tstamp);
   var last_aper = readCookie('last_aper', (init_aper == -1)? 0 : init_aper);
   wf_aper_cb('wf.aper', last_aper, false);     // writes 'last_aper' cookie
   w3_show('id-aper-data');
   
   if (wf.url_tstamp) wfext.tstamp = wf.url_tstamp;
}

function waterfall_aper_algo_cb(path, idx, first)
{
   if (first) {
      idx = +readCookie('last_aper_algo', wfext.aper_algo_e.OFF);
      w3_set_value(path, idx);
   } else {
      idx = +idx;
   }
   //console.log('waterfall_aper_algo_cb ENTER path='+ path +' idx='+ idx +' first='+ first);
   //kiwi_trace('waterfall_aper_algo_cb');
   wf.aper_algo = wfext.aper_algo = +idx;

   if (wfext.aper_algo == wfext.aper_algo_e.OFF) {
      w3_hide(w3_el('id-wfext-aper-param').parentElement);
      w3_innerHTML('id-wfext-aper-param-field', 'aperture auto-scale on <br> waterfall pan/zoom only');
      colormap_update();
   } else {
      var f_a = wfext.aper_algo;
      var f_s = wfext.aper_algo_s[f_a];
      var f_p = wfext.aper_params;
      var val = f_p[f_s +'_val'];
      //console.log('waterfall_aper_algo_cb menu='+ f_a +'('+ f_s +') val='+ val);
   
      // update slider to match menu change
      w3_show(w3_el('id-wfext-aper-param').parentElement);
      waterfall_aper_param_cb('id-wfext-aper-param-slider', val, /* done */ true, /* first */ false);
      //console.log('waterfall_aper_algo_cb EXIT path='+ path +' menu='+ f_a +'('+ f_s +') param='+ wfext.aper_param);
   }
   
	writeCookie('last_aper_algo', wfext.aper_algo.toString());
   freqset_select();
}

function waterfall_aper_param_cb(path, val, done, first)
{
   if (first) return;
   val = +val;
   //console.log('waterfall_aper_param_cb ENTER path='+ path +' val='+ val +' done='+ done);
   //kiwi_trace('waterfall_aper_param_cb');
   var f_a = wfext.aper_algo;
   var f_s = wfext.aper_algo_s[f_a];
   var f_p = wfext.aper_params;

   if (isUndefined(val) || isNaN(val)) {
      val = f_p[f_s +'_def'];
      //console.log('waterfall_aper_param_cb using default='+ val +'('+ typeof(val) +')');
      var lsf = parseFloat(readCookie('last_aper_algo'));
      var lsfp = parseFloat(readCookie('last_aper_param'));
      if (lsf == f_a && !isNaN(lsfp)) {
         //console.log('waterfall_aper_param_cb USING READ_COOKIE last_aper_param='+ lsfp);
         val = lsfp;
      }
   }

	wf.aper_param = wfext.aper_param = f_p[f_s +'_val'] = val;
	//console.log('waterfall_aper_param_cb UPDATE slider='+ val +' menu='+ f_a +' done='+ done +' first='+ first);

   // needed because called by waterfall_aper_algo_cb()
   w3_slider_setup('id-wfext-aper-param-slider', f_p[f_s +'_min'], f_p[f_s +'_max'], f_p[f_s +'_step'], val);
   w3_innerHTML('id-wfext-aper-param-field', (f_a == wfext.aper_algo_e.OFF)? '' : (val +' '+ wfext.aper_param_s[f_a]));

   if (done) {
	   //console.log('waterfall_aper_param_cb DONE WRITE_COOKIE last_aper_param='+ val.toFixed(2));
	   writeCookie('last_aper_param', val.toFixed(2));
      colormap_update();
      freqset_select();
   }

   //console.log('waterfall_aper_param_cb EXIT path='+ path);
}

function waterfall_maxmin_cb()
{
   w3_flash_fade('id-wfext-maxmin', 'cyan', 50, 300, '#575757');
   w3_innerHTML('id-wfext-max', maxdb.toString().positiveWithSign());
   w3_innerHTML('id-wfext-max-comp', wf.auto_maxdb.toString().positiveWithSign());
   w3_innerHTML('id-wfext-max-ceil', wf.auto_ceil.val.toString().positiveWithSign());
   w3_innerHTML('id-wfext-min', mindb.toString().positiveWithSign());
   w3_innerHTML('id-wfext-min-comp', wf.auto_mindb.toString().positiveWithSign());
   w3_innerHTML('id-wfext-min-floor', wf.auto_floor.val.toString().positiveWithSign());
}

function wfext_tstamp_cb(path, idx, first)
{
   //if (first) return;
   console.log('wfext_tstamp_cb TOP first='+ first);
   wfext.tstamp_i = idx = +idx;
   w3_set_value(path, idx);      // for benefit of direct callers
   var tstamp_s = wfext.tstamp_s[idx];
   var isCustom = (tstamp_s == 'custom');
   var el_custom = w3_el('id-wfext-tstamp-custom');

   if (first) {
      if (!wf.url_tstamp) {
         if (isCustom) wfext_tstamp_custom_cb(el_custom, wfext.tstamp);
         return;
      }
      console.log('wfext_tstamp_cb HAVE url_tstamp='+ wf.url_tstamp);
      var stop = false;
      w3_select_enum(path, function(el, idx) {
         if (stop || el.innerHTML != 'custom') return;
         console.log('wfext_tstamp_cb MATCH url_tstamp='+ wf.url_tstamp);
         wfext_tstamp_custom_cb('id-wfext-tstamp-custom', wf.url_tstamp);
         wfext_tstamp_cb(path, el.value);
         stop = true;
      });
      wf.url_tstamp = 0;   // don't do on subsequent extension opens
      return;
   }

   console.log('wfext_tstamp_cb idx='+ idx +' tstamp_s='+ tstamp_s +' isCustom='+ isCustom);
   w3_show_hide(el_custom, isCustom);

   if (isCustom) {
	   wfext_tstamp_custom_cb(el_custom, w3_get_value(el_custom));
   } else {
      wfext.tstamp = wfext.tstamp_v[idx];
   }
}

function wfext_tstamp_custom_cb(path, val)
{
   console.log('wfext_tstamp_custom_cb val='+ val);
   wfext.tstamp = w3_clamp(+val, 2, 60*60, 2);
   w3_set_value(path, wfext.tstamp);
   w3_show_block(path);
}

function wfext_winf_cb(path, idx, first)
{
   //if (first) return;    // NB: commented out so default from wfext.winf_i will be sent to server
   w3_num_cb(path, idx, first);
	wf_send('SET window_func='+ +idx);
}

function wfext_interp_cb(path, idx, first)
{
   if (first) return;
   w3_num_cb(path, idx, first);
   idx = +idx + (wfext.cic_comp? 10:0);
	wf_send('SET interp='+ idx);
	console.log('wfext_interp_cb idx='+ idx);
}

function wfext_cic_comp_cb(path, checked, first)
{
   if (first) return;
   w3_bool_cb(path, checked, first);
   wfext_interp_cb('', wfext.interp_i);
}

/*
function waterfall_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Waterfall Help') +
         '<br><br>' +
         w3_div('w3-text-css-orange', '<b>Aperture auto mode</b>') +
         'xxx' +

         '<br><br>' +
         w3_div('w3-text-css-orange', '<b>Waterfall timestamps</b>') +
         'xxx' +
         '';
      confirmation_show_content(s, 610, 350);
   }
   return true;
}
*/
