/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2004,2005 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "constant-rate-wifi-manager.h"
#include "ns3/string.h"
#include "ns3/assert.h"
#include "ns3/log.h"

#define Min(a,b) ((a < b) ? a : b)

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ConstantRateWifiManager");

NS_OBJECT_ENSURE_REGISTERED (ConstantRateWifiManager);

TypeId
ConstantRateWifiManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ConstantRateWifiManager")
    .SetParent<WifiRemoteStationManager> ()
    .SetGroupName ("Wifi")
    .AddConstructor<ConstantRateWifiManager> ()
    .AddAttribute ("DataMode", "The transmission mode to use for every data packet transmission",
                   StringValue ("OfdmRate6Mbps"),
                   MakeWifiModeAccessor (&ConstantRateWifiManager::m_dataMode),
                   MakeWifiModeChecker ())
    .AddAttribute ("ControlMode", "The transmission mode to use for every RTS packet transmission.",
                   StringValue ("OfdmRate6Mbps"),
                   MakeWifiModeAccessor (&ConstantRateWifiManager::m_ctlMode),
                   MakeWifiModeChecker ())
  ;
  return tid;
}

void
ConstantRateWifiManager::SetDataMode (WifiMode mode)
{
std::cout << "sally test ConstantRateWifiManager, SetDataMode=" << mode << std::endl;
  m_dataMode = mode;
}

void
ConstantRateWifiManager::SetControlMode (WifiMode mode)
{
std::cout << "sally test ConstantRateWifiManager, SetControlMode=" << mode << std::endl;
  m_ctlMode = mode;
}

ConstantRateWifiManager::ConstantRateWifiManager ()
{
std::cout << "sally test ConstantRateWifiManager, ConstantRateWifiManager" << std::endl;
  NS_LOG_FUNCTION (this);
}

ConstantRateWifiManager::~ConstantRateWifiManager ()
{
  NS_LOG_FUNCTION (this);
}

WifiRemoteStation *
ConstantRateWifiManager::DoCreateStation (void) const
{
std::cout << "sally test ConstantRateWifiManager, DoCreateStation" << std::endl;
  NS_LOG_FUNCTION (this);
  WifiRemoteStation *station = new WifiRemoteStation ();
  return station;
}

void
ConstantRateWifiManager::DoReportRxOk (WifiRemoteStation *station,
                                       double rxSnr, WifiMode txMode)
{
std::cout << "sally test ConstantRateWifiManager, DoReportRxOk" << std::endl;
  NS_LOG_FUNCTION (this << station << rxSnr << txMode);
}

void
ConstantRateWifiManager::DoReportRtsFailed (WifiRemoteStation *station)
{
std::cout << "sally test ConstantRateWifiManager, DoReportRtsFailed " << station << std::endl;
  NS_LOG_FUNCTION (this << station);
}

void
ConstantRateWifiManager::DoReportDataFailed (WifiRemoteStation *station)
{
std::cout << "sally test ConstantRateWifiManager, DoReportDataFailed " << station << std::endl;
  NS_LOG_FUNCTION (this << station);
}

void
ConstantRateWifiManager::DoReportRtsOk (WifiRemoteStation *st,
                                        double ctsSnr, WifiMode ctsMode, double rtsSnr)
{
std::cout << "sally test ConstantRateWifiManager, DoReportRtsOk, st " << st << ", ctsSnr=" << ctsSnr << ", ctsMode=" << ctsMode << ", rtsSnr=" << rtsSnr << std::endl;
  NS_LOG_FUNCTION (this << st << ctsSnr << ctsMode << rtsSnr);
}

void
ConstantRateWifiManager::DoReportDataOk (WifiRemoteStation *st,
                                         double ackSnr, WifiMode ackMode, double dataSnr)
{
std::cout << "sally test ConstantRateWifiManager, DoReportDataOk, st=" << st << ", ackSnr=" << ackSnr << ", ackMode=" << ackMode << ", dataSnr=" << dataSnr << std::endl;
  NS_LOG_FUNCTION (this << st << ackSnr << ackMode << dataSnr);
}

void
ConstantRateWifiManager::DoReportFinalRtsFailed (WifiRemoteStation *station)
{
std::cout << "sally test ConstantRateWifiManager, DoReportFinalRtsFailed, station=" << station << std::endl;
  NS_LOG_FUNCTION (this << station);
}

void
ConstantRateWifiManager::DoReportFinalDataFailed (WifiRemoteStation *station)
{
std::cout << "sally test ConstantRateWifiManager, DoReportFinalDataFailed, station=" << station << std::endl;
  NS_LOG_FUNCTION (this << station);
}

WifiTxVector
ConstantRateWifiManager::DoGetDataTxVector (WifiRemoteStation *st)
{
std::cout << "sally test ConstantRateWifiManager, DoGetDataTxVector, st=" << st << std::endl;
  NS_LOG_FUNCTION (this << st);
  return WifiTxVector (m_dataMode, GetDefaultTxPowerLevel (), GetLongRetryCount (st), GetShortGuardInterval (st), Min(GetNumberOfTransmitAntennas (), GetNumberOfSupportedRxAntennas (st)), 0, GetChannelWidth (st), GetAggregation (st), false);
}

WifiTxVector
ConstantRateWifiManager::DoGetRtsTxVector (WifiRemoteStation *st)
{
std::cout << "sally test ConstantRateWifiManager, DoGetRtsTxVector, st=" << st << std::endl;
  NS_LOG_FUNCTION (this << st);
  return WifiTxVector (m_ctlMode, GetDefaultTxPowerLevel (), GetShortRetryCount (st), GetShortGuardInterval (st), 1, 0, GetChannelWidth (st), GetAggregation (st), false);
}

bool
ConstantRateWifiManager::IsLowLatency (void) const
{
std::cout << "sally test ConstantRateWifiManager, IsLowLatency" << std::endl;
  NS_LOG_FUNCTION (this);
  return true;
}

} //namespace ns3
