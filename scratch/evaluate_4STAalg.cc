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

/**
 * This script is used to evaluate allocation of Static Service Periods in IEEE 802.11ad.
 * The scenario consists of 3 DMG STAs (West + South + East) and one DMG PCP/AP as following:
 *
 *                         DMG AP (0,1)
 *
 *
 * West DMG STA (-1,0)                      East DMG STA (1,0)
 *
 *
 *                      South DMG STA (0,-1)
 *
 * Once all the stations have assoicated successfully with the PCP/AP. The PCP/AP allocates three SPs
 * to perform TxSS between all the stations. Once West DMG STA has completed TxSS phase with East and
 * South DMG STAs. The PCP/AP will allocate two static service periods for communication as following:
 *
 * SP1: West DMG STA -----> East DMG STA (SP Length = 3.2ms)
 * SP2: West DMG STA -----> South DMG STA (SP Length = 3.2ms)
 *
 * From the PCAP files, we can see that data transmission takes place during its SP. In addition, we can
 * notice in the announcement of the two Static Allocation Periods inside each DMG Beacon.
 */

NS_LOG_COMPONENT_DEFINE ("EvaluateSP4STA");

using namespace ns3;
using namespace std;

/* Network Nodes */
Ptr<WifiNetDevice> apWifiNetDevice;
Ptr<WifiNetDevice> staFirstWifiNetDevice;
Ptr<WifiNetDevice> staSecondWifiNetDevice;
Ptr<WifiNetDevice> staThirdWifiNetDevice;

NetDeviceContainer staDevices;

Ptr<DmgApWifiMac> apWifiMac;
Ptr<DmgStaWifiMac> staFirstWifiMac;
Ptr<DmgStaWifiMac> staSecondWifiMac;
Ptr<DmgStaWifiMac> staThirdWifiMac;

/*** Access Point Variables ***/
uint8_t assoicatedStations = 0;           /* Total number of assoicated stations with the AP */
uint8_t stationsTrained = 0;              /* Number of BF trained stations */
bool scheduledStaticPeriods = false;      /* Flag to indicate whether we scheduled Static Service Periods or not */
uint8_t ThroughputCount = 0;              /* Calculate how many times the throughput is calculated. */ 
double totalThroughput = 0;               /* Calculate the total throughput of concurrent transmission. */


void
CalculateThroughput (Ptr<PacketSink> sink, uint64_t lastTotalRx, double averageThroughput)
{
  if (ThroughputCount == 4)
  {
     ThroughputCount = 0;
     totalThroughput = 0;
  }

  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx() - lastTotalRx) * (double) 8/1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << "junjun " << now.GetSeconds () << '\t' << cur << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  averageThroughput += cur;
  totalThroughput += cur;
  ThroughputCount++;
  std::cout << "sally test CalculateThroughput, totalThroughput=" << totalThroughput << ", ThroughputCount=" << ThroughputCount << std::endl;

  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput, sink, lastTotalRx, averageThroughput);
}

void
StationAssoicated (Ptr<DmgStaWifiMac> staWifiMac, Mac48Address address)
{
  std::cout << "DMG STA " << staWifiMac->GetAddress () << " associated with DMG AP " << address << std::endl;
  std::cout << "Association ID (AID) = " << staWifiMac->GetAssociationID () << std::endl;
  assoicatedStations++;
  /* Map AID to MAC Addresses in each node instead of requesting information */
  Ptr<DmgStaWifiMac> dmgStaMac;
  for (NetDeviceContainer::Iterator i = staDevices.Begin (); i != staDevices.End (); ++i)
    {
      dmgStaMac = StaticCast<DmgStaWifiMac> (StaticCast<WifiNetDevice> (*i)->GetMac ());
      if (dmgStaMac != staWifiMac)
        {
          dmgStaMac->MapAidToMacAddress (staWifiMac->GetAssociationID (), staWifiMac->GetAddress ());
        }
    }
  /* Check if all stations have assoicated with the AP */
  if (assoicatedStations == 3)
    {
      std::cout << "All stations got associated with " << address << std::endl;
      /* Schedule Beamforming Training SP */
      apWifiMac->AllocateBeamformingServicePeriod (staFirstWifiMac->GetAssociationID (), staSecondWifiMac->GetAssociationID (), 0, true);
      apWifiMac->AllocateBeamformingServicePeriod (staFirstWifiMac->GetAssociationID (), staThirdWifiMac->GetAssociationID (), 500, true);
      apWifiMac->AllocateBeamformingServicePeriod (staSecondWifiMac->GetAssociationID (), staThirdWifiMac->GetAssociationID (), 1000, true);
    }
}


