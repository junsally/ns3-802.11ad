/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, 2016 IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */

#include "ns3/log.h"
#include "ns3/double.h"
#include "directional-flattop-antenna.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DirectionalFlatTopAntenna");

NS_OBJECT_ENSURE_REGISTERED (DirectionalFlatTopAntenna);

TypeId
DirectionalFlatTopAntenna::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DirectionalFlatTopAntenna")
    .SetGroupName ("Wifi")
    .SetParent<DirectionalAntenna> ()
    .AddConstructor<DirectionalFlatTopAntenna> ()
    .AddAttribute ("RadiationEfficiency", "The radiation efficiency of directional antenna.",
                   DoubleValue (0.9),
                   MakeDoubleAccessor (&DirectionalFlatTopAntenna::SetRadiationEfficiency,
                                       &DirectionalFlatTopAntenna::GetRadiationEfficiency),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

DirectionalFlatTopAntenna::DirectionalFlatTopAntenna ()
{
  NS_LOG_FUNCTION (this);
  m_antennas = 1;
  m_sectors = 1;
  m_omniAntenna = true;
}

DirectionalFlatTopAntenna::~DirectionalFlatTopAntenna ()
{
  NS_LOG_FUNCTION (this);
}

double
DirectionalFlatTopAntenna::GetTxGainDbi (double angle) const
{
  NS_LOG_FUNCTION (this << angle);
std::cout << "sally test tx gain: " << GetGainDbi (angle, m_txSectorId, m_txAntennaId) << " tx sector: " << m_txSectorId << std::endl;
  return GetGainDbi (angle, m_txSectorId, m_txAntennaId);
}

double
DirectionalFlatTopAntenna::GetRxGainDbi (double angle) const
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
DirectionalFlatTopAntenna::IsPeerNodeInTheCurrentSector (double angle) const
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
DirectionalFlatTopAntenna::GetGainDbi (double angle, uint8_t sectorId, uint8_t antennaId) const
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
      gain = 10 * log10 (GetRadiationEfficiency() * 2 * M_PI / m_mainLobeWidth);

std::cout << "sally test gain within mainlobe: " << "gain=" << gain << ", mainlobewidth=" << m_mainLobeWidth << std::endl;

    }
  else
    {
      gain = GetSideLobeGain ();
    }

  NS_LOG_DEBUG ("Angle=" << angle << ", LowerLimit=" << lowerLimit << ", UpperLimit=" << upperLimit
                << ", MainLobeWidth=" << m_mainLobeWidth << ", Gain=" << gain);
  return gain;
}

void 
DirectionalFlatTopAntenna::SetRadiationEfficiency (double RadEfficiency)
{
  NS_LOG_FUNCTION (this);
//  NS_ASSERT (m_mainLobeWidth/(2*M_PI) < RadEfficiency && RadEfficiency <= 1); 
  m_radiationEfficiency = RadEfficiency;
}

double 
DirectionalFlatTopAntenna::GetRadiationEfficiency (void) const
{
  NS_LOG_FUNCTION (this); 
  return m_radiationEfficiency;
}

double
DirectionalFlatTopAntenna::GetSideLobeGain (void) const
{
  NS_LOG_FUNCTION (this);
  double sideLobeGain;
  sideLobeGain = 10 * log10 ((1 - GetRadiationEfficiency()) * 2 * M_PI / (2 * M_PI - m_mainLobeWidth));
std::cout << "sally test side lobe gain: " << sideLobeGain << std::endl;

  return sideLobeGain;
}

}
