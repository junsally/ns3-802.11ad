/*
 * Copyright (c) 2015, 2016 IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "common-functions.h"
#include <string>
#include <stdlib.h>
#include <cmath>


/**
 * This script is used to evaluate IEEE 802.11ad Throughput for different PHY Layers.
 * Network topology is simple and consists of One Access Point + One Station.
 *
 * To print the acheived throughput for each MCS type the following command:
 * ./waf --run "evaluate_throughput"
 *
 * The script will print the achieved throughput in Mbps for each MCS starting from MCS1-MCS24.
 * Each DMG STA supports two level of aggregation (A-MSDU + A-MPDU).
 */

NS_LOG_COMPONENT_DEFINE ("EvaluateDmgThroughput");

using namespace ns3;
using namespace std;

Ptr<Node> apWifiNode;
Ptr<Node> staWifiNode;
Ptr<Node> intfWifiNode;

int
main(int argc, char *argv[])
{
  string applicationType = "onoff";             /* Type of the Tx application */
  uint32_t payloadSize = 1472;                  /* Transport Layer Payload size in bytes. */
  string socketType = "ns3::UdpSocketFactory";  /* Socket Type (TCP/UDP) */
  uint32_t maxPackets = 0;                      /* Maximum Number of Packets */
  string tcpVariant = "ns3::TcpNewReno";        /* TCP Variant Type. */
  uint32_t bufferSize = 131072;                 /* TCP Send/Receive Buffer Size. */
  string phyMode = "DMG_MCS";                   /* Type of the Physical Layer. */
  double distance = 1;                          /* The distance between transmitter and receiver in meters. */
  bool verbose = false;                         /* Print Logging Information. */
  double simulationTime = 2;                    /* Simulation time in seconds. */
  bool pcapTracing = false;                     /* PCAP Tracing is enabled or not. */
  std::list<std::string> dataRateList;          /* List of the maximum data rate supported by the standard*/
  std::string channelState = "a";               /* channel state for propagation loss model, can be n, l, o and a */
  double theta = 90;                            /* the angle to x axis for interference station in x-y plane */


  /** MCS List **/
  /* SC PHY */
  dataRateList.push_back ("385Mbps");           //MCS1
  dataRateList.push_back ("770Mbps");           //MCS2
  dataRateList.push_back ("962.5Mbps");         //MCS3
  dataRateList.push_back ("1155Mbps");          //MCS4
  dataRateList.push_back ("1251.25Mbps");       //MCS5
  dataRateList.push_back ("1540Mbps");          //MCS6
  dataRateList.push_back ("1925Mbps");          //MCS7
  dataRateList.push_back ("2310Mbps");          //MCS8
  dataRateList.push_back ("2502.5Mbps");        //MCS9
  dataRateList.push_back ("3080Mbps");          //MCS10
  dataRateList.push_back ("3850Mbps");          //MCS11
  dataRateList.push_back ("4620Mbps");          //MCS12
  /* OFDM PHY */
  dataRateList.push_back ("693.00Mbps");        //MCS13
  dataRateList.push_back ("866.25Mbps");        //MCS14
  dataRateList.push_back ("1386.00Mbps");       //MCS15
  dataRateList.push_back ("1732.50Mbps");       //MCS16
  dataRateList.push_back ("2079.00Mbps");       //MCS17
  dataRateList.push_back ("2772.00Mbps");       //MCS18
  dataRateList.push_back ("3465.00Mbps");       //MCS19
  dataRateList.push_back ("4158.00Mbps");       //MCS20
  dataRateList.push_back ("4504.50Mbps");       //MCS21
  dataRateList.push_back ("5197.50Mbps");       //MCS22
  dataRateList.push_back ("6237.00Mbps");       //MCS23
  dataRateList.push_back ("6756.75Mbps");       //MCS24

  /* Command line argument parser setup. */
  CommandLine cmd;

  cmd.AddValue ("applicationType", "Type of the Tx Application: onoff or bulk", applicationType);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("socketType", "Type of the Socket (ns3::TcpSocketFactory, ns3::UdpSocketFactory)", socketType);
  cmd.AddValue ("maxPackets", "Maximum number of packets to send", maxPackets);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpTahoe, TcpReno, TcpNewReno, TcpWestwood, TcpWestwoodPlus ", tcpVariant);
  cmd.AddValue ("bufferSize", "TCP Buffer Size (Send/Receive)", bufferSize);
  cmd.AddValue ("dist", "distance between nodes", distance);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable PCAP Tracing", pcapTracing);
  cmd.AddValue ("channelState", "Channel state 'l'=LOS, 'n'=NLOS, 'a'=all", channelState);
  cmd.AddValue ("theta", "the angle of interference station to x axis in x-y plane, range from 0 to 180", theta);
  cmd.Parse (argc, argv);

  /* Global params: no fragmentation, no RTS/CTS, fixed rate for all packets */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("999999"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));

  /*** Configure TCP Options ***/
  /* Select TCP variant */
  TypeId tid = TypeId::LookupByName (tcpVariant);
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tid));
  /* Configure TCP Segment Size */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (bufferSize));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (bufferSize));
  Config::SetDefault ("ns3::MmWavePropagationLossModel::ChannelStates", StringValue (channelState));


  cout << "MCS" << '\t' << "Throughput (Mbps)" << endl;

  uint i = 1; /* MCS Index */   //Sally modified

