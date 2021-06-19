// this file auto-generated by the e_cpu assembler -- edits will be overwritten

`ifndef _GEN_kiwi_VH_
`define _GEN_kiwi_VH_

// from assembler DEF directives:

`define USE_SDR    // DEFh 0x1
	parameter GPS_CHANS = 12;    // DEFp 0xc
`define DEF_GPS_CHANS
`define USE_GPS    // DEFh 0x1
`define ARTIX_7A35    // DEFh 0x1
//`define ZYNQ_7007    // DEFh 0x0
	parameter FPGA_VER = 4'd1;    // DEFp 0x1
`define DEF_FPGA_VER
	parameter FW_ID = 20480;    // DEFp 0x5000
`define DEF_FW_ID
	parameter ADC_BITS = 16;    // DEFp 0x10
`define DEF_ADC_BITS
	parameter DEFAULT_NSYNC = 2;    // DEFp 0x2
`define DEF_DEFAULT_NSYNC
`define USE_GEN    // DEFh 0x1
`define USE_LOGGER    // DEFh 0x1
`define USE_CPU_CTR    // DEFh 0x1
`define USE_DEBUG    // DEFh 0x1
//`define USE_RX_SEQ    // DEFh 0x0
`define USE_VIVADO    // DEFh 0x1
`define SERIES_7    // DEFh 0x1
//`define SPI_PUMP_CHECK    // DEFh 0x0
//`define STACK_CHECK    // DEFh 0x0
//`define SND_SEQ_CHECK    // DEFh 0x0
//`define SND_TIMING_CK    // DEFh 0x0
	parameter FPGA_ID_RX4_WF4 = 4'd0;    // DEFp 0x0
//`define DEF_FPGA_ID_RX4_WF4
	parameter FPGA_ID_RX8_WF2 = 4'd1;    // DEFp 0x1
`define DEF_FPGA_ID_RX8_WF2
	parameter FPGA_ID_RX3_WF3 = 4'd2;    // DEFp 0x2
`define DEF_FPGA_ID_RX3_WF3
	parameter FPGA_ID_RX14_WF0 = 4'd3;    // DEFp 0x3
`define DEF_FPGA_ID_RX14_WF0
	parameter FPGA_ID_OTHER = 4'd4;    // DEFp 0x4
`define DEF_FPGA_ID_OTHER
	parameter NUM_CMDS_BASE = 13;    // DEFp 0xd
`define DEF_NUM_CMDS_BASE
	parameter NUM_CMDS_SDR = 12;    // DEFp 0xc
`define DEF_NUM_CMDS_SDR
	parameter NUM_CMDS_GPS = 16;    // DEFp 0x10
`define DEF_NUM_CMDS_GPS
	parameter NUM_CMDS_OTHER = 0;    // DEFp 0x0
//`define DEF_NUM_CMDS_OTHER
	parameter NUM_CMDS = 41;    // DEFp 0x29
`define DEF_NUM_CMDS
`define SPI_32    // DEFh 0x1
	parameter SPIBUF_W = 2048;    // DEFp 0x800
`define DEF_SPIBUF_W
	parameter SPIBUF_B = 4096;    // DEFp 0x1000
`define DEF_SPIBUF_B
	parameter SPIBUF_BMAX = 4094;    // DEFp 0xffe
`define DEF_SPIBUF_BMAX
	parameter RX1_WIDE_DECIM = 1250;    // DEFp 0x4e2
`define DEF_RX1_WIDE_DECIM
	parameter RX2_WIDE_DECIM = 5;    // DEFp 0x5
`define DEF_RX2_WIDE_DECIM
	parameter RX1_STD_DECIM = 1488;    // DEFp 0x5d0
`define DEF_RX1_STD_DECIM
	parameter RX2_STD_DECIM = 7;    // DEFp 0x7
`define DEF_RX2_STD_DECIM
	parameter MAX_SND_RATE = 20250;    // DEFp 0x4f1a
`define DEF_MAX_SND_RATE
	parameter SND_RATE_3CH = 20250;    // DEFp 0x4f1a
`define DEF_SND_RATE_3CH
	parameter SND_RATE_4CH = 12000;    // DEFp 0x2ee0
`define DEF_SND_RATE_4CH
	parameter SND_RATE_8CH = 12000;    // DEFp 0x2ee0
