/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
 * Copyright (c) 2015,2016 IMDEA Networks Institute
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
 * Authors: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 *          Ghada Badawy <gbadawy@gmail.com>
 *          Sébastien Deronne <sebastien.deronne@gmail.com>
 *          Hany Assasa <hany.assasa@gmail.com>
 */

#include "yans-wifi-phy.h"
#include "yans-wifi-channel.h"
#include "wifi-phy-state-helper.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ampdu-tag.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("YansWifiPhy");

NS_OBJECT_ENSURE_REGISTERED (YansWifiPhy);

TypeId
YansWifiPhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::YansWifiPhy")
    .SetParent<WifiPhy> ()
    .SetGroupName ("Wifi")
    .AddConstructor<YansWifiPhy> ()
  ;
  return tid;
}

YansWifiPhy::YansWifiPhy ()
{
  NS_LOG_FUNCTION (this);
  m_antenna = 0;
  m_rdsActivated = false;
}

YansWifiPhy::~YansWifiPhy ()
{
  NS_LOG_FUNCTION (this);
}

void
YansWifiPhy::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_channel = 0;
}

bool
YansWifiPhy::DoChannelSwitch (uint16_t nch)
{
  if (!IsInitialized ())
    {
      //this is not channel switch, this is initialization
      NS_LOG_DEBUG ("initialize to channel " << nch);
      return true;
    }

  NS_ASSERT (!IsStateSwitching ());
  switch (m_state->GetState ())
    {
    case YansWifiPhy::RX:
      NS_LOG_DEBUG ("drop packet because of channel switching while reception");
      m_endPlcpRxEvent.Cancel ();
      m_endRxEvent.Cancel ();
      goto switchChannel;
      break;
    case YansWifiPhy::TX:
      NS_LOG_DEBUG ("channel switching postponed until end of current transmission");
      Simulator::Schedule (GetDelayUntilIdle (), &WifiPhy::SetChannelNumber, this, nch);
      break;
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::IDLE:
      goto switchChannel;
      break;
    case YansWifiPhy::SLEEP:
      NS_LOG_DEBUG ("channel switching ignored in sleep mode");
      break;
    default:
      NS_ASSERT (false);
      break;
    }

  return false;

switchChannel:

  NS_LOG_DEBUG ("switching channel " << GetChannelNumber () << " -> " << nch);
  m_state->SwitchToChannelSwitching (GetChannelSwitchDelay ());
  m_interference.EraseEvents ();
  /*
   * Needed here to be able to correctly sensed the medium for the first
   * time after the switching. The actual switching is not performed until
   * after m_channelSwitchDelay. Packets received during the switching
   * state are added to the event list and are employed later to figure
   * out the state of the medium after the switching.
   */
  return true;
}

bool
YansWifiPhy::DoFrequencySwitch (uint32_t frequency)
{
  if (!IsInitialized ())
    {
      //this is not channel switch, this is initialization
      NS_LOG_DEBUG ("start at frequency " << frequency);
      return true;
    }

  NS_ASSERT (!IsStateSwitching ());
  switch (m_state->GetState ())
    {
    case YansWifiPhy::RX:
      NS_LOG_DEBUG ("drop packet because of channel/frequency switching while reception");
      m_endPlcpRxEvent.Cancel ();
      m_endRxEvent.Cancel ();
      goto switchFrequency;
      break;
    case YansWifiPhy::TX:
      NS_LOG_DEBUG ("channel/frequency switching postponed until end of current transmission");
      Simulator::Schedule (GetDelayUntilIdle (), &WifiPhy::SetFrequency, this, frequency);
      break;
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::IDLE:
      goto switchFrequency;
      break;
    case YansWifiPhy::SLEEP:
      NS_LOG_DEBUG ("frequency switching ignored in sleep mode");
      break;
    default:
      NS_ASSERT (false);
      break;
    }

  return false;

switchFrequency:

  NS_LOG_DEBUG ("switching frequency " << GetFrequency () << " -> " << frequency);
  m_state->SwitchToChannelSwitching (GetChannelSwitchDelay ());
  m_interference.EraseEvents ();
  /*
   * Needed here to be able to correctly sensed the medium for the first
   * time after the switching. The actual switching is not performed until
   * after m_channelSwitchDelay. Packets received during the switching
   * state are added to the event list and are employed later to figure
   * out the state of the medium after the switching.
   */
  return true;
}

