/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
 *          Sébastien Deronne <sebastien.deronne@gmail.com>
 */

#include "interference-helper.h"
#include "wifi-phy.h"
#include "error-rate-model.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InterferenceHelper");

/****************************************************************
 *       Phy event class
 ****************************************************************/

InterferenceHelper::Event::Event (uint32_t size, WifiTxVector txVector,
                                  enum WifiPreamble preamble,
                                  Time duration, double rxPower)
  : m_size (size),
    m_txVector (txVector),
    m_preamble (preamble),
    m_startTime (Simulator::Now ()),
    m_endTime (m_startTime + duration),
    m_rxPowerW (rxPower)
{
}

InterferenceHelper::Event::Event (WifiTxVector txVector,
                                  Time duration,
                                  double rxPower)
  : m_size (0),
    m_txVector (txVector),
    m_preamble (WIFI_PREAMBLE_NONE),
    m_startTime (Simulator::Now ()),
    m_endTime (m_startTime + duration),
    m_rxPowerW (rxPower)
{
}

InterferenceHelper::Event::~Event ()
{
}

Time
InterferenceHelper::Event::GetDuration (void) const
{
  return m_endTime - m_startTime;
}

Time
InterferenceHelper::Event::GetStartTime (void) const
{
  return m_startTime;
}

Time
InterferenceHelper::Event::GetEndTime (void) const
{
  return m_endTime;
}

double
InterferenceHelper::Event::GetRxPowerW (void) const
{
  return m_rxPowerW;
}

uint32_t
InterferenceHelper::Event::GetSize (void) const
{
  return m_size;
}

WifiTxVector
InterferenceHelper::Event::GetTxVector (void) const
{
  return m_txVector;
}

WifiMode
InterferenceHelper::Event::GetPayloadMode (void) const
{
  return m_txVector.GetMode ();
}

enum WifiPreamble
InterferenceHelper::Event::GetPreambleType (void) const
{
  return m_preamble;
}


/****************************************************************
 *       Class which records SNIR change events for a
 *       short period of time.
 ****************************************************************/

InterferenceHelper::NiChange::NiChange (Time time, double delta)
  : m_time (time),
    m_delta (delta)
{
}

Time
InterferenceHelper::NiChange::GetTime (void) const
{
  return m_time;
}

double
InterferenceHelper::NiChange::GetDelta (void) const
{
  return m_delta;
}

bool
InterferenceHelper::NiChange::operator < (const InterferenceHelper::NiChange& o) const
{
  return (m_time < o.m_time);
}


/****************************************************************
 *       The actual InterferenceHelper
 ****************************************************************/

InterferenceHelper::InterferenceHelper ()
  : m_errorRateModel (0),
    m_firstPower (0.0),
    m_rxing (false)
{
}

InterferenceHelper::~InterferenceHelper ()
{
  EraseEvents ();
  m_errorRateModel = 0;
}

Ptr<InterferenceHelper::Event>
InterferenceHelper::Add (uint32_t size, WifiTxVector txVector,
                         enum WifiPreamble preamble,
                         Time duration, double rxPowerW)
{

std::cout << "sally test add mode: " << "size=" << size << ", time duration=" << duration << ", rxPowerW=" << rxPowerW << std::endl;
  Ptr<InterferenceHelper::Event> event;

  event = Create<InterferenceHelper::Event> (size,
                                             txVector,
                                             preamble,
                                             duration,
                                             rxPowerW);
  AppendEvent (event);
  return event;
}

void
InterferenceHelper::AddForeignSignal (Time duration, double rxPowerW)
{
std::cout << "sally test addforeignsignal: " << "duration=" << duration << ", rxpowerw=" << rxPowerW << std::endl;

  // Parameters other than duration and rxPowerW are unused for this type
  // of signal, so we provide dummy versions
  WifiTxVector fakeTxVector;
  Add (0, fakeTxVector, WIFI_PREAMBLE_NONE, duration, rxPowerW);
}

