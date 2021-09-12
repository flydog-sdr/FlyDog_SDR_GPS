// Copyright (c) 2017 Peter Jennings, VE3SUN

var ibp = {
   scan_ext_name: 'IBP_scan',    // NB: must match IBP_scan.c:ibp_scan_ext.name
   first_time: true,

   SLOTS: 18,     // number of slots/stations
   ALL: 20,
   BANDS: 30,

   run: false,
   annotate: true,
   autosave: false,
   lineCanvas: 0,
   mindb_band: [],
   canvasSaved: false,
   oldSlot: -1,
   monitorBeacon: -1,
   sound: false,
   band: 0,
   bands_s: [ "IBP 20m", "IBP 17m", "IBP 15m", "IBP 12m", "IBP 10m" ],
   freqs: [ '14.100', '18.110', '21.150', '24.930', '28.200' ]
};

function IBP_scan_main()
{
   //console.log('IBP_scan_main');
   ext_switch_to_client(ibp.scan_ext_name, ibp.first_time, ibp_recv_msg);  // tell server to use us (again)
   if (!ibp.first_time)
      ibp_controls_setup();
   ibp.first_time = false;
}

function ibp_recv_msg(data)
{
   // process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
   var stringData = arrayBufferToString(data);
   var params = stringData.substring(4).split(" ");

   for (var i=0; i < params.length; i++) {
      var param = params[i].split("=");

      switch (param[0]) {

         case "ready":
            ibp_controls_setup();
            break;

         default:
            console.log('ibp_recv: UNKNOWN CMD '+ param[0]);
            break;
      }
   }
}

function ibp_controls_setup()
{
   var i;

   var data_html =
      time_display_html('IBP_scan') +
      w3_div('id-IBP-report|width:1024px; height:200px; overflow:hidden; position:relative; background-color:white;',
         '<canvas id="id-IBP-canvas" width="1024" height="200" style="position:absolute"></canvas>'
      );
   
   var select = { 'IBP': { value: -2, disabled: 1, selected: 1 }, 'OFF': { value: -1 } };
   w3_obj_enum(dx_ibp_stations, function(key, i, o) {
      select[key] = { value: o.value, text: o.text };
   });
   select['By band:'] = { value: -1, disabled: 1, text: '<b>By band:</b>' };
   select['All Bands'] = { value: ibp.ALL, text: 'All Bands' };
   ibp.bands_s.forEach(function(s, i) {
      select[s] = { value: ibp.BANDS+i, text: s };
   });

   var controls_html =
      w3_div('id-tc-controls w3-text-white',
         w3_div('w3-medium w3-text-aqua',
            '<b><a href="http://www.ncdxf.org/beacon/index.html">International Beacon Project</a> (IBP) Scanner</b>'
         ),

         w3_col_percent('w3-margin-T-4',
            w3_div('', 'by VE3SUN'), 25,
            w3_div('', 'Info: <b><a href="http://ve3sun.com/KiwiSDR/IBP.html" target="_blank">ve3sun.com/KiwiSDR/IBP</a></b>'), 55,
            '', 10
         ),
         
         w3_inline('w3-halign-space-between w3-margin-T-8|width:90%;/',
            w3_select('id-IBP-menu w3-left w3-margin-right w3-show-inline', '', '', '', 0, select, 'IBP_set'),
            w3_checkbox('w3-label-inline w3-label-not-bold', 'Annotate Waterfall', 'ibp.annotate', true, 'w3_bool_cb'),
            w3_checkbox('w3-label-inline w3-label-not-bold', 'Autosave PNG', 'ibp.autosave', false, 'IBP_Autosave')
         )
      );
   
   //console.log('ibp_controls_setup');
   ext_panel_show(controls_html, data_html, null);
   ext_set_controls_width_height(475, 90);
   time_display_setup('IBP_scan');
	IBP_environment_changed( {resize:1} );
   
   // use extension parameter as beacon station call (or 'cycle' for cycle mode)
   // e.g. kiwisdr.local:8073/?ext=ibp,4u1un (upper or lowercase)
   var call = ext_param();
   if (call)  {
      call = call.toLowerCase();
      var idx = -1;
      w3_obj_enum(dx_ibp_stations, function(key, i, o) {
         if (key.toLowerCase() == call)
            idx = i;
      });
      var cycle;
      if (idx != -1 && ((cycle = (idx >= ibp.SLOTS && call == 'cycle')) || idx < ibp.SLOTS)) {
         if (cycle) idx = ibp.ALL;
         //console.log('IBP: URL set '+ call);
         //console.log('IBP URL_param='+ idx);
         IBP_set('', idx);
      }
   }

   ibp.autosave = readCookie('IBP_PNG_Autosave');
   if (ibp.autosave != 'true') ibp.autosave = false;
   w3_checkbox_set('id-IBP-Autosave', ibp.autosave);
   
   var c = document.createElement('canvas');
   c.width = 16; c.height = 1;
   var ctx = c.getContext('2d');
   ctx.fillStyle = "white";
   ctx.fillRect(0, 0, 8, 1);
   ctx.fillStyle = "black";
   ctx.fillRect(8, 0, 8, 1);
   ibp.lineCanvas = c;
   
   var canv = w3_el('id-IBP-canvas');
   var label = '';
   if (canv) {
      var ctx = canv.getContext("2d");
      ctx.fillStyle="#ffffff";
      ctx.fillRect(0,0,1024,200);
      
      ctx.font = "12px Arial";
      ctx.fillStyle = "red";
      ctx.textAlign = "center";
      
      w3_obj_enum(dx_ibp_stations, function(key, i, o) {
         ctx.fillText(key, 102 + o.value*51, 16); 
      });

      for (i=0; i < 5; i++) {
         label = ibp.freqs[i];
         ctx.fillText(label, 45, 36*i+40); 
      }
   }

   var cookie = readCookie('mindb_band');
   if (cookie) {
      var obj = kiwi_JSON_parse('ibp_controls_setup', cookie);
      if (obj) ibp.mindb_band = obj;
   }

   ibp.run = true;
}

