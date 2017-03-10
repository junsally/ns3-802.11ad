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
      .SetGroupName ("Wifi")
      .AddConstructor<ErrorRateModelSensitivityOFDM> ()
      ;
  return tid;
}

ErrorRateModelSensitivityOFDM::ErrorRateModelSensitivityOFDM ()
{

}

double 
ErrorRateModelSensitivityOFDM::GetBerFromSensitivityLut (double deltaRSS, double Sinr) const
{
  if ((deltaRSS < -12.0) || (Sinr < 0))
{std::cout << "sally test ber function: GetBerFromSensitivityLut - 1st case" << std::endl;
       return sensitivity_ber (0);}
   
  else if (deltaRSS > 6.0)
{std::cout << "sally test ber function: GetBerFromSensitivityLut - 2nd case" << std::endl;
       return sensitivity_ber (180);}
 
  else
{std::cout << "sally test ber function: GetBerFromSensitivityLut - 3rd case" << std::endl;
       return sensitivity_ber ((int) std::abs((10 * (deltaRSS + 12))));  }                                                         
}

double
ErrorRateModelSensitivityOFDM::GetBerFromSinrLut (int MCSindex, double Sinr) const
{
  double sinrdB;
  int sinrdB_index;
  double linearK, linearB, linearY;

  sinrdB = 10 * log10 (Sinr); 
  sinrdB_index = std::floor(sinrdB) - 4;  // lookup table records from 4dB to 28dB 
  
  if (sinrdB < 4)
{std::cout << "sally test ber function: GetBerFromSinrLut - 1st case" << std::endl;
      return sinr_ber (MCSindex, 0);}
  else if (sinrdB > 28)
{std::cout << "sally test ber function: GetBerFromSinrLut - 2nd case" << std::endl;
      return sinr_ber (MCSindex, 24);} 
  else if (sinr_ber(MCSindex, sinrdB_index) == 0)
{std::cout << "sally test ber function: GetBerFromSinrLut - 3rd case" << std::endl;
      return sinr_ber (MCSindex, sinrdB_index);}
  else if (sinr_ber(MCSindex, sinrdB_index + 1) == 0)
{
      linearK = -300 - 10*log10 (sinr_ber(MCSindex, sinrdB_index));
      linearB =  (sinrdB_index + 1) * 10*log10 (sinr_ber(MCSindex, sinrdB_index)) + sinrdB_index * 300;
      linearY = linearK * (sinrdB - 4) + linearB;
std::cout << "sinrdB=" << sinrdB << ", sinr_ber(MCSindex, sinrdB_index)=" << sinr_ber(MCSindex, sinrdB_index) << ", MCSindex=" << MCSindex << ", sinrdB_index=" << sinrdB_index << std::endl;
std::cout << "sally test ber function: GetBerFromSinrLut - 4th case" << ", linearK=" << linearK << ", linearB=" << linearB << ", linearY=" << linearY << std::endl;
      return pow (10, linearY/10);}
  else
    {
      linearK = 10*log10 (sinr_ber(MCSindex, sinrdB_index + 1)) - 10*log10 (sinr_ber(MCSindex, sinrdB_index)); 
      linearB =  (sinrdB_index + 1) * 10*log10 (sinr_ber(MCSindex, sinrdB_index)) - sinrdB_index * 10*log10 (sinr_ber(MCSindex, sinrdB_index + 1)); 
      linearY = linearK * (sinrdB - 4) + linearB;     
std::cout << "sinrdB=" << sinrdB << ", sinr_ber(MCSindex, sinrdB_index)=" << sinr_ber(MCSindex, sinrdB_index) << ", MCSindex=" << MCSindex << ", sinrdB_index=" << sinrdB_index << std::endl; 
std::cout << "sally test ber function: GetBerFromSinrLut - 5th case" << ", linearK=" << linearK << ", linearB=" << linearB << ", linearY=" << linearY << std::endl;
      return pow (10, linearY/10); 
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

std::cout << "modename before selection of ber calculation by sally: " << modename << std::endl;

  /* This is kinda silly, but convert from SNR back to RSS (Hardcoding RxNoiseFigure)*/
  double noise = 1.3803e-23 * 290.0 * txVector.GetChannelWidth () * 1000000;

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

std::cout << "ber=" << ber << ", rss_delta=" << rss_delta << ", sinr=" << sinr << ", rss=" << rss << ", bits=" << nbits << "sally test ber result in error rate model" << std::endl;

  NS_LOG_DEBUG ("ber=" << ber << ", rss_delta=" << rss_delta << ", MCS_index=" << MCS_index << ", sinr=" << sinr << ", rss=" << rss << ", bits=" << nbits);

  /* Compute PSR from BER */
  return pow (1 - ber, nbits);
}

} // namespace ns3
