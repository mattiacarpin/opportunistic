/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#include <stdio.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store.h"
#include <ns3/buildings-helper.h>


using namespace ns3;

int main (int argc, char *argv[])
{	
	//Clear the file where the association RNTI-IMSI is managed

	std::remove("RNTI_IMSI_MAP.txt");


	int numberOfNodes=10;
	double simTime=3;
	bool fading=true;
	int RBs=25;
	int earfcn1=500;
	double min_SNR_dB=10.0;
	double mean_SNR_dB=15.0;

	//LogComponentEnable ("TdMtFfMacScheduler", LOG_LEVEL_ALL);
	//LogComponentEnable ("LteEnbMac", LOG_LEVEL_ALL);


	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));		//Modello per il calcolo del CQI che valuta ciascun RB
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(160));		//Supporto per numerosi UEs
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(30.0));				//Potenza trasmissiva (dBm) utilizzabile dagli eNode
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(23.0));				//Potenza trasmissiva (dBm) utilizzabile dagli UEs
	Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue(5.0));
	Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5.0));

	//Config::SetDefault ("ns3::FriisPropagationLossModel::Frequency", DoubleValue(2150000));
	//Config::SetDefault ("ns3::FriisPropagationLossModel::SystemLoss", DoubleValue(1));


	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
	lteHelper->SetEnbDeviceAttribute("DlBandwidth",UintegerValue(RBs));
	lteHelper->SetEnbDeviceAttribute("DlEarfcn",UintegerValue(earfcn1));
	lteHelper->SetEnbDeviceAttribute("UlEarfcn",UintegerValue(earfcn1+18000));

	lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::FriisPropagationLossModel"));

	if (fading)
	{
		lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
		lteHelper->SetFadingModelAttribute ("TraceFilename", StringValue ("src/lte/model/fading-traces/FadingTrace.fad"));
		lteHelper->SetFadingModelAttribute ("TraceLength", TimeValue (Seconds (30.0)));
		lteHelper->SetFadingModelAttribute ("SamplesNum", UintegerValue (30000));
		lteHelper->SetFadingModelAttribute ("WindowSize", TimeValue (Seconds (0.5)));
		lteHelper->SetFadingModelAttribute ("RbNum", UintegerValue (25));
	}

	// Create Nodes: eNodeB and UE
	NodeContainer enbNodes;
	NodeContainer ueNodes;
	enbNodes.Create (1);
	ueNodes.Create (numberOfNodes);

	// Create Devices and install them in the Nodes (eNB and UE)
	NetDeviceContainer enbDevs;
	NetDeviceContainer ueDevs;


	//To decide where to place users, I need the link budget to compute the correct distance.
	double min_SNR=pow(10,(min_SNR_dB/10));

	double mean_SNR=pow(10,(mean_SNR_dB/10));

	double range=(mean_SNR-min_SNR)*2;

	double delta=range/(numberOfNodes-1);



	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	for (int i=0;i<numberOfNodes;i++)
	{
		double SNR_i=i*delta+min_SNR;
		double SNR_i_dB=10*log10(SNR_i);

		double K=26.33;	//This is the constant I need to compute the SNR for user

		double d=1000*pow(10,((K-SNR_i_dB)/20));

		positionAlloc->Add (Vector(d, 0, 0));
	}


	MobilityHelper mobility;
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.SetPositionAllocator(positionAlloc);
	mobility.Install(ueNodes);

	MobilityHelper mobility2;
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	Ptr<ListPositionAllocator> positionAlloc2 = CreateObject<ListPositionAllocator> ();
	positionAlloc2->Add (Vector(0, 0, 0));
	mobility2.SetPositionAllocator(positionAlloc2);
	mobility2.Install(enbNodes);

	//lteHelper->SetSchedulerType ("ns3::TdBetFfMacScheduler");
	lteHelper->SetSchedulerType ("ns3::TdMtFfMacScheduler");
	//lteHelper->SetSchedulerAttribute("HarqEnabled",BooleanValue(false));

	enbDevs = lteHelper->InstallEnbDevice (enbNodes);
	ueDevs = lteHelper->InstallUeDevice (ueNodes);

	// Attach a UE to a eNB
	lteHelper->Attach (ueDevs, enbDevs.Get (0));

	// Activate a data radio bearer
	enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
	EpsBearer bearer (q);
	lteHelper->ActivateDataRadioBearer (ueDevs, bearer);
	lteHelper->EnableTraces ();

	Simulator::Stop (Seconds (simTime));
	Simulator::Run ();

	Simulator::Destroy ();

	//Now parse the file reporting the SINR experienced by the users
	std::string line;
	std::ifstream myfile2 ("DlRsrpSinrStats.txt");

	int repetitions=0;
	std::vector<double> AverageSNR(numberOfNodes);

	if (myfile2.is_open())
	{
		while ( std::getline (myfile2,line) )
		{
			std::istringstream iss(line);
			int tab=0;
			int currentIMSI=0;
			do
			{
				std::string sub;
				iss >> sub;
				if (tab==2)
				{
					std::istringstream i(sub);
					i >> currentIMSI;
				}

				if (tab==5)
				{
					double temp=0;
					std::istringstream i(sub);
					i >> temp;
					AverageSNR[currentIMSI-1]+=temp;
				}
				tab++;
			} while (iss);
			//std::cout<<line<<std::endl;
			repetitions++;
		}
		myfile2.close();
	}
	int L=AverageSNR.size();
	for (int i=0;i<L;i++)
	{
		std::cout<<"User ["<<i+1<<"] has SINR="<<10*log10(AverageSNR[i]/(repetitions/numberOfNodes))<<" dB"<<std::endl;
	}
	return 0;
}