Ptr<InterferenceHelper::Event>
InterferenceHelper::Add (WifiTxVector txVector, Time duration, double rxPowerW)
{

std::cout << "sally test add mode2: " << "time duration=" << duration << ", rxPowerW=" << rxPowerW << std::endl;

  Ptr<InterferenceHelper::Event> event;

  event = Create<InterferenceHelper::Event> (txVector,
                                             duration,
                                             rxPowerW);
  AppendEvent (event);
  return event;
}

void
InterferenceHelper::SetNoiseFigure (double value)
{
  m_noiseFigure = value;
}

double
InterferenceHelper::GetNoiseFigure (void) const
{
  return m_noiseFigure;
}

void
InterferenceHelper::SetErrorRateModel (Ptr<ErrorRateModel> rate)
{
  m_errorRateModel = rate;
}

Ptr<ErrorRateModel>
InterferenceHelper::GetErrorRateModel (void) const
{
  return m_errorRateModel;
}

Time
InterferenceHelper::GetEnergyDuration (double energyW)
{
  NS_LOG_FUNCTION (this << energyW);
  Time now = Simulator::Now ();
  double noiseInterferenceW = 0.0;
  Time end = now;
  noiseInterferenceW = m_firstPower;
std::cout << "sally test noiseInterference1: " << noiseInterferenceW << " energyW: " << energyW << std::endl;

  int sally2 = 0;
  for (NiChanges::const_iterator i = m_niChanges.begin (); i != m_niChanges.end (); i++)
    {
      sally2++;
std::cout << "sally test noiseInterference2: " << noiseInterferenceW << " get delta:" << i->GetDelta() << " sally2 value: " << sally2 << std::endl;
      noiseInterferenceW += i->GetDelta ();
 
      end = i->GetTime ();
      if (end < now)
        {
          continue;
        }
      if (noiseInterferenceW < energyW)
        {
          break;
        }
    }
  return end > now ? end - now : MicroSeconds (0);
}

void
InterferenceHelper::AppendEvent (Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << event);
  Time now = Simulator::Now ();
  if (!m_rxing)
    {
std::cout << "sally test time in appendevent" << " time now=" << now << std::endl;
      NiChanges::iterator nowIterator = GetPosition (now);
      int sally1 = 0;
      for (NiChanges::iterator i = m_niChanges.begin (); i != nowIterator; i++)
        {
          sally1++;
std::cout << "sally test m_firstPower1: " << m_firstPower << " deltavalue: " << i->GetDelta() << " sally1 value: " << sally1 << std::endl;
          m_firstPower += i->GetDelta ();

        }
      m_niChanges.erase (m_niChanges.begin (), nowIterator);
      m_niChanges.insert (m_niChanges.begin (), NiChange (event->GetStartTime (), event->GetRxPowerW ()));
    }
  else
    {
std::cout << "sally test AppendEvent in rx state" << std::endl;
      AddNiChangeEvent (NiChange (event->GetStartTime (), event->GetRxPowerW ()));
    }
  AddNiChangeEvent (NiChange (event->GetEndTime (), -event->GetRxPowerW ()));

}


double
InterferenceHelper::CalculateSnr (double signal, double noiseInterference, uint32_t channelWidth) const
{
  //thermal noise at 290K in J/s = W
  static const double BOLTZMANN = 1.3803e-23;
  //Nt is the power of thermal noise in W
  double Nt = BOLTZMANN * 290.0 * channelWidth * 1000000;
  //receiver noise Floor (W) which accounts for thermal noise and non-idealities of the receiver
  double noiseFloor = m_noiseFigure * Nt;
  if (noiseInterference < 0) noiseInterference = -noiseInterference;  //sally add
  double noise = noiseFloor + noiseInterference;
  double snr = signal / noise; //linear scale
  NS_LOG_DEBUG ("bandwidth(MHz)=" << channelWidth << ", signal(W)= " << signal << ", noise(W)=" << noiseFloor << ", interference(W)=" << noiseInterference << ", snr(linear)=" << snr);

  std::cout << "bandwidth(MHz)=" << channelWidth << ", signal(W)= " << signal << ", noise(W)=" << noiseFloor << ", noise figure=" << m_noiseFigure << ", interference(W)=" << noiseInterference << ", snr(linear)=" << snr << "  sally test interference model " << std::endl;

  return snr;
}