Ptr<WifiChannel>
YansWifiPhy::GetChannel (void) const
{
  return m_channel;
}

void
YansWifiPhy::SetChannel (Ptr<YansWifiChannel> channel)
{
  m_channel = channel;
  m_channel->Add (this);
}

void
YansWifiPhy::SetSleepMode (void)
{
  NS_LOG_FUNCTION (this);
  switch (m_state->GetState ())
    {
    case YansWifiPhy::TX:
      NS_LOG_DEBUG ("setting sleep mode postponed until end of current transmission");
      Simulator::Schedule (GetDelayUntilIdle (), &YansWifiPhy::SetSleepMode, this);
      break;
    case YansWifiPhy::RX:
      NS_LOG_DEBUG ("setting sleep mode postponed until end of current reception");
      Simulator::Schedule (GetDelayUntilIdle (), &YansWifiPhy::SetSleepMode, this);
      break;
    case YansWifiPhy::SWITCHING:
      NS_LOG_DEBUG ("setting sleep mode postponed until end of channel switching");
      Simulator::Schedule (GetDelayUntilIdle (), &YansWifiPhy::SetSleepMode, this);
      break;
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::IDLE:
      NS_LOG_DEBUG ("setting sleep mode");
      m_state->SwitchToSleep ();
      break;
    case YansWifiPhy::SLEEP:
      NS_LOG_DEBUG ("already in sleep mode");
      break;
    default:
      NS_ASSERT (false);
      break;
    }
}

void
YansWifiPhy::ResumeFromSleep (void)
{
  NS_LOG_FUNCTION (this);
  switch (m_state->GetState ())
    {
    case YansWifiPhy::TX:
    case YansWifiPhy::RX:
    case YansWifiPhy::IDLE:
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::SWITCHING:
      {
        NS_LOG_DEBUG ("not in sleep mode, there is nothing to resume");
        break;
      }
    case YansWifiPhy::SLEEP:
      {
        NS_LOG_DEBUG ("resuming from sleep mode");
        Time delayUntilCcaEnd = m_interference.GetEnergyDuration (DbmToW (GetCcaMode1Threshold ()));
        m_state->SwitchFromSleep (delayUntilCcaEnd);
        break;
      }
    default:
      {
        NS_ASSERT (false);
        break;
      }
    }
}

void
YansWifiPhy::SetReceiveOkCallback (RxOkCallback callback)
{
  m_state->SetReceiveOkCallback (callback);
}

void
YansWifiPhy::SetReceiveErrorCallback (RxErrorCallback callback)
{
  m_state->SetReceiveErrorCallback (callback);
}

