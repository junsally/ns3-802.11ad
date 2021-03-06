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
#include "math.h"

#define PI 3.141592653

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
  double simulationTime = 5.01;                   /* Simulation time in seconds. */
  bool pcapTracing = false;                     /* PCAP Tracing is enabled or not. */
  uint16_t sectorNum = 12;                        /* Number of sectors in total. */
  
  double sta1_xPos = 1;                         /* X axis position of station 1. */
  double sta1_yPos = 0;                         /* Y axis position of station 1. */
  double sta2_xPos = 0;                         /* X axis position of station 2. */
  double sta2_yPos = 1;                         /* Y axis position of station 2. */
  double sta3_xPos = 1;                         /* X axis position of station 3. */
  double sta3_yPos = 1;                         /* Y axis position of station 3. */
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
  uint64_t apNodeLastTotalRx = 0;
  double apNodeAverageThroughput = 0;

  uint64_t staFirstNodeLastTotalRx = 0;
  double staFirstNodeAverageThroughput = 0;

  uint64_t staSecondNodeLastTotalRx = 0;
  double staSecondNodeAverageThroughput = 0;

  uint64_t staThirdNodeLastTotalRx = 0;
  double staThirdNodeAverageThroughput = 0;

  /* calculate sector number */
  double positionArray[4][2] = {{0,0}, {sta1_xPos, sta1_yPos}, {sta2_xPos, sta2_yPos}, {sta3_xPos, sta3_yPos}};
  int selectedSEC[4][4] = {{0}};
  double deltaX = 0;
  double deltaY = 0;
  double beamwidthDegree = 360 / sectorNum;

  for (int m=0; m<4; m++)
  {
      for (int n=0; n<4; n++)
      {
          deltaX = positionArray[n][0] - positionArray[m][0];
          deltaY = positionArray[n][1] - positionArray[m][1];
          if ((deltaX == 0) && (deltaY == 0)) selectedSEC[m][n] = 0;
          else if ((deltaX == 0) && (deltaY > 0)) selectedSEC[m][n] = ceil(90/beamwidthDegree);
          else if ((deltaX == 0) && (deltaY < 0)) selectedSEC[m][n] = ceil(270/beamwidthDegree);
          else if ((deltaX > 0) && (deltaY == 0)) selectedSEC[m][n] = 1;
          else if ((deltaX < 0) && (deltaY == 0)) selectedSEC[m][n] = ceil(180/beamwidthDegree);
          else if ((deltaX > 0) && (deltaY > 0)) selectedSEC[m][n] = ceil(atan(deltaY/deltaX)*180/PI/beamwidthDegree);
          else if ((deltaX > 0) && (deltaY < 0)) selectedSEC[m][n] = ceil((360+atan(deltaY/deltaX)*180/PI)/beamwidthDegree);
          else selectedSEC[m][n] = ceil((180+atan(deltaY/deltaX)*180/PI)/beamwidthDegree);
      }
  }

  /* test selectedSEC */
  for (int m=0; m<4; m++)
  {
      for (int n=0; n<4; n++)
      {
          std::cout << "sally test selectedSEC: " << selectedSEC[m][n] << std::endl;
      }
  }
 
  /* tell which transmissions has the same best sector */
  int nonDirectSTA = 0;
  bool sameSector[4][3] = {{false}};
  bool isAllCapable[4] = {true, true, true, true};
 
  for (int m=0; m<4; m++)
  {
      if (m == 0)
      {
          if (selectedSEC[0][1] == selectedSEC[0][2]) {sameSector[0][0] = true; sameSector[0][1] = true; isAllCapable[0] = false;}
          if (selectedSEC[0][1] == selectedSEC[0][3]) {sameSector[0][0] = true; sameSector[0][2] = true; isAllCapable[0] = false;}
          if (selectedSEC[0][2] == selectedSEC[0][3]) {sameSector[0][1] = true; sameSector[0][2] = true; isAllCapable[0] = false;}
      }
      else if (m == 1)
      {
          if (selectedSEC[1][0] == selectedSEC[1][2]) {sameSector[1][0] = true; sameSector[1][1] = true; isAllCapable[1] = false;}
          if (selectedSEC[1][0] == selectedSEC[1][3]) {sameSector[1][0] = true; sameSector[1][2] = true; isAllCapable[1] = false;}
          if (selectedSEC[1][2] == selectedSEC[1][3]) {sameSector[1][1] = true; sameSector[1][2] = true; isAllCapable[1] = false;}
      }
      else if (m == 2)
      {
          if (selectedSEC[2][0] == selectedSEC[2][1]) {sameSector[2][0] = true; sameSector[2][1] = true; isAllCapable[2] = false;}
          if (selectedSEC[2][0] == selectedSEC[2][3]) {sameSector[2][0] = true; sameSector[2][2] = true; isAllCapable[2] = false;}
          if (selectedSEC[2][1] == selectedSEC[2][3]) {sameSector[2][1] = true; sameSector[2][2] = true; isAllCapable[2] = false;}
      }
      else
      {
          if (selectedSEC[3][0] == selectedSEC[3][1]) {sameSector[3][0] = true; sameSector[3][1] = true; isAllCapable[3] = false;}
          if (selectedSEC[3][0] == selectedSEC[3][2]) {sameSector[3][0] = true; sameSector[3][2] = true; isAllCapable[3] = false;}
          if (selectedSEC[3][1] == selectedSEC[3][2]) {sameSector[3][1] = true; sameSector[3][2] = true; isAllCapable[3] = false;}
      }

  }

  /* test sameSector */
  for (int m=0; m<4; m++)
  {
      std::cout << "sally test isAllCapable: " << isAllCapable[m] << std::endl;
      for (int n=0; n<3; n++)
      {
          std::cout << "sally test sameSector: " << sameSector[m][n] << std::endl;
      }
  }

  /* algorithm for transmission allocation*/
  int state = 1;
  int concurrentSTA = 0;

  for (int m=0; m<4; m++)
  {
      if (isAllCapable[m] == true) continue;
      else if ((sameSector[m][0] == true) && (sameSector[m][1] == true) && (sameSector[m][2] == true))
      {
          state = 3;
          break;
      }
      else if (sameSector[m][0] == false)
      {
          if ((m == 0) || (m == 1))
          {
              if ((isAllCapable[2] == true) && (isAllCapable[3] == true)) {state = 2; concurrentSTA = 1; break;}
              else {state = 3; break;}
          }
          else if (m == 2)
          {
              if ((isAllCapable[1] == true) && (isAllCapable[3] == true)) {state = 2; concurrentSTA = 2; break;}
              else {state = 3; break;}
          }
          else
          {
              if ((isAllCapable[1] == true) && (isAllCapable[2] == true)) {state = 2; concurrentSTA = 3; break;}
              else {state = 3; break;}
          }
      }
      else if (sameSector[m][1] == false)
      {
          if (m == 0)
          {
              if ((isAllCapable[1] == true) && (isAllCapable[3] == true)) {state = 2; concurrentSTA = 2; break;}
              else {state = 3; break;}
          }
          else if ((m == 1) || (m == 2))
          {
              if ((isAllCapable[0] == true) && (isAllCapable[3] == true)) {state = 2; concurrentSTA = 3; break;}
              else {state = 3; break;}
          }
          else
          {
              if ((isAllCapable[0] == true) && (isAllCapable[2] == true)) {state = 2; concurrentSTA = 2; break;}
              else {state = 3; break;}
          }
      }
      else if (sameSector[m][2] == false)
      {
          if (m == 0)
          {
              if ((isAllCapable[1] == true) && (isAllCapable[2] == true)) {state = 2; concurrentSTA = 3; break;}
              else {state = 3; break;}
          }
          else if (m == 1)
          {
              if ((isAllCapable[0] == true) && (isAllCapable[2] == true)) {state = 2; concurrentSTA = 2; break;}
              else {state = 3; break;}
          }
          else
          {
              if ((isAllCapable[0] == true) && (isAllCapable[1] == true)) {state = 2; concurrentSTA = 1; break;}
              else {state = 3; break;}
          }
      }
      else
          std::cout << "invalid state of isAllCapable and sameSector." << std::endl;
  }

