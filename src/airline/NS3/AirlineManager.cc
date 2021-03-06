/*
 * Copyright (C) 2017 Rahul Jadhav <nyrahul@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU
 * General Public License v2. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     airline
 * @{
 *
 * @file
 * @brief       NS3 specific airline module for managing virtual node set.
 *
 * @author      Rahul Jadhav <nyrahul@gmail.com>
 *
 * @}
 */

#define	_AIRLINEMANAGER_CC_

#include "AirlineManager.h"
#include "Airline.h"
#include "Command.h"
#include "mac_stats.h"
#include "Nodeinfo.h"

int AirlineManager::cmd_node_position(uint16_t id, char *buf, int buflen)
{
	int n=0;
	ofstream of;
	NodeContainer const & nodes = NodeContainer::GetGlobal (); 
	if(buf[0]) {
		of.open(buf);
		if(!of.is_open()) {
			return snprintf(buf, buflen, "could not open file %s", buf);
		} else {
			n = snprintf(buf, buflen, "SUCCESS");
		}
	}
	for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i) 
	{ 
		Ptr<Node> node = *i; 
		//std::string name = Names::FindName (node);
		Ptr<MobilityModel> mob = node->GetObject<MobilityModel> (); 
		if (! mob) continue; // Strange, node has no mobility model installed. Skip. 

		Vector pos = mob->GetPosition (); 
		Ptr<LrWpanNetDevice> dev = node->GetDevice(0)->GetObject<LrWpanNetDevice>();
		if(id == 0xffff || id == node->GetId()) {
			if(of.is_open()) {
				of << "Node " << node->GetId() << " Location= " << pos.x << " " << pos.y << " " << pos.z 
					  << "\n"; 
			} else {
				n += snprintf(buf+n, buflen-n, "%d loc= %.2f %.2f %.2f\n", node->GetId(), pos.x, pos.y, pos.z);
				if(n > (buflen-50)) {
					n += snprintf(buf+n, buflen-n, "[TRUNC]");
					break;
				}
			}
		}
	}
	of.close();
	return n;
}

void AirlineManager::setPositionAllocator(NodeContainer & nodes)
{
	MobilityHelper mobility;
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	//TODO: In the future this could support different types of mobility models

	if(CFG("topologyType") == "grid") {
		int gw=stoi(CFG("gridWidth"));
		INFO << "Using GridPositionAllocator\n";
		mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
				"MinX", DoubleValue(.0),
				"MinY", DoubleValue(.0),
				"DeltaX", DoubleValue(stod(CFG("fieldX"))/gw),
				"DeltaY", DoubleValue(stod(CFG("fieldY"))/gw),
				"GridWidth", UintegerValue(gw),
				"LayoutType", StringValue("RowFirst"));
	} else if(CFG("topologyType") == "randrect") {
		char x_buf[128], y_buf[128];
		snprintf(x_buf, sizeof(x_buf), "ns3::UniformRandomVariable[Min=0.0|Max=%s]", CFG("fieldX").c_str());
		snprintf(y_buf, sizeof(y_buf), "ns3::UniformRandomVariable[Min=0.0|Max=%s]", CFG("fieldY").c_str());
		INFO << "Using RandomRectanglePositionAllocator\n";
		mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
				"X", StringValue(x_buf),
				"Y", StringValue(y_buf));
	} else {
		ERROR << "Unknown topologyType: " << CFG("topologyType") << " in cfg\n";
		throw FAILURE;
	}
	mobility.Install (nodes);
}

int AirlineManager::cmd_set_node_position(uint16_t id, char *buf, int buflen)
{
	char *ptr, *saveptr;
	double x, y, z=0;
	NodeContainer const & nodes = NodeContainer::GetGlobal (); 
	int numNodes = stoi(CFG("numOfNodes"));

	if(!IN_RANGE(id, 0, numNodes)) {
		return snprintf(buf, buflen, "NodeID mandatory for setting node pos id=%d", id);
	}
	ptr = strtok_r(buf, " ", &saveptr);
	if(!ptr) return snprintf(buf, buflen, "invalid loc format! No x pos!");
	x=stod(ptr);
	ptr = strtok_r(NULL, " ", &saveptr);
	if(!ptr) return snprintf(buf, buflen, "invalid loc format! No y pos!");
	y=stod(ptr);
	ptr = strtok_r(NULL, " ", &saveptr);
	if(ptr) z = stod(ptr);

	Ptr<MobilityModel> mob = nodes.Get(id)->GetObject<MobilityModel>();
	Vector m_position = mob->GetPosition();
	m_position.x = x;
	m_position.y = y;
	m_position.z = z;
	mob->SetPosition(m_position);
	return snprintf(buf, buflen, "SUCCESS");
}

