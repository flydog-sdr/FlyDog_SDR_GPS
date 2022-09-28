// Copyright (c) 2018 John Seamons, ZL/KF6VO

var cw = {
   ext_name: 'CW_decoder',    // NB: must match cw_decoder.cpp:cw_decoder_ext.name
   first_time: true,
   pboff: -1,
   wspace: true,
   thresh: false,
   threshold: 49,

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, cols: 135 },

   log_mins: 0,
   log_interval: null,
   log_txt: '',

   last_last: 0
};

function CW_decoder_main()
{
	ext_switch_to_client(cw.ext_name, cw.first_time, cw_decoder_recv);		// tell server to use us (again)
	if (!cw.first_time)
		cw_decoder_controls_setup();
	cw.first_time = false;
}

function cw_decoder_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('cw_decoder_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('cw_decoder_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('cw_decoder_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				kiwi_load_js(['pkgs/js/graph.js'], 'cw_decoder_controls_setup');
				break;

			case "cw_chars":
				cw_decoder_output_chars(param[1]);
				break;

			case "cw_wpm":
				w3_innerHTML('id-cw-wpm', param[1] +' WPM');
				break;

			case "cw_train":
			   var el = w3_el('id-cw-train');
			   if (!el) break;
			   var p = +param[1];
			   if (p < 0)
			      w3_innerHTML(el, 'error '+ (-p) +'/4');
			   else
			      w3_innerHTML(el, 'train '+ p +'/98');
			   w3_background_color(el, (p < 0)? 'orange':'lime');
            w3_show_hide(el, p);
				break;
			
			case "cw_plot":
			   graph_plot(cw.gr, +param[1]);
			   break;

			default:
				console.log('cw_decoder_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function cw_decoder_output_chars(c)
{
   cw.console_status_msg_p.s = c;      // NB: already encoded on C-side
   cw.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('CW', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-cw-console-msgs', 'id-cw-console-msg', cw.console_status_msg_p);
}

function cw_decoder_controls_setup()
{
   var data_html =
      time_display_html('cw') +
      
      w3_div('id-cw-data|left:150px; width:1044px; height:300px; overflow:hidden; position:relative; background-color:mediumBlue;',
         '<canvas id="id-cw-canvas" width="1024" height="180" style="position:absolute; padding: 10px"></canvas>',
			w3_div('id-cw-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|top:200px; width:1024px; height:100px; position:absolute; overflow-x:hidden;',
			   '<pre><code id="id-cw-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-cw-controls w3-text-white',
			w3_divs('',
            w3_col_percent('',
               w3_div('',
				      w3_div('w3-medium w3-text-aqua', '<b>CW decoder</b>'),
                  w3_div('id-cw-wpm w3-margin-T-4', '0 WPM')
				   ), 30,
					w3_div('', 'From Loftur Jonasson, TF3LJ / VE2LJX <br> and the <b><a href="https://github.com/df8oe/UHSDR" target="_blank">UHSDR project</a></b> &copy; 2016'), 55
				),
				w3_inline('w3-margin-T-4/w3-margin-between-16',
               w3_button('w3-padding-smaller', 'Clear', 'cw_clear_button_cb', 0),
               w3_checkbox('w3-label-inline w3-label-not-bold', 'word space<br>correction', 'cw.wspace', true, 'cw_decoder_wsc_cb'),
               w3_input('id-cw-threshold/w3-label-not-bold/|padding:0;width:auto|size=4', 'threshold', 'cw.threshold', cw.threshold, 'cw_decoder_threshold_cb'),
               w3_button('w3-padding-smaller', 'Reset', 'cw_reset_cb', 0),
               w3_div('id-cw-train w3-padding-small w3-text-black w3-hide', 'train'),
               w3_button('id-cw-log w3-padding-smaller w3-purple', 'Log', 'cw_log_cb'),
               w3_input('id-cw-log-mins/w3-label-not-bold/w3-ext-retain-input-focus|padding:0;width:auto|size=4',
                  'log min', 'cw.log_mins', cw.log_mins, 'cw_log_mins_cb')
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('cw');

	cw.canvas = w3_el('id-cw-canvas');
	cw.canvas.ctx = cw.canvas.getContext("2d");

   cw.gr = graph_init(cw.canvas, { dBm:0, speed:1, averaging:true });
	//graph_mode(cw.gr, 'auto');
	graph_mode(cw.gr, 'fixed', 55-10, 30+5);
	graph_clear(cw.gr);
	cw_decoder_threshold_cb('cw.threshold', cw.threshold);

   ext_set_data_height(300);
	ext_set_controls_width_height(550, 100);
	
	var p = ext_param();
	cw.pboff_locked = parseFloat(p);
	console.log('CW pboff_locked='+ cw.pboff_locked);
	
   cw_clear_button_cb();
	ext_send('SET cw_start');
	cw.pboff = -1;
	CW_decoder_environment_changed();
}

function CW_decoder_environment_changed(changed)
{
   // detect passband offset change and inform C-side
   var pboff = Math.abs(ext_get_passband_center_freq() - ext_get_carrier_freq());
   if (cw.pboff != pboff) {
      var first_and_locked = (cw.pboff == -1 && cw.pboff_locked);
      var pbo = first_and_locked? cw.pboff_locked : pboff;
	   if (first_and_locked || !cw.pboff_locked) {
         console.log('CW ENV new pbo='+ pbo);
	      ext_send('SET cw_pboff='+ pbo);
	   }
      cw.pboff = pboff;
   }
}

function cw_clear_button_cb(path, idx, first)
{
   if (first) return;
   cw.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-cw-console-msgs', 'id-cw-console-msg', cw.console_status_msg_p);
   cw.log_txt = '';
}

function cw_log_mins_cb(path, val)
{
   cw.log_mins = w3_clamp(+val, 0, 24*60, 0);
   console.log('cw_log_mins_cb path='+ path +' val='+ val +' log_mins='+ cw.log_mins);
	w3_set_value(path, cw.log_mins);

   kiwi_clearInterval(cw.log_interval);
   if (cw.log_mins != 0) {
      console.log('CW logging..');
      cw.log_interval = setInterval(function() { cw_log_cb(); }, cw.log_mins * 60000);
   }
}

function cw_log_cb()
{
   var ts = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode;
   var txt = new Blob([cw.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = 'CW.'+ ts +'.log.txt';
   document.body.appendChild(a);
   console.log('cw_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

function cw_decoder_wsc_cb(path, checked, first)
{
   if (first) return;
   ext_send('SET cw_wsc='+ (checked? 1:0));
}

function cw_decoder_thresh_cb(path, checked, first)
{
   if (first) return;
   ext_send('SET cw_thresh='+ (checked? 1:0));
}

function cw_decoder_threshold_cb(path, val)
{
	var threshold_dB = parseFloat(val);
	if (!threshold_dB || isNaN(threshold_dB)) return;
   console.log('cw_decoder_threshold_cb path='+ path +' val='+ val +' threshold_dB='+ threshold_dB);
	w3_num_cb(path, threshold_dB);
	cw.threshold = threshold_dB;
	graph_threshold(cw.gr, cw.threshold);
	ext_send('SET cw_threshold='+ Math.pow(10, cw.threshold/10).toFixed(0));
}

function cw_reset_cb(path, idx, first)
{
   if (first) return;
   ext_send('SET cw_reset');
	cw.pboff = -1;
	CW_decoder_environment_changed();
}

function CW_decoder_blur()
{
	ext_set_data_height();     // restore default height
	ext_send('SET cw_stop');
   kiwi_clearInterval(cw.log_interval);
}

// called to display HTML for configuration parameters in admin interface
function CW_decoder_config_html()
{
   ext_config_html(cw, 'cw', 'CW', 'CW decoder configuration');
}

function CW_decoder_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'CW decoder help') +
         '<br>Use the CWN mode with its narrow passband to maximize the signal/noise ratio. <br>' +
         'The decoder doesn\'t do very well with weak or fading signals. <br><br>' +
         'Adjust the <i>threshold</i> value so the red line in the signal level display is just under the <br>' +
         'average value of the signal peaks. <br>' +
         'The <i>word space correction</i> checkbox sets the algorithm used to determine word spacing. ' +
         '';
      confirmation_show_content(s, 610, 150);
   }
   return true;
}