double
InterferenceHelper::CalculateNoiseInterferenceW (Ptr<InterferenceHelper::Event> event, NiChanges *ni) const
{
  NS_LOG_FUNCTION (this << event << ni);
  double noiseInterference = m_firstPower;
std::cout << "sally test noiseInterference3: " << noiseInterference << " m_firstPower2: " << m_firstPower << std::endl;

  NS_ASSERT (m_rxing);
  int sally5=0;
  for (NiChanges::const_iterator i = m_niChanges.begin () + 1; i != m_niChanges.end (); i++)
    { 
      sally5++;
std::cout << "sally test in CalculateNoiseInterferenceW: " << "event->getEndTime=" << event->GetEndTime() << ", i->getTime=" << i->GetTime() <<", event->GetRxPowerW=" << event->GetRxPowerW() << ", i->GetDelta=" << i->GetDelta() << " sally5:" << sally5 << std::endl; 
      if ((event->GetEndTime () == i->GetTime ()) && event->GetRxPowerW () == -i->GetDelta ()) 
        {
          break;
        }
      ni->push_back (*i);
    }
  ni->insert (ni->begin (), NiChange (event->GetStartTime (), noiseInterference));
  ni->push_back (NiChange (event->GetEndTime (), 0));

  std::cout << "noise interference = " << noiseInterference << "  sally test 4" << std::endl;

  return noiseInterference;
}

double
InterferenceHelper::CalculateChunkSuccessRate (double snir, Time duration, WifiMode mode, WifiTxVector txVector) const
{
  if (duration == NanoSeconds (0))
    {
      return 1.0;
    }
  uint64_t rate = mode.GetPhyRate (txVector);
  uint64_t nbits = (uint64_t)(rate * duration.GetSeconds ());

std::cout << "sally debug nbits: " << "rate = " << rate << "duration = " << duration.GetSeconds() << "nbits = " << nbits << std::endl;

  double csr = m_errorRateModel->GetChunkSuccessRate (mode, txVector, snir, (uint32_t)nbits);
  
  std::cout << "csr = " << csr << "  sally test" << std::endl;

  return csr;
}

