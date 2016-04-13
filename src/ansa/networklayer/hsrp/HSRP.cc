//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "HSRP.h"

namespace inet {

Define_Module(HSRP);


HSRP::HSRP() {
}

/**
 * Startup initializacion of HSRP
 */
void HSRP::initialize(int stage)
{

    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this); //usable interfaces of tihs router
        HsrpMulticast = new L3Address(HSRP_MULTICAST_ADDRESS.c_str());
        socket = new UDPSocket(); //UDP socket used for sending messages
        socket->setOutputGate(gate("udpOut"));
        socket->bind(HSRP_UDP_PORT);
        this->parseConfig(par(CONFIG_PAR));
    }
}

/**
 * Omnet++ function for handeling incoming messages
 */
void HSRP::handleMessage(cMessage *msg)
{
    //get message from netwlayer
    if (msg->getArrivalGate()->isName("udpIn")){
        if (dynamic_cast<HSRPMessage*>(msg))
        {
            HSRPMessage *HSRPm = dynamic_cast<HSRPMessage*>(msg);

//            EV << "Recieved packet '" << HSRPm << "' from network layer, ";

            for (int i = 0; i < (int) virtualRouterTable.size(); i++){
//                printf("group virtRouterGroup: %d, IID: %d", virtualRouterTable.at(i)->getGroup(), virtualRouterTable.at(i)->getInterface()->getInterfaceId());

                if (virtualRouterTable.at(i)->getGroup() == HSRPm->getGroup()) //&&
//                        ((IPv4ControlInfo *)msg->getControlInfo())->getInterfaceId() ==
//                                virtualRouterTable.at(i)->getInterface()->getInterfaceId()
//                   )
                {
//                    EV << "sending Advertisement to Virtual Router '" << virtualRouterTable.at(i) << "'" << endl;
                    send(msg, "hsrpOut", i);
                    return;
                }
            }
            EV << "unknown Virtual Router ID" << HSRPm->getGroup() << ", discard it." << endl;
//            printf("group HSRPm: %d, HSRPm IID: %d", HSRPm->getGroup(), ((IPv4ControlInfo *)msg->getControlInfo())->getInterfaceId());
            delete msg;
        }
    }
    //get message from HSRP
    else if (msg->getArrivalGate()->isName("hsrpIn")){
//        EV << "Recieved advertisement '" << msg << "' from Virtual Router, sending to network layer." << endl;
        send(msg, "udpOut");
    }
} //end handleMessage

void HSRP::addVirtualRouter(int interface, int vrid, const char* ifnam, std::string vip, int priority, bool preempt){
    int gateSize = virtualRouterTable.size() + 1;
    this->setGateSize("hsrpIn",gateSize);
    this->setGateSize("hsrpOut", gateSize);

    // name
    std::stringstream tmp;
    tmp << "VR_" << ifnam << "_" << vrid;
    std::string name = tmp.str();

    // create
    cModuleType *moduleType = cModuleType::get("ansa.networklayer.hsrp.HSRPVirtualRouter");
    cModule *module = moduleType->create(name.c_str(), this);

    // set up parameters
    module->par("deviceId") = par("deviceId");
    module->par("configData") = par("configData");
    module->par("vrid") = vrid;
    module->par("interface") = interface;
    module->par("virtualIP") = vip;
    module->par("priority") = priority;
    module->par("preempt") = preempt;
    module->par("interfaceTableModule") = ift->getFullPath();
    cModule *containingModule = findContainingNode(this);
    module->par("arp") = containingModule->getSubmodule("networkLayer")->getSubmodule("ipv4")->getSubmodule("arp")->getFullPath();

    std::cout<< "full path:"<<ift->getFullPath()<<std::endl;
    module->finalizeParameters();

    // set up gate
    this->gate("hsrpOut", virtualRouterTable.size())->connectTo(module->gate("udpIn"));
    module->gate("udpOut")->connectTo(this->gate("hsrpIn", virtualRouterTable.size()));
    module->buildInside();

    // load
    module->scheduleStart(simTime());

    virtualRouterTable.push_back(dynamic_cast<HSRPVirtualRouter *>(module));

    updateDisplayString();
}

