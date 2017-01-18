/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */
#ifndef ERROR_RATE_MODEL_SENSITIVITY_OFDM
#define ERROR_RATE_MODEL_SENSITIVITY_OFDM

#include <stdint.h>
#include "wifi-mode.h"
#include "error-rate-model.h"
#include "dsss-error-rate-model.h"

namespace ns3 {

class ErrorRateModelSensitivityOFDM : public ErrorRateModel
{
public:
  static TypeId GetTypeId (void);

  ErrorRateModelSensitivityOFDM ();

  virtual double GetChunkSuccessRate (WifiMode mode, WifiTxVector txVector, double sinr, uint32_t nbits) const;


private:
  double GetBerFromSensitivityLut (double deltaRSS, double Sinr) const;
  double GetBerFromSinrLut (int MCSindex, double Sinr) const;

};

} // namespace ns3

#endif /* ERROR_RATE_MODEL_SENSITIVITY_OFDM */