double
InterferenceHelper::CalculatePlcpPayloadPer (Ptr<const InterferenceHelper::Event> event, NiChanges *ni) const
{
  NS_LOG_FUNCTION (this);
  double psr = 1.0; /* Packet Success Rate */
  NiChanges::iterator j = ni->begin ();
  Time previous = (*j).GetTime ();
  WifiMode payloadMode = event->GetPayloadMode ();
  WifiPreamble preamble = event->GetPreambleType ();
  Time plcpHeaderStart = (*j).GetTime () + WifiPhy::GetPlcpPreambleDuration (event->GetTxVector (), preamble); //packet start time + preamble
  Time plcpHsigHeaderStart = plcpHeaderStart + WifiPhy::GetPlcpHeaderDuration (event->GetTxVector (), preamble); //packet start time + preamble + L-SIG
  Time plcpHtTrainingSymbolsStart = plcpHsigHeaderStart + WifiPhy::GetPlcpHtSigHeaderDuration (preamble) + WifiPhy::GetPlcpVhtSigA1Duration (preamble) + WifiPhy::GetPlcpVhtSigA2Duration (preamble); //packet start time + preamble + L-SIG + HT-SIG or VHT-SIG-A (A1 + A2)
  Time plcpPayloadStart = plcpHtTrainingSymbolsStart + WifiPhy::GetPlcpHtTrainingSymbolDuration (preamble, event->GetTxVector ()) + WifiPhy::GetPlcpVhtSigBDuration (preamble); //packet start time + preamble + L-SIG + HT-SIG or VHT-SIG-A (A1 + A2) + (V)HT Training + VHT-SIG-B
  double noiseInterferenceW = (*j).GetDelta ();
std::cout << "sally test noiseInterference5: " << noiseInterferenceW << std::endl;

  double powerW = event->GetRxPowerW ();
std::cout << "sally test rxpower: " << powerW << std::endl;

  j++;
  while (ni->end () != j)
    {
      Time current = (*j).GetTime ();
      NS_LOG_DEBUG ("previous= " << previous << ", current=" << current);
      NS_ASSERT (current >= previous);
      //Case 1: Both previous and current point to the payload
      if (previous >= plcpPayloadStart)
        {
          psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                          noiseInterferenceW,
                                                          event->GetTxVector ().GetChannelWidth ()),
                                            current - previous,
                                            payloadMode, event->GetTxVector ());

          NS_LOG_DEBUG ("Both previous and current point to the payload: mode=" << payloadMode << ", psr=" << psr);
std::cout << "sally test calculateplcppayloadper cases:  " << "Both previous and current point to the payload: mode=" << payloadMode << ", psr=" << psr << std::endl;
        }
      //Case 2: previous is before payload and current is in the payload
      else if (current >= plcpPayloadStart)
        {
          psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                          noiseInterferenceW,
                                                          event->GetTxVector ().GetChannelWidth ()),
                                            current - plcpPayloadStart,
                                            payloadMode, event->GetTxVector ());
          NS_LOG_DEBUG ("previous is before payload and current is in the payload: mode=" << payloadMode << ", psr=" << psr);
std::cout << "sally test calculateplcppayloadper cases:  " << "previous is before payload and current is in the payload: mode=" << payloadMode << ", psr=" << psr << std::endl;
        }
std::cout << "sally test noiseInterference6: " << noiseInterferenceW << " delta value: " << (*j).GetDelta() << std::endl;
      noiseInterferenceW += (*j).GetDelta ();


      previous = (*j).GetTime ();
      j++;
    }

  double per = 1 - psr;
  return per;
}

