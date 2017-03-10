/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */

#ifndef DIRECTIONAL_FLATTOP_ANTENNA_H
#define DIRECTIONAL_FLATTOP_ANTENNA_H

#include "directional-antenna.h"

namespace ns3 {

/**
 * \brief Directional Antenna functionality for 60 GHz based on flat-top Antenna Model.
 */
class DirectionalFlatTopAntenna : public DirectionalAntenna
{
public:
  static TypeId GetTypeId (void);
  DirectionalFlatTopAntenna (void);
  virtual ~DirectionalFlatTopAntenna (void);

  double GetSideLobeGain (void) const;

  /* Virtual Functions */
  double GetTxGainDbi (double angle) const;
  double GetRxGainDbi (double angle) const;

  virtual bool IsPeerNodeInTheCurrentSector (double angle) const;

protected:
  double GetGainDbi (double angle, uint8_t sectorId, uint8_t antennaId) const;
  void SetRadiationEfficiency (double RadEfficiency);
  double GetRadiationEfficiency (void) const;
  
  double m_radiationEfficiency;         /* radiation efficiency of directional antenna. */
};

} // namespace ns3

#endif /* DIRECTIONAL_FLATTOP_ANTENNA_H */