`define DEF_SND_RATE_8CH
	parameter SND_RATE_14CH = 12000;    // DEFp 0x2ee0
`define DEF_SND_RATE_14CH
	parameter RX_DECIM_3CH = 6250;    // DEFp 0x186a
`define DEF_RX_DECIM_3CH
	parameter RX_DECIM_4CH = 10416;    // DEFp 0x28b0
`define DEF_RX_DECIM_4CH
	parameter RX_DECIM_8CH = 10416;    // DEFp 0x28b0
`define DEF_RX_DECIM_8CH
	parameter RX_DECIM_14CH = 10416;    // DEFp 0x28b0
`define DEF_RX_DECIM_14CH
	parameter RXBUF_SIZE_3CH = 16384;    // DEFp 0x4000
`define DEF_RXBUF_SIZE_3CH
	parameter RXBUF_SIZE_4CH = 8192;    // DEFp 0x2000
`define DEF_RXBUF_SIZE_4CH
	parameter RXBUF_SIZE_8CH = 16384;    // DEFp 0x4000
`define DEF_RXBUF_SIZE_8CH
	parameter RXBUF_SIZE_14CH = 32768;    // DEFp 0x8000
`define DEF_RXBUF_SIZE_14CH
	parameter NRX_IQW = 3;    // DEFp 0x3
`define DEF_NRX_IQW
	parameter NRX_SPI = 2047;    // DEFp 0x7ff
`define DEF_NRX_SPI
	parameter NRX_OVHD = 5;    // DEFp 0x5
`define DEF_NRX_OVHD
	parameter NRX_SAMPS_RPT = 8;    // DEFp 0x8
`define DEF_NRX_SAMPS_RPT
//`define USE_RX_CIC24    // DEFh 0x0
	parameter RX1_BITS = 22;    // DEFp 0x16
`define DEF_RX1_BITS
	parameter RX2_BITS = 18;    // DEFp 0x12
`define DEF_RX2_BITS
	parameter RXO_BITS = 24;    // DEFp 0x18
`define DEF_RXO_BITS
	parameter RX1_STAGES = 3;    // DEFp 0x3
`define DEF_RX1_STAGES
	parameter RX2_STAGES = 5;    // DEFp 0x5
`define DEF_RX2_STAGES
	parameter MAX_ZOOM = 14;    // DEFp 0xe
`define DEF_MAX_ZOOM
	parameter NWF_FFT = 8192;    // DEFp 0x2000
`define DEF_NWF_FFT
	parameter NWF_IQW = 2;    // DEFp 0x2
`define DEF_NWF_IQW
	parameter NWF_NXFER = 9;    // DEFp 0x9
`define DEF_NWF_NXFER
	parameter NWF_SAMPS = 911;    // DEFp 0x38f
`define DEF_NWF_SAMPS
	parameter NWF_SAMPS_RPT = 50;    // DEFp 0x32
`define DEF_NWF_SAMPS_RPT
	parameter NWF_SAMPS_LOOP = 18;    // DEFp 0x12
`define DEF_NWF_SAMPS_LOOP
	parameter NWF_SAMPS_LOOP2 = 900;    // DEFp 0x384
`define DEF_NWF_SAMPS_LOOP2
	parameter NWF_SAMPS_REM = 11;    // DEFp 0xb
`define DEF_NWF_SAMPS_REM
`define USE_WF_1CIC    // DEFh 0x1
`define USE_WF_CIC24    // DEFh 0x1
//`define USE_WF_MEM24    // DEFh 0x0
//`define USE_WF_NEW    // DEFh 0x0
	parameter WF1_STAGES = 5;    // DEFp 0x5
`define DEF_WF1_STAGES
	parameter WF2_STAGES = 5;    // DEFp 0x5
`define DEF_WF2_STAGES
	parameter WF1_BITS = 24;    // DEFp 0x18
`define DEF_WF1_BITS
	parameter WF2_BITS = 24;    // DEFp 0x18
`define DEF_WF2_BITS
	parameter WFO_BITS = 16;    // DEFp 0x10
`define DEF_WFO_BITS
	parameter WF_1CIC_MAXD = 8192;    // DEFp 0x2000
`define DEF_WF_1CIC_MAXD
	parameter WF_2CIC_MAXD = 0;    // DEFp 0x0
//`define DEF_WF_2CIC_MAXD
	parameter MAX_GPS_CHANS = 12;    // DEFp 0xc
