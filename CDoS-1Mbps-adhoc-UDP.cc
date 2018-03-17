
/* This is an ns-3 simulation to demonstrate the feasibility of cascading DoS attacks on Wi-Fi networks. 
 * This simulation shows that an adversary can take advantage of the interference coupling phenomenon to 
 * launch a widespread network attack from a single location.
 *
 * More simulation results can be found in paper
 * Liangxiao Xin, David Starobinski, and Guevara Noubir, "Cascading Denial of Service Attacks on Wi-Fi Networks," 
 * IEEE CNS 2016, Philadelphia, PA, October 2016
 *
 * Authors: Liangxiao Xin <xlx@bu.edu>
 */

/*
 * Topology: 
 * [node 0] <-80dB-> [node 1] <-70dB-> [node 2] <-80dB-> [node 3] <-70dB-> [node 4]...[node N-2] <-70dB-> [node N-1]
 *
 * Transmission pair: node 2i -> node 2i+1
 * 
 * Goal:
 * When increase the transmission rate at link node N-2 -> node N-1, all the other tranmission pairs vanishs their throughput.
 *
 */
#include "ns3/core-module.h"
#include "ns3/propagation-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"

#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sys/stat.h>

using namespace ns3;


//start a single experiment 
void experiment (bool enableCtsRts, uint16_t NumofNode, uint16_t DurationofSimulation, double FirstNodeLoad, double RestNodeLoad)
{
  // 0. Enable or disable CTS/RTS
  UintegerValue ctsThr = (enableCtsRts ? UintegerValue (100) : UintegerValue (10000000));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);
  Config::SetDefault ("ns3::WifiNetDevice::Mtu", UintegerValue(2296));

  /**Static ARP setup**/
  Config::SetDefault ("ns3::ArpCache::DeadTimeout", TimeValue (Seconds (0)));
  Config::SetDefault ("ns3::ArpCache::AliveTimeout", TimeValue (Seconds (120000)));

  // 1. Create nodes 
  NodeContainer nodes;
  nodes.Create (NumofNode);

  // 2. we use 
  for (size_t i = 0; i < NumofNode; ++i){
    nodes.Get (i)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  }

  // 3. Create propagation loss matrix
  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (150); // set default loss to 200 dB (no link)
  for (size_t i = 0; i < (uint16_t)(NumofNode-1); ++i){
    if ( (i%2) == 0){
      lossModel->SetLoss (nodes.Get (i)->GetObject<MobilityModel>(), nodes.Get (i+1)->GetObject<MobilityModel>(), 80);
    }else{
      lossModel->SetLoss (nodes.Get (i+1)->GetObject<MobilityModel>(), nodes.Get (i)->GetObject<MobilityModel>(), 70);
    }
  }

  // 4. Create & setup wifi channel
  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());

  // 5. Install wireless devices
  /*constant rate wifi manager*/
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                "DataMode",StringValue ("DsssRate1Mbps"), 
                                "ControlMode",StringValue ("DsssRate1Mbps"),
                                "FragmentationThreshold",UintegerValue(2300),
                                "MaxSlrc", UintegerValue(7));
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel);
	
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac"); // use ad-hoc MAC
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);	

  // 6. Install TCP/IP stack & assign IP addresses
  InternetStackHelper internet;
  internet.Install (nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.0.0.0");
  ipv4.Assign (devices);

  // 7. Install applications: two CBR streams each saturating the channel
  /*UPD socket*/ 
  ApplicationContainer cbrApps;
  uint16_t cbrPort = 12345;
  // flow 1:  node i -> node i+1
  std::vector<OnOffHelper*> onoffhelpers;
  std::vector<PacketSinkHelper*> sinks;

  for (size_t i = 0; i < (NumofNode/2); ++i){
    //set nodes as senders
    std::stringstream ipv4address;
    std::stringstream offtime_first;
    std::stringstream offtime_rest;
    ipv4address << "10.0.0." << (i*2+2);
    OnOffHelper *onoffhelper = new OnOffHelper("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address (ipv4address.str().c_str()), cbrPort+i));
    onoffhelper->SetAttribute ("PacketSize", UintegerValue (1500));
    if ( i == (uint16_t)(NumofNode/2-1) ){
      if (FirstNodeLoad == 1){
        onoffhelper->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      }else if (FirstNodeLoad == 0){
        onoffhelper->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));	
      }else {
        std::stringstream ontime_first;
        double pkt_time_first = (double)1/1000000*1500*8;
        ontime_first << "ns3::ConstantRandomVariable[Constant=" << pkt_time_first << "]";
        onoffhelper->SetAttribute ("OnTime",  StringValue (ontime_first.str()));
        offtime_first << "ns3::ExponentialRandomVariable[Mean=" << 1/(FirstNodeLoad*(1/pkt_time_first))-pkt_time_first << "]";
        onoffhelper->SetAttribute ("OffTime", StringValue (offtime_first.str()));
      } 
      onoffhelper->SetAttribute ("DataRate", StringValue ("1000000bps"));
    }else{
      std::stringstream ontime_rest;
      double pkt_time_rest = (double)1/1000000*1500*8;
      ontime_rest << "ns3::ConstantRandomVariable[Constant=" << pkt_time_rest << "]";
      onoffhelper->SetAttribute ("OnTime",  StringValue (ontime_rest.str()));
      offtime_rest << "ns3::ExponentialRandomVariable[Mean=" << 1/(RestNodeLoad*(1/pkt_time_rest))-pkt_time_rest << "]";
      onoffhelper->SetAttribute ("OffTime", StringValue (offtime_rest.str()));
      onoffhelper->SetAttribute ("DataRate", StringValue ("1000000bps"));
    }
    onoffhelper->SetAttribute ("StartTime", TimeValue (Seconds (3.100+i*0.01)));
    cbrApps.Add (onoffhelper->Install (nodes.Get (i*2)));
    onoffhelpers.push_back(onoffhelper);

    //set nodes as receivers
    PacketSinkHelper *sink = new PacketSinkHelper("ns3::UdpSocketFactory",Address(InetSocketAddress (Ipv4Address::GetAny (), cbrPort+i)));
    cbrApps.Add (sink->Install (nodes.Get(i*2+1)));
  }
 

   /** \internal
     * We also use separate UDP applications that will send a single
     * packet before the CBR flows start.
     * This is a workaround for the lack of perfect ARP, see \bugid{187}
     */

  std::vector<UdpEchoClientHelper*> echoClientHelpers;
  uint16_t  echoPort = 9;
  ApplicationContainer pingApps;
  // again using different start times to workaround Bug 388 and Bug 912
  for (size_t i = 0; i < (NumofNode/2); ++i){
    std::stringstream ipv4address;
    ipv4address << "10.0.0." << (i*2+2);
    UdpEchoClientHelper *echoClientHelper = new UdpEchoClientHelper(Ipv4Address(ipv4address.str().c_str()), echoPort);
    echoClientHelper->SetAttribute ("MaxPackets", UintegerValue (1));
    echoClientHelper->SetAttribute ("Interval", TimeValue (Seconds (100000.0)));
    echoClientHelper->SetAttribute ("PacketSize", UintegerValue (10));
    echoClientHelper->SetAttribute ("StartTime", TimeValue (Seconds (0.001+i/1000)));
    pingApps.Add (echoClientHelper->Install (nodes.Get (i*2)));
    echoClientHelpers.push_back(echoClientHelper);
  }

	

  // 8. Install FlowMonitor on all nodes
  mkdir("CDoS-1Mbps-adhoc-UDP-01",S_IRWXU | S_IRWXG | S_IRWXO);
  char pathname [50];
  std::stringstream filename;
  std::stringstream foldername;
  sprintf (pathname, "./CDoS-1Mbps-adhoc-UDP-01/u_0=%.2frho=%.2f",FirstNodeLoad, RestNodeLoad);
  foldername << pathname;
  filename << pathname << "/nodes";
  mkdir(foldername.str().c_str(),S_IRWXU | S_IRWXG | S_IRWXO);
  AthstatsHelper athstats;
  athstats.EnableAthstats (filename.str().c_str(), devices);

  // 9. Run simulation for 10 seconds
  Simulator::Stop (Seconds (DurationofSimulation));
  Simulator::Run ();

  // 11. Cleanup
  Simulator::Destroy ();
}



int main (int argc, char **argv){
  //Packet::EnablePrinting ();
  //std::cout << RngSeedManager::GetSeed() << std::endl;
  RngSeedManager::SetSeed(1);
  //std::cout << RngSeedManager::GetSeed() << std::endl;
  uint16_t numofnode = 82;
  uint16_t durationofsimulation = 1003;
  double firstnodeload;
  double restnodeload;
  for (size_t i = 1; i < 50; ++i){
    firstnodeload = (double)0.02*i;
    for (size_t j = 13; j<14; ++j){
      restnodeload = (double)j/100;
      std::cout << " first node = " << firstnodeload << " rest node = " << restnodeload << std::endl;
      experiment (false, numofnode, durationofsimulation, firstnodeload, restnodeload);
    }
  }
  return 0;
}