double
InterferenceHelper::CalculatePlcpHeaderPer (Ptr<const InterferenceHelper::Event> event, NiChanges *ni) const
{
  NS_LOG_FUNCTION (this);
  double psr = 1.0; /* Packet Success Rate */
  NiChanges::iterator j = ni->begin ();
  Time previous = (*j).GetTime ();
  WifiMode payloadMode = event->GetPayloadMode ();
  WifiPreamble preamble = event->GetPreambleType ();
  WifiMode htHeaderMode;
  if (preamble == WIFI_PREAMBLE_HT_MF || preamble == WIFI_PREAMBLE_HT_GF)
    {
      //mode for PLCP header fields sent with HT modulation
      htHeaderMode = WifiPhy::GetHtPlcpHeaderMode (payloadMode);
    }
  else if (preamble == WIFI_PREAMBLE_VHT)
    {
      //mode for PLCP header fields sent with VHT modulation
      htHeaderMode = WifiPhy::GetVhtPlcpHeaderMode (payloadMode);
    }
  WifiMode headerMode = WifiPhy::GetPlcpHeaderMode (payloadMode, preamble, event->GetTxVector ());
  Time plcpHeaderStart = (*j).GetTime () + WifiPhy::GetPlcpPreambleDuration (event->GetTxVector (), preamble); //packet start time + preamble
  Time plcpHsigHeaderStart = plcpHeaderStart + WifiPhy::GetPlcpHeaderDuration (event->GetTxVector (), preamble); //packet start time + preamble + L-SIG
  Time plcpHtTrainingSymbolsStart = plcpHsigHeaderStart + WifiPhy::GetPlcpHtSigHeaderDuration (preamble) + WifiPhy::GetPlcpVhtSigA1Duration (preamble) + WifiPhy::GetPlcpVhtSigA2Duration (preamble); //packet start time + preamble + L-SIG + HT-SIG or VHT-SIG-A (A1 + A2)
  Time plcpPayloadStart = plcpHtTrainingSymbolsStart + WifiPhy::GetPlcpHtTrainingSymbolDuration (preamble, event->GetTxVector ()) + WifiPhy::GetPlcpVhtSigBDuration (preamble); //packet start time + preamble + L-SIG + HT-SIG or VHT-SIG-A (A1 + A2) + (V)HT Training + VHT-SIG-B
  double noiseInterferenceW = (*j).GetDelta ();
std::cout << "sally test noiseInterference7: " << noiseInterferenceW << std::endl;

  double powerW = event->GetRxPowerW ();
  j++;
  while (ni->end () != j)
    {
      Time current = (*j).GetTime ();
      NS_LOG_DEBUG ("previous= " << previous << ", current=" << current);
std::cout << "sally test calculateplcpheaderper  " << "previous= " << previous << ", current=" << current << std::endl;

      NS_ASSERT (current >= previous);
      //Case 1: previous and current after playload start: nothing to do
      if (previous >= plcpPayloadStart)
        {
          psr *= 1;
          NS_LOG_DEBUG ("Case 1 - previous and current after playload start: nothing to do");
std::cout << "sally test calculateplcpheaderper cases  " << "Case 1 - previous and current after playload start: nothing to do" << std::endl;
        }
      //Case 2: previous is in (V)HT training or in VHT-SIG-B: Non (V)HT will not enter here since it didn't enter in the last two and they are all the same for non (V)HT
      else if (previous >= plcpHtTrainingSymbolsStart)
        {
          NS_ASSERT ((preamble != WIFI_PREAMBLE_LONG) && (preamble != WIFI_PREAMBLE_SHORT));
          //Case 2a: current after payload start
          if (current >= plcpPayloadStart)
            {
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              event->GetTxVector ().GetChannelWidth ()),
                                                plcpPayloadStart - previous,
                                                htHeaderMode, event->GetTxVector ());

              NS_LOG_DEBUG ("Case 2a - previous is in (V)HT training or in VHT-SIG-B and current after payload start: mode=" << htHeaderMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 2a - previous is in (V)HT training or in VHT-SIG-B and current after payload start: mode=" << htHeaderMode << ", psr=" << psr << std::endl;
            }
          //Case 2b: current is in (V)HT training or in VHT-SIG-B
          else
            {
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              event->GetTxVector ().GetChannelWidth ()),
                                                current - previous,
                                                htHeaderMode, event->GetTxVector ());

              NS_LOG_DEBUG ("Case 2b - previous is in (V)HT training or in VHT-SIG-B and current is in (V)HT training or in VHT-SIG-B: mode=" << htHeaderMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 2b - previous is in (V)HT training or in VHT-SIG-B and current is in (V)HT training or in VHT-SIG-B: mode=" << htHeaderMode << ", psr=" << psr << std::endl;
            }
        }
      //Case 3: previous is in HT-SIG or VHT-SIG-A: Non (V)HT will not enter here since it didn't enter in the last two and they are all the same for non (V)HT
      else if (previous >= plcpHsigHeaderStart)
        {
          NS_ASSERT ((preamble != WIFI_PREAMBLE_LONG) && (preamble != WIFI_PREAMBLE_SHORT));
          //Case 3a: current after payload start
          if (current >= plcpPayloadStart)
            {
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              event->GetTxVector ().GetChannelWidth ()),
                                                plcpPayloadStart - plcpHtTrainingSymbolsStart,
                                                htHeaderMode, event->GetTxVector ());

              //Case 3ai: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  //VHT-SIG-A is sent using legacy OFDM modulation
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 3ai - previous is in VHT-SIG-A and current after payload start: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3ai - previous is in VHT-SIG-A and current after payload start: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 3aii: HT mixed format or HT greenfield
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - previous,
                                                    htHeaderMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 3aii - previous is in HT-SIG and current after payload start: mode=" << htHeaderMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3aii - previous is in HT-SIG and current after payload start: mode=" << htHeaderMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 3b: current is in (V)HT training or in VHT-SIG-B
          else if (current >= plcpHtTrainingSymbolsStart)
            {
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              event->GetTxVector ().GetChannelWidth ()),
                                                current - plcpHtTrainingSymbolsStart,
                                                htHeaderMode, event->GetTxVector ());

              //Case 3bi: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  //VHT-SIG-A is sent using legacy OFDM modulation
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 3bi - previous is in VHT-SIG-A and current is in VHT training or in VHT-SIG-B: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3bi - previous is in VHT-SIG-A and current is in VHT training or in VHT-SIG-B: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 3bii: HT mixed format or HT greenfield
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - previous,
                                                    htHeaderMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 3bii - previous is in HT-SIG and current is in HT training: mode=" << htHeaderMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3bii - previous is in HT-SIG and current is in HT training: mode=" << htHeaderMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 3c: current with previous in HT-SIG or VHT-SIG-A
          else
            {
              //Case 3bi: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  //VHT-SIG-A is sent using legacy OFDM modulation
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 3ci - previous with current in VHT-SIG-A: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3ci - previous with current in VHT-SIG-A: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 3bii: HT mixed format or HT greenfield
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - previous,
                                                    htHeaderMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 3cii - previous with current in HT-SIG: mode=" << htHeaderMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3cii - previous with current in HT-SIG: mode=" << htHeaderMode << ", psr=" << psr << std::endl;
                }
            }
        }
      //Case 4: previous in L-SIG: HT GF will not reach here because it will execute the previous if and exit
      else if (previous >= plcpHeaderStart)
        {
          NS_ASSERT (preamble != WIFI_PREAMBLE_HT_GF);
          //Case 4a: current after payload start
          if (current >= plcpPayloadStart)
            {
              //Case 4ai: Non (V)HT format
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpPayloadStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4ai - previous in L-SIG and current after payload start: mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4ai - previous in L-SIG and current after payload start: mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 4aii: VHT format
              else if (preamble == WIFI_PREAMBLE_VHT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpPayloadStart - plcpHtTrainingSymbolsStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4aii - previous is in L-SIG and current after payload start: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4aii - previous is in L-SIG and current after payload start: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 4aiii: HT mixed format
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpPayloadStart - plcpHsigHeaderStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHsigHeaderStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4aiii - previous in L-SIG and current after payload start: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4aiii - previous in L-SIG and current after payload start: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 4b: current is in (V)HT training or in VHT-SIG-B. Non (V)HT will not come here since it went in previous if or if the previous if is not true this will be not true
          else if (current >= plcpHtTrainingSymbolsStart)
            {
              NS_ASSERT ((preamble != WIFI_PREAMBLE_LONG) && (preamble != WIFI_PREAMBLE_SHORT));

              //Case 4bi: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHtTrainingSymbolsStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4bi - previous is in L-SIG and current in VHT training or in VHT-SIG-B: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4bi - previous is in L-SIG and current in VHT training or in VHT-SIG-B: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;

                }
              //Case 4bii: HT mixed format
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHsigHeaderStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHsigHeaderStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4bii - previous in L-SIG and current in HT training: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4bii - previous in L-SIG and current in HT training: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 4c: current in HT-SIG or in VHT-SIG-A. Non (V)HT will not come here since it went in previous if or if the previous if is not true this will be not true
          else if (current >= plcpHsigHeaderStart)
            {
              NS_ASSERT ((preamble != WIFI_PREAMBLE_LONG) && (preamble != WIFI_PREAMBLE_SHORT));

              //Case 4ci: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4ci - previous is in L-SIG and current in VHT-SIG-A: mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4ci - previous is in L-SIG and current in VHT-SIG-A: mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 4cii: HT mixed format
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHsigHeaderStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHsigHeaderStart - previous,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 4cii - previous in L-SIG and current in HT-SIG: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 4cii - previous in L-SIG and current in HT-SIG: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 4d: current with previous in L-SIG
          else
            {
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              event->GetTxVector ().GetChannelWidth ()),
                                                current - previous,
                                                headerMode, event->GetTxVector ());

              NS_LOG_DEBUG ("Case 3c - current with previous in L-SIG: mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 3c - current with previous in L-SIG: mode=" << headerMode << ", psr=" << psr << std::endl;
            }
        }
      //Case 5: previous is in the preamble works for all cases
      else
        {
          //Case 5a: current after payload start
          if (current >= plcpPayloadStart)
            {
              //Case 5ai: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpPayloadStart - plcpHeaderStart,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5a - previous is in the preamble and current is after payload start: mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5a - previous is in the preamble and current is after payload start: mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 5aii: VHT format
              else if (preamble == WIFI_PREAMBLE_VHT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpPayloadStart - plcpHtTrainingSymbolsStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - plcpHeaderStart,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5aii - previous is in the preamble and current is after payload start: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5aii - previous is in the preamble and current is after payload start: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;
                }

              //Case 5aiii: HT format
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpPayloadStart - plcpHsigHeaderStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHsigHeaderStart - plcpHeaderStart, //HT GF: plcpHsigHeaderStart - plcpHeaderStart = 0
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5aiii - previous is in the preamble and current is after payload start: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5aiii - previous is in the preamble and current is after payload start: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 5b: current is in (V)HT training or in VHT-SIG-B. Non (V)HT will not come here since it went in previous if or if the previous if is not true this will be not true
          else if (current >= plcpHtTrainingSymbolsStart)
            {
              NS_ASSERT ((preamble != WIFI_PREAMBLE_LONG) && (preamble != WIFI_PREAMBLE_SHORT));

              //Case 5bi: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHtTrainingSymbolsStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHtTrainingSymbolsStart - plcpHeaderStart,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5bi - previous is in the preamble and current in VHT training or in VHT-SIG-B: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5bi - previous is in the preamble and current in VHT training or in VHT-SIG-B: VHT mode=" << htHeaderMode << ", non-VHT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 45ii: HT mixed format
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHsigHeaderStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHsigHeaderStart - plcpHeaderStart,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5bii - previous is in the preamble and current in HT training: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5bii - previous is in the preamble and current in HT training: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 5c: current in HT-SIG or in VHT-SIG-A. Non (V)HT will not come here since it went in previous if or if the previous if is not true this will be not true
          else if (current >= plcpHsigHeaderStart)
            {
              NS_ASSERT ((preamble != WIFI_PREAMBLE_LONG) && (preamble != WIFI_PREAMBLE_SHORT));

              //Case 5ci: VHT format
              if (preamble == WIFI_PREAMBLE_VHT)
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHeaderStart,
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5ci - previous is in preamble and current in VHT-SIG-A: mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5ci - previous is in preamble and current in VHT-SIG-A: mode=" << headerMode << ", psr=" << psr << std::endl;
                }
              //Case 5cii: HT mixed format
              else
                {
                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    current - plcpHsigHeaderStart,
                                                    htHeaderMode, event->GetTxVector ());

                  psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                  noiseInterferenceW,
                                                                  event->GetTxVector ().GetChannelWidth ()),
                                                    plcpHsigHeaderStart - plcpHeaderStart, //HT GF: plcpHsigHeaderStart - plcpHeaderStart = 0
                                                    headerMode, event->GetTxVector ());

                  NS_LOG_DEBUG ("Case 5cii - previous in preamble and current in HT-SIG: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5cii - previous in preamble and current in HT-SIG: HT mode=" << htHeaderMode << ", non-HT mode=" << headerMode << ", psr=" << psr << std::endl;
                }
            }
          //Case 5d: current is in L-SIG. HT GF will not come here
          else if (current >= plcpHeaderStart)
            {
              NS_ASSERT (preamble != WIFI_PREAMBLE_HT_GF);

              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              event->GetTxVector ().GetChannelWidth ()),
                                                current - plcpHeaderStart,
                                                headerMode, event->GetTxVector ());

              NS_LOG_DEBUG ("Case 5d - previous is in the preamble and current is in L-SIG: mode=" << headerMode << ", psr=" << psr);
std::cout << "sally test calculateplcpheaderper cases  " << "Case 5d - previous is in the preamble and current is in L-SIG: mode=" << headerMode << ", psr=" << psr << std::endl;
            }
        }
