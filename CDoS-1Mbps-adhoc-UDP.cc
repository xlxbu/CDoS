/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Pavel Boyko <boyko@iitp.ru>
 */

/*
 * Classical hidden terminal problem and its RTS/CTS solution.
 *
 * Topology: [node i] <-- -50 dB --> [node i+1] <-- -50 dB --- [node i+2] <-- -50 dB --> [node i+3] 
 * 
 * This example illustrates the use of 
 *  - Wifi in ad-hoc mode
 *  - Matrix propagation loss model
 *  - Use of OnOffApplication to generate CBR stream 
 *  - IP flow monitor
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


// Run single 10 seconds experiment with enabled or disabled RTS/CTS mechanism
void experiment (bool enableCtsRts, uint16_t NumofNode, uint16_t DurationofSimulation, double first_node_load, double rest_node_load, double Pass_loss, uint16_t R, uint16_t pkt_length)
{
  // 0. Enable or disable CTS/RTS
  UintegerValue ctsThr = (enableCtsRts ? UintegerValue (100) : UintegerValue (10000000));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);
	Config::SetDefault ("ns3::WifiNetDevice::Mtu", UintegerValue(2296));
	Config::SetDefault ("ns3::ArpCache::DeadTimeout", TimeValue (Seconds (0)));
	Config::SetDefault ("ns3::ArpCache::AliveTimeout", TimeValue (Seconds (120000)));
	//Config::SetDefault ("ns3::WifiPhy::TxGain", DoubleValue(-10));
	
	//Config::SetDefault ("ns3::ApWifiMac::BeaconInterval", TimeValue (Seconds (1)));
	//Config::SetDefault ("ns3::StaWifiMac::MaxMissedBeacons", UintegerValue(10000000));
	
	//LogComponentEnable ("ApWifiMac", LOG_LEVEL_INFO);
	//LogComponentEnable ("StaWifiMac", LOG_LEVEL_INFO);

  // 1. Create 3 nodes 
  NodeContainer nodes;
  nodes.Create (NumofNode);

  // 2. Place nodes somehow, this is required by every wireless simulation
  for (size_t i = 0; i < NumofNode; ++i)
    {
      nodes.Get (i)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
    }

  // 3. Create propagation loss matrix
  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (150); // set default loss to 200 dB (no link)
	for (size_t i = 0; i < (uint16_t)(NumofNode-1); ++i)
		{
			if ( (i%2) == 0)
				{
					lossModel->SetLoss (nodes.Get (i)->GetObject<MobilityModel>(), nodes.Get (i+1)->GetObject<MobilityModel>(), 80);
				}else
				{
					lossModel->SetLoss (nodes.Get (i+1)->GetObject<MobilityModel>(), nodes.Get (i)->GetObject<MobilityModel>(), Pass_loss);
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
																"MaxSlrc", UintegerValue(R));
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel);
	
	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac"); // use ad-hoc MAC
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);	

  //devices.Get (1)->TraceConnectWithoutContext ("PhyTxDrop", MakeCallback (&RxDrop));
  // uncomment the following to have athstats output
  // AthstatsHelper athstats;
  // athstats.EnableAthstats(enableCtsRts ? "rtscts-athstats-node" : "basic-athstats-node" , nodes);

  // uncomment the following to have pcap output
  // wifiPhy.EnablePcap (enableCtsRts ? "rtscts-pcap-node" : "basic-pcap-node" , nodes);


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
	//PacketSinkHelper sink ("ns3::UdpSocketFactory",Address(InetSocketAddress (Ipv4Address::GetAny (), cbrPort)));
	

  for (size_t i = 0; i < (NumofNode/2); ++i)
    {
			//set nodes as senders
      std::stringstream ipv4address;
			std::stringstream offtime_first;
			std::stringstream offtime_rest;
      ipv4address << "10.0.0." << (i*2+2);
      OnOffHelper *onoffhelper = new OnOffHelper("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address (ipv4address.str().c_str()), cbrPort+i));
      onoffhelper->SetAttribute ("PacketSize", UintegerValue (pkt_length));
			//onoffhelper->SetAttribute ("MaxBytes", UintegerValue (200000000));
      //onoffhelper->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=0.016]"));
      if ( i == (uint16_t)(NumofNode/2-1) )
      {
				if (first_node_load == 1)
				{
					onoffhelper->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
					onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
				}else if (first_node_load == 0) {
					onoffhelper->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));	
				}else {
					std::stringstream ontime_first;
        	double pkt_time_first = (double)1/1000000*pkt_length*8;
        	//std::cout << "pkt_time_first = "  << pkt_time_first << std::endl;
        	ontime_first << "ns3::ConstantRandomVariable[Constant=" << pkt_time_first << "]";
        	onoffhelper->SetAttribute ("OnTime",  StringValue (ontime_first.str()));
        	offtime_first << "ns3::ExponentialRandomVariable[Mean=" << 1/(first_node_load*(1/pkt_time_first))-pkt_time_first << "]";
        	//std::cout << offtime_first.str() << std::endl;
        	onoffhelper->SetAttribute ("OffTime", StringValue (offtime_first.str()));
				}
				onoffhelper->SetAttribute ("DataRate", StringValue ("1000000bps"));
				//onoffhelper->SetAttribute ("StartTime", TimeValue (Seconds (153)));
				//onoffhelper->SetAttribute ("StopTime", TimeValue (Seconds (353)));
      } else {
				//onoffhelper->SetAttribute ("MaxBytes", UintegerValue (400000000));
				std::stringstream ontime_rest;
				double pkt_time_rest = (double)1/1000000*pkt_length*8;
				//std::cout << "pkt_time_rest = "  << pkt_time_rest << std::endl;
				ontime_rest << "ns3::ConstantRandomVariable[Constant=" << pkt_time_rest << "]";
				onoffhelper->SetAttribute ("OnTime",  StringValue (ontime_rest.str()));
				offtime_rest << "ns3::ExponentialRandomVariable[Mean=" << 1/(rest_node_load*(1/pkt_time_rest))-pkt_time_rest << "]";
				//std::cout << offtime_rest.str() << std::endl;
        onoffhelper->SetAttribute ("OffTime", StringValue (offtime_rest.str()));
				onoffhelper->SetAttribute ("DataRate", StringValue ("1000000bps"));
				//onoffhelper->SetAttribute ("StartTime", TimeValue (Seconds (3.100+i*0.01)));
				//onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=0.0585]")); //0.12
        //onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=0.107]")); //0.13
      }
      //onoffhelper->SetAttribute ("DataRate", StringValue ("1000000bps"));
      onoffhelper->SetAttribute ("StartTime", TimeValue (Seconds (3.100+i*0.01)));
      cbrApps.Add (onoffhelper->Install (nodes.Get (i*2)));
      onoffhelpers.push_back(onoffhelper);

			//set nodes as receivers
			PacketSinkHelper *sink = new PacketSinkHelper("ns3::UdpSocketFactory",Address(InetSocketAddress (Ipv4Address::GetAny (), cbrPort+i)));
			cbrApps.Add (sink->Install (nodes.Get(i*2+1)));
    }
 


/*TCP socket*/   
/*
	ApplicationContainer cbrApps;
  uint16_t cbrPort = 12345;
  // flow 1:  node i -> node i+1
  std::vector<OnOffHelper*> onoffhelpers;
  //PacketSinkHelper sink ("ns3::TcpSocketFactory",Address(InetSocketAddress (Ipv4Address::GetAny (), cbrPort)));
	std::vector<PacketSinkHelper*> sinks;
  for (size_t i = 0; i < (NumofNode/2); ++i)
    {
      //set nodes as senders
      std::stringstream ipv4address;
			std::stringstream offtime_first;
      std::stringstream offtime_rest;
      ipv4address << "10.0.0." << (i*2+2);
			OnOffHelper *onoffhelper = new OnOffHelper("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address (ipv4address.str().c_str()), cbrPort+i));
      onoffhelper->SetAttribute ("PacketSize", UintegerValue (2000));
      onoffhelper->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=0.016]"));
			onoffhelper->SetAttribute ("MaxBytes", UintegerValue (200000000));
      if ( i == (NumofNode/2-1) )
      {
        offtime_first << "ns3::ExponentialRandomVariable[Mean=" << 1/(first_node_load*62.5)-0.016 << "]";
        onoffhelper->SetAttribute ("OffTime", StringValue (offtime_first.str()));
      } else {
        offtime_rest << "ns3::ExponentialRandomVariable[Mean=" << 1/(rest_node_load*62.5)-0.016 << "]";
        onoffhelper->SetAttribute ("OffTime", StringValue (offtime_rest.str()));
        //onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=0.0585]")); //0.12
        //onoffhelper->SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=0.0535]")); //0.13
      }
      onoffhelper->SetAttribute ("DataRate", StringValue ("1000000bps"));
      onoffhelper->SetAttribute ("StartTime", TimeValue (Seconds (3.100-i*0.01)));
      cbrApps.Add (onoffhelper->Install (nodes.Get (i*2)));
      onoffhelpers.push_back(onoffhelper);

      //set nodes as receivers
			PacketSinkHelper *sink = new PacketSinkHelper ("ns3::TcpSocketFactory",Address(InetSocketAddress (Ipv4Address::GetAny (), cbrPort+i)));
			sink->SetAttribute ("StartTime", TimeValue (Seconds (1.00)));
      cbrApps.Add (sink->Install (nodes.Get(i*2+1)));
			sinks.push_back(sink);
    }
*/

	/*Static ARP setup*/
	/*
	for (size_t i = 0; i < NumofNode; ++i)
	{
		Address mac;
		Ipv4Address ip;
		if(i%2 == 0)
		{
			mac = nodes.Get(i+1)->GetDevice(0)->GetAddress();
			ip = nodes.Get(i+1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
		}else
		{
			mac = nodes.Get(i-1)->GetDevice(0)->GetAddress();
			ip = nodes.Get(i-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
		}
		std::cout << "mac: " << mac << " ip: " << ip << std::endl;
		//std::stringstream ipv4address1;
		//ipv4address << "10.0.0." << (i+1);
		//Ptr<ArpCache> arpCache = cbrApps.Get(i)->GetNode()->GetObject<Ipv4L3Protocol>()->GetInterface(1)->GetArpCache();
		Ptr<ArpCache> arpCache = CreateObject<ArpCache> ();
		arpCache->SetAliveTimeout (Seconds(3600*24*365));
		//if(arpCache == NULL)
		//	arpCache = CreateObject<ArpCache>();
		arpCache->Add(ip);
  	//entry->MarkWaitReply(0);
		arpCache->Lookup(ip)->SetMacAddresss(mac);
  	arpCache->Lookup(ip)->MarkPermanent();
		nodes.Get(i)->GetObject<Ipv4L3Protocol>()->GetInterface(1)->SetArpCache(arpCache);
	}
	*/

	/** \internal
 *    * We also use separate UDP applications that will send a single
 *       * packet before the CBR flows start.
 *          * This is a workaround for the lack of perfect ARP, see \bugid{187}
 *             */

  std::vector<UdpEchoClientHelper*> echoClientHelpers;
  uint16_t  echoPort = 9;
  ApplicationContainer pingApps;
  // again using different start times to workaround Bug 388 and Bug 912
  for (size_t i = 0; i < (NumofNode/2); ++i)
  	{
  		std::stringstream ipv4address;
  		ipv4address << "10.0.0." << (i*2+2);
  		UdpEchoClientHelper *echoClientHelper = new UdpEchoClientHelper(Ipv4Address(ipv4address.str().c_str()), echoPort);
  		echoClientHelper->SetAttribute ("MaxPackets", UintegerValue (1));
  		echoClientHelper->SetAttribute ("Interval", TimeValue (Seconds (100000.0)));
  		//echoClientHelper.SetAttribute ("Interval", TimeValue (Seconds (100)));
  		echoClientHelper->SetAttribute ("PacketSize", UintegerValue (10));
  		//echoClientHelper->SetAttribute ("Jitter", UintegerValue (0));
  		echoClientHelper->SetAttribute ("StartTime", TimeValue (Seconds (0.001+i/1000)));
  		pingApps.Add (echoClientHelper->Install (nodes.Get (i*2)));
  		echoClientHelpers.push_back(echoClientHelper);
  	}


	//AsciiTraceHelper ascii;
  //pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("myfirst.tr"));
	

  // 8. Install FlowMonitor on all nodes
  //FlowMonitorHelper flowmon;
	//flowmon.SetMonitorAttribute ("PacketSizeFilter", UintegerValue (500));
	//flowmon.SetMonitorAttribute ("StartTime", TimeValue (Seconds (1000)));
  //Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	mkdir("CDoS-1Mbps-adhoc-UDP-01",S_IRWXU | S_IRWXG | S_IRWXO);
	char pathname [50];
	std::stringstream filename;
	std::stringstream foldername;
	//foldername << "./CRoQ-Minstrel-staticARP-adhoc-UDP/" << "first_node_load=" << first_node_load << "rest_node_load=" << rest_node_load;
	sprintf (pathname, "./CDoS-1Mbps-adhoc-UDP-01/u_0=%.2frho=%.2fR=%dT=%d",first_node_load, rest_node_load, R, pkt_length);
	//filename << "./CRoQ-Minstrel-staticARP-adhoc-UDP/" << "first_node_load=" << first_node_load << "rest_node_load=" << rest_node_load << "/nodes";
	//sprintf (filename, "./CRoQ-Minstrel-staticARP-adhoc-UDP/first_node_load=%.2frest_node_load=%.2f/nodes", first_node_load, rest_node_load);
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

int main (int argc, char **argv)
{
	//Packet::EnablePrinting ();
	//std::cout << RngSeedManager::GetSeed() << std::endl;
	RngSeedManager::SetSeed(1);
	//std::cout << RngSeedManager::GetSeed() << std::endl;
	uint16_t NumofNode = 82;
	uint16_t DurationofSimulation = 1003;
	double first_node_load;
	double rest_node_load;
	double pass_loss = 70;
	for (size_t i = 1; i < 50; ++i)
		{
			first_node_load = (double)0.02*i;
			for (size_t j = 13; j<14; ++j)
				{
					rest_node_load = (double)j/100;
					for (size_t R = 7; R < 8; ++R)
						{
							for (size_t T = 1; T < 2; ++T)
							{
  							std::cout << " first node = " << first_node_load << " rest node = " << rest_node_load << std::endl;
  							experiment (false, NumofNode, DurationofSimulation, first_node_load, rest_node_load, pass_loss, R, T*1500);
							//R = R +3;
							}
						}
				}
		}
  return 0;
}
