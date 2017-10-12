# This repository provides the ns-3.22 simulation codes for casacding DoS attacks in Wi-Fi networks.

Direction:

1. Copy the files yans-wifi-phy.cc and yans-wifi-phy.h under the ns-3.22 direction src/wifi/model/
  In those two files, a new trace, the duration of each packet, is added.

2. Copy the files athstats-helper.cc and athstats-helper.h under the ns-3.22 direction src/wifi/helper/
  athstats-helper captures the trace of the duration of the packets and culumate the total time that a node is transmitting packets.
  
3. Copy the file CDoS-1Mbps-adhoc-UDP.cc under the ns-3.22 direction scratch/