std::cout << "sally test noiseInterference20: " << noiseInterferenceW << " delta value: " << (*j).GetDelta() << std::endl;
      noiseInterferenceW += (*j).GetDelta ();

      previous = (*j).GetTime ();
      j++;
    }

  double per = 1 - psr;
  return per;
}

double
InterferenceHelper::CalculatePlcpTrnSnr (Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << event);
  NiChanges ni;
  double noiseInterferenceW = CalculateNoiseInterferenceW (event, &ni);
std::cout << "sally test noiseInterference8: " << noiseInterferenceW << std::endl;

  double snr = CalculateSnr (event->GetRxPowerW (),
                             noiseInterferenceW,
                             event->GetTxVector ().GetChannelWidth ());
  return snr;
}

struct InterferenceHelper::SnrPer
InterferenceHelper::CalculatePlcpPayloadSnrPer (Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << event);
  NiChanges ni;
  double noiseInterferenceW = CalculateNoiseInterferenceW (event, &ni);
std::cout << "sally test noiseInterference9: " << noiseInterferenceW << std::endl;

  double snr = CalculateSnr (event->GetRxPowerW (),
                             noiseInterferenceW,
                             event->GetTxVector ().GetChannelWidth ());

  /* calculate the SNIR at the start of the packet and accumulate
   * all SNIR changes in the snir vector.
   */
  double per = CalculatePlcpPayloadPer (event, &ni);

  struct SnrPer snrPer;
  snrPer.snr = snr;
  snrPer.per = per;
  return snrPer;
}