`define DEF_MAX_GPS_CHANS
	parameter GPS_INTEG_BITS = 20;    // DEFp 0x14
`define DEF_GPS_INTEG_BITS
	parameter GPS_REPL_BITS = 18;    // DEFp 0x12
`define DEF_GPS_REPL_BITS
	parameter MAX_NAV_BITS = 128;    // DEFp 0x80
`define DEF_MAX_NAV_BITS
	parameter GPS_RPT = 32;    // DEFp 0x20
`define DEF_GPS_RPT
	parameter GPS_SAMPS = 256;    // DEFp 0x100
`define DEF_GPS_SAMPS
	parameter GPS_SAMPS_RPT = 32;    // DEFp 0x20
`define DEF_GPS_SAMPS_RPT
	parameter GPS_SAMPS_LOOP = 8;    // DEFp 0x8
`define DEF_GPS_SAMPS_LOOP
	parameter GPS_IQ_SAMPS = 255;    // DEFp 0xff
`define DEF_GPS_IQ_SAMPS
	parameter GPS_IQ_SAMPS_W = 1020;    // DEFp 0x3fc
`define DEF_GPS_IQ_SAMPS_W
	parameter L1_CODEBITS = 10;    // DEFp 0xa
`define DEF_L1_CODEBITS
	parameter L1_CODELEN = 1023;    // DEFp 0x3ff
`define DEF_L1_CODELEN
	parameter E1B_MODE = 2048;    // DEFp 0x800
`define DEF_E1B_MODE
	parameter E1B_CODEBITS = 12;    // DEFp 0xc
`define DEF_E1B_CODEBITS
	parameter E1B_CODELEN = 4092;    // DEFp 0xffc
`define DEF_E1B_CODELEN
	parameter E1B_CODE_XFERS = 2;    // DEFp 0x2
`define DEF_E1B_CODE_XFERS
	parameter E1B_CODE_LOOP = 2046;    // DEFp 0x7fe
`define DEF_E1B_CODE_LOOP
	parameter E1B_CODE_RPT = 32;    // DEFp 0x20
`define DEF_E1B_CODE_RPT
	parameter E1B_CODE_LOOP2 = 63;    // DEFp 0x3f
`define DEF_E1B_CODE_LOOP2
	parameter E1B_CODE_LOOP3 = 2016;    // DEFp 0x7e0
`define DEF_E1B_CODE_LOOP3
	parameter E1B_CODE_REM = 30;    // DEFp 0x1e