std::cout << "sally test "<< "state=" << state << std::endl;

int deltaSEC = 0;
int delta1 = abs(selectedSEC[0][2]-selectedSEC[0][3]);
int delta2 = abs(selectedSEC[0][1]-selectedSEC[0][3]);
int delta3 = abs(selectedSEC[0][1]-selectedSEC[0][2]);
if (delta1 > 6) delta1 = 12 - delta1;
if (delta2 > 6) delta2 = 12 - delta2;
if (delta3 > 6) delta3 = 12 - delta3;

double distance1 = sqrt (pow(sta1_xPos,2) + pow(sta1_yPos,2));
double distance2 = sqrt (pow(sta2_xPos,2) + pow(sta2_yPos,2));
double distance3 = sqrt (pow(sta3_xPos,2) + pow(sta3_yPos,2));

std::cout << "sally test delta1=" << delta1 << ", delta2=" << delta2 << ", delta3=" << delta3 << std::endl;
std::cout << "sally test distance1=" << distance1 << ", distance2=" << distance2 << ", distance3=" << distance3 << std::endl;

/* allocate transmissions */
if (state == 1)
{
  if (deltaSEC < delta1) {deltaSEC=delta1; nonDirectSTA = 1;}
  if (deltaSEC < delta2) {deltaSEC=delta2; nonDirectSTA = 2;}
  if (deltaSEC < delta3) {deltaSEC=delta3; nonDirectSTA = 3;}

std::cout << "sally test nonDirectSTA=" << nonDirectSTA << ", deltaSEC=" << deltaSEC << std::endl;

  if (nonDirectSTA == 1) // STA1 do not directly transmit to AP
  {
      ApplicationContainer srcApp1;
      OnOffHelper src1 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
      src1.SetAttribute ("MaxBytes", UintegerValue (0));
      src1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src1.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp1 = src1.Install (staSecondNode);
      srcApp1.Start (Seconds (3.0));
      srcApp1.Stop (Seconds (4.0));

      ApplicationContainer srcApp2;
      OnOffHelper src2 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (2), 9999));
      src2.SetAttribute ("MaxBytes", UintegerValue (0));
      src2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src2.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp2 = src2.Install (staFirstNode);
      srcApp2.Start (Seconds (3.0));
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
      srcApp3.Stop (Seconds (5.0));

      ApplicationContainer srcApp4;
      OnOffHelper src4 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (1), 9999));
      src4.SetAttribute ("MaxBytes", UintegerValue (0));
      src4.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src4.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp4 = src4.Install (staFirstNode);
      srcApp4.Start (Seconds (4.0));
      srcApp4.Stop (Seconds (5.0));
  }

  else if (nonDirectSTA == 2) // STA2 do not directly transmit to AP
  {
      ApplicationContainer srcApp5;
      OnOffHelper src5 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
      src5.SetAttribute ("MaxBytes", UintegerValue (0));
      src5.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src5.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src5.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src5.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp5 = src5.Install (staFirstNode);
      srcApp5.Start (Seconds (3.0));
      srcApp5.Stop (Seconds (4.0));

      ApplicationContainer srcApp6;
      OnOffHelper src6 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (2), 9999));
      src6.SetAttribute ("MaxBytes", UintegerValue (0));
      src6.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src6.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src6.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src6.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp6 = src6.Install (staSecondNode);
      srcApp6.Start (Seconds (3.0));
      srcApp6.Stop (Seconds (4.0));

      ApplicationContainer srcApp7;
      OnOffHelper src7 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
      src7.SetAttribute ("MaxBytes", UintegerValue (0));
      src7.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src7.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src7.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src7.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp7 = src7.Install (staThirdNode);
      srcApp7.Start (Seconds (4.0));
      srcApp7.Stop (Seconds (5.0));

      ApplicationContainer srcApp8;
      OnOffHelper src8 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (0), 9999));
      src8.SetAttribute ("MaxBytes", UintegerValue (0));
      src8.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src8.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src8.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src8.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp8 = src8.Install (staSecondNode);
      srcApp8.Start (Seconds (4.0));
      srcApp8.Stop (Seconds (5.0));
  }

  else if (nonDirectSTA == 3) // STA3 do not directly transmit to AP
  {
      ApplicationContainer srcApp9;
      OnOffHelper src9 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
      src9.SetAttribute ("MaxBytes", UintegerValue (0));
      src9.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src9.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src9.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src9.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp9 = src9.Install (staFirstNode);
      srcApp9.Start (Seconds (3.0));
      srcApp9.Stop (Seconds (4.0));

      ApplicationContainer srcApp10;
      OnOffHelper src10 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (1), 9999));
      src10.SetAttribute ("MaxBytes", UintegerValue (0));
      src10.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src10.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src10.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src10.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp10 = src10.Install (staThirdNode);
      srcApp10.Start (Seconds (3.0));
      srcApp10.Stop (Seconds (4.0));

      ApplicationContainer srcApp11;
      OnOffHelper src11 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
      src11.SetAttribute ("MaxBytes", UintegerValue (0));
      src11.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src11.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src11.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src11.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp11 = src11.Install (staSecondNode);
      srcApp11.Start (Seconds (4.0));
      srcApp11.Stop (Seconds (5.0));

      ApplicationContainer srcApp12;
      OnOffHelper src12 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (0), 9999));
      src12.SetAttribute ("MaxBytes", UintegerValue (0));
      src12.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      src12.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
      src12.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      src12.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      srcApp12 = src12.Install (staThirdNode);
      srcApp12.Start (Seconds (4.0));
      srcApp12.Stop (Seconds (5.0));
  }
}