void
YansWifiPhy::StartReceivePreambleAndHeader (Ptr<Packet> packet,
                                            double rxPowerDbm,
                                            WifiTxVector txVector,
                                            enum WifiPreamble preamble,
                                            enum mpduType mpdutype,
                                            Time rxDuration)
{
  //This function should be later split to check separately whether plcp preamble and plcp header can be successfully received.
  //Note: plcp preamble reception is not yet modeled.
  NS_LOG_FUNCTION (this << packet << rxPowerDbm << txVector.GetMode () << preamble << (uint32_t)mpdutype);
  AmpduTag ampduTag;
  Time totalDuration = rxDuration + txVector.GetTrainngFieldLength () * TRNUnit;
  rxPowerDbm += GetRxGain ();
  m_rxDuration = totalDuration; // Duraion of the last frame
  double rxPowerW = DbmToW (rxPowerDbm);
  Time endRx = Simulator::Now () + totalDuration;
  Time preambleAndHeaderDuration = CalculatePlcpPreambleAndHeaderDuration (txVector, preamble);

  if (txVector.GetMode ().GetModulationClass () == WIFI_MOD_CLASS_HT
      && (txVector.GetNss () != (1 + (txVector.GetMode ().GetMcsValue () / 8))))
    {
      NS_FATAL_ERROR ("MCS value does not match NSS value: MCS = " << (uint16_t)txVector.GetMode ().GetMcsValue () << ", NSS = " << (uint16_t)txVector.GetNss ());
    }

  Ptr<InterferenceHelper::Event> event;
  event = m_interference.Add (packet->GetSize (),
                              txVector,
                              preamble,
                              totalDuration,
                              rxPowerW);

  switch (m_state->GetState ())
    {
    case YansWifiPhy::SWITCHING:
      NS_LOG_DEBUG ("drop packet because of channel switching");
      NotifyRxDrop (packet);
      m_plcpSuccess = false;
      /*
       * Packets received on the upcoming channel are added to the event list
       * during the switching state. This way the medium can be correctly sensed
       * when the device listens to the channel for the first time after the
       * switching e.g. after channel switching, the channel may be sensed as
       * busy due to other devices' tramissions started before the end of
       * the switching.
       */
      if (endRx > Simulator::Now () + m_state->GetDelayUntilIdle ())
        {
          //that packet will be noise _after_ the completion of the
          //channel switching.
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::RX:
      NS_LOG_DEBUG ("drop packet because already in Rx (power=" <<
                    rxPowerW << "W)");
      
//      std::cout << "drop packet because already in Rx (power=" << rxPowerW << "W)" << std::endl << std::endl;         //Sally test 

      NotifyRxDrop (packet);
      if (endRx > Simulator::Now () + m_state->GetDelayUntilIdle ())
        {
          //that packet will be noise _after_ the reception of the
          //currently-received packet.
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::TX:
      NS_LOG_DEBUG ("drop packet because already in Tx (power=" <<
                    rxPowerW << "W)");
      NotifyRxDrop (packet);
      if (endRx > Simulator::Now () + m_state->GetDelayUntilIdle ())
        {
          //that packet will be noise _after_ the transmission of the
          //currently-transmitted packet.
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::IDLE:
      if (rxPowerW > GetEdThresholdW ()) //checked here, no need to check in the payload reception (current implementation assumes constant rx power over the packet duration)
        
      {
//                  std::cout << "test rxPowerW " << rxPowerW << std::endl << std::endl; //Sally test


          if (m_rdsActivated)
            {
              NS_LOG_DEBUG ("Receiving as RDS in FD-AF Mode");
              /**
               * We are working in Full Duplex-Amplify and Forward. So we receive the packet without decoding it
               * or checking its header. Then we amplify it and redirect it to the the destination.
               * We take a simple approach to model full duplex communication by swapping current steering sector.
               * Another approach would by adding new PHY layer which adds more complexity to the solution.
               */

              if ((mpdutype == NORMAL_MPDU) || (mpdutype == LAST_MPDU_IN_AGGREGATE))
                {
                  if ((m_rdsSector == m_srcSector) && (m_rdsAntenna == m_srcAntenna))
                    {
                      m_rdsSector = m_dstSector;
                      m_rdsAntenna = m_dstAntenna;

                    }
                  else
                    {
                      m_rdsSector = m_srcSector;
                      m_rdsAntenna = m_srcAntenna;
                    }

                  m_directionalAntenna->SetCurrentTxSectorID (m_rdsSector);
                  m_directionalAntenna->SetCurrentTxAntennaID (m_rdsAntenna);
                }

              /* We simply transmit on the frame on the channel without passing it to the upper layers */
              m_channel->Send (this, packet, GetPowerDbm (txVector.GetTxPowerLevel ()) + GetTxGain (),
                               txVector, preamble, mpdutype, rxDuration);
            }
          else
            {
              if (preamble == WIFI_PREAMBLE_NONE && (m_mpdusNum == 0 || m_plcpSuccess == false))
                {
                  m_plcpSuccess = false;
                  m_mpdusNum = 0;
                  NS_LOG_DEBUG ("drop packet because no PLCP preamble/header has been received");
                  NotifyRxDrop (packet);
                  goto maybeCcaBusy;
                }
              else if (preamble != WIFI_PREAMBLE_NONE && packet->PeekPacketTag (ampduTag) && m_mpdusNum == 0)
                {
                  //received the first MPDU in an A-MPDU
                  m_mpdusNum = ampduTag.GetRemainingNbOfMpdus ();
                  m_rxMpduReferenceNumber++;
                }
              else if (preamble == WIFI_PREAMBLE_NONE && packet->PeekPacketTag (ampduTag) && m_mpdusNum > 0)
                {
                  //received the other MPDUs that are part of the A-MPDU
                  if (ampduTag.GetRemainingNbOfMpdus () < (m_mpdusNum - 1))
                    {
                      NS_LOG_DEBUG ("Missing MPDU from the A-MPDU " << m_mpdusNum - ampduTag.GetRemainingNbOfMpdus ());
                      m_mpdusNum = ampduTag.GetRemainingNbOfMpdus ();
                    }
                  else
                    {
                      m_mpdusNum--;
                    }
                }
              else if (preamble != WIFI_PREAMBLE_NONE && packet->PeekPacketTag (ampduTag) && m_mpdusNum > 0)
                {
                  NS_LOG_DEBUG ("New A-MPDU started while " << m_mpdusNum << " MPDUs from previous are lost");
                  m_mpdusNum = ampduTag.GetRemainingNbOfMpdus ();
                }
              else if (preamble != WIFI_PREAMBLE_NONE && m_mpdusNum > 0 )
                {
                  NS_LOG_DEBUG ("Didn't receive the last MPDUs from an A-MPDU " << m_mpdusNum);
                  m_mpdusNum = 0;
                }

              NS_LOG_DEBUG ("sync to signal (power=" << rxPowerW << "W)");
              //sync to signal
              m_state->SwitchToRx (totalDuration);

              NS_LOG_DEBUG ("SwitchToRx=" << totalDuration);

              NS_ASSERT (m_endPlcpRxEvent.IsExpired ());
              NotifyRxBegin (packet);
              m_interference.NotifyRxStart ();

              if (preamble != WIFI_PREAMBLE_NONE)
                {
                  NS_ASSERT (m_endPlcpRxEvent.IsExpired ());
                  m_endPlcpRxEvent = Simulator::Schedule (preambleAndHeaderDuration, &YansWifiPhy::StartReceivePacket, this,
                                                      packet, txVector, preamble, mpdutype, event);
                }

              NS_ASSERT (m_endRxEvent.IsExpired ());

              /* We are in the normal mode. */
              if (txVector.GetTrainngFieldLength () == 0)
                {
                  m_endRxEvent = Simulator::Schedule (rxDuration, &YansWifiPhy::EndPsduReceive, this,
                                                      packet, preamble, mpdutype, event);
                }
              else
                {
                  m_endRxEvent = Simulator::Schedule (rxDuration, &YansWifiPhy::EndPsduOnlyReceive, this,
                                                      packet, txVector.GetPacketType (), preamble, mpdutype, event);
                }
            }
        }
      else
        {
          NS_LOG_DEBUG ("drop packet because signal power too Small (" <<
                        rxPowerW << "<" << GetEdThresholdW () << ")");
          
//            std::cout << "drop packet because signal power too Small (" << rxPowerW << "<" << GetEdThresholdW () << ")" << std::endl << std::endl; //Sally test

          NotifyRxDrop (packet);
          m_plcpSuccess = false;
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::SLEEP:
      NS_LOG_DEBUG ("drop packet because in sleep mode");
      NotifyRxDrop (packet);
      m_plcpSuccess = false;
      break;
    }

  return;

maybeCcaBusy:
  //We are here because we have received the first bit of a packet and we are
  //not going to be able to synchronize on it
  //In this model, CCA becomes busy when the aggregation of all signals as
  //tracked by the InterferenceHelper class is higher than the CcaBusyThreshold

  Time delayUntilCcaEnd = m_interference.GetEnergyDuration (DbmToW (GetCcaMode1Threshold ()));
  if (!delayUntilCcaEnd.IsZero ())
    {
      NS_LOG_DEBUG ("In CCA Busy State for " << delayUntilCcaEnd);
      m_state->SwitchMaybeToCcaBusy (delayUntilCcaEnd);
    }
  else
    {
      NS_LOG_DEBUG ("Not in CCA Busy State");
    }
}

void
YansWifiPhy::StartReceivePacket (Ptr<Packet> packet,
                                 WifiTxVector txVector,
                                 enum WifiPreamble preamble,
                                 enum mpduType mpdutype,
                                 Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << packet << txVector.GetMode () << preamble << (uint32_t)mpdutype);
  NS_ASSERT (IsStateRx ());
  NS_ASSERT (m_endPlcpRxEvent.IsExpired ());
  WifiMode txMode = txVector.GetMode ();

  struct InterferenceHelper::SnrPer snrPer;
  snrPer = m_interference.CalculatePlcpHeaderSnrPer (event);

  NS_LOG_DEBUG ("snr(dB)=" << RatioToDb (snrPer.snr) << ", per=" << snrPer.per);

  if (m_random->GetValue () > snrPer.per) //plcp reception succeeded
    {
      if (IsModeSupported (txMode) || IsMcsSupported (txMode))
        {
          NS_LOG_DEBUG ("receiving plcp payload"); //endReceive is already scheduled
          m_plcpSuccess = true;
        }
      else //mode is not allowed
        {
          NS_LOG_DEBUG ("drop packet because it was sent using an unsupported mode (" << txMode << ")");
          NotifyRxDrop (packet);
          m_plcpSuccess = false;
        }
    }
  else //plcp reception failed
    {
      NS_LOG_DEBUG ("drop packet because plcp preamble/header reception failed");
      NotifyRxDrop (packet);
      m_plcpSuccess = false;
    }
}

void
YansWifiPhy::SendPacket (Ptr<const Packet> packet, WifiTxVector txVector, WifiPreamble preamble)
{
  SendPacket (packet, txVector, preamble, NORMAL_MPDU);
}

void
YansWifiPhy::SendPacket (Ptr<const Packet> packet, WifiTxVector txVector, WifiPreamble preamble, enum mpduType mpdutype)
{
  NS_LOG_FUNCTION (this << packet << txVector.GetMode ()
    << txVector.GetMode ().GetDataRate (txVector)
    << preamble << (uint32_t)txVector.GetTxPowerLevel () << (uint32_t)mpdutype);
  /* Transmission can happen if:
   *  - we are syncing on a packet. It is the responsability of the
   *    MAC layer to avoid doing this but the PHY does nothing to
   *    prevent it.
   *  - we are idle
   */
  NS_ASSERT (!m_state->IsStateTx () && !m_state->IsStateSwitching ());

  if (txVector.GetNss () > GetNumberOfTransmitAntennas ())
    {
      NS_FATAL_ERROR ("Less TX antennas than number of spatial streams!");
    }

  bool sendTrnFields = false;

  if (m_state->IsStateSleep ())
    {
      NS_LOG_DEBUG ("Dropping packet because in sleep mode");
      NotifyTxDrop (packet);
      return;
    }

  Time frameDuration = CalculateTxDuration (packet->GetSize (), txVector, preamble, GetFrequency (), mpdutype, 1);
  Time txDuration = frameDuration;

  /* Check if the MPDU is single or last aggregate MPDU */
  if (((mpdutype == NORMAL_MPDU && preamble != WIFI_PREAMBLE_NONE) ||
       (mpdutype == LAST_MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE)) && (txVector.GetTrainngFieldLength () > 0))
    {
      NS_LOG_DEBUG ("Send TRN Fields:" << txVector.GetTrainngFieldLength ());
      txDuration += txVector.GetTrainngFieldLength () * TRNUnit;
      sendTrnFields = true;
    }

  if (m_state->IsStateRx ())
    {
      m_endPlcpRxEvent.Cancel ();
      m_endRxEvent.Cancel ();
      m_interference.NotifyRxEnd ();
    }
  NotifyTxBegin (packet);
  uint32_t dataRate500KbpsUnits;
  if (txVector.GetMode ().GetModulationClass () == WIFI_MOD_CLASS_HT || txVector.GetMode ().GetModulationClass () == WIFI_MOD_CLASS_VHT)
    {
      dataRate500KbpsUnits = 128 + txVector.GetMode ().GetMcsValue ();
    }
  else
    {
      dataRate500KbpsUnits = txVector.GetMode ().GetDataRate (txVector.GetChannelWidth (), txVector.IsShortGuardInterval (), 1) * txVector.GetNss () / 500000;
    }
  if (mpdutype == MPDU_IN_AGGREGATE && preamble != WIFI_PREAMBLE_NONE)
    {
      //send the first MPDU in an MPDU
      m_txMpduReferenceNumber++;
    }
  struct mpduInfo aMpdu;
  aMpdu.type = mpdutype;
  aMpdu.mpduRefNumber = m_txMpduReferenceNumber;
  NotifyMonitorSniffTx (packet, (uint16_t)GetFrequency (), GetChannelNumber (), dataRate500KbpsUnits, preamble, txVector, aMpdu);
  m_state->SwitchToTx (txDuration, packet, GetPowerDbm (txVector.GetTxPowerLevel ()), txVector, preamble);
  m_channel->Send (this, packet, GetPowerDbm (txVector.GetTxPowerLevel ()) + GetTxGain (), txVector, preamble, mpdutype, frameDuration);

  /* Send TRN Fields if beam refinement or tracking is required */
  if (sendTrnFields)
    {
      /* Prepare transmission of the first TRN Packet */
      Simulator::Schedule (frameDuration, &YansWifiPhy::SendTrnField, this, txVector, txVector.GetTrainngFieldLength ());
    }

  /* Accummulate the amount of Tx Duration by this station */
  m_txDuration += txDuration;
  /* New Trace for Tx End */
  Simulator::Schedule (txDuration, &WifiPhy::NotifyTxEnd, this, packet);
}

void
YansWifiPhy::SendTrnField (WifiTxVector txVector, uint8_t fieldsRemaining)
{
  NS_LOG_FUNCTION (this << txVector.GetMode () << uint (fieldsRemaining));
  if (txVector.GetPacketType () == TRN_T)
    {
      /* Change Sector ID at the begining of each TRN-T field */
      m_directionalAntenna->SetCurrentTxSectorID (fieldsRemaining);
    }

  fieldsRemaining--;
  if (fieldsRemaining != 0)
    {
      Simulator::Schedule (TRNUnit, &YansWifiPhy::SendTrnField, this, txVector, fieldsRemaining);
    }

  m_channel->SendTrn (this, GetPowerDbm (txVector.GetTxPowerLevel ()) + GetTxGain (), txVector, fieldsRemaining);
}

void
YansWifiPhy::StartReceiveTrnField (WifiTxVector txVector, double rxPowerDbm, uint8_t fieldsRemaining)
{
  NS_LOG_FUNCTION (this << txVector.GetMode () << rxPowerDbm << fieldsRemaining);
  double rxPowerW = DbmToW (rxPowerDbm);
  if (m_plcpSuccess)
    {
      /* Add Interference event for TRN field */
      Ptr<InterferenceHelper::Event> event;
      event = m_interference.Add (txVector,
                                  TRNUnit,
                                  rxPowerW);

      /* Schedule an event for the complete reception of this TRN Field */
      Simulator::Schedule (TRNUnit, &YansWifiPhy::EndReceiveTrnField, this,
                           m_directionalAntenna->GetCurrentRxSectorID (), m_directionalAntenna->GetCurrentRxAntennaID (),
                           txVector, fieldsRemaining, event);

      if (txVector.GetPacketType () == TRN_R)
        {
          /* Change Rx Sector for the next TRN Field */
          m_directionalAntenna->SetCurrentRxSectorID (m_directionalAntenna->GetNextRxSectorID ());
        }
    }
  else
    {
      NS_LOG_DEBUG ("Drop TRN Field because signal power too Small (" << rxPowerW << "<" << GetEdThresholdW ());
   
      std::cout << "Drop TRN Field because signal power too Small (" << rxPowerW << "<" << GetEdThresholdW () << std::endl << std::endl;

    }
}

void
YansWifiPhy::EndReceiveTrnField (uint8_t sectorId, uint8_t antennaId,
                                 WifiTxVector txVector, uint8_t fieldsRemaining,
                                 Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << sectorId << antennaId << txVector.GetMode () << fieldsRemaining << event);

  /* Calculate SNR and report it to the upper layer */
  double snr;
  snr = m_interference.CalculatePlcpTrnSnr (event);
  m_reportSnrCallback (sectorId, antennaId, fieldsRemaining, snr, (txVector.GetPacketType () == TRN_T));

  /* Check if this is the last TRN field in the current transmission */
  if (fieldsRemaining == 0)
    {
      EndReceiveTrnFields ();
    }
}

void
YansWifiPhy::EndReceiveTrnFields (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (IsStateRx ());
  m_interference.NotifyRxEnd ();

  if (m_plcpSuccess && m_psduSuccess)
    {
      m_state->SwitchFromRxEndOk ();
    }
  else
    {
      m_state->SwitchFromRxEndError ();
    }
}

void
YansWifiPhy::RegisterReportSnrCallback (ReportSnrCallback callback)
{
  NS_LOG_FUNCTION (this);
  m_reportSnrCallback = callback;
}

void
YansWifiPhy::ActivateRdsOpereation (uint8_t srcSector, uint8_t srcAntenna,
                                    uint8_t dstSector, uint8_t dstAntenna)
{
  NS_LOG_FUNCTION (this << srcSector << srcAntenna << dstSector << dstAntenna);
  m_rdsActivated = true;
  m_rdsSector = m_srcSector = srcSector;
  m_rdsAntenna = m_srcAntenna = srcAntenna;
  m_dstSector = dstSector;
  m_dstAntenna = dstAntenna;
}

void
YansWifiPhy::ResumeRdsOperation (void)
{
  NS_LOG_FUNCTION (this);
  m_rdsActivated = true;
  m_rdsSector = m_srcSector;
  m_rdsAntenna = m_srcAntenna;
}

void
YansWifiPhy::SuspendRdsOperation (void)
{
  NS_LOG_FUNCTION (this);
  m_rdsActivated = false;
}

void
YansWifiPhy::RegisterListener (WifiPhyListener *listener)
{
  m_state->RegisterListener (listener);
}

void
YansWifiPhy::UnregisterListener (WifiPhyListener *listener)
{
  m_state->UnregisterListener (listener);
}

void
YansWifiPhy::EndPsduReceive (Ptr<Packet> packet, enum WifiPreamble preamble, enum mpduType mpdutype, Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << packet << event);
  NS_ASSERT (IsStateRx ());
  NS_ASSERT (event->GetEndTime () == Simulator::Now ());

  struct InterferenceHelper::SnrPer snrPer;
  snrPer = m_interference.CalculatePlcpPayloadSnrPer (event);
  m_interference.NotifyRxEnd ();

  if (m_plcpSuccess == true)
    {
      NS_LOG_DEBUG ("mode=" << (event->GetPayloadMode ().GetDataRate (event->GetTxVector ())) <<
                    ", snr(dB)=" << RatioToDb (snrPer.snr) << ", per=" << snrPer.per << ", size=" << packet->GetSize ());
      double rnd = m_random->GetValue ();
      if (rnd > snrPer.per)
        {
          NotifyRxEnd (packet);
          uint32_t dataRate500KbpsUnits;
          if ((event->GetPayloadMode ().GetModulationClass () == WIFI_MOD_CLASS_HT) || (event->GetPayloadMode ().GetModulationClass () == WIFI_MOD_CLASS_VHT))
            {
              dataRate500KbpsUnits = 128 + event->GetPayloadMode ().GetMcsValue ();
            }
          else
            {
              dataRate500KbpsUnits = event->GetPayloadMode ().GetDataRate (event->GetTxVector ().GetChannelWidth (), event->GetTxVector ().IsShortGuardInterval (), 1) * event->GetTxVector ().GetNss () / 500000;
            }
          struct signalNoiseDbm signalNoise;
          signalNoise.signal = RatioToDb (event->GetRxPowerW ()) + 30;
          signalNoise.noise = RatioToDb (event->GetRxPowerW () / snrPer.snr) - GetRxNoiseFigure () + 30;
          struct mpduInfo aMpdu;
          aMpdu.type = mpdutype;
          aMpdu.mpduRefNumber = m_rxMpduReferenceNumber;
          NotifyMonitorSniffRx (packet, (uint16_t)GetFrequency (), GetChannelNumber (), dataRate500KbpsUnits, event->GetPreambleType (), event->GetTxVector (), aMpdu, signalNoise);
          m_state->SwitchFromRxEndOk (packet, snrPer.snr, event->GetTxVector (), event->GetPreambleType ());
        }
      else
        {
          /* failure. */
          NS_LOG_DEBUG ("drop packet because the probability to receive it = " << rnd << " is lower than " << snrPer.per);
          NotifyRxDrop (packet);
          m_state->SwitchFromRxEndError (packet, snrPer.snr);
        }
    }
  else
    {
      m_state->SwitchFromRxEndError (packet, snrPer.snr);
    }

  if (preamble == WIFI_PREAMBLE_NONE && mpdutype == LAST_MPDU_IN_AGGREGATE)
    {
      m_plcpSuccess = false;
    }
}

void
YansWifiPhy::EndPsduOnlyReceive (Ptr<Packet> packet, PacketType packetType, enum WifiPreamble preamble, enum mpduType mpdutype, Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << packet << event);
  NS_ASSERT (IsStateRx ());

  bool isEndOfFrame = ((mpdutype == NORMAL_MPDU && preamble != WIFI_PREAMBLE_NONE) || (mpdutype == LAST_MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE));
  struct InterferenceHelper::SnrPer snrPer;
  snrPer = m_interference.CalculatePlcpPayloadSnrPer (event);

  if (m_plcpSuccess == true)
    {
      NS_LOG_DEBUG ("mode=" << (event->GetPayloadMode ().GetDataRate ()) <<
                    ", snr(dB)=" << RatioToDb(snrPer.snr) << ", per=" << snrPer.per << ", size=" << packet->GetSize ());
      double rnd = m_random->GetValue ();
      m_psduSuccess = (rnd > snrPer.per);
      if (m_psduSuccess)
        {
          NotifyRxEnd (packet);
          uint32_t dataRate500KbpsUnits;
          if ((event->GetPayloadMode ().GetModulationClass () == WIFI_MOD_CLASS_HT) || (event->GetPayloadMode ().GetModulationClass () == WIFI_MOD_CLASS_VHT))
            {
              dataRate500KbpsUnits = 128 + event->GetPayloadMode ().GetMcsValue ();
            }
          else
            {
              dataRate500KbpsUnits = event->GetPayloadMode ().GetDataRate (event->GetTxVector ().GetChannelWidth (), event->GetTxVector ().IsShortGuardInterval (), 1) * event->GetTxVector ().GetNss () / 500000;
            }
          struct signalNoiseDbm signalNoise;
          signalNoise.signal = RatioToDb (event->GetRxPowerW ()) + 30;
          signalNoise.noise = RatioToDb (event->GetRxPowerW () / snrPer.snr) - GetRxNoiseFigure () + 30;
          struct mpduInfo aMpdu;
          aMpdu.type = mpdutype;
          aMpdu.mpduRefNumber = m_rxMpduReferenceNumber;
          NotifyMonitorSniffRx (packet, (uint16_t)GetFrequency (), GetChannelNumber (), dataRate500KbpsUnits, event->GetPreambleType (), event->GetTxVector (), aMpdu, signalNoise);
          m_state->ReportPsduEndOk (packet, snrPer.snr, event->GetTxVector (), event->GetPreambleType ());
        }
      else
        {
          /* failure. */
          NS_LOG_DEBUG ("drop packet because the probability to receive it = " << rnd << " is lower than " << snrPer.per);
          NotifyRxDrop (packet);
          m_state->ReportPsduEndError (packet, snrPer.snr);
        }
    }
  else
    {
      m_state->ReportPsduEndError (packet, snrPer.snr);
    }

  if (preamble == WIFI_PREAMBLE_NONE && mpdutype == LAST_MPDU_IN_AGGREGATE)
    {
      m_plcpSuccess = false;
    }

  if (isEndOfFrame && (packetType == TRN_R))
    {
      /* If the received frame has TRN-R Fields, we should sweep antenna configuration at the beginning of each field */
      m_directionalAntenna->SetInDirectionalReceivingMode ();
      m_directionalAntenna->SetCurrentRxSectorID (1);
      m_directionalAntenna->SetCurrentRxAntennaID (1);
    }
}

} //namespace ns3