`define DEF_E1B_CODE_REM
	parameter GET_CHAN_IQ = 0;    // DEFb: bit number for value: 0x1
	parameter GET_SRQ = 1;    // DEFb: bit number for value: 0x2
	parameter GET_SNAPSHOT = 2;    // DEFb: bit number for value: 0x4
	parameter HOST_RX = 3;    // DEFb: bit number for value: 0x8
	parameter GET_RX_SRQ = 4;    // DEFb: bit number for value: 0x10
	parameter GET_CPU_CTR0 = 5;    // DEFb: bit number for value: 0x20
	parameter GET_CPU_CTR1 = 6;    // DEFb: bit number for value: 0x40
	parameter GET_CPU_CTR2 = 7;    // DEFb: bit number for value: 0x80
	parameter GET_CPU_CTR3 = 8;    // DEFb: bit number for value: 0x100
	parameter GET_STATUS = 9;    // DEFb: bit number for value: 0x200
	parameter HOST_TX = 0;    // DEFb: bit number for value: 0x1
	parameter SET_MASK = 1;    // DEFb: bit number for value: 0x2
	parameter SET_CHAN = 2;    // DEFb: bit number for value: 0x4
	parameter SET_CG_NCO = 3;    // DEFb: bit number for value: 0x8
	parameter SET_LO_NCO = 4;    // DEFb: bit number for value: 0x10
	parameter SET_SAT = 5;    // DEFb: bit number for value: 0x20
	parameter SET_E1B_CODE = 6;    // DEFb: bit number for value: 0x40
	parameter SET_PAUSE = 7;    // DEFb: bit number for value: 0x80
	parameter SET_CTRL = 10;    // DEFb: bit number for value: 0x400
	parameter SET_RX_CHAN = 0;    // DEFb: bit number for value: 0x1
	parameter SET_RX_FREQ = 1;    // DEFb: bit number for value: 0x2
	parameter FREQ_L = 2;    // DEFb: bit number for value: 0x4
	parameter SET_RX_NSAMPS = 3;    // DEFb: bit number for value: 0x8
	parameter SET_GEN_FREQ = 4;    // DEFb: bit number for value: 0x10
	parameter SET_GEN_ATTN = 5;    // DEFb: bit number for value: 0x20
	parameter SET_WF_CHAN = 6;    // DEFb: bit number for value: 0x40
	parameter SET_WF_FREQ = 7;    // DEFb: bit number for value: 0x80
	parameter SET_WF_DECIM = 8;    // DEFb: bit number for value: 0x100
	parameter WF_SAMPLER_RST = 9;    // DEFb: bit number for value: 0x200
	parameter SET_CNT_MASK = 10;    // DEFb: bit number for value: 0x400
	parameter HOST_RST = 0;    // DEFb: bit number for value: 0x1
	parameter HOST_RDY = 1;    // DEFb: bit number for value: 0x2
	parameter GET_MEMORY = 2;    // DEFb: bit number for value: 0x4
	parameter GPS_SAMPLER_RST = 3;    // DEFb: bit number for value: 0x8
	parameter GET_GPS_SAMPLES = 4;    // DEFb: bit number for value: 0x10
	parameter GET_LOG = 5;    // DEFb: bit number for value: 0x20
	parameter PUT_LOG = 6;    // DEFb: bit number for value: 0x40
	parameter LOG_RST = 7;    // DEFb: bit number for value: 0x80
	parameter GET_RX_SAMP = 0;    // DEFb: bit number for value: 0x1
	parameter RX_BUFFER_RST = 1;    // DEFb: bit number for value: 0x2
	parameter RX_GET_BUF_CTR = 2;    // DEFb: bit number for value: 0x4
	parameter SET_WF_CONTIN = 3;    // DEFb: bit number for value: 0x8
	parameter GET_WF_SAMP_I = 4;    // DEFb: bit number for value: 0x10
	parameter GET_WF_SAMP_Q = 5;    // DEFb: bit number for value: 0x20
	parameter CLR_RX_OVFL = 6;    // DEFb: bit number for value: 0x40
	parameter FREEZE_TOS = 7;    // DEFb: bit number for value: 0x80
	parameter CPU_CTR_CLR = 8;    // DEFb: bit number for value: 0x100
	parameter CPU_CTR_ENA = 9;    // DEFb: bit number for value: 0x200
	parameter CPU_CTR_DIS = 10;    // DEFb: bit number for value: 0x400
	parameter WF_SAMP_RD_RST = 0;    // DEFb: bit number for value: 0x1
	parameter WF_SAMP_WR_RST = 1;    // DEFb: bit number for value: 0x2
	parameter WF_SAMP_CONTIN = 2;    // DEFb: bit number for value: 0x4
	parameter WF_SAMP_SYNC = 3;    // DEFb: bit number for value: 0x8
	parameter STAT_FPGA_ID = 15;    // DEFp 0xf
`define DEF_STAT_FPGA_ID
	parameter STAT_USER = 240;    // DEFp 0xf0
`define DEF_STAT_USER
	parameter STAT_DNA_DATA = 4;    // DEFb: bit number for value: 0x10
	parameter STAT_FPGA_VER = 3840;    // DEFp 0xf00
`define DEF_STAT_FPGA_VER
	parameter STAT_FW_ID = 28672;    // DEFp 0x7000
`define DEF_STAT_FW_ID
	parameter STAT_OVFL = 15;    // DEFb: bit number for value: 0x8000
	parameter CTRL_OSC_EN = 8;    // DEFb: bit number for value: 0x100
	parameter CTRL_EEPROM_WP = 9;    // DEFb: bit number for value: 0x200
	parameter CTRL_UNUSED_OUT = 9;    // DEFb: bit number for value: 0x200
	parameter CTRL_USE_GEN = 10;    // DEFb: bit number for value: 0x400
	parameter CTRL_CMD_READY = 11;    // DEFb: bit number for value: 0x800
	parameter CTRL_SND_INTR = 12;    // DEFb: bit number for value: 0x1000
	parameter CTRL_DNA_READ = 13;    // DEFb: bit number for value: 0x2000
	parameter CTRL_DNA_SHIFT = 14;    // DEFb: bit number for value: 0x4000
	parameter CTRL_DNA_CLK = 15;    // DEFb: bit number for value: 0x8000

`endif
