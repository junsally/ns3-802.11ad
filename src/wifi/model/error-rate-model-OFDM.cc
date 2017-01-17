/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "ns3/log.h"
#include "ns3/interference-helper.h"

#include "wifi-phy.h"
#include "error-rate-model-OFDM.h"
#include "sinr-ber-lut.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ErrorRateModelOFDM");

NS_OBJECT_ENSURE_REGISTERED (ErrorRateModelOFDM);

TypeId
ErrorRateModelOFDM::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ErrorRateModelOFDM")
      .SetParent<ErrorRateModel> ()
      .AddConstructor<ErrorRateModelOFDM> ()
      ;
  return tid;
}

ErrorRateModelOFDM::ErrorRateModelOFDM ()
{

}

double
ErrorRateModelOFDM::GetCSRAfterLDPC (WifiMode mode, double sinr, uint32_t nbits, double CSR)
{
  NS_ASSERT_MSG(mode.GetModulationClass() == WIFI_MOD_CLASS_DMG_OFDM,
               "Expecting 802.11ad DMG OFDM modulation");
  std::string modename = mode.GetUniqueName ();


  /* Compute sinr in dB */
  double sinrdB = 10 * log10 (sinr);
  double ber;
  int MCS_index;
  int sinrdB_index;
  double linearK, linearB, linearY;

  /**** OFDM PHY ****/
  if (modename == "DMG_MCS15")
      MCS_index = 0;
  else if (modename == "DMG_MCS16")
      MCS_index = 1;
  else if (modename == "DMG_MCS17")
      MCS_index = 2;
  else if (modename == "DMG_MCS18")
      MCS_index = 3;
  else if (modename == "DMG_MCS19")
      MCS_index = 4;
  else if (modename == "DMG_MCS20")
      MCS_index = 5;
  else if (modename == "DMG_MCS21")
      MCS_index = 6;
  else if (modename == "DMG_MCS22")
      MCS_index = 7;
  else if (modename == "DMG_MCS23")
      MCS_index = 8;
  else if (modename == "DMG_MCS24")
      MCS_index = 9;

  else
      return CSR;

  std::cout << "sinr = " << sinr << std::endl;
  std::cout << "sinrdB = " << sinrdB << std::endl;
  std::cout << "MCS_index = " << MCS_index << std::endl;


  /* Compute BER in lookup table */
  sinrdB_index = std::floor(sinrdB) - 4;  // lookup table records from 4dB to 28dB

  if (sinrdB < 4)
      ber = sinr_ber (MCS_index, 0);
  else if (sinrdB > 28)
      ber = sinr_ber (MCS_index, 24);
  else
      linearK = log10 (sinr_ber(MCS_index, sinrdB_index)) - log10 (sinr_ber(MCS_index, sinrdB_index + 1));
      linearB = sinrdB_index * log10 (sinr_ber(MCS_index, sinrdB_index + 1)) - (sinrdB_index + 1) * log10 (sinr_ber(MCS_index, sinrdB_index));
      linearY = linearK * sinrdB + linearB;
      ber = pow (10, linearY);

  NS_LOG_DEBUG ("ber=" << ber << ", MCS_index=" << MCS_index << ", sinr=" << sinr << ", bits=" << nbits);

  /* Compute PSR from BER */
  return pow (1 - ber, nbits);
}

} // namespace ns3
