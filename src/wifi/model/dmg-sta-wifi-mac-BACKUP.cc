/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"

#include "amsdu-subframe-header.h"
#include "dcf-manager.h"
#include "dmg-capabilities.h"
#include "dmg-sta-wifi-mac.h"
#include "ext-headers.h"
#include "mgt-headers.h"
#include "mac-low.h"
#include "msdu-aggregator.h"
#include "wifi-mac-header.h"
#include "random-stream.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DmgStaWifiMac");

NS_OBJECT_ENSURE_REGISTERED (DmgStaWifiMac);

TypeId
DmgStaWifiMac::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DmgStaWifiMac")
    .SetParent<DmgWifiMac> ()
    .AddConstructor<DmgStaWifiMac> ()
    .AddAttribute ("ProbeRequestTimeout", "The interval between two consecutive probe request attempts.",
                    TimeValue (Seconds (0.05)),
                    MakeTimeAccessor (&DmgStaWifiMac::m_probeRequestTimeout),
                    MakeTimeChecker ())
    .AddAttribute ("AssocRequestTimeout", "The interval between two consecutive assoc request attempts.",
                   TimeValue (Seconds (0.5)),
                   MakeTimeAccessor (&DmgStaWifiMac::m_assocRequestTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MaxLostBeacons",
                   "Maximum Number of Lost Beacons.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&DmgStaWifiMac::m_maxLostBeacons),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ActiveProbing",
                   "If true, we send probe requests. If false, we don't."
                   "NOTE: if more than one STA in your simulation is using active probing, "
                   "you should enable it at a different simulation time for each STA, "
                   "otherwise all the STAs will start sending probes at the same time resulting in collisions."
                   "See bug 1060 for more info.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&DmgStaWifiMac::SetActiveProbing, &DmgStaWifiMac::GetActiveProbing),
                   MakeBooleanChecker ())

    /* A-BFT Attributes */
    .AddAttribute ("RSSRetryLimit", "Responder Sector Sweep Retry Limit.",
                   UintegerValue (dot11RSSRetryLimit),
                   MakeUintegerAccessor (&DmgStaWifiMac::m_rssAttemptsLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RSSBackoff", "Maximum Responder Sector Sweep Backoff value.",
                   UintegerValue (dot11RSSBackoff),
                   MakeUintegerAccessor (&DmgStaWifiMac::m_rssBackoffLimit),
                   MakeUintegerChecker<uint32_t> ())

    /* DMG Relay Capabilities */
    .AddAttribute ("RDSDuplexMode", "0 = HD-DF, 1 = FD-AF.",
                    BooleanValue (false),
                    MakeBooleanAccessor (&DmgStaWifiMac::m_rdsDuplexMode),
                    MakeBooleanChecker ())
    .AddAttribute ("RDSLinkChangeInterval", "In MicroSeconds",
                    UintegerValue (200),
                    MakeUintegerAccessor (&DmgStaWifiMac::m_relayLinkChangeInterval),
                    MakeUintegerChecker<uint8_t> (1, std::numeric_limits<uint8_t>::max ()))
    .AddAttribute ("RDSDataSensingTime", "In MicroSeconds. By default, it is set to SIFS plus SBIFS.",
                    UintegerValue (4),
                    MakeUintegerAccessor (&DmgStaWifiMac::m_relayDataSensingTime),
                    MakeUintegerChecker<uint8_t> (1, std::numeric_limits<uint8_t>::max ()))

    .AddTraceSource ("Assoc", "Associated with an access point.",
                     MakeTraceSourceAccessor (&DmgStaWifiMac::m_assocLogger),
                     "ns3::Mac48Address::TracedCallback")
    .AddTraceSource ("DeAssoc", "Association with an access point lost.",
                     MakeTraceSourceAccessor (&DmgStaWifiMac::m_deAssocLogger),
                     "ns3::Mac48Address::TracedCallback")

    /* Relay Procedure Related Traces */
    .AddTraceSource ("ChannelReportReceived", "The DMG STA has received a channel report.",
                     MakeTraceSourceAccessor (&DmgStaWifiMac::m_channelReportReceived),
                     "ns3::Mac48Address::TracedCallback")
    .AddTraceSource ("TransmissionLinkChanged", "The current transmission link has been changed.",
                     MakeTraceSourceAccessor (&DmgStaWifiMac::m_transmissionLinkChanged),
                     "ns3::DmgStaWifiMac::TransmissionLinkChangedTracedCallback")
    .AddTraceSource ("BeamLinkMaintenanceTimerExpired",
                     "The BeamLink maintenance timer associated to a link has expired.",
                     MakeTraceSourceAccessor (&DmgStaWifiMac::m_beamLinkMaintenanceTimerExpired),
                     "ns3::DmgStaWifiMac::BeamLinkMaintenanceTimerExpiredTracedCallback")
  ;
  return tid;
}

DmgStaWifiMac::DmgStaWifiMac ()
  : m_probeRequestEvent (),
    m_assocRequestEvent (),
    m_beaconWatchdogEnd (Seconds (0.0)),
    m_abftEvent (),
    m_linkChangeInterval ()
{
  NS_LOG_FUNCTION (this);
  m_state = BEACON_MISSED;
  a_bftSlot = CreateObject<UniformRandomVariable> ();

  /* RSS Backoff Random Variables */
  m_rssBackoffVariable = CreateObject<UniformRandomVariable> ();
  m_rssBackoffVariable->SetAttribute ("Min", DoubleValue (0));
  m_rssBackoffVariable->SetAttribute ("Max", DoubleValue (m_rssBackoffLimit));
  m_failedRssAttemptsCounter = 0;
  m_rssBackoffRemaining = 0;
  m_nextBeacon = 0;

  /* Relay Variables */
  m_relayMode = false;
  m_sp->SetMissedAckCallback (MakeCallback (&DmgStaWifiMac::MissedAck, this));

  /* Let the lower layers know that we are acting as a non-AP DMG STA in an infrastructure BSS. */
  SetTypeOfStation (DMG_STA);
}

DmgStaWifiMac::~DmgStaWifiMac ()
{
  NS_LOG_FUNCTION(this);
}

void
DmgStaWifiMac::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> DoInitialize" << std::endl;
  /* Initialize DMG STA and start Beacon Interval */
  DmgWifiMac::DoInitialize ();
  StartBeaconInterval ();
}

void
DmgStaWifiMac::DoDispose ()
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> DoDispose" << std::endl;
  DmgWifiMac::DoDispose ();
}

void
DmgStaWifiMac::SetWifiRemoteStationManager (Ptr<WifiRemoteStationManager> stationManager)
{
  NS_LOG_FUNCTION (this << stationManager);
std::cout << "sally test dmgstamac -> SetWifiRemoteStationManager" << std::endl;
  DmgWifiMac::SetWifiRemoteStationManager (stationManager);
}

void
DmgStaWifiMac::SetMaxLostBeacons (uint32_t lost)
{
  NS_LOG_FUNCTION (this << lost);
std::cout << "sally test dmgstamac -> SetMaxLostBeacons, lost=" << lost << std::endl;
  m_maxLostBeacons = lost;
}

void
DmgStaWifiMac::SetProbeRequestTimeout (Time timeout)
{
  NS_LOG_FUNCTION (this << timeout);
std::cout << "sally test dmgstamac -> SetProbeRequestTimeout, timeout=" << timeout << std::endl;
  m_probeRequestTimeout = timeout;
}

void
DmgStaWifiMac::SetAssocRequestTimeout (Time timeout)
{
  NS_LOG_FUNCTION (this << timeout);
std::cout << "sally test dmgstamac -> SetAssocRequestTimeout, timeout=" << timeout << std::endl;
  m_assocRequestTimeout = timeout;
}

void
DmgStaWifiMac::StartActiveAssociation (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> StartActiveAssociation" << std::endl;
  TryToEnsureAssociated ();
}

void
DmgStaWifiMac::SetActiveProbing (bool enable)
{
  NS_LOG_FUNCTION(this << enable);
std::cout << "sally test dmgstamac -> SetActiveProbing, enable=" << enable << std::endl;
  if (enable)
    {
      Simulator::ScheduleNow (&DmgStaWifiMac::TryToEnsureAssociated, this);
    }
  else
    {
      m_probeRequestEvent.Cancel ();
    }
  m_activeProbing = enable;
}
  
bool
DmgStaWifiMac::GetActiveProbing (void) const
{
std::cout << "sally test dmgstamac -> GetActiveProbing, m_activeProbing=" << m_activeProbing << std::endl;
  return m_activeProbing;
}