void AirlineManager::msgrecvCallback(msg_buf_t *mbuf)
{
	NodeContainer const & n = NodeContainer::GetGlobal (); 

	if(mbuf->flags & MBUF_IS_CMD) {
		if(0) { } 
		HANDLE_CMD(mbuf, cmd_node_position)	//NS3 Airline specific command
		HANDLE_CMD(mbuf, cmd_set_node_position)	//NS3 Airline specific command
		else {
			al_handle_cmd(mbuf);
		}
		cl_sendto_q(MTYPE(MONITOR, CL_MGR_ID), mbuf, mbuf->len+sizeof(msg_buf_t));
		return;
	}
	int numNodes = stoi(CFG("numOfNodes"));
	if(!IN_RANGE(mbuf->src_id, 0, numNodes)) {
        ERROR << "rcvd src id=" << mbuf->src_id << " out of range!!\n";
		return;
	}
    if(mbuf->dst_id == CL_DSTID_MACHDR_PRESENT) {
        if(CFG_INT("macHeaderAdd", 1)) {
            ERROR << "rcvd a packet from stackline with DSTID_MACHDR_PRESENT set but config file does not have macHeaderAdd=0\n";
            ERROR << "If you are using openthread, please set macHeaderAdd=0 to prevent Airline from adding its own mac hdr\n";
            return;
        }
    }
	Ptr<Application> nodeApp = n.Get(mbuf->src_id)->GetApplication(0);
	if(!nodeApp) {
		ERROR << "Could not handle msg_buf_t for node " << (int)mbuf->src_id << endl;
		return;
	}
	Ptr<Airline> aline = DynamicCast<Airline> (nodeApp);
	aline->tx(mbuf);
}

void AirlineManager::nodePos(NodeContainer const & nodes, uint16_t id, double & x, double & y, double & z)
{
	MobilityHelper mob;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (x, y, z));
	mob.SetPositionAllocator (positionAlloc);
	mob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mob.Install(nodes.Get(id));
}

void AirlineManager::setNodeSpecificPosition(NodeContainer & nodes) 
{
	uint8_t is_set=0;
	double x, y, z;
	wf::Nodeinfo *ni=NULL;
	for(int i=0;i<(int)nodes.GetN();i++) {
		ni=WF_config.get_node_info(i);
		if(!ni) {
			ERROR << "GetN doesnt match nodes stored in config!!\n";
			return;
		}
		ni->getNodePosition(is_set, x, y, z);
		if(!is_set) continue;
		nodePos(nodes, i, x, y, z);
	}
}

void AirlineManager::setMacHeaderAdd(NodeContainer & nodes)
{
    bool macAdd=CFG_INT("macHeaderAdd", 1);
    if(macAdd) {
        return;
    }
	for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i) 
	{ 
		Ptr<Node> node = *i; 
		Ptr<LrWpanNetDevice> dev = node->GetDevice(0)->GetObject<LrWpanNetDevice>();
        if(dev) {
            dev->GetMac()->SetMacHeaderAdd(macAdd);

            //In case where stackline itself add mac header, the airline needs
            //to be set in promiscuous mode so that all the packets with
            //headers are transmitted as is to the stackline on reception
            //dev->GetMac()->SetPromiscuousMode(1);
        }
	}
}

int AirlineManager::startNetwork(wf::Config & cfg)
{
	try {
		GlobalValue::Bind ("ChecksumEnabled", 
            BooleanValue (CFG_INT("macChecksumEnabled", 1)));
		GlobalValue::Bind ("SimulatorImplementationType", 
		   StringValue ("ns3::RealtimeSimulatorImpl"));

		wf::Macstats::clear();

		NodeContainer nodes;
		nodes.Create (cfg.getNumberOfNodes());
		INFO << "Creating " << cfg.getNumberOfNodes() << " nodes..\n";
		SeedManager::SetSeed(stoi(CFG("randSeed", "0xbabe"), nullptr, 0));

		setPositionAllocator(nodes);

		setNodeSpecificPosition(nodes);

		LrWpanHelper lrWpanHelper;
		NetDeviceContainer devContainer = lrWpanHelper.Install(nodes);
		lrWpanHelper.AssociateToPan (devContainer, CFG_PANID);

		string ns3_capfile = CFG("NS3_captureFile");
		if(!ns3_capfile.empty()) {
			INFO << "NS3 Capture File:" << ns3_capfile << endl;
			lrWpanHelper.EnablePcapAll (ns3_capfile, false /*promiscuous*/);
		}

        setMacHeaderAdd(nodes);

		AirlineHelper airlineApp;
		ApplicationContainer apps = airlineApp.Install(nodes);
		apps.Start(Seconds(0.0));

		ScheduleCommlineRX();
		INFO << "NS3 Simulator::Run initiated...\n";
		Simulator::Run ();
		pause();
		Simulator::Destroy ();
	} catch (int e) {
		ERROR << "Configuration failed\n";
		return FAILURE;
	}
	return SUCCESS;
}

void AirlineManager::ScheduleCommlineRX(void)
{
	m_sendEvent = Simulator::Schedule (Seconds(0.001), &AirlineManager::msgReader, this);
}

void AirlineManager::msgReader(void)
{
	DEFINE_MBUF(mbuf);
	while(1) {
		cl_recvfrom_q(MTYPE(AIRLINE,CL_MGR_ID), mbuf, sizeof(mbuf_buf), CL_FLAG_NOWAIT);
		if(mbuf->len) {
			msgrecvCallback(mbuf);
			usleep(1);
		} else {
			break;
		}
	}
	ScheduleCommlineRX();
}

AirlineManager::AirlineManager(wf::Config & cfg)
{
	m_sendEvent = EventId ();
	startNetwork(cfg);
	INFO << "AirlineManager started" << endl;
}

AirlineManager::~AirlineManager() 
{
	Simulator::Cancel (m_sendEvent);
}