else if (state == 2)
{
   if (concurrentSTA == 1)
   {
       if (distance2 < distance3)
       {
            ApplicationContainer srcApp13;
            OnOffHelper src13 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src13.SetAttribute ("MaxBytes", UintegerValue (0));
            src13.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src13.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src13.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src13.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp13 = src13.Install (staFirstNode);
            srcApp13.Start (Seconds (3.0));
            srcApp13.Stop (Seconds (4.0));

            ApplicationContainer srcApp14;
            OnOffHelper src14 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (1), 9999));
            src14.SetAttribute ("MaxBytes", UintegerValue (0));
            src14.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src14.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src14.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src14.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp14 = src14.Install (staThirdNode);
            srcApp14.Start (Seconds (3.0));
            srcApp14.Stop (Seconds (4.0));

            ApplicationContainer srcApp15;
            OnOffHelper src15 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src15.SetAttribute ("MaxBytes", UintegerValue (0));
            src15.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src15.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src15.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src15.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp15 = src15.Install (staSecondNode);
            srcApp15.Start (Seconds (4.0));
            srcApp15.Stop (Seconds (5.0));
       }
       else
       {
            ApplicationContainer srcApp16;
            OnOffHelper src16 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src16.SetAttribute ("MaxBytes", UintegerValue (0));
            src16.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src16.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src16.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src16.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp16 = src16.Install (staFirstNode);
            srcApp16.Start (Seconds (3.0));
            srcApp16.Stop (Seconds (4.0));

            ApplicationContainer srcApp17;
            OnOffHelper src17 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (2), 9999));
            src17.SetAttribute ("MaxBytes", UintegerValue (0));
            src17.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src17.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src17.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src17.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp17 = src17.Install (staSecondNode);
            srcApp17.Start (Seconds (3.0));
            srcApp17.Stop (Seconds (4.0));

            ApplicationContainer srcApp18;
            OnOffHelper src18 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src18.SetAttribute ("MaxBytes", UintegerValue (0));
            src18.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src18.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src18.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src18.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp18 = src18.Install (staThirdNode);
            srcApp18.Start (Seconds (4.0));
            srcApp18.Stop (Seconds (5.0));
       }
   }
   else if (concurrentSTA == 2)
   {
       if (distance1 < distance3)
       {
            ApplicationContainer srcApp19;
            OnOffHelper src19 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src19.SetAttribute ("MaxBytes", UintegerValue (0));
            src19.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src19.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src19.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src19.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp19 = src19.Install (staSecondNode);
            srcApp19.Start (Seconds (3.0));
            srcApp19.Stop (Seconds (4.0));

            ApplicationContainer srcApp20;
            OnOffHelper src20 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (0), 9999));
            src20.SetAttribute ("MaxBytes", UintegerValue (0));
            src20.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src20.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src20.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src20.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp20 = src20.Install (staThirdNode);
            srcApp20.Start (Seconds (3.0));
            srcApp20.Stop (Seconds (4.0));

            ApplicationContainer srcApp21;
            OnOffHelper src21 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src21.SetAttribute ("MaxBytes", UintegerValue (0));
            src21.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src21.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src21.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src21.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp21 = src21.Install (staFirstNode);
            srcApp21.Start (Seconds (4.0));
            srcApp21.Stop (Seconds (5.0));
       }
       else
       {
            ApplicationContainer srcApp22;
            OnOffHelper src22 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src22.SetAttribute ("MaxBytes", UintegerValue (0));
            src22.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src22.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src22.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src22.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp22 = src22.Install (staSecondNode);
            srcApp22.Start (Seconds (3.0));
            srcApp22.Stop (Seconds (4.0));

            ApplicationContainer srcApp23;
            OnOffHelper src23 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (2), 9999));
            src23.SetAttribute ("MaxBytes", UintegerValue (0));
            src23.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src23.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src23.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src23.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp23 = src23.Install (staFirstNode);
            srcApp23.Start (Seconds (3.0));
            srcApp23.Stop (Seconds (4.0));

            ApplicationContainer srcApp24;
            OnOffHelper src24 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src24.SetAttribute ("MaxBytes", UintegerValue (0));
            src24.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src24.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src24.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src24.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp24 = src24.Install (staThirdNode);
            srcApp24.Start (Seconds (4.0));
            srcApp24.Stop (Seconds (5.0));
       }
   }

   else if (concurrentSTA == 3)
   {
       if (distance1 < distance2)
       {
            ApplicationContainer srcApp25;
            OnOffHelper src25 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src25.SetAttribute ("MaxBytes", UintegerValue (0));
            src25.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src25.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src25.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src25.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp25 = src25.Install (staThirdNode);
            srcApp25.Start (Seconds (3.0));
            srcApp25.Stop (Seconds (4.0));

            ApplicationContainer srcApp26;
            OnOffHelper src26 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (0), 9999));
            src26.SetAttribute ("MaxBytes", UintegerValue (0));
            src26.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src26.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src26.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src26.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp26 = src26.Install (staSecondNode);
            srcApp26.Start (Seconds (3.0));
            srcApp26.Stop (Seconds (4.0));

            ApplicationContainer srcApp27;
            OnOffHelper src27 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src27.SetAttribute ("MaxBytes", UintegerValue (0));
            src27.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src27.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src27.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src27.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp27 = src27.Install (staFirstNode);
            srcApp27.Start (Seconds (4.0));
            srcApp27.Stop (Seconds (5.0));
       }
       else
       {
            ApplicationContainer srcApp28;
            OnOffHelper src28 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src28.SetAttribute ("MaxBytes", UintegerValue (0));
            src28.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src28.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src28.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src28.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp28 = src28.Install (staThirdNode);
            srcApp28.Start (Seconds (3.0));
            srcApp28.Stop (Seconds (4.0));

            ApplicationContainer srcApp29;
            OnOffHelper src29 ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (1), 9999));
            src29.SetAttribute ("MaxBytes", UintegerValue (0));
            src29.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src29.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src29.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src29.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp29 = src29.Install (staFirstNode);
            srcApp29.Start (Seconds (3.0));
            srcApp29.Stop (Seconds (4.0));

            ApplicationContainer srcApp30;
            OnOffHelper src30 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
            src30.SetAttribute ("MaxBytes", UintegerValue (0));
            src30.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            src30.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
            src30.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            src30.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
            srcApp30 = src30.Install (staSecondNode);
            srcApp30.Start (Seconds (4.0));
            srcApp30.Stop (Seconds (5.0));
       }
   }
   else
       std::cout << "invalid concurrentSTA state." << std::endl; 
}