void
DmgStaWifiMac::SendProbeRequest (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> SendProbeRequest" << std::endl;
  WifiMacHeader hdr;
  hdr.SetProbeReq ();
  hdr.SetAddr1 (Mac48Address::GetBroadcast ());
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (Mac48Address::GetBroadcast ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  Ptr<Packet> packet = Create<Packet> ();
  MgtProbeRequestHeader probe;
  probe.SetSsid (GetSsid ());

  packet->AddHeader (probe);

  // The standard is not clear on the correct queue for management
  // frames if we are a QoS AP. The approach taken here is to always
  // use the DCF for these regardless of whether we have a QoS
  // association or not.
  m_dca->Queue(packet, hdr);

  if (m_probeRequestEvent.IsRunning())
    {
      m_probeRequestEvent.Cancel();
    }
  m_probeRequestEvent = Simulator::Schedule(m_probeRequestTimeout, &DmgStaWifiMac::ProbeRequestTimeout, this);
}

void
DmgStaWifiMac::SendAssociationRequest (void)
{
std::cout << "sally test dmgstamac -> SendAssociationRequest" << std::endl;
  NS_LOG_FUNCTION (this << GetBssid ());
  WifiMacHeader hdr;
  hdr.SetAssocReq ();
  hdr.SetAddr1 (GetBssid ());
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  Ptr<Packet> packet = Create<Packet> ();
  MgtAssocRequestHeader assoc;
  assoc.SetSsid (GetSsid ());
  assoc.AddWifiInformationElement (GetDmgCapabilities ());
  assoc.AddWifiInformationElement (GetMultiBandElement ());
  assoc.AddWifiInformationElement (GetRelayCapabilitiesElement ());
  packet->AddHeader (assoc);

  // The standard is not clear on the correct queue for management
  // frames if we are a QoS AP. The approach taken here is to always
  // use the DCF for these regardless of whether we have a QoS
  // association or not.
  m_dca->Queue (packet, hdr);

  if (m_assocRequestEvent.IsRunning ())
    {
      m_assocRequestEvent.Cancel ();
    }

  /* For now, we assume station talks to the DMG AP only */
  SteerAntennaToward (GetBssid ());

  m_assocRequestEvent = Simulator::Schedule (m_assocRequestTimeout, &DmgStaWifiMac::AssocRequestTimeout, this);
}

void 
DmgStaWifiMac::TryToEnsureAssociated (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> TryToEnsureAssociated" << std::endl;
  switch (m_state)
  {
  case ASSOCIATED:
    return;
    break;

  case WAIT_PROBE_RESP:
    /* we have sent a probe request earlier so we
     do not need to re-send a probe request immediately.
     We just need to wait until probe-request-timeout
     or until we get a probe response
    */
    break;

  case BEACON_MISSED:
    /* we were associated but we missed a bunch of beacons
    * so we should assume we are not associated anymore.
    * We try to initiate a probe request now.
    */
    m_linkDown ();
    if (m_activeProbing)
      {
        SetState (WAIT_PROBE_RESP);
        SendProbeRequest ();
      }
    break;

  case WAIT_ASSOC_RESP:
    /* we have sent an assoc request so we do not need to
     re-send an assoc request right now. We just need to
     wait until either assoc-request-timeout or until
     we get an assoc response.
    */
    break;

  case REFUSED:
    /* we have sent an assoc request and received a negative
     assoc resp. We wait until someone restarts an
     association with a given ssid.
    */
    break;
  }
}

void
DmgStaWifiMac::AssocRequestTimeout (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> AssocRequestTimeout" << std::endl;
  SetState (WAIT_ASSOC_RESP);
  SendAssociationRequest ();
}

uint16_t
DmgStaWifiMac::GetAssociationID (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> GetAssociationID" << std::endl;
  if (m_state == ASSOCIATED)
    {
      return m_aid;
    }
  else
    {
      return 0;
    }
}

void
DmgStaWifiMac::CreateAllocation (Mac48Address to, DmgTspecElement &elem)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> CreateAllocation, to=" << to << ", elem=" << elem << std::endl;
  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  DmgAddTSRequestFrame frame;
  frame.SetDmgTspecElement (elem);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.qos = WifiActionHeader::ADDTS_REQUEST;
  actionHdr.SetAction (WifiActionHeader::QOS, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (frame);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::ProbeRequestTimeout (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> ProbeRequestTimeout" << std::endl;
  SetState (WAIT_PROBE_RESP);
  SendProbeRequest ();
}

void
DmgStaWifiMac::MissedBeacons (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> MissedBeacons" << std::endl;
  if (m_beaconWatchdogEnd > Simulator::Now ())
  {
    if (m_beaconWatchdog.IsRunning ())
      {
        m_beaconWatchdog.Cancel ();
      }
    m_beaconWatchdog = Simulator::Schedule (m_beaconWatchdogEnd - Simulator::Now (), &DmgStaWifiMac::MissedBeacons, this);
    return;
  }
  NS_LOG_DEBUG ("beacon missed");
  SetState (BEACON_MISSED);
  TryToEnsureAssociated ();
}

void
DmgStaWifiMac::RestartBeaconWatchdog (Time delay)
{
  NS_LOG_FUNCTION (this << delay);
std::cout << "sally test dmgstamac -> RestartBeaconWatchdog, delay=" << delay << std::endl;
  m_beaconWatchdogEnd = std::max (Simulator::Now () + delay, m_beaconWatchdogEnd);
  if (Simulator::GetDelayLeft (m_beaconWatchdog) < delay && m_beaconWatchdog.IsExpired ())
    {
      NS_LOG_DEBUG ("Restart watchdog.");
      m_beaconWatchdog = Simulator::Schedule (delay, &DmgStaWifiMac::MissedBeacons, this);
    }
}

bool
DmgStaWifiMac::IsAssociated (void) const
{
std::cout << "sally test dmgstamac -> IsAssociated" << std::endl;
  return m_state == ASSOCIATED;
}

bool
DmgStaWifiMac::IsWaitAssocResp (void) const
{
std::cout << "sally test dmgstamac -> IsWaitAssocResp" << std::endl;
  return m_state == WAIT_ASSOC_RESP;
}

void
DmgStaWifiMac::ForwardDataFrame (WifiMacHeader hdr, Ptr<Packet> packet, Mac48Address destAddress)
{
  NS_LOG_FUNCTION (this << packet << destAddress);
std::cout << "sally test dmgstamac -> ForwardDataFrame, hdr=" << hdr << ", destAddress=" << destAddress << std::endl;
  hdr.SetAddr1 (destAddress);
  hdr.SetAddr2 (GetAddress ());
  if (hdr.IsQosAmsdu ())
    {
      hdr.SetType (WIFI_MAC_QOSDATA);
      hdr.SetQosAckPolicy (WifiMacHeader::NORMAL_ACK);
      hdr.SetQosNoAmsdu ();
      MsduAggregator::DeaggregatedMsdus packets = MsduAggregator::Deaggregate (packet);
      for (MsduAggregator::DeaggregatedMsdusCI i = packets.begin (); i != packets.end (); ++i)
        {
          m_sp->Queue ((*i).first, hdr);
        }
    }
  else
    {
      m_sp->Queue (packet, hdr);
    }
}

void
DmgStaWifiMac::Enqueue (Ptr<const Packet> packet, Mac48Address to)
{
  NS_LOG_FUNCTION (this << packet << to);
std::cout << "sally test dmgstamac -> Enqueue, to=" << to << std::endl;

  if (!IsAssociated ())
    {
      NotifyTxDrop (packet);
      TryToEnsureAssociated ();
      return;
    }
  WifiMacHeader hdr;

  // If we are not a QoS AP then we definitely want to use AC_BE to
  // transmit the packet. A TID of zero will map to AC_BE (through \c
  // QosUtilsMapTidToAc()), so we use that as our default here.
  uint8_t tid = 0;

  /* The QoS Data and QoS Null subtypes are the only Data subtypes transmitted by a DMG STA. */
  hdr.SetType (WIFI_MAC_QOSDATA);
  hdr.SetQosAckPolicy (WifiMacHeader::NORMAL_ACK);
  hdr.SetQosNoEosp ();
  hdr.SetQosNoAmsdu ();
  // Transmission of multiple frames in the same TXOP is not
  // supported for now.
  hdr.SetQosTxopLimit (0);
  // Fill in the QoS control field in the MAC header
  tid = QosUtilsGetTidForPacket (packet);
  // Any value greater than 7 is invalid and likely indicates that
  // the packet had no QoS tag, so we revert to zero, which'll
  // mean that AC_BE is used.
  if (tid > 7)
    {
      tid = 0;
    }
  hdr.SetQosTid (tid);
  /* DMG QoS Control*/
  hdr.SetQosRdGrant (m_supportRdp);
  /* The HT Control field is not present in frames transmitted by a DMG STA.
   * The presence of the HT Control field is determined by the Order
   * subfield of the Frame Control field, as specified in 8.2.4.1.10.*/
  hdr.SetNoOrder ();

  // Sanity check that the TID is valid
  NS_ASSERT (tid < 8);

  bool found = false;
  bool isCbap = true;
  AccessPeriodInformation accessPeriodInfo;
  for (DataForwardingTableIterator it = m_dataForwardingTable.begin (); it != m_dataForwardingTable.end (); it++)
    {
      accessPeriodInfo = it->second;
      if (it->first == to)
        {
          isCbap = accessPeriodInfo.isCbapPeriod;
          found = true;
          break;
        }
    }

  if (found)
    {
      hdr.SetAddr1 (accessPeriodInfo.nextHopAddress);
      hdr.SetAddr2 (GetAddress ());
      hdr.SetAddr3 (GetBssid ());
      hdr.SetDsNotTo ();
    }
  else
    {
      /* The DMG AP is our receiver */
      hdr.SetAddr1 (GetBssid ());
      hdr.SetAddr2 (GetAddress ());
      hdr.SetAddr3 (to);
      hdr.SetDsTo ();
    }
  hdr.SetDsNotFrom ();

  /* Check whether we should transmit in CBAP or SP */
  if (isCbap)
    {
      m_edca[QosUtilsMapTidToAc (tid)]->Queue (packet, hdr);
    }
  else
    {
      m_sp->Queue (packet, hdr);
    }
}

void
DmgStaWifiMac::AddForwardingEntry (Mac48Address nextHopAddress)
{
std::cout << "sally test dmgstamac -> AddForwardingEntry, nextHopAddress=" << nextHopAddress << std::endl;
  AccessPeriodInformation info;
  info.isCbapPeriod = true;
  info.nextHopAddress = nextHopAddress;
  m_dataForwardingTable[nextHopAddress] = info;
}

void
DmgStaWifiMac::StartBeaconInterval (void)
{
  NS_LOG_FUNCTION (this << "DMG AP Starting BI at " << Simulator::Now ());
std::cout << "sally test dmgstamac -> StartBeaconInterval" << std::endl;

  /* Disable Channel Access by CBAP */
  EndContentionPeriod ();

  /* Schedule the next period */
  if (m_nextBeacon == 0)
    {
      StartBeaconTransmissionInterval ();
    }
  else
    {
      /* We will not receive DMG Beacon during this BI */
      m_nextBeacon--;
      m_biStartTime = Simulator::Now ();
      if (m_atiPresent)
        {
          StartAnnouncementTransmissionInterval ();
          NS_LOG_DEBUG ("ATI for Station:" << GetAddress () << " is scheduled at " << Simulator::Now ());
        }
      else
        {
          StartDataTransmissionInterval ();
          NS_LOG_DEBUG ("DTI for Station:" << GetAddress () << " is scheduled at " << Simulator::Now ());
        }
    }
}

void
DmgStaWifiMac::StartBeaconTransmissionInterval (void)
{
  NS_LOG_FUNCTION (this << "DMG STA Starting BTI at " << Simulator::Now ());
std::cout << "sally test dmgstamac -> StartBeaconTransmissionInterval" << std::endl;
  m_accessPeriod = CHANNEL_ACCESS_BTI;

  /* Re-initialize variables since we expect to receive DMG Beacon */
  m_sectorFeedbackSent.clear ();

  /* At the begining of the BTI period, a DMG STA should stay in Omni receiving mode */
  m_phy->GetDirectionalAntenna ()->SetInOmniReceivingMode ();
}

void
DmgStaWifiMac::StartAssociationBeamformTraining (void)
{
  NS_LOG_FUNCTION (this << "DMG STA Starting A-BFT at " << Simulator::Now ());
  m_accessPeriod = CHANNEL_ACCESS_ABFT;
std::cout << "sally test dmgstamac -> StartAssociationBeamformingTraining, DMG STA starting A-BFT" << std::endl;

  /* Schedule access period after A-BFT */
  if (m_atiPresent)
    {
      Simulator::Schedule (m_abftDuration + m_mbifs, &DmgStaWifiMac::StartAnnouncementTransmissionInterval, this);
      NS_LOG_DEBUG ("ATI for Station:" << GetAddress () << " is scheduled at " << Simulator::Now () + m_abftDuration + m_mbifs);
    }
  else
    {
      Simulator::Schedule (m_abftDuration + m_mbifs, &DmgStaWifiMac::StartDataTransmissionInterval, this);
      NS_LOG_DEBUG ("DTI for Station:" << GetAddress () << " is scheduled at " << Simulator::Now () + m_abftDuration + m_mbifs);
    }
  m_slotOffset = 0;
  /* Do the actual association beamforming training */
  DoAssociationBeamformingTraining ();
}

void
DmgStaWifiMac::DoAssociationBeamformingTraining (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> DoAssociationBeamformingTraining" << std::endl;

  if ((m_rssBackoffRemaining == 0))
    {
      /* Choose a random SSW Slot to transmit SSW Frames in it */
      a_bftSlot->SetAttribute ("Min", DoubleValue (0));
      a_bftSlot->SetAttribute ("Max", DoubleValue (m_remainingSlotsPerABFT - 1));
      m_slotIndex = a_bftSlot->GetInteger ();

      Time rssTime = m_slotIndex * m_low->GetSectorSweepSlotTime (m_ssFramesPerSlot);
      Simulator::Schedule (rssTime, &DmgStaWifiMac::StartAbftResponderSectorSweep, this, GetBssid (), m_isResponderTXSS);
      NS_LOG_DEBUG ("Selected Sector Slot Index=" << uint (m_slotOffset + m_slotIndex)
                    << ", Start RSS at " << Simulator::Now () + rssTime);

      /* Update upper bound of slots */
      m_remainingSlotsPerABFT -= (m_slotIndex + 1);
      if (m_remainingSlotsPerABFT > 0)
        {
          /* Schedule SSW FBCK Timeout to detect a collision i.e. missing SSW-FBCK */
          Time timeout = (m_slotIndex + 1) * m_low->GetSectorSweepSlotTime (m_ssFramesPerSlot);
          NS_LOG_DEBUG ("Scheduled SSW-FBCK Timeout Event at " << Simulator::Now () + timeout);
          m_sswFbckTimeout = Simulator::Schedule (timeout, &DmgStaWifiMac::MissedSswFeedback, this);
          m_slotOffset += (m_slotIndex + 1);
        }
    }
  else
    {
      m_rssBackoffRemaining--;
    }
}

void
DmgStaWifiMac::MissedSswFeedback (void)
{
  NS_LOG_FUNCTION (this);
  /* Each STA maintains a counter, FailedRSSAttempts, of the consecutive number of times the STA initiates
   * RSS during A-BFTs but does not successfully receive an SSW-Feedback frame as a response. If
   * FailedRSSAttempts exceeds dot11RSSRetryLimit, the STA shall select a backoff count as a random integer
   * drawn from a uniform distribution [0, dot11RSSBackoff), i.e., 0 inclusive through dot11RSSBackoff
   * exclusive. The responder shall decrement the backoff count by one at the end of each A-BFT period in the
   * following beacon intervals. The responder may re-initiate RSS only during an A-BFT when the backoff
   * count becomes zero. The STA shall set FailedRSSAttempts to 0 upon successfully receiving an SSW-
   * Feedback frame during the A-BFT. */
std::cout << "sally test dmgstamac -> MissedSswFeedback" << std::endl;
  m_failedRssAttemptsCounter++;
  if (m_failedRssAttemptsCounter < m_rssAttemptsLimit)
    {
      DoAssociationBeamformingTraining ();
    }
  else
    {
      /* Extract random backoff for backing-off */
      m_rssBackoffRemaining = m_rssBackoffVariable->GetInteger ();
    }
}

void
DmgStaWifiMac::StartAnnouncementTransmissionInterval (void)
{
std::cout << "sally test dmgstamac -> StartAnnouncementTransmissionInterval, DMG STA Starting ATI at " << Simulator::Now () << std::endl;
  NS_LOG_FUNCTION (this << "DMG STA Starting ATI at " << Simulator::Now ());
  m_accessPeriod = CHANNEL_ACCESS_ATI;
  /* We started ATI Period we should stay in Omni Drectional waiting for packets */
  m_phy->GetDirectionalAntenna ()->SetInOmniReceivingMode ();
  Simulator::Schedule (m_atiDuration, &DmgStaWifiMac::StartDataTransmissionInterval, this);
  m_dmgAtiDca->InitiateAtiAccessPeriod (m_atiDuration);
}

void
DmgStaWifiMac::StartDataTransmissionInterval (void)
{
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, DMG STA Starting DTI at " << Simulator::Now () << std::endl;
  NS_LOG_FUNCTION (this << "DMG STA Starting DTI at " << Simulator::Now ());
  m_accessPeriod = CHANNEL_ACCESS_DTI;

  /* Initialize DMG Reception */
  m_receivedDmgBeacon = false;

  /* Schedule the beginning of the next BI interval */
  Time nextBeaconInterval = m_beaconInterval - (Simulator::Now () - m_biStartTime);
  Simulator::Schedule (nextBeaconInterval, &DmgStaWifiMac::StartBeaconInterval, this);
  NS_LOG_DEBUG ("Next Beacon Interval will start at " << Simulator::Now () + nextBeaconInterval);
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, Next Beacon Interval will start at " << Simulator::Now () + nextBeaconInterval << std::endl;

  /* Send Association Request if we are not assoicated */
  if (!IsAssociated ())
    {
      /* We allow normal DCA for access */
      SetState (WAIT_ASSOC_RESP);
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, is not associated, next step is SendAssociationRequest" << std::endl;
      SendAssociationRequest ();
    }

  /**
    * A STA shall not transmit within a CBAP unless at least one of the following conditions is met:
    * — The value of the CBAP Only field is equal to 1 and the value of the CBAP Source field is equal to 0
    *   within the DMG Parameters field of the DMG Beacon that allocates the CBAP
    * — The STA is a PCP/AP and the value of the CBAP Only field is equal to 1 and the value of the CBAP
    *   Source field is equal to 1 within the DMG Parameters field of the DMG Beacon that allocates the CBAP
    * — The value of the Source AID field of the CBAP is equal to the broadcast AID
    * — The STA’s AID is equal to the value of the Source AID field of the CBAP
    * — The STA’s AID is equal to the value of the Destination AID field of the CBAP
    */
  if (m_isCbapOnly && !m_isCbapSource)
    {
      NS_LOG_INFO ("CBAP allocation only in DTI");
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, CBAP allocation only in DTI" << std::endl;
      Simulator::ScheduleNow (&DmgStaWifiMac::StartContentionPeriod, this, BROADCAST_CBAP, nextBeaconInterval);
    }
  else
    {
      AllocationField field;
      for (AllocationFieldList::iterator iter = m_allocationList.begin (); iter != m_allocationList.end (); iter++)
        {
          field = (*iter);
          if (field.GetAllocationType () == SERVICE_PERIOD_ALLOCATION)
            {
              Time spStart = MicroSeconds (field.GetAllocationStart ());
              Time servicePeriodLength = MicroSeconds (field.GetAllocationBlockDuration ());
              NS_ASSERT_MSG (spStart + servicePeriodLength <= nextBeaconInterval,
                             "Allocation should not exceed DTI period.");
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, Allocation should not exceed DTI period." << std::endl;

              if (field.GetSourceAid () == m_aid)
                {
                  uint8_t destAid = field.GetDestinationAid ();
                  Mac48Address destAddress = m_aidMap[destAid];
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case1, destAid=" << destAid << ", destAddress=" << destAddress << std::endl;

                  if (field.GetBfControl ().IsBeamformTraining ())
                    {
                      Simulator::Schedule (spStart, &DmgStaWifiMac::StartBeamformingServicePeriod, this, destAid, destAddress,
                                           true, field.GetBfControl ().IsInitiatorTxss (), servicePeriodLength);
                    }
                  else
                    {
                      /* Add station to the list of stations we can communicate with directly in Service Period */
                      DataForwardingTableIterator forwardingIterator = m_dataForwardingTable.find (destAddress);
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case2, Add station to the list of stations we can communicate with directly in Service Period." << std::endl;

                      if (forwardingIterator == m_dataForwardingTable.end ())
                        {
                          NS_LOG_ERROR ("Did not perform Beamforing Training with " << destAddress);
                          continue;
                        }
                      else
                        {
                          forwardingIterator->second.isCbapPeriod = false;
                        }

                      /* We are the source REDS, check if this SP allocation is protected by an RDS */
                      REDS_PAIR redsPair = std::make_pair (field.GetSourceAid (), field.GetDestinationAid ());
                      RELAY_LINK_MAP_ITERATOR it = m_relayLinkMap.find (redsPair);
                      if (it != m_relayLinkMap.end ())
                        {
                          RELAY_LINK_INFO info = it->second;
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case3" << std::endl;

                          /* Schedule events related to the beginning and end of relay period */
                          Simulator::Schedule (spStart, &DmgStaWifiMac::InitiateRelayPeriods, this, info);
                          Simulator::Schedule (spStart + servicePeriodLength, &DmgStaWifiMac::EndRelayPeriods, this, redsPair);

                          /* Schedule events related to the intervals within the relay period */
                          if ((info.transmissionLink == DIRECT_LINK) && (info.rdsDuplexMode == 0))
                            {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case4" << std::endl;

                              /* Schedule the beginning of this service period */
                              Simulator::Schedule (spStart, &DmgStaWifiMac::StartServicePeriod,
                                                   this, field.GetAllocationID (), servicePeriodLength, destAid, destAddress, true);
                            }
                          else if (info.rdsDuplexMode == 1)
                            {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case5" << std::endl;

                              Simulator::Schedule (spStart, &DmgStaWifiMac::StartFullDuplexRelay,
                                                   this, field.GetAllocationID (), servicePeriodLength, destAid, destAddress, true);
                            }
                        }
                      else
                        {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case6" << std::endl;

                          /* No relay link has been established so schedule normal service period */
                          Simulator::Schedule (spStart, &DmgStaWifiMac::StartServicePeriod,
                                               this, field.GetAllocationID (), servicePeriodLength, destAid, destAddress, true);
                        }

                      Simulator::Schedule (spStart + servicePeriodLength, &DmgStaWifiMac::EndServicePeriod, this);
                    }
                }
              else if ((field.GetSourceAid () == AID_BROADCAST) && (field.GetDestinationAid () == AID_BROADCAST))
                {
                  /* The PCP/AP may create SPs in its beacon interval with the source and destination AID
                   * subfields within an Allocation field set to 255 to prevent transmissions during
                   * specific periods in the beacon interval. This period can used for Dynamic Allocation
                   * of service peridos (Polling) */
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case7, No transmission is allowed from " << field.GetAllocationStart () << " till " << field.GetAllocationBlockDuration () << std::endl;

                  NS_LOG_INFO ("No transmission is allowed from " << field.GetAllocationStart () <<
                               " till " << field.GetAllocationBlockDuration ());
                }
              else if ((field.GetDestinationAid () == m_aid) || (field.GetDestinationAid () == AID_BROADCAST))
                {
                  /* The STA identified by the Destination AID field in the Extended Schedule element
                   * should be in the receive state for the duration of the SP in order to receive
                   * transmissions from the source DMG STA. */
                  uint8_t sourceAid = field.GetSourceAid ();
                  Mac48Address sourceAddress = m_aidMap[sourceAid];
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case8, sourceAid=" << sourceAid << ", sourceAddress=" << sourceAddress << std::endl;

                  if (field.GetBfControl ().IsBeamformTraining ())
                    {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case9" << std::endl;

                      Simulator::Schedule (spStart, &DmgStaWifiMac::StartBeamformingServicePeriod, this, sourceAid, sourceAddress,
                                           false, field.GetBfControl ().IsResponderTxss (), servicePeriodLength);
                    }
                  else
                    {
                      /* Check if this SP allocation is protected by relay */
                      REDS_PAIR redsPair = std::make_pair (field.GetSourceAid (), field.GetDestinationAid ());
                      RELAY_LINK_MAP_ITERATOR it = m_relayLinkMap.find (redsPair);
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case10" << std::endl;

                      if (it != m_relayLinkMap.end ())
                        {
                          RELAY_LINK_INFO info = it->second;
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case11" << std::endl;

                          /* Schedule events related to the beginning and end of relay period */
                          Simulator::Schedule (spStart, &DmgStaWifiMac::InitiateRelayPeriods, this, info);
                          Simulator::Schedule (spStart + servicePeriodLength, &DmgStaWifiMac::EndRelayPeriods, this, redsPair);

                          /* Schedule Data Sensing Timeout to detect missing frame transmission */
                          Simulator::Schedule (MicroSeconds (m_relayDataSensingTime), &DmgStaWifiMac::RelayDataSensingTimeout, this);

                          /* Schedule events related to the intervals within the relay period */
                          if ((info.transmissionLink == DIRECT_LINK) && (info.rdsDuplexMode == 0))
                            {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case12" << std::endl;

                              Simulator::Schedule (spStart, &DmgStaWifiMac::StartServicePeriod, this,
                                                   field.GetAllocationID (), servicePeriodLength, sourceAid, sourceAddress, false);
                            }
                          else if (info.rdsDuplexMode == 1)
                            {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case13" << std::endl;

                              Simulator::Schedule (spStart, &DmgStaWifiMac::StartFullDuplexRelay, this,
                                                   field.GetAllocationID (), servicePeriodLength, sourceAid, sourceAddress, false);
                            }
                        }
                      else
                        {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case14" << std::endl;

                          Simulator::Schedule (spStart, &DmgStaWifiMac::StartServicePeriod, this,
                                               field.GetAllocationID (), servicePeriodLength, sourceAid, sourceAddress, false);
                        }
                    }
                }
              else if ((field.GetSourceAid () != m_aid) && (field.GetDestinationAid () != m_aid))
                {
                  /* Check if we protect this service period as an RDS */
                  REDS_PAIR redsPair = std::make_pair (field.GetSourceAid (), field.GetDestinationAid ());
                  RELAY_LINK_MAP_ITERATOR it = m_relayLinkMap.find (redsPair);
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case15" << std::endl;

                  if (it != m_relayLinkMap.end ())
                    {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case16" << std::endl;

                      RELAY_LINK_INFO info = it->second;
                      Simulator::Schedule (spStart, &DmgStaWifiMac::SwitchToRelayOpertionalMode, this);
                      Simulator::Schedule (spStart + servicePeriodLength, &DmgStaWifiMac::RelayOperationTimeout, this);

                      if (info.rdsDuplexMode == 1) // FD-AF
                        {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case17, Protecting the SP between by an RDS in FD-AF Mode: Source AID=" << info.srcRedsAid << " and Destination AID=" << info.dstRedsAid << std::endl;

                          NS_LOG_INFO ("Protecting the SP between by an RDS in FD-AF Mode: Source AID=" << info.srcRedsAid <<
                                       " and Destination AID=" << info.dstRedsAid);
                          ANTENNA_CONFIGURATION_TX antennaConfigTxSrc = m_bestAntennaConfig[info.srcRedsAddress].first;
                          ANTENNA_CONFIGURATION_TX antennaConfigTxDst = m_bestAntennaConfig[info.dstRedsAddress].first;
                          Simulator::Schedule (spStart, &WifiPhy::ActivateRdsOpereation, m_phy,
                                               antennaConfigTxSrc.first, antennaConfigTxSrc.second,
                                               antennaConfigTxDst.first, antennaConfigTxDst.second);
                          Simulator::Schedule (spStart + servicePeriodLength, &WifiPhy::SuspendRdsOperation, m_phy);
                        }
                    }
                }
            }
          else if ((field.GetAllocationType () == CBAP_ALLOCATION) &&
                  ((field.GetSourceAid () == AID_BROADCAST) || (field.GetSourceAid () == m_aid) || (field.GetDestinationAid () == m_aid)))

            {
std::cout << "sally test dmgstamac -> StartDataTransmissionInterval, case18" << std::endl;

              Time cbapEnd = MicroSeconds (field.GetAllocationStart ()) + MicroSeconds (field.GetAllocationBlockDuration ());
              /* Schedule two events for the beginning of the relay mode */
              Simulator::Schedule (MicroSeconds (field.GetAllocationStart ()), &DmgStaWifiMac::StartContentionPeriod, this,
                                   field.GetAllocationID () ,MicroSeconds (field.GetAllocationBlockDuration ()));
              Simulator::Schedule (cbapEnd, &DmgStaWifiMac::EndContentionPeriod, this);
            }
        }
    }
}

void
DmgStaWifiMac::InitiateRelayPeriods (RELAY_LINK_INFO &info)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> InitiateRelayPeriods" << std::endl;
  m_periodProtected = true;
  m_relayLinkInfo = info;
  /* Schedule peridos assoicated to the transmission link */
  if ((m_relayLinkInfo.transmissionLink == DIRECT_LINK) || (m_relayLinkInfo.rdsDuplexMode == 1))
    {
      m_linkChangeInterval = Simulator::Schedule (MicroSeconds (m_relayLinkInfo.relayLinkChangeInterval),
                                                  &DmgStaWifiMac::RelayLinkChangeIntervalTimeout, this);
    }
}

void
DmgStaWifiMac::EndRelayPeriods (REDS_PAIR &pair)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> EndRelayPeriods" << std::endl;
  m_periodProtected = false;
  /* Store information related to the relay operation mode */
  m_relayLinkMap[pair] = m_relayLinkInfo;
}

