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
#include "error-rate-model-sensitivityOFDM.h"
#include "sensitivity-lut.h"
#include "sinr-ber-lut.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ErrorRateModelSensitivityOFDM");

NS_OBJECT_ENSURE_REGISTERED (ErrorRateModelSensitivityOFDM);

TypeId
ErrorRateModelSensitivityOFDM::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ErrorRateModelSensitivityOFDM")
      .SetParent<ErrorRateModel> ()
      .AddConstructor<ErrorRateModelSensitivityOFDM> ()
      ;
  return tid;
}

ErrorRateModelSensitivityOFDM::ErrorRateModelSensitivityOFDM ()
{

}

double 
ErrorRateModelSensitivityOFDM::GetBerFromSensitivityLut (double deltaRSS, const double Sinr)
{
  if ((deltaRSS < -12.0) || (Sinr < 0))
       return sensitivity_ber (0);
   
  else if (deltaRSS > 6.0)
       return sensitivity_ber (180);
 
  else
       return sensitivity_ber ((int) std::abs((10 * (rss_delta + 12))));                                                           
}

double
ErrorRateModelSensitivityOFDM::GetBerFromSinrLut (int MCSindex, const double Sinr)
{
  sinrdB = 10 * log10 (Sinr); 
  sinrdB_index = std::floor(sinrdB) - 4;  // lookup table records from 4dB to 28dB 
  
  if (sinrdB < 4)
      return sinr_ber (MCS_index, 0);
  else if (sinrdB > 28)
      return sinr_ber (MCS_index, 24); 
  else
    {
      linearK = log10 (sinr_ber(MCS_index, sinrdB_index)) - log10 (sinr_ber(MCS_index, sinrdB_index + 1)); 
      linearB = sinrdB_index * log10 (sinr_ber(MCS_index, sinrdB_index + 1)) - (sinrdB_index + 1) * log10 (sinr_ber(MCS_index, sinrdB_index)); 
      linearY = linearK * sinrdB + linearB;     
      return pow (10, linearY); 
    }
}


double
ErrorRateModelSensitivityOFDM::GetChunkSuccessRate (WifiMode mode, WifiTxVector txVector, double sinr, uint32_t nbits) const
{
  NS_ASSERT_MSG(mode.GetModulationClass () == WIFI_MOD_CLASS_DMG_CTRL ||
    mode.GetModulationClass() == WIFI_MOD_CLASS_DMG_SC ||
    mode.GetModulationClass() == WIFI_MOD_CLASS_DMG_OFDM,
               "Expecting 802.11ad DMG CTRL, SC or OFDM modulation");
  std::string modename = mode.GetUniqueName ();

  /* This is kinda silly, but convert from SNR back to RSS (Hardcoding RxNoiseFigure)*/
  double noise = 1.3803e-23 * 290.0 * txVector.GetChannelWidth () * 10;

  /* Compute RSS in dBm, so add 30 from SNR */
  double rss = 10 * log10 (sinr * noise) + 30;
  double rss_delta;
  double ber;
  int MCS_index;

  /**** Control PHY ****/
  if (modename == "DMG_MCS0")
  {
     rss_delta = rss - -78;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }  

  /**** SC PHY ****/
  else if (modename == "DMG_MCS1")
  {
     rss_delta = rss - -68;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS2")
  {
     rss_delta = rss - -66;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS3")
  {
     rss_delta = rss - -65;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS4")
  {
     rss_delta = rss - -64;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  } 
  else if (modename == "DMG_MCS5")
  {
     rss_delta = rss - -62;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS6")
  {
     rss_delta = rss - -63;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }  
  else if (modename == "DMG_MCS7")
  {
     rss_delta = rss - -62;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS8")
  {
     rss_delta = rss - -61;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);  
  }
  else if (modename == "DMG_MCS9")
  {
     rss_delta = rss - -59;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS10")
  {
     rss_delta = rss - -55;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS11")
  {
     rss_delta = rss - -54;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS12")
  {
     rss_delta = rss - -53;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }


  /**** OFDM PHY ****/
  else if (modename == "DMG_MCS13")
  {
     rss_delta = rss - -66;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS14")
  {
     rss_delta = rss - -64;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS15")
  {
     MCS_index = 0;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS16")
  {
     MCS_index = 1;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS17")
  {
     MCS_index = 2;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS18")
  {
     MCS_index = 3;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS19")
  {
     MCS_index = 4;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS20")
  {
     MCS_index = 5;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS21")
  {
     MCS_index = 6;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS22")
  {
     MCS_index = 7;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS23")
  {
     MCS_index = 8;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }
  else if (modename == "DMG_MCS24")
  {
     MCS_index = 9;
     ber = GetBerFromSinrLut (MCS_index, sinr);
  }

  /**** Low power PHY ****/
  else if (modename == "DMG_MCS25")
  {
     rss_delta = rss - -64;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS26")
  {
     rss_delta = rss - -60;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else if (modename == "DMG_MCS27")
  {
     rss_delta = rss - -57;
     ber = GetBerFromSensitivityLut (rss_delta, sinr);
  }
  else
      NS_FATAL_ERROR("Unrecognized 60 GHz modulation");

  std::cout << "snr = " << snr << std::endl;
  std::cout << "noise = " << noise << std::endl;
  std::cout << "rss = " << rss << std::endl;
  std::cout << "rss_delta = " << rss_delta << std::endl;
  std::cout << "no abs = " << (10 * (rss_delta + 12)) << std::endl;
  std::cout << "with abs = " << abs((10 * (rss_delta + 12))) << std::endl << std::endl;


  NS_LOG_DEBUG ("SENSITIVITY: ber=" << ber << ", rss_delta=" << rss_delta << ", snr=" << snr << ", rss=" << rss << ", bits=" << nbits);

  /* Compute PSR from BER */
  return pow (1 - ber, nbits);
}

} // namespace ns3
