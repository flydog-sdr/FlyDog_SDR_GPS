localparam RX_CFG = 4; // Available values: 4, 3, 8, 14
`define USE_WF

// In case RX_CFG is set to 4, the generated bitstream should be named KiwiSDR.rx4.wf4.bit
// In case RX_CFG is set to 3, the generated bitstream should be named KiwiSDR.rx3.wf3.bit
// In case RX_CFG is set to 8, the generated bitstream should be named KiwiSDR.rx8.wf2.bit
// In case RX_CFG is set to 14, the generated bitstream should be named KiwiSDR.rx14.wf0.bit
// Remove `define USE_WF when RX_CFG is set to 14