void
DmgStaWifiMac::RegisterRelaySelectorFunction (ChannelMeasurementCallback callback)
{
std::cout << "sally test dmgstamac -> RegisterRelaySelectorFunction" << std::endl;
  m_channelMeasurementCallback = callback;
}

void
DmgStaWifiMac::RelayLinkChangeIntervalTimeout (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> RelayLinkChangeIntervalTimeout, DMG STA Starting Link Change Interval at " << Simulator::Now () << std::endl;
  NS_LOG_INFO ("DMG STA Starting Link Change Interval at " << Simulator::Now ());
  if (m_relayLinkInfo.rdsDuplexMode == 1) // FD-AF
    {
      if (CheckTimeAvailabilityForPeriod (GetRemainingAllocationTime (), MicroSeconds (m_relayLinkInfo.relayLinkChangeInterval)))
        {
          /* Schedule the next Link Change Interval */
          m_linkChangeInterval = Simulator::Schedule (MicroSeconds (m_relayLinkInfo.relayLinkChangeInterval),
                                                      &DmgStaWifiMac::RelayLinkChangeIntervalTimeout, this);

          /* Schedule Data Sensing Timeout Event at the destination REDS */
          if (m_relayLinkInfo.dstRedsAid == m_aid)
            {
              m_relayDataExchanged = false;

              /* Schedule Data Sensing Timeout to detect missing frame transmission */
              Simulator::Schedule (MicroSeconds (m_relayLinkInfo.relayDataSensingTime),
                                   &DmgStaWifiMac::RelayDataSensingTimeout, this);
            }
          else if ((m_relayLinkInfo.switchTransmissionLink) && (m_relayLinkInfo.srcRedsAid == m_aid))
            {
              /* If the source REDS decides to change the link at the start of the following Link Change Interval period and
               * the Normal mode is used, the source REDS shall start its frame transmission after Data Sensing Time from
               * the start of the following Link Change Interval period. */
              m_relayLinkInfo.switchTransmissionLink = false;
              if (m_relayLinkInfo.transmissionLink == DIRECT_LINK)
                {
                  m_relayLinkInfo.transmissionLink = RELAY_LINK;
                }
              else
                {
                  m_relayLinkInfo.transmissionLink = RELAY_LINK;
                }
              m_transmissionLinkChanged (GetAddress (), m_relayLinkInfo.transmissionLink);
              SteerAntennaToward (m_relayLinkInfo.selectedRelayAddress);
              Simulator::Schedule (MicroSeconds (m_relayLinkInfo.relayDataSensingTime),
                                   &DmgStaWifiMac::ResumeServicePeriodTransmission, this);
            }
        }
    }
}

bool
DmgStaWifiMac::CheckTimeAvailabilityForPeriod (Time servicePeriodDuration, Time partialDuration)
{
std::cout << "sally test dmgstamac -> CheckTimeAvailabilityForPeriod, servicePeriodDuration=" << servicePeriodDuration << ", partialDuration=" << partialDuration << std::endl;
  Time remainingTime = servicePeriodDuration - partialDuration;
  return (remainingTime >= partialDuration);
}

void
DmgStaWifiMac::StartFullDuplexRelay (AllocationID allocationID, Time length,
                                     uint8_t peerAid, Mac48Address peerAddress, bool isSource)
{
  NS_LOG_FUNCTION (this << length << uint32_t (peerAid) << peerAddress << isSource);
std::cout << "sally test dmgstamac -> StartFullDuplexRelay, allocationID=" << allocationID << ", length=" << length << ", peerAid=" << peerAid << ", peerAddress=" << peerAddress << ", isSource=" << isSource << std::endl;
  m_currentAllocationID = allocationID;
  m_currentAllocation = SERVICE_PERIOD_ALLOCATION;
  m_currentAllocationLength = length;
  m_allocationStarted = Simulator::Now ();
  m_peerStationAid = peerAid;
  m_peerStationAddress = peerAddress;
  m_moreData = true;
  m_servicePeriodStartedCallback (GetAddress (), peerAddress);
  /* Check current transmission link */
  if (m_relayLinkInfo.transmissionLink == DIRECT_LINK)
    {
      SteerAntennaToward (peerAddress);
    }
  else if (m_relayLinkInfo.transmissionLink == RELAY_LINK)
    {
      SteerAntennaToward (m_relayLinkInfo.selectedRelayAddress);
    }
  m_sp->StartServicePeriod (allocationID, peerAddress, length);
  if (isSource)
    {
      m_sp->InitiateTransmission ();
    }
}