int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472;                  /* Transport Layer Payload size in bytes. */
  string dataRate = "1000Mbps";                  /* Application Layer Data Rate. */
  uint32_t msduAggregationSize = 7935;          /* The maximum aggregation size for A-MSDU in Bytes. */
  uint32_t queueSize = 10;                   /* Wifi Mac Queue Size. */
  string phyMode = "DMG_MCS24";                 /* Type of the Physical Layer. */
  bool verbose = false;                         /* Print Logging Information. */
  double simulationTime = 4.51;                   /* Simulation time in seconds. */
  bool pcapTracing = false;                     /* PCAP Tracing is enabled or not. */
  uint16_t sectorNum = 12;                        /* Number of sectors in total. */
  double sta1_xPos = 1;                         /* X axis position of station 1. */
  double sta1_yPos = 0;                         /* Y axis position of station 1. */
  double sta2_xPos = 0;                         /* X axis position of station 2. */
  double sta2_yPos = 1;                         /* Y axis position of station 2. */
  double sta3_xPos = 1;                         /* X axis position of station 3. */
  double sta3_yPos = 1;                         /* Y axis position of station 2. */
  double radEfficiency = 0.9;                   /* radiation efficiency of the directional antenna. */
  double txPower = 60.0;                       /* transmit power in dBm. */
  double threshold = -20;                       /* CCA mode1 threshold */
  double maxDelay = 10.0;                      /* maxdelay in millisecond. */

  /* Command line argument parser setup. */
  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Payload size in bytes", dataRate);
  cmd.AddValue ("msduAggregation", "The maximum aggregation size for A-MSDU in Bytes", msduAggregationSize);
  cmd.AddValue ("queueSize", "The size of the Wifi Mac Queue", queueSize);
  cmd.AddValue ("phyMode", "802.11ad PHY Mode", phyMode);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable PCAP Tracing", pcapTracing);
  cmd.AddValue ("sectorNum", "Number of sectors in total", sectorNum);
  cmd.AddValue ("sta1x", "X axis position of station 1", sta1_xPos);
  cmd.AddValue ("sta1y", "Y axis position of station 1", sta1_yPos);
  cmd.AddValue ("sta2x", "X axis position of station 2", sta2_xPos);
  cmd.AddValue ("sta2y", "Y axis position of station 2", sta2_yPos);
  cmd.AddValue ("sta3x", "X axis position of station 3", sta3_xPos);
  cmd.AddValue ("sta3y", "Y axis position of station 3", sta3_yPos);
  cmd.AddValue ("radEfficiency", "radition efficiency (between 0 and 1)", radEfficiency);
  cmd.AddValue ("txPower", "transmit power in dBm", txPower);
  cmd.AddValue ("threshold", "CCA mode1 threshold", threshold);
  cmd.AddValue ("maxDelay", "maximum delay in millisecond", maxDelay);
  cmd.Parse (argc, argv);

  /* Global params: no fragmentation, no RTS/CTS, fixed rate for all packets */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("999999"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));
  Config::SetDefault ("ns3::WifiMacQueue::MaxPacketNumber", UintegerValue (queueSize));
  Config::SetDefault ("ns3::WifiMacQueue::MaxDelay", TimeValue (MilliSeconds (maxDelay)));

  /**** WifiHelper is a meta-helper: it helps creates helpers ****/
  WifiHelper wifi;

  /* Basic setup */
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ad);

  /* Turn on logging */
  if (verbose)
    {
      wifi.EnableLogComponents ();
      LogComponentEnable ("EvaluateServicePeriod", LOG_LEVEL_ALL);
    }

  /**** Set up Channel ****/
  YansWifiChannelHelper wifiChannel ;
  /* Simple propagation delay model */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* Friis model with standard-specific wavelength */