else if (state == 3)
{ 
    ApplicationContainer srcApp31;
    OnOffHelper src31 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
    src31.SetAttribute ("MaxBytes", UintegerValue (0));
    src31.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    src31.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
    src31.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    src31.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
    srcApp31 = src31.Install (staFirstNode);
    srcApp31.Start (Seconds (3.0));
    srcApp31.Stop (Seconds (3.5));

    ApplicationContainer srcApp32;
    OnOffHelper src32 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
    src32.SetAttribute ("MaxBytes", UintegerValue (0));
    src32.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    src32.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
    src32.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    src32.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
    srcApp32 = src32.Install (staSecondNode);
    srcApp32.Start (Seconds (3.5));
    srcApp32.Stop (Seconds (4.0));

    ApplicationContainer srcApp33;
    OnOffHelper src33 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), 9999));
    src33.SetAttribute ("MaxBytes", UintegerValue (0));
    src33.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    src33.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
    src33.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    src33.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
    srcApp33 = src33.Install (staThirdNode);
    srcApp33.Start (Seconds (4.0));
    srcApp33.Stop (Seconds (4.5));
}



  /* Schedule Throughput Calulcations */
  Simulator::Schedule (Seconds (3.1), &CalculateThroughput, StaticCast<PacketSink> (sinks.Get (0)),
                       apNodeLastTotalRx, apNodeAverageThroughput);

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