void
DmgStaWifiMac::SuspendRelayPeriod (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> SuspendRelayPeriod" << std::endl;
  m_sp->DisableChannelAccess ();
}

void
DmgStaWifiMac::MissedAck (const WifiMacHeader &hdr)
{
  NS_LOG_FUNCTION (this << hdr);
std::cout << "sally test dmgstamac -> MissedAck" << std::endl;
  if (m_periodProtected && (hdr.GetAddr1 () == m_relayLinkInfo.dstRedsAddress))
    {
      /* If a source REDS transmits a frame to the destination REDS via the direct link but does not receive an
       * expected ACK frame or BA frame from the destination REDS during a Link Change Interval period, the
       * source REDS should change the link used for frame transmission at the start of the following Link Change
       * Interval period and use the RDS to forward frames to the destination REDS. */
      m_relayLinkInfo.switchTransmissionLink = true;
      SuspendServicePeriodTransmission ();
    }
}

void
DmgStaWifiMac::RelayDataSensingTimeout (void)
{
  NS_LOG_FUNCTION (this << m_relayDataExchanged << m_dcfManager->IsReceiving () << m_moreData);
std::cout << "sally test dmgstamac -> RelayDataSensingTimeout, m_relayDataExchanged=" << m_relayDataExchanged << ", m_dcfManager->IsReceiving=" << m_dcfManager->IsReceiving () << ", m_moreData=" << m_moreData << std::endl;
  if (m_relayLinkInfo.rdsDuplexMode == 1) // FD-AF
    {
      if ((!m_relayDataExchanged) && (!m_dcfManager->IsReceiving ()) && m_moreData)
        {
          m_relayLinkInfo.switchTransmissionLink = true;
          if (m_relayLinkInfo.transmissionLink == DIRECT_LINK)
            {
              /* In the Normal mode, if the destination REDS does not receive a valid frame from the source REDS within
               * Data Sensing Time after the start of a Link Change Interval, the destination REDS shall immediately change
               * the link to attempt to receive frames from the source REDS through the RDS. If the More Data field in the
               * last frame received from the source REDS is equal to 0, then the destination REDS shall not switch to the
               * link in the next Link Change Interval period even if it does not receive a frame during the Data Sensing
               * Time. An example of frame transfer under Normal mode with FD-AF RDS is illustrated in Figure 9-77. */
              NS_LOG_DEBUG ("Destinations REDS did not receive frames during data sensing interval so switch to the relay link");
              m_relayLinkInfo.transmissionLink = RELAY_LINK;
              SteerAntennaToward (m_relayLinkInfo.selectedRelayAddress);
            }
          else
            {
              NS_LOG_DEBUG ("Destinations REDS did not receive frames during data sensing interval so switch to the direct link");
              m_relayLinkInfo.transmissionLink = DIRECT_LINK;
              SteerAntennaToward (m_relayLinkInfo.srcRedsAddress);
            }
          m_transmissionLinkChanged (GetAddress (), m_relayLinkInfo.transmissionLink);
        }
    }
}

void
DmgStaWifiMac::SwitchTransmissionLink (uint8_t srcAid, uint8_t dstAid)
{
  NS_LOG_FUNCTION (this << uint32_t (srcAid) << uint32_t (dstAid));
std::cout << "sally test dmgstamac -> SwitchTransmissionLink, srcAid=" << srcAid << ", dstAid=" << dstAid << std::endl;
  REDS_PAIR redsPair = std::make_pair (srcAid, dstAid);
  RELAY_LINK_MAP_ITERATOR it = m_relayLinkMap.find (redsPair);
  if (it != m_relayLinkMap.end ())
    {
      /* Check if we are currently in a service period being protected by an RDS */
      if (m_periodProtected && (srcAid == m_relayLinkInfo.srcRedsAid) && (dstAid == m_relayLinkInfo.dstRedsAid))
        {
          m_relayLinkInfo.switchTransmissionLink = true;
        }
      else
        {
          RELAY_LINK_INFO info = it->second;
          if (info.transmissionLink == DIRECT_LINK)
            {
              info.transmissionLink = RELAY_LINK;
            }
          else
            {
              info.transmissionLink = DIRECT_LINK;
            }
          it->second = info;
        }
    }
}

void
DmgStaWifiMac::SwitchToRelayOpertionalMode (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> SwitchToRelayOperationalMode" << std::endl;
  m_relayMode = true;
}

void
DmgStaWifiMac::RelayOperationTimeout (void)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> RelayOperationTimeout" << std::endl;
  m_relayMode = false;
}

void
DmgStaWifiMac::StartBeamformingServicePeriod (uint8_t peerAid, Mac48Address peerAddress, bool isInitiator, bool isTxss, Time length)
{
  NS_LOG_FUNCTION (this << uint (peerAid) << peerAddress << isInitiator << isTxss << length );
std::cout << "sally test dmgstamac -> StartBeamformingServicePeriod, peerAid=" << peerAid << ", peerAddress=" << peerAddress << ", isInitiator=" << isInitiator << ", isTxss=" << isTxss << ", length=" << length << std::endl;

  m_currentAllocation = SERVICE_PERIOD_ALLOCATION;
  m_currentAllocationLength = length;
  m_allocationStarted = Simulator::Now ();
  m_peerStationAid = peerAid;
  m_peerStationAddress = peerAddress;
  m_beamformingTxss = isTxss;
  NS_LOG_INFO ("DMG STA Initiating Beamforming with " << peerAddress << " at " << Simulator::Now ());
  if (isInitiator)
    {
      StartInitiatorSectorSweep (peerAddress, isTxss);
    }
  else
    {
      /* We are the responder and we should stay in Quasi Omni Receiving Mode */
      m_phy->GetDirectionalAntenna ()->SetInOmniReceivingMode ();
    }
}

void
DmgStaWifiMac::StartInitiatorSectorSweep (Mac48Address address, bool isTxss)
{
  NS_LOG_FUNCTION (this << address << isTxss);
  NS_LOG_INFO ("DMG STA Starting ISS with " << address << " at " << Simulator::Now ());
std::cout << "sally test dmgstamac -> StartInitiatorSectorSweep, address=" << address << ", isTxss=" << isTxss << std::endl;

  m_isIssInitiator = true;
  if (isTxss)
    {
      StartTransmitSectorSweep (address, BeamformingInitiator);
    }
  else
    {
      StartReceiveSectorSweep (address, BeamformingInitiator);
    }
}

void
DmgStaWifiMac::StartResponderSectorSweep (Mac48Address address, bool isTxss)
{
  NS_LOG_FUNCTION (this << address << isTxss);
  NS_LOG_INFO ("DMG STA Starting RSS at " << Simulator::Now ());
std::cout << "sally test dmgstamac -> StartResponderSectorSweep, address=" << address << ", isTxss=" << isTxss << std::endl;

  m_isIssInitiator = false;
  /* Obtain antenna configuration for the highest received SNR from the DMG AP to feed it back */
  m_feedbackAntennaConfig = GetBestAntennaConfiguration (address, true);

  if (isTxss)
    {
      StartTransmitSectorSweep (address, BeamformingResponder);
    }
  else
    {
      /* The initiator is switching receive antennas at the same time. */
      m_phy->GetDirectionalAntenna ()->SetInOmniReceivingMode ();
      StartReceiveSectorSweep (address, BeamformingResponder);
    }
}

void
DmgStaWifiMac::StartAbftResponderSectorSweep (Mac48Address address, bool isTxss)
{
  NS_LOG_FUNCTION (this << address << isTxss);
std::cout << "sally test dmgstamac -> StartAbftResponderSectorSweep" << std::endl;

  m_allocationStarted = Simulator::Now ();
  m_currentAllocationLength =  m_low->GetSectorSweepDuration (m_ssFramesPerSlot);
  StartResponderSectorSweep (address, isTxss);
}

void
DmgStaWifiMac::StartTransmitSectorSweep (Mac48Address address, BeamformingDirection direction)
{
  NS_LOG_FUNCTION (this << address << direction);
  NS_LOG_INFO ("DMG STA Starting TxSS at " << Simulator::Now ());
std::cout << "sally test dmgstamac -> StartTransmitSectorSweep, address=" << address << ", direction=" << direction << std::endl;

  m_sectorId = 1;
  m_antennaId = 1;
  m_totalSectors = m_phy->GetDirectionalAntenna ()->GetNumberOfSectors () *
                   m_phy->GetDirectionalAntenna ()->GetNumberOfAntennas () - 1;

  if (direction == BeamformingInitiator)
    {
      Simulator::ScheduleNow (&DmgStaWifiMac::SendIssSectorSweepFrame, this, address,
                              direction, m_sectorId, m_antennaId, m_totalSectors);
    }
  else if (direction == BeamformingResponder)
    {
      Simulator::ScheduleNow (&DmgStaWifiMac::SendSectorSweepFrame, this, address,
                              direction, m_sectorId, m_antennaId, m_totalSectors);
    }
}

void
DmgStaWifiMac::StartReceiveSectorSweep (Mac48Address address, BeamformingDirection direction)
{
  NS_LOG_FUNCTION (this << address << direction);
  NS_LOG_INFO ("DMG STA Starting RxSS at " << Simulator::Now ());
std::cout << "sally test dmgstamac -> StartReceiveSectorSweep, address=" << address << ", direction=" << direction << std::endl;

}

void
DmgStaWifiMac::SendIssSectorSweepFrame (Mac48Address address, BeamformingDirection direction,
                                        uint8_t sectorID, uint8_t antennaID,  uint16_t count)
{
std::cout << "sally test dmgstamac -> SendIssSectorSweepFrame, address=" << address << ", direction=" << direction << ", sectorID=" << sectorID << ", antennaID=" << antennaID << ", count=" << count << std::endl;

  WifiMacHeader hdr;
  hdr.SetType (WIFI_MAC_CTL_DMG_SSW);

  /* Header Duration*/
  hdr.SetDuration (GetRemainingAllocationTime ());

  /* Other Fields */
  hdr.SetAddr1 (address);           // MAC address of the STA that is the intended receiver of the sector sweep.
  hdr.SetAddr2 (GetAddress ());     // MAC address of the transmitter STA of the sector sweep frame.
  hdr.SetNoMoreFragments ();
  hdr.SetNoRetry ();

  Ptr<Packet> packet = Create<Packet> ();
  CtrlDMG_SSW sswFrame;

  DMG_SSW_Field ssw;
  ssw.SetDirection (direction);
  ssw.SetCountDown (count);
  ssw.SetSectorID (sectorID);
  ssw.SetDMGAntennaID (antennaID);

  DMG_SSW_FBCK_Field sswFeedback;
  sswFeedback.IsPartOfISS (true);
  sswFeedback.SetSector (m_totalSectors);
  sswFeedback.SetDMGAntenna (m_phy->GetDirectionalAntenna ()->GetNumberOfAntennas ());
  sswFeedback.SetPollRequired (false);

  /* Set the fields in SSW Frame */
  sswFrame.SetSswField (ssw);
  sswFrame.SetSswFeedbackField (sswFeedback);
  packet->AddHeader (sswFrame);

  /* Set Antenna Direction */
  m_phy->GetDirectionalAntenna ()->SetCurrentTxSectorID (sectorID);
  m_phy->GetDirectionalAntenna ()->SetCurrentTxAntennaID (antennaID);

  NS_LOG_INFO ("Sending SSW Frame " << Simulator::Now () << " with "
               << uint32_t (m_sectorId) << " " << uint32_t (m_antennaId));

  /* Send Control Frames directly without DCA + DCF Manager */
  MacLowTransmissionParameters params;
  params.EnableOverrideDurationId (hdr.GetDuration ());
  params.DisableRts ();
  params.DisableAck ();
  params.DisableNextData ();
  m_low->StartTransmission (packet,
                            &hdr,
                            params,
                            MakeCallback (&DmgStaWifiMac::FrameTxOk, this));
}

void
DmgStaWifiMac::SendRssSectorSweepFrame (Mac48Address address, BeamformingDirection direction,
                                        uint8_t sectorID, uint8_t antennaID,  uint16_t count)
{
  WifiMacHeader hdr;
  hdr.SetType (WIFI_MAC_CTL_DMG_SSW);
std::cout << "sally test dmgstamac -> SendRssSectorSweepFrame, address=" << address << ", direction=" << direction << ", sectorID=" << sectorID << ", antennaID=" << antennaID << ", count=" << count << std::endl;

  /* Header Duration*/
  hdr.SetDuration (GetRemainingAllocationTime ());

  /* Other Fields */
  hdr.SetAddr1 (address);           // MAC address of the STA that is the intended receiver of the sector sweep.
  hdr.SetAddr2 (GetAddress ());     // MAC address of the transmitter STA of the sector sweep frame.
  hdr.SetNoMoreFragments ();
  hdr.SetNoRetry ();

  Ptr<Packet> packet = Create<Packet> ();
  CtrlDMG_SSW sswFrame;

  DMG_SSW_Field ssw;
  ssw.SetDirection (direction);
  ssw.SetCountDown (count);
  ssw.SetSectorID (sectorID);
  ssw.SetDMGAntennaID (antennaID);

  DMG_SSW_FBCK_Field sswFeedback;
  sswFeedback.IsPartOfISS (false);
  sswFeedback.SetSector (m_feedbackAntennaConfig.first);
  sswFeedback.SetDMGAntenna (m_feedbackAntennaConfig.second);
  sswFeedback.SetPollRequired (false);

  /* Set the fields in SSW Frame */
  sswFrame.SetSswField (ssw);
  sswFrame.SetSswFeedbackField (sswFeedback);
  packet->AddHeader (sswFrame);

  if (m_isResponderTXSS)
    {
      /* Set Antenna Direction */
      m_phy->GetDirectionalAntenna ()->SetCurrentTxSectorID (sectorID);
      m_phy->GetDirectionalAntenna ()->SetCurrentTxAntennaID (antennaID);

      NS_LOG_INFO ("Sending SSW Frame " << Simulator::Now () << " with "
                   << uint32_t (m_sectorId) << " " << uint32_t (m_antennaId));
    }

  /* Send Control Frames directly without DCA + DCF Manager */
  MacLowTransmissionParameters params;
  params.EnableOverrideDurationId (hdr.GetDuration ());
  params.DisableRts ();
  params.DisableAck ();
  params.DisableNextData ();
  m_low->StartTransmission (packet,
                            &hdr,
                            params,
                            MakeCallback (&DmgStaWifiMac::FrameTxOk, this));
}