//  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (56.16e9));
  wifiChannel.AddPropagationLoss ("ns3::MmWavePropagationLossModel", "Frequency", DoubleValue (73e9));

  /**** Setup physical layer ****/
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  /* Nodes will be added to the channel we set up earlier */
  wifiPhy.SetChannel (wifiChannel.Create ());
  /* All nodes transmit at 10 dBm == 10 mW, no adaptation */
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPower));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPower));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  /* Sensitivity model includes implementation loss and noise figure */
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (3));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (threshold));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (threshold + 3));
  /* Set the phy layer error model */
//  wifiPhy.SetErrorRateModel ("ns3::SensitivityModel60GHz");
  wifiPhy.SetErrorRateModel ("ns3::ErrorRateModelSensitivityOFDM");

std::cout << "sally test phyMode: " << phyMode << std::endl;

  /* Set default algorithm for all nodes to be constant rate */
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "ControlMode", StringValue (phyMode),
                                                                "DataMode", StringValue (phyMode));
  /* Give all nodes directional antenna */
  wifiPhy.EnableAntenna (true, true);
  wifiPhy.SetAntenna ("ns3::DirectionalFlatTopAntenna",
                      "Sectors", UintegerValue (sectorNum),
                      "Antennas", UintegerValue (1),
                      "RadiationEfficiency", DoubleValue (radEfficiency));

  /* Make four nodes and set them up with the phy and the mac */
  NodeContainer wifiNodes;
  wifiNodes.Create (4);
  Ptr<Node> apNode = wifiNodes.Get (0);
  Ptr<Node> staFirstNode = wifiNodes.Get (1);
  Ptr<Node> staSecondNode = wifiNodes.Get (2);
  Ptr<Node> staThirdNode = wifiNodes.Get (3);

  /* Add a DMG upper mac */
  DmgWifiMacHelper wifiMac = DmgWifiMacHelper::Default ();

  /* Install DMG PCP/AP Node */
  Ssid ssid = Ssid ("test802.11ad");
  wifiMac.SetType ("ns3::DmgApWifiMac",
                   "Ssid", SsidValue(ssid),
                   "BE_MaxAmpduSize", UintegerValue (0),
                   "BE_MaxAmsduSize", UintegerValue (msduAggregationSize),
                   "SSSlotsPerABFT", UintegerValue (8), "SSFramesPerSlot", UintegerValue (sectorNum),
                   "BeaconInterval", TimeValue (MilliSeconds (100)),
                   "BeaconTransmissionInterval", TimeValue (MicroSeconds (800)),
                   "ATIDuration", TimeValue (MicroSeconds (1000)));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (wifiPhy, wifiMac, apNode);

  /* Install DMG STA Nodes */
  wifiMac.SetType ("ns3::DmgStaWifiMac",
                   "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false),
                   "BE_MaxAmpduSize", UintegerValue (0),
                   "BE_MaxAmsduSize", UintegerValue (msduAggregationSize));

  staDevices = wifi.Install (wifiPhy, wifiMac, NodeContainer (staFirstNode, staSecondNode, staThirdNode));

  /* Setting mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));   /* PCP/AP */
  positionAlloc->Add (Vector (sta1_xPos, sta1_yPos, 0.0));   /* first STA */
  positionAlloc->Add (Vector (sta2_xPos, sta2_yPos, 0.0));   /* second STA */
  positionAlloc->Add (Vector (sta3_xPos, sta3_yPos, 0.0));   /* third STA */

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
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* We do not want any ARP packets */
  PopulateArpCache ();

  /*** Install Applications ***/

  /* Install Simple UDP Server on both south and east Node */
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9999));
  ApplicationContainer sinks = sinkHelper.Install (NodeContainer (apNode, staFirstNode, staSecondNode, staThirdNode));

  /** East Node Variables **/
  uint64_t staApNodeLastTotalRx = 0;
  double staApNodeAverageThroughput = 0;

  uint64_t staFirstNodeLastTotalRx = 0;
  double staFirstNodeAverageThroughput = 0;

  uint64_t staSecondNodeLastTotalRx = 0;
  double staSecondNodeAverageThroughput = 0;

  uint64_t staThirdNodeLastTotalRx = 0;
  double staThirdNodeAverageThroughput = 0;

  /* STA1 STA2 and STA3 transmit to AP. */
  ApplicationContainer srcApp1;
  OnOffHelper src1 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
  src1.SetAttribute ("MaxBytes", UintegerValue (0));
  src1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  src1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
  src1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  src1.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  srcApp1 = src1.Install (staFirstNode);
  srcApp1.Start (Seconds (3.0));
  srcApp1.Stop (Seconds (3.5));

  ApplicationContainer srcApp2;
  OnOffHelper src2 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
  src2.SetAttribute ("MaxBytes", UintegerValue (0));
  src2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  src2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
  src2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  src2.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  srcApp2 = src2.Install (staSecondNode);
  srcApp2.Start (Seconds (3.5));
  srcApp2.Stop (Seconds (4.0));

  ApplicationContainer srcApp3;
  OnOffHelper src3 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
  src3.SetAttribute ("MaxBytes", UintegerValue (0));
  src3.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  src3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
  src3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  src3.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  srcApp3 = src3.Install (staThirdNode);
  srcApp3.Start (Seconds (4.0));
  srcApp3.Stop (Seconds (4.5));

  Simulator::Schedule (Seconds (3.1), &CalculateThroughput, StaticCast<PacketSink> (sinks.Get (0)),
                       staApNodeLastTotalRx, staApNodeAverageThroughput);

  Simulator::Schedule (Seconds (3.1), &CalculateThroughput, StaticCast<PacketSink> (sinks.Get (1)),
                       staFirstNodeLastTotalRx, staFirstNodeAverageThroughput);

  Simulator::Schedule (Seconds (3.1), &CalculateThroughput, StaticCast<PacketSink> (sinks.Get (2)),
                       staSecondNodeLastTotalRx, staSecondNodeAverageThroughput);

  Simulator::Schedule (Seconds (3.1), &CalculateThroughput, StaticCast<PacketSink> (sinks.Get (3)),
                       staThirdNodeLastTotalRx, staThirdNodeAverageThroughput);


  /* Enable Traces */
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("Traces/AccessPoint", apDevice, false);
      wifiPhy.EnablePcap ("Traces/staFirstNode", staDevices.Get (0), false);
      wifiPhy.EnablePcap ("Traces/staSecondNode", staDevices.Get (1), false);
      wifiPhy.EnablePcap ("Traces/staThirdNode", staDevices.Get (2), false);
    }

  /* Stations */
  apWifiNetDevice = StaticCast<WifiNetDevice> (apDevice.Get (0));
  staFirstWifiNetDevice = StaticCast<WifiNetDevice> (staDevices.Get (0));
  staSecondWifiNetDevice = StaticCast<WifiNetDevice> (staDevices.Get (1));
  staThirdWifiNetDevice = StaticCast<WifiNetDevice> (staDevices.Get (2));

  apWifiMac = StaticCast<DmgApWifiMac> (apWifiNetDevice->GetMac ());
  staFirstWifiMac = StaticCast<DmgStaWifiMac> (staFirstWifiNetDevice->GetMac ());
  staSecondWifiMac = StaticCast<DmgStaWifiMac> (staSecondWifiNetDevice->GetMac ());
  staThirdWifiMac = StaticCast<DmgStaWifiMac> (staThirdWifiNetDevice->GetMac ());

  /** Connect Traces **/
  staFirstWifiMac->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&StationAssoicated, staFirstWifiMac));
  staSecondWifiMac->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&StationAssoicated, staSecondWifiMac));
  staThirdWifiMac->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&StationAssoicated, staThirdWifiMac));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
