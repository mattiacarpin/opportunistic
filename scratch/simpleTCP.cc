#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/internet-module.h>
#include <ns3/lte-module.h>
#include <ns3/config-store-module.h>
#include <ns3/buildings-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/applications-module.h>
#include <ns3/log.h>
#include <iomanip>
#include <ios>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LenaDualStripe");


int main (int argc, char *argv[])
{
	double simTime=1.0;
	
	//Qui metto le coordinate del terminale
	float x=0;
	float y=200;
	
	
	int bw1=100;
	int earfcn1=0;
	//Definisco tutto quello che riguarda la mia stazione base, come al solito
	NodeContainer enb;
	enb.Create (1);
	
	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));		//Modello per il calcolo del CQI che valuta ciascun RB
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(320));		//Supporto per numerosi UEs
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(24.0));				//Potenza trasmissiva (dBm) utilizzabile dagli eNode
	
	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
	lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::FriisPropagationLossModel"));
  	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");			//Modello per il corretto computo dell'interferenza


	Ptr<ListPositionAllocator> positionAllocEnb = CreateObject<ListPositionAllocator> (); 		
  	positionAllocEnb->Add (Vector (0.0, 0.0,0.0)); 			
  	MobilityHelper mobilityEnb;
  	mobilityEnb.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  	mobilityEnb.SetPositionAllocator (positionAllocEnb);
 	mobilityEnb.Install (enb);
	
	lteHelper->SetEnbDeviceAttribute("DlEarfcn",UintegerValue(earfcn1));
	lteHelper->SetEnbDeviceAttribute("UlEarfcn",UintegerValue(earfcn1+18000));
	lteHelper->SetEnbDeviceAttribute("DlBandwidth",UintegerValue(bw1));
	lteHelper->SetEnbDeviceAttribute("UlBandwidth",UintegerValue(bw1));
	
	//===========================================//
	NetDeviceContainer enbDev  = lteHelper->InstallEnbDevice (enb);
	//===========================================//
	
	
	Ptr<EpcHelper> epcHelper=CreateObject<EpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);
	
	//Ora configuro il mio bel terminale!
	NodeContainer ue;
	ue.Create(1);
	Ptr<ListPositionAllocator> position = CreateObject<ListPositionAllocator> ();
	position->Add (Vector (x, y, 0.0));
	MobilityHelper ue1Mobility;
  	ue1Mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  	ue1Mobility.SetPositionAllocator (position);
 	ue1Mobility.Install (ue);
 	
	//===========================================//
	NetDeviceContainer ueDev=lteHelper->InstallUeDevice(ue);
	//===========================================//
	
	//Inizio a generare il protocollo
	Ipv4Address remoteHostAddr;
	
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	
	Ipv4InterfaceContainer ueIpIfaces;
	Ptr<Node> remoteHost;

	   // Create a single RemoteHost
	NodeContainer remoteHostContainer;
	remoteHostContainer.Create (1);
	remoteHost = remoteHostContainer.Get (0);
	InternetStackHelper internet;
	internet.Install (remoteHostContainer);

      // Create the Internet
	PointToPointHelper p2ph;
	p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
	p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
	p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
	Ptr<Node> pgw = epcHelper->GetPgwNode ();
	NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
	Ipv4AddressHelper ipv4h;
	ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
	Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
      // in this container, interface 0 is the pgw, 1 is the remoteHost
	remoteHostAddr = internetIpIfaces.GetAddress (1);


	Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
	remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);


	internet.Install (ue);
	ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDev));
	
	
	
	lteHelper->Attach(ueDev,enbDev.Get(0));
	
	//Ora configuro la mia applicazione
	uint16_t dlPort = 10000;
	uint16_t ulPort = 20000;
	// randomize a bit start times to avoid simulation artifacts
	// (e.g., buffer overflows due to packet transmissions happening
	// exactly at the same time) 
	Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable> ();
	// TCP needs to be started late enough so that all UEs are connected
	// otherwise TCP SYN packets will get lost
	startTimeSeconds->SetAttribute ("Min", DoubleValue (0.100));
	startTimeSeconds->SetAttribute ("Max", DoubleValue (0.110));

	Ptr<Node> SingleUe = ue.Get (0);
	Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (SingleUe->GetObject<Ipv4> ());
	ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);


	int u=0;
	ApplicationContainer clientApps;
	ApplicationContainer serverApps;
	BulkSendHelper dlClientHelper ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIfaces.GetAddress (u), dlPort));
    dlClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
    clientApps.Add (dlClientHelper.Install (remoteHost));
    PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory",InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    serverApps.Add (dlPacketSinkHelper.Install (SingleUe));

    BulkSendHelper ulClientHelper ("ns3::TcpSocketFactory",InetSocketAddress (remoteHostAddr, ulPort));
    ulClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
    clientApps.Add (ulClientHelper.Install (SingleUe));
    PacketSinkHelper ulPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
    serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

	Ptr<EpcTft> tft = Create<EpcTft> ();

    EpcTft::PacketFilter dlpf;
    dlpf.localPortStart = dlPort;
    dlpf.localPortEnd = dlPort;
    tft->Add (dlpf); 

    EpcTft::PacketFilter ulpf;
    ulpf.remotePortStart = ulPort;
    ulpf.remotePortEnd = ulPort;
    tft->Add (ulpf);

    EpsBearer bearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT);
    lteHelper->ActivateDedicatedEpsBearer (ueDev.Get (u), bearer, tft);

    Time startTime = Seconds (startTimeSeconds->GetValue ());
    serverApps.Start (startTime);
    clientApps.Start (startTime);

	//lteHelper->EnablePdcpTraces ();
	lteHelper->EnableTraces ();

	Simulator::Stop (Seconds (simTime));
  	Simulator::Run ();
  	Simulator::Destroy ();
  	
	return 0;
}
