// Copyright (c) 2020 Kari Karvonen, OH1KK

var iframe = {
   ext_name: 'iframe',    // NB: must match iframe.cpp:iframe_ext.name
   first_time: true,
   
   SRC_IFRAME: 0,
   SRC_HTML: 1,
   
   CMD1: 0,
};

function iframe_main()
{
   ext_switch_to_client(iframe.ext_name, iframe.first_time, iframe_recv);     // tell server to use us (again)
   if (!iframe.first_time) iframe_controls_setup();
   iframe.first_time = false;
}

function iframe_recv(data)
{
   var firstChars = arrayBufferToStringLen(data, 3);
   
   // process data sent from server/C by ext_send_data_msg()
   if (firstChars == "DAT") {
      var ba = new Uint8Array(data, 4);
      var cmd = ba[0];

      if (cmd == iframe.CMD1) {
         // do something ...
      } else {
         console.log('iframe_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
      }
      return;
   }
   
   // process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
   var stringData = arrayBufferToString(data);
   var params = stringData.substring(4).split(" ");

   for (var i=0; i < params.length; i++) {
      var param = params[i].split("=");

      switch (param[0]) {

         case "ready":
            iframe_controls_setup();
            break;
         default:
            console.log('iframe_recv: UNKNOWN CMD '+ param[0]);
            break;
      }
   }
}

function iframe_controls_setup()
{
   iframe.src        = ext_get_cfg_param('iframe.src', 0, EXT_NO_SAVE);
   iframe.title      = ext_get_cfg_param_string('iframe.title',   '', EXT_NO_SAVE);
   iframe.helptext   = ext_get_cfg_param_string('iframe.help',    '', EXT_NO_SAVE);
   iframe.url        = ext_get_cfg_param_string('iframe.url',     '', EXT_NO_SAVE);
   iframe.html       = ext_get_cfg_param_string('iframe.html',    '', EXT_NO_SAVE);
   iframe.width      = ext_get_cfg_param('iframe.width',   0, EXT_NO_SAVE);
   iframe.height     = ext_get_cfg_param('iframe.height',  0, EXT_NO_SAVE);

   /* sanity checks */
   iframe.width = parseInt(iframe.width);
   iframe.height = parseInt(iframe.height);
   var margin = 20, top_line = 25; 
   if (iframe.width <= 0) iframe.width = 450;
   if (iframe.height <= 0) iframe.height = 450;
   if (iframe.helptext == '') iframe.helptext = '';
   if (iframe.url == '') iframe.url = '/gfx/kiwi-with-headphones.51x67.png';
   if (iframe.title == '') iframe.title = 'iframe extension';
   //console_log_fqn('iframe', 'iframe.url', 'iframe.html', 'iframe.title', 'iframe.width', 'iframe.height', 'iframe.helptext');
   
   var controls_html =
      w3_div('w3-text-white',
         w3_div('w3-medium', '<b>'+iframe.title+'</b>'),
         w3_div('id-iframe-container w3-margin-T-8 w3-relative',
            '<iframe id="id-iframe-src"' +
               ' style="width:'+ px(iframe.width) +'; height:'+ px(iframe.height) +'; border:0;">' +
            '</iframe>'
         )
      );

   ext_panel_show(controls_html, null, null);
   ext_set_controls_width_height(iframe.width + margin, iframe.height + margin + top_line);

   if (iframe.src == iframe.SRC_IFRAME) {
      w3_attribute('id-iframe-src', 'src', iframe.url);
   } else {
      w3_attribute('id-iframe-src', 'srcdoc', iframe.html);
   }
}

function iframe_blur()
{
   // remove iframe content so e.g. it closes web sockets etc.
   w3_innerHTML('id-iframe-container', '');
}

// called to display HTML for configuration parameters in admin interface
function iframe_config_html()
{
   iframe.src = ext_get_cfg_param('iframe.src', 0, EXT_SAVE);

   var s =
      w3_text('w3-text-black',
         'The iframe extension can display content from two sources: <br>' +
         '<ul>' +
            '<li>An arbitrary URL</li>' +
            '<li>The specified HTML/Javascript</li>' +
         '</ul>' +
         'Both sources are wrapped in a browser iframe for better isolation from the Kiwi user interface.'
      ) +
      '<hr>' +

      w3_inline_percent('w3-valign-start/',
         w3_divs('/w3-margin-bottom',
            w3_select('w3-label-inline', 'Source', '', 'iframe.src', iframe.src, ['URL', 'HTML'], 'iframe_src_cb'),
            w3_input_get('id-iframe-url//', 'URL', 'iframe.url', 'w3_string_set_cfg_cb', ''),
            w3_textarea_get_param('id-iframe-html//w3-input-any-change|width:100%',
               w3_div('',
                  w3_text('w3-bold w3-text-teal w3-show-block', 'HTML/Javascript'),
                  w3_text('w3-text-black', 'Press enter(return) key while positioned at end of text to submit data.')
               ),
               'iframe.html', 10, 50, 'webpage_string_cb', ''
            )
         ), 65,
         '', 5,
         w3_divs('/w3-margin-bottom',
            '&nbsp;',
            w3_input_get('', 'Title text',    'iframe.title',   'w3_string_set_cfg_cb', ''),
            w3_input_get('', 'Window width',  'iframe.width',   'w3_num_set_cfg_cb', 0),
            w3_input_get('', 'Window height', 'iframe.height',  'w3_num_set_cfg_cb', 0),
            w3_input_get('', 'Help text',     'iframe.help',    'w3_string_set_cfg_cb', ''),
            w3_div('w3-right w3-text-black', 'iframe by Kari Karvonen, OH1KK')
         )
      );

   ext_config_html(iframe, 'iframe', 'iframe', 'iframe extension configuration', s);
}

function iframe_src_cb(path, idx, first)
{
   iframe.src = +idx;
   admin_select_cb(path, iframe.src, first);
   //console.log('iframe_src_cb: src='+ iframe.src +' '+ (iframe.src != iframe.SRC_IFRAME) +' '+ (iframe.src != iframe.SRC_HTML));
   w3_disable('id-iframe-url', iframe.src != iframe.SRC_IFRAME);
   //w3_set_props('id-iframe-url', 'w3-disabled w3-pointer-events-none', iframe.src != iframe.SRC_IFRAME);
   w3_disable('id-iframe-html', iframe.src != iframe.SRC_HTML);
   //w3_set_props('id-iframe-html', 'w3-disabled w3-pointer-events-none', iframe.src != iframe.SRC_HTML);
}

function iframe_help(show)
{
   if (show) {
      var s =  w3_text('w3-medium w3-bold w3-text-aqua', iframe.title +' help') + '<br>'+ iframe.helptext+ '';
      confirmation_show_content(s, 610, 125);
   }
   return true;
}
