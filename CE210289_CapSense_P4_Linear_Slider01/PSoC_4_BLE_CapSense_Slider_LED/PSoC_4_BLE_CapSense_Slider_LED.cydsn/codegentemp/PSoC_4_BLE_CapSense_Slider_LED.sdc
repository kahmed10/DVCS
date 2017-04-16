# THIS FILE IS AUTOMATICALLY GENERATED
# Project: C:\Users\Sarah\Documents\PSoC Creator\CE210289_CapSense_P4_Linear_Slider01\PSoC_4_BLE_CapSense_Slider_LED\PSoC_4_BLE_CapSense_Slider_LED.cydsn\PSoC_4_BLE_CapSense_Slider_LED.cyprj
# Date: Thu, 13 Apr 2017 22:45:32 GMT
#set_units -time ns
create_clock -name {CapSense_SampleClk(FFB)} -period 21250 -waveform {0 10625} [list [get_pins {ClockBlock/ff_div_5}]]
create_clock -name {CapSense_SenseClk(FFB)} -period 21250 -waveform {0 10625} [list [get_pins {ClockBlock/ff_div_4}]]
create_clock -name {CyRouted1} -period 83.333333333333329 -waveform {0 41.6666666666667} [list [get_pins {ClockBlock/dsi_in_0}]]
create_clock -name {CyWCO} -period 30517.578125 -waveform {0 15258.7890625} [list [get_pins {ClockBlock/wco}]]
create_clock -name {CyLFCLK} -period 30517.578125 -waveform {0 15258.7890625} [list [get_pins {ClockBlock/lfclk}]]
create_clock -name {CyILO} -period 31250 -waveform {0 15625} [list [get_pins {ClockBlock/ilo}]]
create_clock -name {CyECO} -period 41.666666666666664 -waveform {0 20.8333333333333} [list [get_pins {ClockBlock/eco}]]
create_clock -name {CyIMO} -period 83.333333333333329 -waveform {0 41.6666666666667} [list [get_pins {ClockBlock/imo}]]
create_clock -name {CyHFCLK} -period 83.333333333333329 -waveform {0 41.6666666666667} [list [get_pins {ClockBlock/hfclk}]]
create_clock -name {CySYSCLK} -period 83.333333333333329 -waveform {0 41.6666666666667} [list [get_pins {ClockBlock/sysclk}]]
create_generated_clock -name {CapSense_SampleClk} -source [get_pins {ClockBlock/hfclk}] -edges {1 255 511} [list]
create_generated_clock -name {CapSense_SenseClk} -source [get_pins {ClockBlock/hfclk}] -edges {1 255 511} [list]
create_generated_clock -name {PrISM_Clock} -source [get_pins {ClockBlock/hfclk}] -edges {1 121 241} [list [get_pins {ClockBlock/udb_div_0}]]

set_false_path -from [get_pins {__ONE__/q}]

# Component constraints for C:\Users\Sarah\Documents\PSoC Creator\CE210289_CapSense_P4_Linear_Slider01\PSoC_4_BLE_CapSense_Slider_LED\PSoC_4_BLE_CapSense_Slider_LED.cydsn\TopDesign\TopDesign.cysch
# Project: C:\Users\Sarah\Documents\PSoC Creator\CE210289_CapSense_P4_Linear_Slider01\PSoC_4_BLE_CapSense_Slider_LED\PSoC_4_BLE_CapSense_Slider_LED.cydsn\PSoC_4_BLE_CapSense_Slider_LED.cyprj
# Date: Thu, 13 Apr 2017 22:45:29 GMT