struct InterferenceHelper::SnrPer
InterferenceHelper::CalculatePlcpHeaderSnrPer (Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << event);
  NiChanges ni;
  double noiseInterferenceW = CalculateNoiseInterferenceW (event, &ni);
std::cout << "sally test noiseInterference10: " << noiseInterferenceW << std::endl;

  double snr = CalculateSnr (event->GetRxPowerW (),
                             noiseInterferenceW,
                             event->GetTxVector ().GetChannelWidth ());

  /* calculate the SNIR at the start of the plcp header and accumulate
   * all SNIR changes in the snir vector.
   */
  double per = CalculatePlcpHeaderPer (event, &ni);

  struct SnrPer snrPer;
  snrPer.snr = snr;
  snrPer.per = per;
  return snrPer;
}

void
InterferenceHelper::EraseEvents (void)
{
  NS_LOG_FUNCTION (this);
  m_niChanges.clear ();
  m_rxing = false;
  m_firstPower = 0.0;
}

InterferenceHelper::NiChanges::iterator
InterferenceHelper::GetPosition (Time moment)
{
  NS_LOG_FUNCTION (this << moment);
  return std::upper_bound (m_niChanges.begin (), m_niChanges.end (), NiChange (moment, 0));
}

void
InterferenceHelper::AddNiChangeEvent (NiChange change)
{
  NS_LOG_FUNCTION (this);
  m_niChanges.insert (GetPosition (change.GetTime ()), change);
}

void
InterferenceHelper::NotifyRxStart ()
{
  NS_LOG_FUNCTION (this);
  m_rxing = true;
}

void
InterferenceHelper::NotifyRxEnd ()
{
  NS_LOG_FUNCTION (this);
  m_rxing = false;
}

} //namespace ns3