function IBP_environment_changed(changed)
{
   if (!changed.resize) return;
   var el = w3_el('id-IBP-report');
   var left = (window.innerWidth - 1024 - time_display_width()) / 2;
   el.style.left = px(left);
}

function IBP_scan_blur()
{
   //console.log('IBP_scan_blur');
   IBP_set('', -1);
   ibp.run = false;
}

function IBP_Autosave(path, checked)
{
   ibp.autosave = checked? true:false;
   writeCookie('IBP_PNG_Autosave', ibp.autosave);
}

// If menu has ever been selected then we restore band to 20m on blur,
// else leave alone so e.g. zoom won't change.

function IBP_set(path, v, first)    // called by IBP selector with beacon value
{
   if (first) return;
   v = +v;
   //console.log('IBP_set v='+ v);
   w3_el('id-IBP-menu').value = v;     // for benefit of direct callers
   var selected = (v >= 0);
   ibp.band = 0;
   ibp.monitorBeacon = v;

   if (v >= ibp.BANDS) {
      ibp.band = v-ibp.BANDS;
      //console.log('IBP_set MENU band='+ ibp.band);
   } else

   if (v < 0) {    // menu = "off"
      w3_el('id-IBP-menu').selectedIndex = 0;
      if (selected) {
         ibp.band = 0;
         //console.log('IBP_set MENU=off ibp.band=0');
      }
   }

   select_band(ibp.bands_s[ibp.band]);
   //console.log('IBP_set ibp.band='+ ibp.band +' ibp.monitorBeacon='+ ibp.monitorBeacon);
}

   
function IBP_bandchange(d, BeaconN)
{
   if (ibp.monitorBeacon == ibp.ALL) {
      var band = Math.floor(d.getTime() / 180000) % 5; // 3 min per band each 15 min

      if (band != ibp.band) {
         ibp.band = band;
         select_band(ibp.bands_s[ibp.band]);
         return ibp.band;
      }
      return false;
   }

   if (ibp.monitorBeacon != (BeaconN + ibp.SLOTS-1) % ibp.SLOTS) return false;
   
   ibp.band++;
   ibp.band %= 5;
   select_band(ibp.bands_s[ibp.band]);
   return ibp.band;
}

function ibp_save_Canvas(d)
{
   var canv = w3_el('id-IBP-canvas');
   if (canv) {
      var ctx = canv.getContext("2d");
      ctx.fillStyle = "black";
      ctx.fillText(d.getUTCHours().leadingZeros() +':'+ d.getUTCMinutes().leadingZeros() +' UTC', 40, 16); 

      var imgURL = canv.toDataURL("image/png");
      var dlLink = document.createElement('a');
      dlLink.download = 'IBP '+ d.getUTCFullYear() + (d.getUTCMonth()+1).leadingZeros() + d.getUTCDate().leadingZeros() +' '+
         d.getUTCHours().leadingZeros() +'h'+ d.getUTCMinutes().leadingZeros() +'Z.png';
      dlLink.href = imgURL;
      dlLink.dataset.downloadurl = ["image/png", dlLink.download, dlLink.href].join(':');
   
      document.body.appendChild(dlLink);
      dlLink.click();
      document.body.removeChild(dlLink);
      ctx.fillStyle = "#ffffff";
      ctx.fillRect(0,0,75,19);
   }
}