//  std::list<std::string>::const_iterator iter = dataRateList.end ();//Sally add
//  iter--; //Sally add
//  std::list<std::string>::const_iterator iter = dataRateList.begin ();//Sally add

   for (std::list<std::string>::const_iterator iter = dataRateList.begin (); iter != dataRateList.end (); iter++, i++) //MCS
    {
  
     if ((i != 1) && (i != 24)) continue; //Sally add

      /**** WifiHelper is a meta-helper: it helps creates helpers ****/
      WifiHelper wifi;

      /* Basic setup */
      wifi.SetStandard (WIFI_PHY_STANDARD_80211ad);

      /* Turn on logging */
      if (verbose)
        {
          wifi.EnableLogComponents ();
          LogComponentEnable ("EvaluateDmgThroughput", LOG_LEVEL_ALL);
        }

      /**** Set up Channel ****/
      YansWifiChannelHelper wifiChannel ;
      /* Simple propagation delay model */
      wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      /* mmwave propagation loss model with standard-specific wavelength */
      wifiChannel.AddPropagationLoss ("ns3::MmWavePropagationLossModel", "Frequency", DoubleValue (73e9));

      /**** SETUP ALL NODES ****/
      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
      /* Nodes will be added to the channel we set up earlier */
      wifiPhy.SetChannel (wifiChannel.Create ());
      /* All nodes transmit at 10 dBm == 10 mW, no adaptation */
      wifiPhy.Set ("TxPowerStart", DoubleValue (100.0));
      wifiPhy.Set ("TxPowerEnd", DoubleValue (100.0));
      wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
      wifiPhy.Set ("TxGain", DoubleValue (0));
      wifiPhy.Set ("RxGain", DoubleValue (0));
      /* Sensitivity model includes implementation loss and noise figure */
      wifiPhy.Set ("RxNoiseFigure", DoubleValue (3));
      wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-109));
      wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-109 + 3));
      /* Set the phy layer error model */
//      wifiPhy.SetErrorRateModel ("ns3::SensitivityModel60GHz");
      wifiPhy.SetErrorRateModel ("ns3::ErrorRateModelSensitivityOFDM");
      /* Set default algorithm for all nodes to be constant rate */
      ostringstream mcs;
      mcs << i;
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "ControlMode", StringValue ("DMG_MCS0"),
                                                                    "DataMode", StringValue (phyMode + mcs.str ()));
      /* Give all nodes steerable antenna */
      wifiPhy.EnableAntenna (true, true);
      wifiPhy.SetAntenna ("ns3::Directional60GhzAntenna",
                          "Sectors", UintegerValue (8),
                          "Antennas", UintegerValue (1));

      /* Make two nodes and set them up with the phy and the mac */
      NodeContainer wifiNodes;
      wifiNodes.Create (3);
      apWifiNode = wifiNodes.Get (0);
      staWifiNode = wifiNodes.Get (1);
      intfWifiNode = wifiNodes.Get (2);

      /**** Allocate a default Adhoc Wifi MAC ****/
      /* Add a DMG upper mac */
      DmgWifiMacHelper wifiMac = DmgWifiMacHelper::Default ();

      Ssid ssid = Ssid ("test802.11ad");
      wifiMac.SetType ("ns3::DmgApWifiMac",
                       "Ssid", SsidValue(ssid),
                       "QosSupported", BooleanValue (true), "DmgSupported", BooleanValue (true),
                       "BE_MaxAmpduSize", UintegerValue (262143), //Enable A-MPDU with the highest maximum size allowed by the standard
                       "BE_MaxAmsduSize", UintegerValue (7935),
                       "SSSlotsPerABFT", UintegerValue (8), "SSFramesPerSlot", UintegerValue (8),
                       "BeaconInterval", TimeValue (MicroSeconds (102400)),
                       "BeaconTransmissionInterval", TimeValue (MicroSeconds (600)),
                       "ATIDuration", TimeValue (MicroSeconds (300)));

      NetDeviceContainer apDevice;
      apDevice = wifi.Install (wifiPhy, wifiMac, apWifiNode);

      wifiMac.SetType ("ns3::DmgStaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue (false),
                       "BE_MaxAmpduSize", UintegerValue (262143), //Enable A-MPDU with the highest maximum size allowed by the standard
                       "BE_MaxAmsduSize", UintegerValue (7935),
                       "QosSupported", BooleanValue (true), "DmgSupported", BooleanValue (true));

      NetDeviceContainer staDevices;