void
DmgStaWifiMac::SendSectorSweepFrame (Mac48Address address, BeamformingDirection direction,
                                     uint8_t sectorID, uint8_t antennaID,  uint16_t count)
{
std::cout << "sally test dmgstamac -> SendSectorSweepFrame, address=" << address << ", direction=" << direction << ", sectorID=" << sectorID << ", antennaID=" << antennaID << ", count=" << count << std::endl;
 
  WifiMacHeader hdr;
  hdr.SetType (WIFI_MAC_CTL_DMG_SSW);

  /* Header Duration*/
  hdr.SetDuration (GetRemainingAllocationTime ());

  /* Other Fields */
  hdr.SetAddr1 (address);           // MAC address of the STA that is the intended receiver of the sector sweep.
  hdr.SetAddr2 (GetAddress ());     // MAC address of the transmitter STA of the sector sweep frame.
  hdr.SetNoMoreFragments ();
  hdr.SetNoRetry ();

  Ptr<Packet> packet = Create<Packet> ();
  CtrlDMG_SSW sswFrame;

  DMG_SSW_Field ssw;
  ssw.SetDirection (direction);
  ssw.SetCountDown (count);
  ssw.SetSectorID (sectorID);
  ssw.SetDMGAntennaID (antennaID);

  DMG_SSW_FBCK_Field sswFeedback;
  sswFeedback.IsPartOfISS (false);
  sswFeedback.SetSector (m_feedbackAntennaConfig.first);
  sswFeedback.SetDMGAntenna (m_feedbackAntennaConfig.second);
  sswFeedback.SetPollRequired (false);

  /* Set the fields in SSW Frame */
  sswFrame.SetSswField (ssw);
  sswFrame.SetSswFeedbackField (sswFeedback);
  packet->AddHeader (sswFrame);

  if (m_isResponderTXSS)
    {
      /* Set Antenna Direction */
      m_phy->GetDirectionalAntenna ()->SetCurrentTxSectorID (sectorID);
      m_phy->GetDirectionalAntenna ()->SetCurrentTxAntennaID (antennaID);

      NS_LOG_INFO ("Sending SSW Frame " << Simulator::Now () << " with "
                   << uint32_t (m_sectorId) << " " << uint32_t (m_antennaId));
    }

  /* Send Control Frames directly without DCA + DCF Manager */
  MacLowTransmissionParameters params;
  params.EnableOverrideDurationId (hdr.GetDuration ());
  params.DisableRts ();
  params.DisableAck ();
  params.DisableNextData ();
  m_low->StartTransmission (packet,
                            &hdr,
                            params,
                            MakeCallback (&DmgStaWifiMac::FrameTxOk, this));
}

void
DmgStaWifiMac::SendSswFbckFrame (Mac48Address receiver)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> SendSswFbckFrame, receiver=" << receiver << std::endl;
  WifiMacHeader hdr;
  /* The Duration field is set until the end of the current allocation */
  hdr.SetDuration (GetRemainingAllocationTime ());
  hdr.SetType (WIFI_MAC_CTL_DMG_SSW_FBCK);
  hdr.SetAddr1 (receiver);        // Receiver.
  hdr.SetAddr2 (GetAddress ());   // Transmiter.

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (hdr);

  CtrlDMG_SSW_FBCK fbck;          // SSW-FBCK Frame.
  DMG_SSW_FBCK_Field feedback;    // SSW-FBCK Field.
  feedback.IsPartOfISS (false);
  /* Obtain antenna configuration for the highest received SNR from DMG STA to feed it back */
  m_feedbackAntennaConfig = GetBestAntennaConfiguration (receiver, true);
  feedback.SetSector (m_feedbackAntennaConfig.first);
  feedback.SetDMGAntenna (m_feedbackAntennaConfig.second);

  BRP_Request_Field request;
  request.SetMID_REQ (false);
  request.SetBC_REQ (false);

  BF_Link_Maintenance_Field maintenance;
  maintenance.SetAsMaster (true);

  fbck.SetSswFeedbackField (feedback);
  fbck.SetBrpRequestField (request);
  fbck.SetBfLinkMaintenanceField (maintenance);

  packet->AddHeader (fbck);
  NS_LOG_INFO ("Sending SSW-FBCK Frame to " << receiver << " at " << Simulator::Now ());

  /* Set the best sector for transmission */
  ANTENNA_CONFIGURATION_TX antennaConfigTx = m_bestAntennaConfig[receiver].first;
  m_phy->GetDirectionalAntenna ()->SetCurrentTxSectorID (antennaConfigTx.first);
  m_phy->GetDirectionalAntenna ()->SetCurrentTxAntennaID (antennaConfigTx.second);

  /* Send Control Frames directly without DCA + DCF Manager */
  MacLowTransmissionParameters params;
  params.EnableOverrideDurationId (hdr.GetDuration ());
  params.DisableRts ();
  params.DisableAck ();
  params.DisableNextData ();
  m_low->StartTransmission (packet,
                            &hdr,
                            params,
                            MakeCallback (&DmgStaWifiMac::FrameTxOk, this));
}

void
DmgStaWifiMac::SendSswAckFrame (Mac48Address receiver)
{
std::cout << "sally test dmgstamac -> SendSswAckFrame, receiver=" << receiver << std::endl;

  NS_LOG_FUNCTION (this);
  /* send a SSW Feedback when you receive a SSW Slot after MBIFS. */
  WifiMacHeader hdr;
  hdr.SetType (WIFI_MAC_CTL_DMG_SSW_ACK);
  hdr.SetAddr1 (receiver);        // Receiver.
  hdr.SetAddr2 (GetAddress ());   // Transmiter.
  /* The Duration field is set until the end of the current allocation */
  Time duration = m_sswFbckDuration - (GetSifs () + NanoSeconds (sswAckTxTime));
  NS_ASSERT (duration > Seconds (0));
  hdr.SetDuration (m_currentAllocationLength);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (hdr);

  CtrlDMG_SSW_FBCK ackFrame;      // SSW-ACK Frame.
  DMG_SSW_FBCK_Field feedback;    // SSW-FBCK Field.

  /* Obtain antenna configuration for the highest received SNR from DMG STA to feed it back */
  m_feedbackAntennaConfig = GetBestAntennaConfiguration (receiver, true);

  feedback.IsPartOfISS (false);
  feedback.SetSector (m_feedbackAntennaConfig.first);
  feedback.SetDMGAntenna (m_feedbackAntennaConfig.second);

  BRP_Request_Field request;
  request.SetMID_REQ (false);
  request.SetBC_REQ (false);

  BF_Link_Maintenance_Field maintenance;
  maintenance.SetAsMaster (false); /* Slave of data transfer */

  ackFrame.SetSswFeedbackField (feedback);
  ackFrame.SetBrpRequestField (request);
  ackFrame.SetBfLinkMaintenanceField (maintenance);

  packet->AddHeader (ackFrame);
  NS_LOG_INFO ("Sending SSW-ACK Frame to " << receiver << " at " << Simulator::Now ());

  /* Set the best sector for transmission */
  ANTENNA_CONFIGURATION_TX antennaConfigTx = m_bestAntennaConfig[receiver].first;
  m_phy->GetDirectionalAntenna ()->SetCurrentTxSectorID (antennaConfigTx.first);
  m_phy->GetDirectionalAntenna ()->SetCurrentTxAntennaID (antennaConfigTx.second);

  /* Send Control Frames directly without DCA + DCF Manager */
  MacLowTransmissionParameters params;
  params.EnableOverrideDurationId (hdr.GetDuration ());
  params.DisableRts ();
  params.DisableAck ();
  params.DisableNextData ();
  m_low->StartTransmission (packet,
                            &hdr,
                            params,
                            MakeCallback (&DmgStaWifiMac::FrameTxOk, this));
}

void
DmgStaWifiMac::FrameTxOk (const WifiMacHeader &hdr)
{
std::cout << "sally test dmgstamac -> FrameTxOk, hdr=" << &hdr << std::endl;

  NS_LOG_FUNCTION (this);
  if (hdr.IsSSW ())
    {
      if (m_totalSectors > 0)
        {
          if (m_sectorId < m_phy->GetDirectionalAntenna ()->GetNumberOfSectors ())
            {
              m_sectorId++;
            }
          else if ((m_sectorId == m_phy->GetDirectionalAntenna ()->GetNumberOfSectors ()) &&
                   (m_antennaId < m_phy->GetDirectionalAntenna ()->GetNumberOfAntennas ()))
            {
              m_sectorId = 1;
              m_antennaId++;
            }

          m_totalSectors--;
          if (m_accessPeriod == CHANNEL_ACCESS_ABFT)
            {
              Simulator::Schedule (m_sbifs, &DmgStaWifiMac::SendSectorSweepFrame, this, hdr.GetAddr1 (),
                                   BeamformingResponder, m_sectorId, m_antennaId, m_totalSectors);
            }
          else
            {
              /* We are performing BF during DTI period */
              if (m_isIssInitiator)
                {
                  Simulator::Schedule (m_sbifs, &DmgStaWifiMac::SendIssSectorSweepFrame, this, hdr.GetAddr1 (),
                                       BeamformingInitiator, m_sectorId, m_antennaId, m_totalSectors);
                }
              else
                {
                  Simulator::Schedule (m_sbifs, &DmgStaWifiMac::SendRssSectorSweepFrame, this, hdr.GetAddr1 (),
                                       BeamformingResponder, m_sectorId, m_antennaId, m_totalSectors);
                }
            }
        }
      else
        {
          /* Last SSW Frame was sent, so we wait for the SSW-FBCK from the either the DMG AP or DMG STA */
          m_phy->GetDirectionalAntenna ()->SetInOmniReceivingMode ();
        }
    }
  else if (hdr.IsSSW_ACK ())
    {   
      /* We are SLS Respodner, raise callback for SLS Phase Completion */
      ANTENNA_CONFIGURATION antennaConfig = GetBestAntennaConfiguration (hdr.GetAddr1 () ,true);
      m_slsCompleted (hdr.GetAddr1 (), CHANNEL_ACCESS_DTI, antennaConfig.first, antennaConfig.second);
    }
}

void
DmgStaWifiMac::BrpSetupCompleted (Mac48Address address)
{
std::cout << "sally test dmgstamac -> BrpSetupCompleted, address=" << address << std::endl;

  NS_LOG_FUNCTION (this << address);
}

void
DmgStaWifiMac::NotifyBrpPhaseCompleted (void)
{
std::cout << "sally test dmgstamac -> NotifyBrpPhaseCompleted" << std::endl;

  NS_LOG_FUNCTION (this);
}

void
DmgStaWifiMac::RequestInformation (Mac48Address stationAddress)
{
std::cout << "sally test dmgstamac -> RequestInformation, stationAddress=" << stationAddress << std::endl;

  NS_LOG_FUNCTION (this << stationAddress);
  /* Obtain Information about the node like DMG Capabilities and AID */
  ExtInformationRequest requestHdr;
  Ptr<RequestElement> requestElement = Create<RequestElement> ();
  requestElement->AddRequestElementID (IE_DMG_CAPABILITIES);
  requestElement->AddRequestElementID (IE_RELAY_CAPABILITIES);

  requestHdr.SetSubjectAddress (stationAddress);
  requestHdr.SetRequestInformationElement (requestElement);
  SendInformationRequest (GetBssid (), requestHdr);
}

void
DmgStaWifiMac::ForwardActionFrame (Mac48Address to, WifiActionHeader &actionHdr, Header &actionBody)
{
std::cout << "sally test dmgstamac -> ForwardActionFrame, to=" << to << std::endl;

  NS_LOG_FUNCTION (this << to);
  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (actionBody);
  packet->AddHeader (actionHdr);
  m_dca->Queue (packet, hdr);
}

Ptr<RelayTransferParameterSetElement>
DmgStaWifiMac::GetRelayTransferParameterSet (void) const
{
std::cout << "sally test dmgstamac -> GetRelayTransferParameterSet" << std::endl;

  Ptr<RelayTransferParameterSetElement> element = Create<RelayTransferParameterSetElement> ();
  element->SetDuplexMode (m_rdsDuplexMode);
  element->SetCooperationMode (false);              /* Link Switching Type only */
  element->SetTxMode (false);                       /* Normal mode */
  element->SetLinkChangeInterval (m_relayLinkChangeInterval);
  element->SetDataSensingTime (m_relayDataSensingTime);
  return element;
}

/**
 * Functions for Relay Discovery/Selection/RLS/Tear Down
 */