function ibp_annotate_waterfall(beaconN)
{
   var c = wf_cur_canvas;
   c.ctx.strokeStyle="red";
   var al = wf_canvas_actual_line+1;
   if (al > c.height) al -= 2; // fixes the 1 in 200 lines that go missing - oops, doesn't FIXME - try setting not done and returning
   c.ctx.moveTo(0, al); 
   c.ctx.lineTo(c.width, al);  
   
   c.ctx.rect(0, al, c.width, 1);
   var pattern = c.ctx.createPattern(ibp.lineCanvas, 'repeat');
   c.ctx.fillStyle = pattern;
   c.ctx.fill();
   
   var call, location;
   w3_obj_enum(dx_ibp_stations, function(key, i, o) {
      if (i == beaconN) { call = key; location = o.text; }
   });

   var sL = call +' '+ location;

   al = wf_canvas_actual_line;
   var x = (c.width - c.ctx.measureText(sL).width)/2;
   var yoff = 14;
	w3_fillText_shadow(c, sL, x, al+yoff, 'Arial', 13, 'lime', 4);
   
   if (wf_canvas_actual_line+10 > c.height) {   // overlaps end of canvas
      var c2 = wf_canvases[1];
      if (c2) w3_fillText_shadow(c2, sL, x, al-c.height+yoff, 'Arial', 13, 'lime', 4);
   }
}

// called every waterfall update
function IBP_scan_plot(oneline_image)
{
   if (!ibp.run) return;
   var canv = w3_el('id-IBP-canvas');
   if (!canv) return;

   var ctx = canv.getContext("2d");
   var subset = new ImageData(1,1);
   
   var d = new Date(dx_ibp_server_time_ms + (Date.now() - dx_ibp_local_time_epoch_ms));
   var msec = d.getTime();
   var bsec = Math.floor((msec % 10000) / 200);    // for 50 pixel slot image
   var slot = Math.floor(msec/10000) % ibp.SLOTS;
      
   var f = get_visible_freq_range();
   var fb = Math.floor((f.center - 14e6) / 3e6);
   var plot_y = 20 + 36*fb;
   
   var beaconN = (slot - fb + ibp.SLOTS) % ibp.SLOTS;  // actual beacon transmitting
   var plot_x = 76 + 51 * beaconN;
   //console.log('IBP slot='+ slot +' x='+ plot_x +' y='+ plot_y);
   
   if (ibp.autosave) {
      if (slot) {
         ibp.canvasSaved = false;
      } else {
         if (!ibp.canvasSaved) {
            ctx.fillStyle="red";
            ctx.fillRect(plot_x-1,plot_y,1,35);  // mark save time on canvas

            ibp_save_Canvas(d);
            ibp.canvasSaved = true;

            ctx.fillStyle="#ffffff";
            ctx.fillRect(plot_x-1,plot_y,1,35);  // unmark it
         }
      }     
   }

   if ((ibp.oldSlot > -1) && (ibp.oldSlot != slot)) {
      if (ibp.annotate)
         ibp_annotate_waterfall((beaconN + ibp.SLOTS-1) % ibp.SLOTS); 
      
      var new_band = IBP_bandchange(d, beaconN);
      if (new_band !== false) {  // returns new band if band changed, else false
         if (ibp.mindb_band[new_band] && (mindb != ibp.mindb_band[new_band]))
            setmindb(true,ibp.mindb_band[new_band]);
         ibp.oldSlot = -2;
         return;
      }
   }

   if (ibp.oldSlot != slot) {
      if (kiwi.muted && (ibp.monitorBeacon == beaconN)) {
         toggle_or_set_mute();
         setTimeout(function() { toggle_or_set_mute()}, 50000);
      }
      ctx.fillStyle = "#000055";
      ctx.fillRect(plot_x,plot_y,50,35);
   } else {
      if (ibp.mindb_band[fb] != mindb) { 
         ibp.mindb_band[fb] = mindb;
         writeCookie('mindb_band', JSON.stringify(ibp.mindb_band));
      }
   }

   for (var i = 495; i < 530; i++) {
      for (var j = 0; j < 4; j++) {
        subset.data[j] = oneline_image.data[4*i+j];
        ctx.putImageData(subset, plot_x+bsec, plot_y+i-495);
     }
   }

   ibp.oldSlot = slot;
}
