/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, 2016 IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */

#include "ns3/log.h"
#include "directional-60-ghz-antenna.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Directional60GhzAntenna");

NS_OBJECT_ENSURE_REGISTERED (Directional60GhzAntenna);

TypeId
Directional60GhzAntenna::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Directional60GhzAntenna")
    .SetGroupName ("Wifi")
    .SetParent<DirectionalAntenna> ()
    .AddConstructor<Directional60GhzAntenna> ()
  ;
  return tid;
}

Directional60GhzAntenna::Directional60GhzAntenna ()
{
  NS_LOG_FUNCTION (this);
  m_antennas = 1;
  m_sectors = 1;
  m_omniAntenna = true;
}

Directional60GhzAntenna::~Directional60GhzAntenna ()
{
  NS_LOG_FUNCTION (this);
}

double
Directional60GhzAntenna::GetTxGainDbi (double angle) const
{
  NS_LOG_FUNCTION (this << angle);
std::cout << "sally test tx gain: " << GetGainDbi (angle, m_txSectorId, m_txAntennaId) << " tx sector: " << m_txSectorId << std::endl;
  return GetGainDbi (angle, m_txSectorId, m_txAntennaId);
}

double
Directional60GhzAntenna::GetRxGainDbi (double angle) const
{
  NS_LOG_FUNCTION (this << angle);
  if (m_omniAntenna)
    {
std::cout << "sally test rx gain: " << "omni gain: " << 0 << std::endl;
      return 0;
    }
  else
    {
std::cout << "sally test rx gain: " << "not omni gain: " << GetGainDbi (angle, m_rxSectorId, m_rxAntennaId) << "rx sector: " << m_rxSectorId << std::endl;
      return GetGainDbi (angle, m_rxSectorId, m_rxAntennaId);
    }
}

bool
Directional60GhzAntenna::IsPeerNodeInTheCurrentSector (double angle) const
{
  NS_LOG_FUNCTION (this << angle);
  double lowerLimit, upperLimit;

  if (angle < 0)
    {
      angle = 2 * M_PI + angle;
    }

  lowerLimit = m_mainLobeWidth * double (m_txSectorId - 1);
  upperLimit = m_mainLobeWidth * double (m_txSectorId);

  if ((lowerLimit <= angle) && (angle <= upperLimit))
    {
      return true;
    }
  else
    {
      return false;
    }
}

double
Directional60GhzAntenna::GetGainDbi (double angle, uint8_t sectorId, uint8_t antennaId) const
{
  NS_LOG_FUNCTION (this << angle << sectorId << antennaId);
  double gain, lowerLimit, upperLimit;

  if (angle < 0)
    {
      angle = 2 * M_PI + angle;
    }

  lowerLimit = m_mainLobeWidth * double (sectorId - 1);
  upperLimit = m_mainLobeWidth * double (sectorId);

  if ((lowerLimit <= angle) && (angle <= upperLimit))
    {
      double virtualAngle = std::abs (angle - (m_mainLobeWidth/2 + m_mainLobeWidth * double (sectorId - 1)));
      gain = GetMaxGainDbi () - 3.01 * pow (2 * virtualAngle/GetHalfPowerBeamWidth (), 2);
std::cout << "sally test gain within mainlobe: " << "gain=" << gain << ", mainlobewidth=" << m_mainLobeWidth << ", max gain=" << GetMaxGainDbi() << ", virtualangle=" << virtualAngle << ", halfpowerbeamwidth=" << GetHalfPowerBeamWidth() << std::endl;

      NS_LOG_DEBUG ("VirtualAngle=" << virtualAngle);
    }
  else
    {
      gain = GetSideLobeGain ();
    }

  NS_LOG_DEBUG ("Angle=" << angle << ", LowerLimit=" << lowerLimit << ", UpperLimit=" << upperLimit
                << ", MainLobeWidth=" << m_mainLobeWidth << ", Gain=" << gain);
  return gain;
}

double
Directional60GhzAntenna::GetMaxGainDbi (void) const
{
  NS_LOG_FUNCTION (this);
  double maxGain;
  maxGain = 10 * log10 (pow (1.6162/sin (GetHalfPowerBeamWidth () / 2), 2));
  return maxGain;
}

double
Directional60GhzAntenna::GetHalfPowerBeamWidth (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mainLobeWidth/2.6;
}

double
Directional60GhzAntenna::GetSideLobeGain (void) const
{
  NS_LOG_FUNCTION (this);
  double sideLobeGain;
  sideLobeGain = -0.4111 * log(GetHalfPowerBeamWidth ()) - 10.597;
std::cout << "sally test side lobe gain: " << sideLobeGain << std::endl;

  return sideLobeGain;
}

}
