// Copyright (c) 2017-2019 John Seamons, ZL4VO/KF6VO

var dcf77 = {
   arm: 0,
   NOISE_THRESHOLD: 3,
   cur: 0,
   sec: 0,
   msec: 0,
   dcnt: 0,
   tick: 0,
   line: 0,
   
   end: null
};

function dcf77_dmsg(s)
{
   w3_innerHTML('id-tc-dcf77', s);
}

// see: en.wikipedia.org/wiki/DCF77
function dcf77_legend()
{
   if ((dcf77.line++ & 3) == 0)
      tc_dmsg('0 wwwwwwwwwwwwww EASNL 1 mmmmmmm p hhhhhh p dddddd www mmmmm yyyyyyyy p 0<br>');
}

function dcf77_decode(bits)
{
   // bits are what the minute _will be_ at the approaching minute boundary
   
   var min  = tc_bcd(bits, 21, 7, 1);
   var hour = tc_bcd(bits, 29, 6, 1);
   var day  = tc_bcd(bits, 36, 6, 1);
   var wday = tc_bcd(bits, 42, 3, 1);
   var mo   = tc_bcd(bits, 45, 5, 1) - 1;
   var yr   = tc_bcd(bits, 50, 8, 1) + 2000;
   var tz   = bits[17]? 'CEST' : (bits[18]? 'CET' : 'TZ?');

   var s = day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' '+ tz;
   tc_dmsg('  '+ s +'<br>');
   tc_stat('lime', 'Time decoded: '+ s);
}

function dcf77_clr()
{
   var d = dcf77;
   
   d.cnt = d.cur = d.one_width = d.zero_width = 0;
   d.arm = d.no_modulation = d.dcnt = d.modct = d.line = d.sec = d.msec = 0;
}

// called at 100 Hz (10 msec)
function dcf77_ampl(ampl)
{
	var i;
	var d = dcf77;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.5)? 1:0;
	if (!tc.ref) { tc.data = ampl; tc.ref = 1; }
	
	// de-noise signal
   if (ampl == d.cur) {
   	d.cnt = 0;
   } else {
   	d.cnt++;
   	if (d.cnt > d.NOISE_THRESHOLD) {
   		d.cur = ampl;
   		d.cnt = 0;
   		//if (tc.state == tc.ACQ_SYNC)
   		//   tc_dmsg((tc.data? '1':'0') +':'+ (tc.data? d.one_width : d.zero_width) +' ');
   		//if (tc.state == tc.ACQ_SYNC && !tc.data)
   		//   tc_dmsg(d.zero_width +' ');
   		tc.data ^= 1;
   		if (tc.data) d.one_width = 0; else d.zero_width = 0;
   	}
   }

   if (tc.data) d.one_width++; else d.zero_width++;
	
	if (d.one_width == 170) {
	   d.arm = 1;
	}
	
	if (d.arm && tc.data == 0) {
	   tc.trig = 0;
      tc.sample_point = 25;
      d.sec = 0;
      d.msec = 0;
      if (tc.state == tc.ACQ_SYNC) {
         tc_stat('cyan', 'Found sync');
      } else {
         dcf77_decode(tc.raw);    // for the minute just reached
      }
      dcf77_legend();
      tc.raw = [];
	   d.arm = 0;
      tc.state = tc.ACQ_DATA;
	}
	
   d.msec += 10;

   if (d.msec == 1000) {
      d.sec++;
      d.msec = 0;
   }

	if (tc.state == tc.ACQ_DATA && tc.sample_point == tc.trig) {
	   var b = (d.zero_width > 15)? 1:0;
      tc.raw[d.sec] = b;
      tc_dmsg(b);
      if ([0,14,19,20,27,28,34,35,41,44,49,57,58].includes(d.dcnt)) tc_dmsg(' ');
      //tc_dmsg(d.zero_width +' ');
      d.dcnt++;
      if (d.dcnt == 60) d.dcnt = 0;
   }
}

function dcf77_focus()
{
}


function dcf77_blur()
{
   var el;
	el = w3_el('id-tc-bits');
	if (el) el.innerHTML = '';
	el = w3_el('id-tc-dcf77');
	if (el) el.innerHTML = '';
}


function dcf77_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-bits') + w3_div('id-tc-dcf77');
}
