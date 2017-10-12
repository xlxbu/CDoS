# This repository provides the ns-3.22 simulation codes for casacding DoS attacks in Wi-Fi networks.

In this simulation, we demonstrate the feasibility of cascading DoS attacks in Wi-Fi networks in ns-3 simulator. We build up an ad hoc network containing 41 pairs of nodes. The retry limit is 7 (default value in Wi-Fi). The topology of the network is as shown as follows.

node81 <- 80dB -> node80 <- 70dB -> node79 <- 80dB -> node78 <- 70dB -> ... node2 <- 80dB -> node1 <- 70dB -> node0

Node (2i) transmits 1500 bytes UDP packets to node (2i+1) (0<=i<=40). The packet generation rate of nodes (except node80) is 8.125 pkts/s.  Node 80 is the attacker. By changing the packet generation rate of node 80, we are able to observe a sudden jump of the utilization at node 0, which is referred to as a phase transition. 

Direction:

1. Copy the files yans-wifi-phy.cc and yans-wifi-phy.h under the ns-3.22 direction src/wifi/model/.
  In those two files, a new trace, the duration of each packet, is added.

2. Copy the files athstats-helper.cc and athstats-helper.h under the ns-3.22 direction src/wifi/helper/.
  The file athstats-helper captures the trace of the duration of the packets and culumate the total time that a node is transmitting packets. It creates a file that records the data of the simulation.
  
3. Copy the file CDoS-1Mbps-adhoc-UDP.cc under the ns-3.22 direction scratch/

4. Run simulation
  $ ./waf --run scratch/CDoS-1Mbps-adhoc-UDP
  
