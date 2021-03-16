// Copyright (c) 2016 John Seamons, ZL/KF6VO

var devl = {
   ext_name: 'devl',    // NB: must match iq.c:iq_display_ext.name
   first_time: true,
};

function devl_main()
{
	ext_switch_to_client(devl.ext_name, devl.first_time, devl_recv);		// tell server to use us (again)
	if (!devl.first_time)
		devl_controls_setup();
	devl.first_time = false;
}

function devl_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('devl_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('devl_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('devl_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				devl_controls_setup();
				break;

			default:
				console.log('devl_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function devl_num_cb(path, val)
{
   var v;
   if (val.startsWith('0x'))
      v = parseInt(val);
   else
      v = parseFloat(val);
	if (val == '') v = 0;
	if (isNaN(v)) {
	   w3_set_value(path, 0);
	   v = 0;
	}
	console.log('devl_num_cb: path='+ path +' val='+ val +' v='+ v);
	v = v.toFixed(6);
	setVarFromString(path, v);
	ext_send('SET '+ path +'='+ v);
}

function devl_controls_setup()
{
	var controls_html =
		w3_div('id-devl-controls w3-text-white',
			w3_divs('/w3-tspace-8',
				w3_div('w3-medium w3-text-aqua', '<b>Development controls</b>'),
            w3_divs('/w3-tspace-8',
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p0', 'devl.p0', devl.p0, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p1', 'devl.p1', devl.p1, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p2', 'devl.p2', devl.p2, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p3', 'devl.p3', devl.p3, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p4', 'devl.p4', devl.p4, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p5', 'devl.p5', devl.p5, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p6', 'devl.p6', devl.p6, 'devl_num_cb'),
               w3_input('w3-label-inline w3-label-not-bold//w3-padding-small||size=10', 'p7', 'devl.p7', devl.p7, 'devl_num_cb')
            )
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(null, 350);
}

/*
// called to display HTML for configuration parameters in admin interface
function devl_display_config_html()
{
   ext_config_html(devl, 'devl', 'devl', 'development controls configuration');
}
*/