//      staDevice = wifi.Install (wifiPhy, wifiMac, NodeContainer (staWifiNode, intfWifiNode));
      staDevices.Add (wifi.Install (wifiPhy, wifiMac, staWifiNode));
      staDevices.Add (wifi.Install (wifiPhy, wifiMac, intfWifiNode));

/*      wifiMac.SetType ("ns3::DmgStaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue (false),
                       "BE_MaxAmpduSize", UintegerValue (262143), //Enable A-MPDU with the highest maximum size allowed by the standard
                       "BE_MaxAmsduSize", UintegerValue (7935),
                       "QosSupported", BooleanValue (true), "DmgSupported", BooleanValue (true));

      NetDeviceContainer intfDevice;
      intfDevice = wifi.Install (wifiPhy, wifiMac, intfWifiNode);
*/

      /* Setting mobility model, Initial Position 1 meter apart */
      MobilityHelper mobility;
      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
      positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // position for PCP/AP
      positionAlloc->Add (Vector (distance, 0.0, 0.0)); // position for staWifiNode
      positionAlloc->Add (Vector (distance*cos(theta*M_PI/180), distance*sin(theta*M_PI/180), 0.0)); // position for intfWifiNode

std::cout << "sally test position for interferer: " << "x=" << distance*cos(theta*M_PI/180) << ", y=" << distance*sin(theta*M_PI/180) << std::endl;

      mobility.SetPositionAllocator (positionAlloc);
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (wifiNodes);

      /* Internet stack*/
      InternetStackHelper stack;
      stack.Install (wifiNodes);

      Ipv4AddressHelper address;
      address.SetBase ("10.0.0.0", "255.255.255.0");
      Ipv4InterfaceContainer apInterface;
      apInterface = address.Assign (apDevice);
      Ipv4InterfaceContainer staInterface;
      staInterface = address.Assign (staDevices);
/*      Ipv4InterfaceContainer intfInterface;
      intfInterface = address.Assign (intfDevice);
*/
      /* Populate routing table */
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

      /* We do not want any ARP packets */
      PopulateArpCache ();




/*  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address ("10.1.1.1"), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  // Interferer will send to a different port; we will not see a
  // "Received packet" message
  Ptr<Socket> interferer = Socket::CreateSocket (c.Get (2), tid);
  InetSocketAddress interferingAddr = InetSocketAddress (Ipv4Address ("255.255.255.255"), 49000);
  interferer->SetAllowBroadcast (true);
  interferer->Connect (interferingAddr);

*/



      /* Install Simple UDP Server on the access point */
      PacketSinkHelper sinkHelper (socketType, InetSocketAddress (Ipv4Address::GetAny (), 9999));
//      PacketSinkHelper sinkHelper (socketType, InetSocketAddress (apInterface.GetAddress (0), 9999));
      ApplicationContainer sinkApp = sinkHelper.Install (apWifiNode);
      Ptr<PacketSink> sink = StaticCast<PacketSink> (sinkApp.Get (0));
      sinkApp.Start (Seconds (0.0));

      /* Install TCP/UDP Transmitter on the station */
      Address dest (InetSocketAddress (apInterface.GetAddress (0), 9999));
      ApplicationContainer srcApp;
      if (applicationType == "onoff")
        {
          OnOffHelper src (socketType, dest);
          src.SetAttribute ("MaxBytes", UintegerValue (0));
          src.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          src.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
          src.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          src.SetAttribute ("DataRate", DataRateValue (DataRate (*iter)));

          srcApp = src.Install (NodeContainer(staWifiNode, intfWifiNode));
        }
      else if (applicationType == "bulk")
        {
          BulkSendHelper src (socketType, dest);
          srcApp = src.Install (NodeContainer(staWifiNode, intfWifiNode));
        }

      srcApp.Start (Seconds (1.0));

      /* Install TCP/UDP Transmitter interference on the station */
/*      Address destintf (InetSocketAddress (apInterface.GetAddress (0), 9000));
      ApplicationContainer intfApp;
      if (applicationType == "onoff")
        {
          OnOffHelper intf (socketType, destintf);
          intf.SetAttribute ("MaxBytes", UintegerValue (0));
          intf.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          intf.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
          intf.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          intf.SetAttribute ("DataRate", DataRateValue (DataRate (*iter)));

          intfApp = intf.Install (intfWifiNode);
        }
      else if (applicationType == "bulk")
        {
          BulkSendHelper intf (socketType, destintf);
          intfApp= intf.Install (intfWifiNode);
        }

      intfApp.Start (Seconds (1.0));

*/
      if (pcapTracing)
        {
          wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
          wifiPhy.EnablePcap ("Traces/AccessPoint" + mcs.str (), apDevice, false);
          wifiPhy.EnablePcap ("Traces/Station" + mcs.str (), staDevices, false);
        }

      Simulator::Stop (Seconds (simulationTime));
      Simulator::Run ();

      /* Calculate Throughput */
      cout << "SMCS" << mcs.str () << '\t' << sink->GetTotalRx () * (double) 8/1e6 << endl;

      Simulator::Destroy ();
    }

  return 0;
}
