/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Magister Solutions
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
 * Author: Jani Puttonen <jani.puttonen@magister.fi>
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-module.h"
#include "ns3/satellite-module.h"
#include "ns3/ipv4-l3-protocol.h"


using namespace ns3;

/**
 * \file sat-multi-application-rtn-example.cc
 * \ingroup satellite
 *
 * \brief  MultiApplicationRtn-example using satellite network in the RTN link. The
 *         example utilizes OnOff and CBR application and presents how to sent the
 *         ToS field values for them.
 *
 *         To see help for user arguments:
 *         execute command -> ./waf --run "sat-multi-application-rtn-example --PrintHelp"
 *
 *         On-Off and CBR applications have been configured to send packets from users
 *         connected to UTs to users connected to GWs (RTN link). The share of end users
 *         using CBR and On-Off may be configured by command line argument.
 */

NS_LOG_COMPONENT_DEFINE ("sat-multi-application-rtn-example");

int
main (int argc, char *argv[])
{
//  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
//  LogComponentEnable ("CbrApplication", LOG_LEVEL_ALL);
//  LogComponentEnable ("PacketSink", LOG_LEVEL_ALL);
  LogComponentEnable ("sat-multi-application-rtn-example", LOG_LEVEL_INFO);

  uint32_t endUsersPerUt (3);
  uint32_t utsPerBeam (3);
  double cbrProbability (0.5);
  uint32_t packetSize = 512;
  std::string interval = "1s";
  double simLength (15.0);

  Time appStartTime = Seconds (0.001);
  Time appStopTime = Seconds (10.0);

  /// Set simulation output details
  auto simulationHelper = CreateObject<SimulationHelper> ("example-multi-application-rtn");
  Config::SetDefault ("ns3::SatEnvVariables::EnableSimulationOutputOverwrite", BooleanValue (true));
  Config::SetDefault ("ns3::SatHelper::PacketTraceEnabled", BooleanValue (true));

  // read command line parameters given by user
  CommandLine cmd;
  cmd.AddValue ("endUsersPerUt", "Number of end users per UT", endUsersPerUt);
  cmd.AddValue ("utsPerBeam", "Number of UTs per spot-beam", utsPerBeam);
  cmd.AddValue ("cbrProbability", "Probability of CBR end users", cbrProbability);
  cmd.AddValue ("simLength", "Simulation length in seconds", simLength);
  simulationHelper->AddDefaultUiArguments (cmd);
  cmd.Parse (argc, argv);

  simulationHelper->SetUtCountPerBeam (1);
  simulationHelper->SetUserCountPerUt (1);
  simulationHelper->SetBeamSet ({12,22});
  simulationHelper->SetSimulationTime (simLength);

  // No PHY errors
  SatPhyRxCarrierConf::ErrorModel em (SatPhyRxCarrierConf::EM_NONE);
  Config::SetDefault ("ns3::SatUtHelper::FwdLinkErrorModel", EnumValue (em));
  Config::SetDefault ("ns3::SatGwHelper::RtnLinkErrorModel", EnumValue (em));

  // Creating the reference system. Note, currently the satellite module supports
  // only one reference system, which is named as "Scenario72". The string is utilized
  // in mapping the scenario to the needed reference system configuration files. Arbitrary
  // scenario name results in fatal error.
  Ptr<SatHelper> helper = simulationHelper->CreateSatScenario ();

  // Get the end users so that it is possible to attach
  // applications on them
  NodeContainer utUsers = helper->GetUtUsers ();
  NodeContainer gwUsers = helper->GetGwUsers ();

  // Random variable for sharing the UTs to CBR and On-Off users
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();

  NodeContainer utCbrUsers;
  NodeContainer utOnOffUsers;

  /* DSCP values

  BE:
  DscpDefault = 0x00,

  AF:
  DSCP_CS1 = 0x20,
  DSCP_AF11 = 0x28,
  DSCP_AF12 = 0x30,
  DSCP_AF13 = 0x38,

  DSCP_CS2 = 0x40,
  DSCP_AF21 = 0x48,
  DSCP_AF22 = 0x50,
  DSCP_AF23 = 0x58,

  DSCP_CS3 = 0x60,
  DSCP_AF31 = 0x68,
  DSCP_AF32 = 0x70,
  DSCP_AF33 = 0x78,

  DSCP_CS4 = 0x80,
  DSCP_AF41 = 0x88,
  DSCP_AF42 = 0x90,
  DSCP_AF43 = 0x98,

  EF:
  DSCP_CS5 = 0xA0,
  DSCP_EF = 0xB8,

  DSCP_CS6 = 0xC0,
  DSCP_CS7 = 0xE0
  */

  // ToS fields of applications
  uint8_t onOffTos (0x28);
  uint8_t cbrTos (0xB8);

  // Divide the users into CBR and On-Off users and set the ToS values.
  for (NodeContainer::Iterator i = utUsers.Begin ();  i != utUsers.End (); i++)
    {
      // CBR
      if (rand->GetValue () < cbrProbability)
        {
          utCbrUsers.Add (*i);
        }
      // OnOff
      else
        {
          utOnOffUsers.Add (*i);
        }
    }

  NS_LOG_INFO ("Number of created CBR users: " << utCbrUsers.GetN () << ", On-Off users: " << utOnOffUsers.GetN ());

  ApplicationContainer gwCbrSinkApps;
  ApplicationContainer gwOnOffSinkApps;
  ApplicationContainer utCbrApps;
  ApplicationContainer utOnOffApps;

  //---- Start CBR application definitions

  NS_LOG_INFO ("Creating CBR applications and sinks");

  uint16_t port = 9;
  Time startDelay = appStartTime;

  uint32_t cbrGwUserId (0);
  uint32_t onOffGwUserId (1);

  if (utCbrUsers.GetN () > 0)
    {
      // create application on UT user
      PacketSinkHelper cbrSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (helper->GetUserAddress (gwUsers.Get (cbrGwUserId)), port));
      CbrHelper cbrHelper ("ns3::UdpSocketFactory", InetSocketAddress (helper->GetUserAddress (gwUsers.Get (cbrGwUserId)), port));
      cbrHelper.SetAttribute ("Interval", StringValue (interval));
      cbrHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

  	  // Set destination addresses
  	  InetSocketAddress cbrDest (helper->GetUserAddress (gwUsers.Get (cbrGwUserId)), port);
  	  cbrDest.SetTos (cbrTos);

      // Cbr and Sink applications creation. CBR to UT users and sinks to GW users.
      gwCbrSinkApps.Add (cbrSinkHelper.Install (gwUsers.Get (cbrGwUserId)));
      gwCbrSinkApps.Get (0)->SetStartTime (Seconds (0.1));
      gwCbrSinkApps.Get (0)->SetStopTime (appStopTime);

      for ( uint32_t i = 0; i < utCbrUsers.GetN (); i++)
        {


          cbrHelper.SetAttribute ("Remote", AddressValue (Address (cbrDest)));
          cbrSinkHelper.SetAttribute ("Local", AddressValue (Address (cbrDest)));

          utCbrApps.Add (cbrHelper.Install (utCbrUsers.Get (i)));

          startDelay += Seconds (0.001);

          // Set start and end times
          utCbrApps.Get (i)->SetStartTime (startDelay);
          utCbrApps.Get (i)->SetStopTime (appStopTime);
        }
    }
  //---- Stop CBR application definitions

  //---- Start OnOff application definitions

  if (utOnOffUsers.GetN () > 0)
    {
      NS_LOG_INFO ("Creating OnOff applications and sinks");

      std::string dataRate = "100kb/s";
      std::string onTime = "2.0";
      std::string offTime = "2.0";

      Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (packetSize));
      Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (dataRate));
      Config::SetDefault ("ns3::OnOffApplication::OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + onTime + "]"));
      Config::SetDefault ("ns3::OnOffApplication::OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + offTime + "]"));

      // create helpers for application creation
      // set address of the first UT connected user
      PacketSinkHelper onOffSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (helper->GetUserAddress (gwUsers.Get (0)), port));
      OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (helper->GetUserAddress (gwUsers.Get (0)), port));

      startDelay = appStartTime;

  	  // Set destination addresses
  	  InetSocketAddress onOffDest (helper->GetUserAddress (gwUsers.Get (onOffGwUserId)), port);
  	  onOffDest.SetTos (onOffTos);

      // Cbr and Sink applications creation
      gwOnOffSinkApps.Add (onOffSinkHelper.Install (gwUsers.Get (onOffGwUserId)));
      gwOnOffSinkApps.Get (0)->SetStartTime (Seconds (0.1));
      gwOnOffSinkApps.Get (0)->SetStopTime (appStopTime);

      for ( uint32_t i = 0; i < utOnOffUsers.GetN (); i++)
        {
          onOffHelper.SetAttribute ("Remote", AddressValue (Address (onOffDest)));
          onOffSinkHelper.SetAttribute ("Local", AddressValue (Address (onOffDest)));

          utOnOffApps.Add (onOffHelper.Install (utOnOffUsers.Get (i)));

          startDelay += Seconds (0.001);

          utOnOffApps.Get (i)->SetStartTime (startDelay);
          utOnOffApps.Get (i)->SetStopTime (appStopTime);
        }
    }

  //---- Stop OnOff application definitions

  // prompt info of the used parameters
  NS_LOG_INFO ("--- sat-multi-application-rtn-example ---");
  NS_LOG_INFO ("  Packet size in bytes: " << packetSize);
  NS_LOG_INFO ("  Packet sending interval: " << interval);
  NS_LOG_INFO ("  Simulation length: " << simLength);
  NS_LOG_INFO ("  Number of UTs: " << utsPerBeam);
  NS_LOG_INFO ("  Number of end users per UT: " << endUsersPerUt);
  NS_LOG_INFO ("  ");

  simulationHelper->RunSimulation ();
  return 0;
}