void
DmgStaWifiMac::SendChannelMeasurementRequest (Mac48Address to, uint8_t token)
{
std::cout << "sally test dmgstamac -> SendChannelMeasurementRequest, to=" << to << ", token=" << token << std::endl;

  NS_LOG_FUNCTION (this << to << token);
  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtMultiRelayChannelMeasurementRequest requestHdr;
  requestHdr.SetDialogToken (token);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_MULTI_RELAY_CHANNEL_MEASUREMENT_REQUEST;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (requestHdr);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::SendChannelMeasurementReport (Mac48Address to, uint8_t token, ChannelMeasurementInfoList &measurementList)
{
std::cout << "sally test dmgstamac -> SendChannelMeasurementReport, to=" << to << ", token=" << token << std::endl;

  NS_LOG_FUNCTION (this);
  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtMultiRelayChannelMeasurementReport responseHdr;
  responseHdr.SetDialogToken (token);
  responseHdr.SetChannelMeasurementList (measurementList);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_MULTI_RELAY_CHANNEL_MEASUREMENT_REPORT;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (responseHdr);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::StartRelayDiscovery (Mac48Address stationAddress)
{
std::cout << "sally test dmgstamac -> StartRelayDiscovery, stationAddress=" << stationAddress << std::endl;

  NS_LOG_FUNCTION (this << stationAddress);
  /* Establish Relay with specific DMG STA */
  InformationMapIterator it = m_informationMap.find (stationAddress);
  if (it != m_informationMap.end ())
    {
      /* We already have information about the DMG STA */
      StationInformation info = static_cast<StationInformation> (it->second);
      /* Check if the remote DMG STA is Relay Capable */
      Ptr<RelayCapabilitiesElement> capabilitiesElement
          = StaticCast<RelayCapabilitiesElement> (info.second[IE_RELAY_CAPABILITIES]);
      RelayCapabilitiesInfo capabilitiesInfo = capabilitiesElement->GetRelayCapabilitiesInfo ();
      if (capabilitiesInfo.GetRelayUsability ())
        {
          /* Initialize Relay variables */
          m_relayLinkInfo.srcRedsAid = m_aid;
          m_relayLinkInfo.srcRedsAddress = GetAddress ();
          m_relayLinkInfo.dstRedsAid = info.first->GetAID ();
          m_relayLinkInfo.dstRedsAddress = stationAddress;
          m_relayLinkInfo.dstRedsCapabilitiesInfo = capabilitiesInfo;
          m_relayLinkInfo.waitingDestinationRedsReports = false;
          m_relayLinkInfo.relayLinkEstablished = false;
          m_relayLinkInfo.transmissionLink = DIRECT_LINK;
          m_relayLinkInfo.switchTransmissionLink = false;
          m_relayLinkInfo.relayForwardingActivated = false;
          /* Send Relay Search Request Frame to the PCP/AP */
          SendRelaySearchRequest (0, m_relayLinkInfo.dstRedsAid);
        }
      else
        {
          std::cout << "Cannot establish relay link with DMG STA=" << stationAddress << std::endl;
        }
    }
  else
    {
      /* Obtain Information about the node like DMG Capabilities, AID, and Relay Capabilities */
      RequestInformation (GetBssid ());
    }
}

void
DmgStaWifiMac::SendRelaySearchRequest (uint8_t token, uint16_t destinationAid)
{
  NS_LOG_FUNCTION (this << token << destinationAid);
std::cout << "sally test dmgstamac -> SendRelaySearchRequest, token=" << token << ", destinationAid=" << destinationAid << std::endl;

  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (GetBssid ());
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtRelaySearchRequestHeader requestHdr;
  requestHdr.SetDialogToken (token);
  requestHdr.SetDestinationRedsAid (destinationAid);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_RELAY_SEARCH_REQUEST;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (requestHdr);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::SendRlsRequest (Mac48Address to, uint8_t token, uint16_t sourceAid, uint16_t relayAid, uint16_t destinationAid)
{
  NS_LOG_FUNCTION (this << to << token << sourceAid << relayAid << destinationAid);
std::cout << "sally test dmgstamac -> SendRlsRequest, to=" << to << ", token=" << token << ", sourceAid=" << sourceAid << ", relayAid=" << relayAid << ", destinationAid=" << destinationAid << std::endl;

  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtRlsRequest requestHdr;
  requestHdr.SetDialogToken (token);
  requestHdr.SetSourceAid (sourceAid);
  requestHdr.SetRelayAid (relayAid);
  requestHdr.SetDestinationAid (destinationAid);

  RelayCapabilitiesInfo srcInfo = GetRelayCapabilitiesInfo ();
  requestHdr.SetSourceCapabilityInformation (srcInfo);
  requestHdr.SetRelayCapabilityInformation (m_relayLinkInfo.rdsCapabilitiesInfo);
  requestHdr.SetDestinationCapabilityInformation (m_relayLinkInfo.dstRedsCapabilitiesInfo);
  requestHdr.SetRelayTransferParameterSet (GetRelayTransferParameterSet ());

  /* Store current relay information */
  m_relayLinkInfo.rdsDuplexMode = m_rdsDuplexMode;
  m_relayLinkInfo.relayLinkChangeInterval = m_relayLinkChangeInterval;
  m_relayLinkInfo.relayDataSensingTime = m_relayDataSensingTime;

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_RLS_REQUEST;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (requestHdr);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::SendRlsResponse (Mac48Address to, uint8_t token, uint16_t destinationStatus, uint16_t relayStatus)
{
  NS_LOG_FUNCTION (this << token);
std::cout << "sally test dmgstamac -> SendRlsResponse, to=" << to << ", token=" << token << ", destinationStatus=" << destinationStatus << ", relayStatus=" << relayStatus << std::endl;

  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtRlsResponse reponseHdr;
  reponseHdr.SetDialogToken (token);
  reponseHdr.SetDestinationStatusCode (destinationStatus);
  reponseHdr.SetRelayStatusCode (relayStatus);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_RLS_RESPONSE;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (reponseHdr);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::SendRlsAnnouncment (Mac48Address to, uint16_t destination_aid, uint16_t relay_aid, uint16_t source_aid)
{
  NS_LOG_FUNCTION (this);
std::cout << "sally test dmgstamac -> SendRlsAnnouncment, to=" << to << ", destination_aid=" << destination_aid << ", relay_aid=" << relay_aid << ", source_aid=" << source_aid << std::endl;

  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtRlsAnnouncment announcmentHdr;
  announcmentHdr.SetStatusCode (0);
  announcmentHdr.SetDestinationAid (destination_aid);
  announcmentHdr.SetRelayAid (relay_aid);
  announcmentHdr.SetSourceAid (source_aid);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_RLS_ANNOUNCEMENT;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (announcmentHdr);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::SendRelayTeardown (Mac48Address to, uint16_t sourceAid, uint16_t destinationAid, uint16_t relayAid)
{
  NS_LOG_FUNCTION (this << to << sourceAid << destinationAid << relayAid);
std::cout << "sally test dmgstamac -> SendRelayTeardown, to=" << to << ", sourceAid=" << sourceAid << ", destinationAid=" << destinationAid << ", relayAid=" << relayAid << std::endl;

  WifiMacHeader hdr;
  hdr.SetAction ();
  hdr.SetAddr1 (to);
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  hdr.SetNoOrder ();

  ExtRlsTearDown frame;
  frame.SetSourceAid (sourceAid);
  frame.SetDestinationAid (destinationAid);
  frame.SetRelayAid (relayAid);

  WifiActionHeader actionHdr;
  WifiActionHeader::ActionValue action;
  action.dmgAction = WifiActionHeader::DMG_RLS_TEARDOWN;
  actionHdr.SetAction (WifiActionHeader::DMG, action);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (frame);
  packet->AddHeader (actionHdr);

  m_dca->Queue (packet, hdr);
}

void
DmgStaWifiMac::RemoveRelayEntry (uint16_t sourceAid, uint16_t destinationAid)
{
  NS_LOG_FUNCTION (this << sourceAid << destinationAid);
std::cout << "sally test dmgstamac -> RemoveRelayEntry, sourceAid=" << sourceAid << ", destinationAid=" << destinationAid << std::endl;

  REDS_PAIR redsPair = std::make_pair (sourceAid, destinationAid);
  RELAY_LINK_MAP_ITERATOR it = m_relayLinkMap.find (redsPair);
  if (it != m_relayLinkMap.end ())
    {
      RELAY_LINK_INFO info = it->second;
      if (info.srcRedsAid == m_aid)
        {
          m_sp->ChangePacketsAddress (info.selectedRelayAddress, info.dstRedsAddress);
          m_dataForwardingTable[info.dstRedsAddress].nextHopAddress = info.dstRedsAddress;
        }
      m_relayLinkMap.erase (it);
    }
}

void
DmgStaWifiMac::TeardownRelay (Mac48Address to, Mac48Address destinationAddress,
                              uint16_t sourceAid, uint16_t destinationAid, uint16_t relayAid)
{
  NS_LOG_FUNCTION (this << to << destinationAddress << sourceAid << destinationAid << relayAid);
std::cout << "sally test dmgstamac -> TeardownRelay, to=" << to << ", destinationAddress=" << destinationAddress << ", sourceAid=" << sourceAid << ", destinationAid=" << destinationAid << ", relayAid=" << relayAid << std::endl;

  RemoveRelayEntry (sourceAid, destinationAid);
  SendRelayTeardown (to, sourceAid, destinationAid, relayAid);
  SendRelayTeardown (destinationAddress, sourceAid, destinationAid, relayAid);
  SendRelayTeardown (GetBssid (), sourceAid, destinationAid, relayAid);
}

void
DmgStaWifiMac::StartRlsProcedure (void)
{
  NS_LOG_FUNCTION (this);
  SendRlsRequest (m_relayLinkInfo.selectedRelayAddress, 10, m_aid,
                  m_relayLinkInfo.selectedRelayAid, m_relayLinkInfo.dstRedsAid);
}

Ptr<MultiBandElement>
DmgStaWifiMac::GetMultiBandElement (void) const
{
  Ptr<MultiBandElement> multiband = Create<MultiBandElement> ();
  multiband->SetStaRole (ROLE_NON_PCP_NON_AP);
  multiband->SetStaMacAddressPresent (false); /* The same MAC address is used across all the bands */
  multiband->SetBandID (Band_4_9GHz);
  multiband->SetOperatingClass (18);          /* Europe */
  multiband->SetChannelNumber (1);
  multiband->SetBssID (GetBssid ());
  multiband->SetConnectionCapability (1);     /* AP */
  multiband->SetFstSessionTimeout (1);
std::cout << "sally test dmgstamac -> GetMultiBandElement, multiband=" << multiband << std::endl;
  return multiband;
}

void
DmgStaWifiMac::Receive (Ptr<Packet> packet, const WifiMacHeader *hdr)
{
  NS_LOG_FUNCTION (this << packet << hdr);
std::cout << "sally test dmgstamac -> Receive, hdr=" << *hdr << std::endl;

  if (hdr->GetAddr3 () == GetAddress ())
    {
std::cout << "sally test dmgstamac -> Receive, case1, packet sent by us." << std::endl;
      NS_LOG_LOGIC ("packet sent by us.");
      return;
    }
  else if (hdr->GetAddr1 () != GetAddress () && !hdr->GetAddr1 ().IsGroup () && !hdr->IsDMGBeacon ())
    {
      NS_LOG_LOGIC ("packet is not for us");
std::cout << "sally test dmgstamac -> Receive, case2, packet is not for us." << std::endl;
      NotifyRxDrop (packet);
      return;
    }
  else if (hdr->IsData ())
    {
      if (!IsAssociated () && hdr->GetAddr2 () != GetBssid ())
        {
          NS_LOG_LOGIC ("Received data frame while not associated: ignore");
std::cout << "sally test dmgstamac -> Receive, case3, Received data frame while not associated: ignore" << std::endl;
          NotifyRxDrop (packet);
          return;
        }

      if (hdr->IsQosData ())
        {
          /* Relay Related Variables */
          m_relayDataExchanged = true;
          m_moreData = hdr->IsMoreData ();
          if (hdr->IsQosAmsdu ())
            {
std::cout << "sally test dmgstamac -> Receive, case4, hdr->IsQosData, hdr->IsQosAmsdu" << std::endl;
              NS_ASSERT (hdr->GetAddr3 () == GetBssid ());
              DeaggregateAmsduAndForward (packet, hdr);
              packet = 0;
            }
          else
            {
std::cout << "sally test dmgstamac -> Receive, case5, hdr->IsQosData, !hdr->IsQosAmsdu, ForwardUp, hdr->GetAddr3=" << hdr->GetAddr3 () << ", hdr->GetAddr1 ()" << hdr->GetAddr1 () << std::endl;
              ForwardUp (packet, hdr->GetAddr3 (), hdr->GetAddr1 ());
            }
        }
      else
        {
std::cout << "sally test dmgstamac -> Receive, case6, hdr->IsData, the last case, hdr->GetAddr3=" << hdr->GetAddr3 () << ", hdr->GetAddr1 ()" << hdr->GetAddr1 () << std::endl;
          ForwardUp (packet, hdr->GetAddr3 (), hdr->GetAddr1 ());
        }
      return;
    }
  else if (hdr->IsProbeReq ()|| hdr->IsAssocReq ())
    {
      // This is a frame aimed at an AP, so we can safely ignore it.
std::cout << "sally test dmgstamac -> Receive, case7, hdr->IsProbeReq () || hdr->IsAssocReq (), This is a frame aimed at an AP, so we can safely ignore it." << std::endl;
      NotifyRxDrop (packet);
      return;
    }
  else if (hdr->IsAction () || hdr->IsActionNoAck ())
    {
      WifiActionHeader actionHdr;
      packet->RemoveHeader (actionHdr);
std::cout << "sally test dmgstamac -> Receive, case8, hdr->IsAction=" << hdr->IsAction () << ", hdr->IsActionNoAck=" << hdr->IsActionNoAck () << ", actionHdr.GetCategory=" << actionHdr.GetCategory () << ", actionHdr.GetAction ().dmgAction=" << actionHdr.GetAction ().dmgAction << std::endl;

      switch (actionHdr.GetCategory ())
        {
        case WifiActionHeader::DMG:
          switch (actionHdr.GetAction ().dmgAction)
            {
            case WifiActionHeader::DMG_RELAY_SEARCH_RESPONSE:
              {
                ExtRelaySearchResponseHeader responseHdr;
                packet->RemoveHeader (responseHdr);
                /* The response contains list of RDSs in the current DMG BSS */
                m_rdsList = responseHdr.GetRelayCapableList ();
                return;
              }
            case WifiActionHeader::DMG_MULTI_RELAY_CHANNEL_MEASUREMENT_REQUEST:
              {
                NS_LOG_LOGIC ("Received Multi-Relay Channel Measurement Request from " << hdr->GetAddr2 ());
                ExtMultiRelayChannelMeasurementRequest requestHdr;
                packet->RemoveHeader (requestHdr);
                /* Prepare the Channel Report */
                ChannelMeasurementInfoList list;
                Ptr<ExtChannelMeasurementInfo> elem;
                double measuredsnr;
                uint8_t snr;
                if (m_rdsActivated)
                  {
                    /** We are the RDS and we received the request from the source REDS **/
                    /* Obtain Channel Measurement between the source REDS and RDS */
                    GetBestAntennaConfiguration (hdr->GetAddr2 (), true, measuredsnr);
                    snr = -(unsigned int) (4 * (measuredsnr - 19));
                    elem = Create<ExtChannelMeasurementInfo> ();
                    elem->SetPeerStaAid (m_macMap[hdr->GetAddr2 ()]);
                    elem->SetSnr (snr);
                    list.push_back (elem);
                  }
                else
                  {
                    /**
                     * We are the destination REDS and we've received the request from the source REDS.
                     * Report back the measurement information between destination REDS and all the available RDS.
                     */
                    for (RelayCapableStaList::iterator iter = m_rdsList.begin (); iter != m_rdsList.end (); iter++)
                      {
                        elem = Create<ExtChannelMeasurementInfo> ();
                        GetBestAntennaConfiguration (hdr->GetAddr2 (), true, measuredsnr);
                        snr = -(unsigned int) (4 * (measuredsnr - 19));
                        elem->SetPeerStaAid (iter->first);
                        elem->SetSnr (snr);
                        list.push_back (elem);
                      }
                  }
                SendChannelMeasurementReport (hdr->GetAddr2 (), requestHdr.GetDialogToken (), list);
                return;
              }
            case WifiActionHeader::DMG_MULTI_RELAY_CHANNEL_MEASUREMENT_REPORT:
              {
                if (m_relayLinkInfo.srcRedsAid == m_aid)
                  {
                    ExtMultiRelayChannelMeasurementReport responseHdr;
                    packet->RemoveHeader (responseHdr);
                    if (!m_relayLinkInfo.waitingDestinationRedsReports)
                      {
                        /* Perform BF with the destination REDS, currently this is done by invoking a callback function
                         * to the main program. The main program schedules manually a service period between the source REDS
                         * and destination REDS */

                        /* Send Multi-Relay Channel Measurement Request to the Destination REDS */
                        m_relayLinkInfo.waitingDestinationRedsReports = true;
                        /* Store the measurement values between the source REDS and the RDS */
                        m_channelMeasurementList = responseHdr.GetChannelMeasurementInfoList ();
                      }
                    else
                      {
                        /**
                         * The source REDS is aware of the following channel measurements with:
                         * 1. Zero or more RDS.
                         * 2. Between Destination REDS and zero or more RDS.
                         * The Source REDS shall select on of the previous RDS.
                         */

                        /* Report the measurements to the user to decide relay selection */
                        m_relayLinkInfo.selectedRelayAid = m_channelMeasurementCallback (m_channelMeasurementList,
                                                                                         responseHdr.GetChannelMeasurementInfoList (),
                                                                                         m_relayLinkInfo.selectedRelayAddress);

                        m_relayLinkInfo.rdsCapabilitiesInfo = m_rdsList[m_relayLinkInfo.selectedRelayAid];

                      }
                    m_channelReportReceived (hdr->GetAddr2 ());
                  }
                return;
              }
            case WifiActionHeader::DMG_RLS_REQUEST:
              {
                ExtRlsRequest requestHdr;
                packet->RemoveHeader (requestHdr);
                /** Store the parameters of the ongoing RLS procedure **/
                /* Store the AID and address of the source and destination REDS */
                m_relayLinkInfo.srcRedsAid = requestHdr.GetSourceAid ();
                m_relayLinkInfo.srcRedsAddress = hdr->GetAddr2 ();
                m_relayLinkInfo.dstRedsAid = requestHdr.GetDestinationAid ();
                m_relayLinkInfo.dstRedsAddress = m_aidMap[requestHdr.GetDestinationAid ()];
                /* Store Parameters related t o the relay link */
                Ptr<RelayTransferParameterSetElement> elem = requestHdr.GetRelayTransferParameterSet ();
                m_relayLinkInfo.rdsDuplexMode = elem->GetDuplexMode ();
                m_relayLinkInfo.relayLinkChangeInterval = elem->GetLinkChangeInterval ();
                m_relayLinkInfo.relayDataSensingTime = elem->GetDataSensingTime ();
                m_relayLinkInfo.relayFirstPeriod = elem->GetFirstPeriod ();
                m_relayLinkInfo.relaySecondPeriod = elem->GetSecondPeriod ();

                if (m_aid == requestHdr.GetRelayAid ())
                  {
                    /* We are the selected RDS so resend RLS Request to the Destination REDS */
                    NS_LOG_LOGIC ("Received RLS Request from Source REDS="
                                  << hdr->GetAddr2 () << ", resend RLS Request to Destination REDS");
                    /* Upon receiving the RLS Request frame, the RDS shall transmit an RLS Request frame to
                     * the destination REDS containing the same information as received within the frame body
                     * of the source REDS’s RLS Request frame. */
                    WifiActionHeader actionHdr;
                    WifiActionHeader::ActionValue action;
                    action.dmgAction = WifiActionHeader::DMG_RLS_REQUEST;
                    actionHdr.SetAction (WifiActionHeader::DMG, action);
                    ForwardActionFrame (m_relayLinkInfo.dstRedsAddress, actionHdr, requestHdr);
                  }
                else if (m_aid == requestHdr.GetDestinationAid ())
                  {
                    /* We are the destination REDS, so we send RLS Response to the selected RDS */
                    NS_LOG_LOGIC ("Received RLS Request from the selected RDS "
                                  << hdr->GetAddr2 () << ", send an RLS Response to RDS");
                    m_relayLinkInfo.selectedRelayAddress = hdr->GetAddr2 ();
                    m_relayLinkInfo.relayLinkEstablished = true;
                    /* Create data structure of the established relay link at the destination REDS */
                    REDS_PAIR redsPair = std::make_pair (m_relayLinkInfo.srcRedsAid, m_relayLinkInfo.dstRedsAid);
                    m_relayLinkMap[redsPair] = m_relayLinkInfo;
                    /* Send RLS Resposne to the selected RDS*/
                    SendRlsResponse (m_relayLinkInfo.selectedRelayAddress, requestHdr.GetDialogToken (), 0, 0);
                  }

                return;
              }
            case WifiActionHeader::DMG_RLS_RESPONSE:
              {
                ExtRlsResponse responseHdr;
                packet->RemoveHeader (responseHdr);
                if (m_rdsActivated)
                  {
                    /* We are the RDS, resend RLS Response to Source REDS */
                    NS_LOG_LOGIC ("Receveid RLS Response from the destination REDS="
                                  << hdr->GetAddr2 () << ", send RLS Response to the Source REDS");
                    m_relayLinkInfo.selectedRelayAid = m_aid;
                    SendRlsResponse (m_relayLinkInfo.srcRedsAddress, responseHdr.GetDialogToken (),
                                     responseHdr.GetDestinationStatusCode (), 0);
                    if (responseHdr.GetDestinationStatusCode () == 0)
                      {
                        /* Create data structure of the established relay link at the RDS */
                        REDS_PAIR redsPair = std::make_pair (m_relayLinkInfo.srcRedsAid, m_relayLinkInfo.dstRedsAid);
                        m_relayLinkInfo.relayLinkEstablished = true;
                        m_relayLinkMap[redsPair] = m_relayLinkInfo;
                      }
                  }
                else
                  {
                    /* This node is the Source REDS */
                    if ((responseHdr.GetRelayStatusCode () == 0) && (responseHdr.GetDestinationStatusCode () == 0))
                      {
                        /* Create data structure of the established relay link */
                        REDS_PAIR redsPair = std::make_pair (m_aid, m_relayLinkInfo.dstRedsAid);
                        m_relayLinkInfo.relayLinkEstablished = true;
                        m_relayLinkMap[redsPair] = m_relayLinkInfo;
                        /* Invoke callback for the completion of the RLS procedure */
                        m_rlsCompleted (m_relayLinkInfo.selectedRelayAddress);
                        /* Send RLS Announcement frame to PCP/AP */
                        SendRlsAnnouncment (GetBssid (),
                                            m_relayLinkInfo.dstRedsAid,
                                            m_relayLinkInfo.selectedRelayAid,
                                            m_aid);
                        /* We can redo BF (Optional) */
                        NS_LOG_LOGIC ("Relay Link Switch procedure is Success, RDS operates in " << m_rdsDuplexMode
                                      << " Mode, Send RLS Announcement to the PCP/AP=" << GetBssid ());
                      }
                  }
                return;
              }
            case WifiActionHeader::DMG_RLS_TEARDOWN:
              {
                NS_LOG_LOGIC ("Received RLS Tear Down Frame from=" << hdr->GetAddr2 ());
                ExtRlsTearDown header;
                packet->RemoveHeader (header);
                /* Search for the pair REDS */
                RemoveRelayEntry (header.GetSourceAid (), header.GetDestinationAid ());
                return;
              }
            case WifiActionHeader::DMG_INFORMATION_RESPONSE:
              {
                ExtInformationResponse responseHdr;
                packet->RemoveHeader (responseHdr);

                /* Record the Information Obtained */
                Mac48Address stationAddress = responseHdr.GetSubjectAddress ();

                /* If this field is set to the broadcast address, then the STA is providing
                 * information regarding all associated STAs.*/
                if (stationAddress.IsBroadcast ())
                  {
                    NS_LOG_LOGIC ("Received DMG Information Response frame regarding all DMG STAs in the DMG BSS.");
                  }
                else
                  {
                    NS_LOG_LOGIC ("Received DMG Information Response frame regarding " << stationAddress);
                    StationInformation information;
                    Ptr<DmgCapabilities> capabilities = responseHdr.GetDmgCapabilitiesList () [0];
                    information.first = capabilities;
                    information.second = responseHdr.GetListOfInformationElement ();
                    /* There is only one station in the response */
                    m_informationMap[responseHdr.GetSubjectAddress ()] = information;
                    MapAidToMacAddress (capabilities->GetAID (), responseHdr.GetSubjectAddress ());
                  }
                return;
              }
            default:
              NS_FATAL_ERROR ("Unsupported Action frame received");
              return;
            }
        default:
          packet->AddHeader (actionHdr);
          DmgWifiMac::Receive (packet, hdr);
          return;
        }
    }
  else if (hdr->IsSSW ())
    {
      CtrlDMG_SSW sswFrame;
      packet->RemoveHeader (sswFrame);
      DMG_SSW_Field ssw = sswFrame.GetSswField ();
      DMG_SSW_FBCK_Field sswFeedback = sswFrame.GetSswFeedbackField ();
std::cout << "sally test dmgstamac -> Receive, case9, hdr->IsSSW" << std::endl;

      /* Map the antenna configuration for the frames received by SLS of the DMG-STA */
      MapTxSnr (hdr->GetAddr2 (), ssw.GetSectorID (), ssw.GetDMGAntennaID (), m_stationManager->GetRxSnr ());

      if (ssw.GetDirection () == BeamformingResponder)
        {
          NS_LOG_LOGIC ("Received SSW frame as part of RSS from=" << hdr->GetAddr2 ());
          /* The SSW Frame we received is part of RSS */
          /* Not part of ISS i.e. the SSW Feedback Field Contains the Fedbeck of the ISS */
          sswFeedback.IsPartOfISS (false);
std::cout << "sally test dmgstamac -> Receive, case10, hdr->IsSSW, Received SSW frame as part of RSS from=" << hdr->GetAddr2 () << std::endl;

          /* If we receive one SSW Frame at least, then we schedule SSW-FBCK */
          if (!m_sectorFeedbackSent[hdr->GetAddr2 ()])
            {
              m_sectorFeedbackSent[hdr->GetAddr2 ()] = true;

              /* Set the best TX antenna configuration reported by the SSW-FBCK Field */
              DMG_SSW_FBCK_Field sswFeedback = sswFrame.GetSswFeedbackField ();
              sswFeedback.IsPartOfISS (false);

              /* The Sector Sweep Frame contains feedback about the the best Tx Sector in the DMG-AP with the sending DMG-STA */
              ANTENNA_CONFIGURATION_TX antennaConfigTx = std::make_pair (sswFeedback.GetSector (), sswFeedback.GetDMGAntenna ());
              ANTENNA_CONFIGURATION_RX antennaConfigRx = std::make_pair (NO_ANTENNA_CONFIG, NO_ANTENNA_CONFIG);
              m_bestAntennaConfig[hdr->GetAddr2 ()] = std::make_pair (antennaConfigTx, antennaConfigRx);

              NS_LOG_LOGIC ("Best TX Antenna Sector Config by this DMG STA to DMG STA=" << hdr->GetAddr2 ()
                            << ": SectorID=" << uint32_t (antennaConfigTx.first)
                            << ", AntennaID=" << uint32_t (antennaConfigTx.second));
std::cout << "sally test dmgstamac -> Receive, case11, Best TX Antenna Sector Config by this DMG STA to DMG STA=" << hdr->GetAddr2 () << ": SectorID=" << uint32_t (antennaConfigTx.first) << ", AntennaID=" << uint32_t (antennaConfigTx.second) << std::endl;

              Time sswFbckTime = m_low->GetSectorSweepDuration (ssw.GetCountDown ()) + m_mbifs;
              Simulator::Schedule (sswFbckTime, &DmgStaWifiMac::SendSswFbckFrame, this, hdr->GetAddr2 ());
              NS_LOG_LOGIC ("Scheduled SSW-FBCK Frame to " << hdr->GetAddr2 ()
                            << " at " << Simulator::Now () + sswFbckTime);
std::cout << "sally test dmgstamac -> Receive, case12, Scheduled SSW-FBCK Frame to " << hdr->GetAddr2 () << " at " << Simulator::Now () + sswFbckTime << std::endl;

            }
        }
      else
        {
          NS_LOG_LOGIC ("Received SSW frame as part of ISS from=" << hdr->GetAddr2 ());
          sswFeedback.IsPartOfISS (true);
std::cout << "sally test dmgstamac -> Receive, case13, Received SSW frame as part of ISS from=" << hdr->GetAddr2 () << std::endl;

          if (m_rssEvent.IsExpired ())
            {
              Time rssTime = m_low->GetSectorSweepDuration (ssw.GetCountDown ()) + GetMbifs () ;
              m_rssEvent = Simulator::Schedule (rssTime, &DmgStaWifiMac::StartResponderSectorSweep, this,
                                                hdr->GetAddr2 (), m_beamformingTxss);
              NS_LOG_LOGIC ("Scheduled RSS Period for Station=" << GetAddress () << " at " << Simulator::Now () + rssTime);
std::cout << "sally test dmgstamac -> Receive, case14, Scheduled RSS Period for Station=" << GetAddress () << " at " << Simulator::Now () + rssTime << std::endl;

            }
        }
      return;
    }
  else if (hdr->IsSSW_FBCK ())
    {
      NS_LOG_LOGIC ("Received SSW-FBCK frame from=" << hdr->GetAddr2 ());
std::cout << "sally test dmgstamac -> Receive, case15, Received SSW-FBCK frame from=" << hdr->GetAddr2 () << std::endl;

      /* We are the SLS Respodner */
      CtrlDMG_SSW_FBCK fbck;
      packet->RemoveHeader (fbck);

      /* The SSW-FBCK contains the best TX antenna by this station */
      DMG_SSW_FBCK_Field sswFeedback = fbck.GetSswFeedbackField ();
      sswFeedback.IsPartOfISS (false);

      /* Record best antenna configuration */
      ANTENNA_CONFIGURATION_TX antennaConfigTx = std::make_pair (sswFeedback.GetSector (), sswFeedback.GetDMGAntenna ());
      ANTENNA_CONFIGURATION_RX antennaConfigRx = std::make_pair (NO_ANTENNA_CONFIG, NO_ANTENNA_CONFIG);
      m_bestAntennaConfig[hdr->GetAddr2 ()] = std::make_pair (antennaConfigTx, antennaConfigRx);

      if (m_accessPeriod == CHANNEL_ACCESS_ABFT)
        {
          NS_LOG_LOGIC ("Best TX Antenna Sector Config by this DMG STA to DMG AP=" << hdr->GetAddr2 ()
                        << ": SectorID=" << uint32_t (antennaConfigTx.first)
                        << ", AntennaID=" << uint32_t (antennaConfigTx.second));
std::cout << "sally test dmgstamac -> Receive, case16, Best TX Antenna Sector Config by this DMG STA to DMG AP=" << hdr->GetAddr2 () << ": SectorID=" << uint32_t (antennaConfigTx.first) << ", AntennaID=" << uint32_t (antennaConfigTx.second) << std::endl;

          /* Raise an event that we selected the best sector to the DMG AP */
          m_slsCompleted (hdr->GetAddr2 (), CHANNEL_ACCESS_BHI, antennaConfigTx.first, antennaConfigTx.second);

          /* We received SSW-FBCK so we cancel the timeout event */
          m_slotIndex = 0;
          m_failedRssAttemptsCounter = 0;
          m_sswFbckTimeout.Cancel ();
        }
      else if (m_accessPeriod == CHANNEL_ACCESS_DTI)
        {
          m_sswFbckDuration = hdr->GetDuration ();
          NS_LOG_LOGIC ("Best TX Antenna Config by this DMG STA to DMG STA=" << hdr->GetAddr2 ()
                        << ": SectorID=" << uint32_t (antennaConfigTx.first)
                        << ", AntennaID=" << uint32_t (antennaConfigTx.second));
std::cout << "sally test dmgstamac -> Receive, case17, Best TX Antenna Config by this DMG STA to DMG STA=" << hdr->GetAddr2 () << ": SectorID=" << uint32_t (antennaConfigTx.first) << ", AntennaID=" << uint32_t (antennaConfigTx.second) << " Scheduled SSW-ACK Frame to " << hdr->GetAddr2 () << " at " << Simulator::Now () + m_mbifs << std::endl;

          NS_LOG_LOGIC ("Scheduled SSW-ACK Frame to " << hdr->GetAddr2 () << " at " << Simulator::Now () + m_mbifs);

          /* We add the station to the list of the stations we can directly communicate with */
          AddForwardingEntry (hdr->GetAddr2 ());

          Simulator::Schedule (m_mbifs, &DmgStaWifiMac::SendSswAckFrame, this, hdr->GetAddr2 ());
        }

      return;
    }
  else if (hdr->IsSSW_ACK ())
    {
      NS_LOG_LOGIC ("Received SSW-ACK frame from=" << hdr->GetAddr2 ());
std::cout << "sally test dmgstamac -> Receive, case18, Received SSW-ACK frame from=" << hdr->GetAddr2 () << std::endl;

      /* We are the SLS Initiator */
      CtrlDMG_SSW_ACK sswAck;
      packet->RemoveHeader (sswAck);

      /* We add the station to the list of the stations we can directly communicate with */
      AddForwardingEntry (hdr->GetAddr2 ());

      /* Raise a callback */
      ANTENNA_CONFIGURATION antennaConfig = GetBestAntennaConfiguration (hdr->GetAddr2 () ,true);
      m_slsCompleted (hdr->GetAddr2 (), CHANNEL_ACCESS_DTI, antennaConfig.first, antennaConfig.second);

      return;
    }
  else if (hdr->IsDMGBeacon ())
    {
      NS_LOG_LOGIC ("Received DMG Beacon frame with BSSID=" << hdr->GetAddr1 ());
std::cout << "sally test dmgstamac -> Receive, case19, Received DMG Beacon frame with BSSID=" << hdr->GetAddr1 () << std::endl;

      ExtDMGBeacon beacon;
      packet->RemoveHeader (beacon);

      bool goodBeacon = false;
      if (GetSsid ().IsBroadcast () || beacon.GetSsid ().IsEqual (GetSsid ()))
        {
          goodBeacon = true;
        }

      if (goodBeacon)
        {
          /* Check if we have already received DMG Beacon */
          if (!m_receivedDmgBeacon)
            {
              m_receivedDmgBeacon = true;
              m_stationSnrMap.erase (hdr->GetAddr1 ());

              if ((m_state == ASSOCIATED) && (beacon.GetBSSID () == GetBssid ()))
                {
                  Time delay = MicroSeconds (beacon.GetBeaconIntervalUs () * m_maxLostBeacons);
                  RestartBeaconWatchdog (delay);
                }

              /* Beacon Interval Field */
              ExtDMGBeaconIntervalCtrlField beaconInterval = beacon.GetBeaconIntervalControlField ();
              m_nextBeacon = beaconInterval.GetNextBeacon ();
              m_atiPresent = beaconInterval.IsATIPresent ();
              m_nextAbft = beaconInterval.GetNextABFT ();
              m_nBI = beaconInterval.GetN_BI ();
              m_ssSlotsPerABFT = beaconInterval.GetABFT_Length ();
              m_ssFramesPerSlot = beaconInterval.GetFSS ();
              m_isResponderTXSS = beaconInterval.IsResponderTXSS ();

              /* DMG Parameters */
              ExtDMGParameters parameters = beacon.GetDMGParameters ();
              m_isCbapOnly = parameters.Get_CBAP_Only ();
              m_isCbapSource = parameters.Get_CBAP_Source ();

              /* DMG Operation Element */
              Ptr<DmgOperationElement> operationElement
                  = StaticCast<DmgOperationElement> (beacon.GetInformationElement (IE_DMG_OPERATION));

              /* Next DMG ATI Element */
              Ptr<NextDmgAti> atiElement = StaticCast<NextDmgAti> (beacon.GetInformationElement (IE_NEXT_DMG_ATI));
              m_atiDuration = MicroSeconds (atiElement->GetAtiDuration ());

              /* Organizing medium access periods (Synchronization with TSF) */
              m_abftDuration = NanoSeconds (m_ssSlotsPerABFT * m_low->GetSectorSweepSlotTime (m_ssFramesPerSlot));
              m_abftDuration = MicroSeconds (ceil ((double) m_abftDuration.GetNanoSeconds () / 1000));
              m_btiDuration = MicroSeconds (operationElement->GetMinBHIDuration ()) - m_abftDuration - m_atiDuration - 2 * GetMbifs ();
              m_biStartTime = MicroSeconds (beacon.GetTimestamp ()) + hdr->GetDuration () - m_btiDuration;
              m_beaconInterval = MicroSeconds (beacon.GetBeaconIntervalUs ());
              NS_LOG_DEBUG ("BI Started=" << m_biStartTime.As (Time::US)
                            << ", BTI Duration=" << m_btiDuration.As (Time::US)
                            << ", BeaconInterval=" << m_beaconInterval.As (Time::US)
                            << ", BHIDuration=" << MicroSeconds (operationElement->GetMinBHIDuration ()).As (Time::US)
                            << ", TSF=" << MicroSeconds (beacon.GetTimestamp ()).As (Time::US)
                            << ", HDR-Duration=" << hdr->GetDuration ().As (Time::US)
                            << ", FrameDuration=" << m_phy->GetLastRxDuration ());

              if (!beaconInterval.IsCCPresent () && !beaconInterval.IsDiscoveryMode ())
                {
                  Time startTime = m_btiDuration + m_mbifs - (Simulator::Now () - m_biStartTime);
                  if (m_nextAbft == 0)
                    {
                      /* Schedule A-BFT following the end of the BTI Period */
                      SetBssid (hdr->GetAddr1 ());
                      m_slotIndex = 0;
                      m_remainingSlotsPerABFT = m_ssSlotsPerABFT;
                      m_abftEvent = Simulator::Schedule (startTime, &DmgStaWifiMac::StartAssociationBeamformTraining, this);
                      NS_LOG_DEBUG ("A-BFT Period for Station=" << GetAddress () << " is scheduled at " << Simulator::Now () + startTime);
                    }
                  else
                    {
                      /* Schedule ATI period following the end of BTI Period */
                      if (m_atiPresent)
                        {
                          Simulator::Schedule (startTime, &DmgStaWifiMac::StartAnnouncementTransmissionInterval, this);
                          NS_LOG_DEBUG ("ATI for Station:" << GetAddress () << " is scheduled at " << Simulator::Now () + startTime);
                        }
                      else
                        {
                          Simulator::Schedule (startTime, &DmgStaWifiMac::StartDataTransmissionInterval, this);
                          NS_LOG_DEBUG ("DTI for Station:" << GetAddress () << " is scheduled at " << Simulator::Now () + startTime);
                        }
                    }
                }

              /* A STA shall not transmit in the A-BFT of a beacon interval if it does not receive at least one DMG Beacon
               * frame during the BTI of that beacon interval.*/

              /** Check the existance of Information Element Fields **/

              /* Extended Scheudle Element */
              Ptr<ExtendedScheduleElement> scheduleElement =
                  StaticCast<ExtendedScheduleElement> (beacon.GetInformationElement (IE_EXTENDED_SCHEDULE));
              if (scheduleElement != 0)
                {
                  m_allocationList = scheduleElement->GetAllocationFieldList ();
                }
            }

          /* Sector Sweep Field */
          DMG_SSW_Field ssw = beacon.GetSSWField ();

          /* Map the antenna configuration, Addr1=BSSID */
          MapTxSnr (hdr->GetAddr1 (), ssw.GetSectorID (), ssw.GetDMGAntennaID (), m_stationManager->GetRxSnr ());
        }

      return;
    }
  else if (hdr->IsProbeResp ())
    {
      if (m_state == WAIT_PROBE_RESP)
        {
std::cout << "sally test dmgstamac -> Receive, case20, hdr->IsProbeResp (), m_state == WAIT_PROBE_RESP" << std::endl;

          MgtProbeResponseHeader probeResp;
          packet->RemoveHeader (probeResp);
          if (!probeResp.GetSsid ().IsEqual (GetSsid ()))
            {
              //not a probe resp for our ssid.
              return;
            }
          SetBssid (hdr->GetAddr3 ());
          Time delay = MicroSeconds (probeResp.GetBeaconIntervalUs () * m_maxLostBeacons);
          RestartBeaconWatchdog (delay);
          if (m_probeRequestEvent.IsRunning ())
            {
              m_probeRequestEvent.Cancel ();
            }
          SetState (WAIT_ASSOC_RESP);
          SendAssociationRequest ();
        }
      return;
    }
  else if (hdr->IsAssocResp ())
    {
      if (m_state == WAIT_ASSOC_RESP)
        {
std::cout << "sally test dmgstamac -> Receive, case21, hdr->IsAssocResp (), m_state == WAIT_PROBE_RESP" << std::endl;

          MgtAssocResponseHeader assocResp;
          packet->RemoveHeader (assocResp);
          if (m_assocRequestEvent.IsRunning ())
            {
              m_assocRequestEvent.Cancel ();
            }
          if (assocResp.GetStatusCode ().IsSuccess ())
            {
              m_aid = assocResp.GetAid ();
              SetState (ASSOCIATED);
              NS_LOG_DEBUG ("Association completed with " << hdr->GetAddr1 ());
              if (!m_linkUp.IsNull ())
                {
                  m_linkUp ();
                }
            }
          else
            {
              NS_LOG_DEBUG ("Association Refused");
              SetState (REFUSED);
            }
        }
      return;
  }

  DmgWifiMac::Receive (packet, hdr);
}

Ptr<DmgCapabilities>
DmgStaWifiMac::GetDmgCapabilities (void) const
{
  Ptr<DmgCapabilities> capabilities = Create<DmgCapabilities> ();
  capabilities->SetStaAddress (GetAddress ()); /* STA MAC Adress*/
  capabilities->SetAID (static_cast<uint8_t> (m_aid));

  /* DMG STA Capability Information Field */
  capabilities->SetReverseDirection (m_supportRdp);
  capabilities->SetNumberOfRxDmgAntennas (1);   /* Hardcoded Now */
  capabilities->SetNumberOfSectors (8);         /* Hardcoded Now */
  capabilities->SetRxssLength (8);              /* Hardcoded Now */
  capabilities->SetAmpduParameters (5, 0);      /* Hardcoded Now (Maximum A-MPDU + No restriction) */
  capabilities->SetSupportedMCS (12, 24, 12 ,24, false, true); /* LP SC is not supported yet */
  capabilities->SetAppduSupported (false);      /* Currently A-PPDU Agregation is not supported */
std::cout << "sally test dmgstamac -> GetDmgCapabilities, capabilities=" << capabilities << std::endl;
  return capabilities;
}

void
DmgStaWifiMac::SetState (MacState value)
{
std::cout << "sally test dmgstamac -> SetState, value=" << value << std::endl;
  enum MacState previousState = m_state;
  m_state = value;
  if (value == ASSOCIATED && previousState != ASSOCIATED)
    {
      m_assocLogger (GetBssid ());
    }
  else if (value != ASSOCIATED && previousState == ASSOCIATED)
    {
      m_deAssocLogger (GetBssid ());
    }
}

} // namespace ns3