void HSRP::parseConfig(cXMLElement *config){
    //naparsuje config - >>> a rovnou zaklada HSRPVirtRoutery

    //Config element is empty
    if (!config)
        return;

    //Go through all interfaces and look for HSRP section
    cXMLElementList msa = config->getChildrenByTagName("Interface");
    for (cXMLElementList::iterator i = msa.begin(); i != msa.end(); ++i) {
        cXMLElement* m = *i;
        std::string ifname;
        ifname = m->getAttribute("name");

        //Get through each group
        cXMLElementList gr = m->getElementsByTagName("Group");
        for (cXMLElementList::iterator j = gr.begin(); j != gr.end(); ++j) {
            cXMLElement* group = *j;

            //get GID
            int gid;
            std::stringstream strGID;
            if (!group->getAttribute("id")) {
                EV << "Config XML file missing tag or attribute - Group id" << endl;
                gid = 0; //def val
            } else
            {
                strGID << group->getAttribute("id");
                strGID >> gid;
                EV_DEBUG << "Setting GID:" <<gid<< endl;
            }

            //get Priority
            std::stringstream strValue2;
            int priority;
            if (!group->getAttribute("priority")) {
                EV << "Config XML file missing tag or attribute - Priority" << endl;
                priority = 100;//def val
            } else
            {
                strValue2 << group->getAttribute("priority");
                strValue2 >> priority;
                EV_DEBUG << "Setting priority:" <<priority<< endl;
            }

            //get Preempt flag
            bool preempt;
            if (!group->getAttribute("preempt")) {
                EV << "Config XML file missing tag or attribute - Preempt" << endl;
                preempt = false; //def val
            } else
            {
                if (strcmp("false",group->getAttribute("preempt"))){
                    preempt = false;
                }else
                {
                    preempt = true;
                }
                EV_DEBUG << "Setting preemption:" <<preempt<< endl;
            }

            //get interface id
            int iid = ift->getInterfaceByName(ifname.c_str())->getInterfaceId();

            //get virtual IP
            std::string virtip;
            if (!group->getAttribute("ip")) {
                EV << "Config XML file missing tag or attribute - Ip" << endl;
                virtip = "0.0.0.0"; //sign that ip is not set
            } else
            {
                virtip = group->getAttribute("ip");
                if (is_unique(virtip, iid)){
                    EV_DEBUG << "Setting virtip:" <<virtip<< endl;
                }else{
                    std::cerr<<par("deviceId").stringValue()<<" Group "<<gid<<": Wrong HSRP group IP in config file."<<endl;
                    fflush(stderr);
                    continue;
                }
            }


            checkAndJoinMulticast(iid);
            addVirtualRouter(iid , gid, ifname.c_str(), virtip, priority, preempt);
        }// end each group
    }//end each interface
}//end parseConfig

//interface join multicast group if it is not joined yet
void HSRP::checkAndJoinMulticast(int InterfaceId){
    bool contain=false;
    for (int i = 0; i < (int) multicastInterfaces.size(); i++){
        if (multicastInterfaces[i] == InterfaceId){
            contain = true;
        }
    }

    if (!contain){
        socket->joinMulticastGroup(*HsrpMulticast,InterfaceId);
        multicastInterfaces.push_back(InterfaceId);
    }
}


//check if virtual IP is unique in this router
bool HSRP::is_unique(std::string virtip, int iid){

//    IPv4Address *adr = new IPv4Address(virtip.c_str());

    //check local virtual IPs
    for (int i = 0; i < (int) virtualRouterTable.size(); i++){
//        EV_DEBUG<<"Compared virtual Address:"<<virtualRouterTable.at(i)->getIPaddress()->str(false)<<endl;

        if (virtip.compare(virtualRouterTable.at(i)->par("virtualIP").stringValue()) == 0){
            EV<<par("deviceId").stdstringValue()<<" % Address "<<virtip<<" in group "<<(int)virtualRouterTable.at(i)->par("group")<<"."<<endl;
            return false;
        }
    }

    //check physical IP of actual interface
    std::string InterfaceIP = ift->getInterfaceById(iid)->ipv4Data()->getIPAddress().str(false);
//    EV_DEBUG<<"Compared physical Address: "<<InterfaceIP<<endl;
    if (InterfaceIP.compare(virtip) == 0){
        EV<<par("deviceId").stringValue()<<" % Address cannot equal interface IP address."<<endl;
        return false;
    }

    //TODO: check overlapping ip with another interface
    return true;
}

void HSRP::updateDisplayString()
{
    if (!ev.isGUI())
        return;

    char buf[80];
    if (virtualRouterTable.size() == 1)
        sprintf(buf, "%d group", virtualRouterTable.size());
    else if (virtualRouterTable.size() > 1)
        sprintf(buf, "%d groups", virtualRouterTable.size());

    getDisplayString().setTagArg("t", 0, buf);
}


HSRP::~HSRP() {
    printf("destrukce HSRP\n");
}

} /* namespace inet */
