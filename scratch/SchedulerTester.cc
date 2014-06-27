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
	srand(1);

	//LogComponentEnable ("TdMtFfMacScheduler", LOG_LEVEL_ALL);
	//LogComponentEnable ("LteEnbMac", LOG_LEVEL_ALL);

	//Clear the file where the association RNTI-IMSI is managed
	std::remove("numberOfNodes.txt");
	std::remove("RNTI_IMSI_MAP_2.txt");
	std::remove("SchedulerLOG.txt");
	std::remove("Metric.txt");

	int numberOfNodes=3;
	double simTime=10;
	bool fading=true;		//Enable the fading in the channel
	int RBs=25;
	int earfcn1=500;
	double min_SNR_dB=5.0;
	double mean_SNR_dB=15.0;

	std::ofstream myfile;
	myfile.open ("numberOfNodes.txt");
	myfile << numberOfNodes;
	myfile.close();

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
		lteHelper->SetFadingModelAttribute ("TraceFilename", StringValue ("src/lte/model/fading-traces/Rayleigh_channel_60.fad"));
		lteHelper->SetFadingModelAttribute ("TraceLength", TimeValue (Seconds (60.0)));
		lteHelper->SetFadingModelAttribute ("SamplesNum", UintegerValue (60000));
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
		//double SNR_i=max_SNR-i*delta;

		double SNR_i=min_SNR+i*delta;


		double SNR_i_dB=10*log10(SNR_i);

		double K=26.33;	//This is the constant I need to compute the SNR for user

		double d=1000*pow(10,((K-SNR_i_dB)/20));

		std::cout<<"User "<<i+1<<", SNR="<<SNR_i<<" ("<<SNR_i_dB<<"dB), d="<<d<<" m"<<std::endl;

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

	lteHelper->SetSchedulerType ("ns3::TdMtFfMacScheduler");
	lteHelper->SetSchedulerAttribute("HarqEnabled",BooleanValue(false));

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


	std::cout<<"=========== Statistics ==========="<<std::endl;
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

	std::cout<<"============ ACCESS PROBABILITIES ==========="<<std::endl;

	std::ifstream logFile ("SchedulerLOG.txt");
	int totalCount=0;
	std::vector<double> accessSlots(numberOfNodes);
	if (logFile.is_open())
	{
		while ( std::getline (logFile,line) )
		{
			std::istringstream iss(line);
			double time=0;
			iss>> time;
			int winnerIMSI=0;
			iss>>winnerIMSI;
			if(winnerIMSI!=0)
			{totalCount++;accessSlots[winnerIMSI-1]++;}
		}
		logFile.close();
	}
	for (int i=0;i<numberOfNodes;i++)
	{
		std::cout<<"p("<<i+1<<")="<<accessSlots[i]/totalCount<<std::endl;
	}



	/*std::cout<<"============ USER THROUGHPUT ==========="<<std::endl;
	std::ifstream txData ("DlRlcStats.txt");

	//double lastTime=0.0;
	//std::vector<double> txBytes(numberOfNodes);
	double txBytes[numberOfNodes];
	if (txData.is_open())
	{
		while ( std::getline (txData,line) )
		{
			std::istringstream iss(line);
			int tab=0;
			int IMSI=0;
			int TxB=0;
			do
			{
				if (tab==3)
					iss>>IMSI;
				if(tab==7)
					iss>>TxB;
				tab++;
			} while (iss);
			txBytes[IMSI-1]+=TxB;
		}
		txData.close();
	}*/

	return 0;
}
