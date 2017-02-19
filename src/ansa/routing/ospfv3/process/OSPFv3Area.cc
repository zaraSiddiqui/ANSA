#include "ansa/routing/ospfv3/process/OSPFv3Area.h"

namespace inet{

OSPFv3Area::OSPFv3Area(IPv4Address areaID, OSPFv3Instance* parent)
{
    this->areaID=areaID;
    this->containingInstance=parent;
    this->externalRoutingCapability = true;

    WATCH_PTRVECTOR(this->interfaceList);
}

OSPFv3Area::~OSPFv3Area()
{
}

bool OSPFv3Area::hasInterface(std::string interfaceName)
{
    std::map<std::string, OSPFv3Interface*>::iterator interfaceIt = this->interfaceByName.find(interfaceName);
    if(interfaceIt == this->interfaceByName.end())
        return false;

    return true;
}//hasArea
OSPFv3Interface* OSPFv3Area::getInterfaceById(int id)
{
    std::map<int, OSPFv3Interface*>::iterator interfaceIt = this->interfaceById.find(id);
    if(interfaceIt == this->interfaceById.end())
        return nullptr;

    return interfaceIt->second;
}//getInterfaceById

void OSPFv3Area::addInterface(OSPFv3Interface* newInterface)
{
    this->interfaceList.push_back(newInterface);
    this->interfaceByName[newInterface->getIntName()]=newInterface;
    this->interfaceById[newInterface->getInterfaceId()]=newInterface;
}//addArea

void OSPFv3Area::init()
{
    for(auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++)
    {
        (*it)->setInterfaceIndex(this->getInstance()->getUniqueId());
        (*it)->processEvent(OSPFv3Interface::INTERFACE_UP_EVENT);
    }
}

OSPFv3Interface* OSPFv3Area::findVirtualLink(IPv4Address routerID)
{
    int interfaceNum = this->interfaceList.size();
    for (int i = 0; i < interfaceNum; i++) {
        if ((interfaceList.at(i)->getType() == OSPFv3Interface::VIRTUAL_TYPE) &&
            (interfaceList.at(i)->getNeighborById(routerID) != nullptr))
        {
            return interfaceList.at(i);
        }
    }
    return nullptr;
}

void OSPFv3Area::addInterAreaLSA(OSPFv3InterAreaPrefixLSA* newLSA)
{
    this->interAreaLSAList.push_back(newLSA);
    IPv4Address LSAIP = newLSA->getHeader().getLinkStateID();
    this->interAreaLSAById[LSAIP]=newLSA;
}//addInterAreaLSA


OSPFv3InterAreaPrefixLSA* OSPFv3Area::getInterAreaLSAbyId(IPv4Address LSAId)
{
    std::map<IPv4Address, OSPFv3InterAreaPrefixLSA* >::iterator it = this->interAreaLSAById.find(LSAId);
    if(it!=this->interAreaLSAById.end())
        return it->second;

    return nullptr;
}//getInterAreaLSA

OSPFv3LSAHeader* OSPFv3Area::findLSA(LSAKeyType lsaKey)
{
    EV_DEBUG << "FIND LSA:\n";

    switch (lsaKey.LSType) {
    case ROUTER_LSA: {
        EV_DEBUG << "looking for lsa type ROUTER\n";
        OSPFv3RouterLSA* lsa = this->getRouterLSAbyKey(lsaKey);
        if(lsa == nullptr) {
            EV_DEBUG << "FIND LSA - nullptr returned\n";
            return nullptr;
        }
        else {
            OSPFv3LSAHeader* lsaHeader = &(lsa->getHeader());
            EV_DEBUG << "FIND LSA - header returned\n";
            return lsaHeader;
        }
    }
    break;

    case NETWORK_LSA: {
        EV_DEBUG << "looking for lsa type NETWORK\n";
        OSPFv3RouterLSA* lsa = this->getRouterLSAbyKey(lsaKey);
                if(lsa == nullptr) {
                    EV_DEBUG << "FIND LSA - nullptr returned\n";
                    return nullptr;
                }
                else {
                    OSPFv3LSAHeader* lsaHeader = &(lsa->getHeader());
                    EV_DEBUG << "FIND LSA - header returned\n";
                    return lsaHeader;
                }
        //return this->getNetworkLSAbyId(lsaKey.linkStateID);
    }
    break;

//    case SUMMARYLSA_NETWORKS_TYPE:
//    case SUMMARYLSA_ASBOUNDARYROUTERS_TYPE: {
//        auto areaIt = areasByID.find(areaID);
//        if (areaIt != areasByID.end()) {
//            return areaIt->second->findSummaryLSA(lsaKey);
//        }
//    }
//    break;
//
//    case AS_EXTERNAL_LSA_TYPE: {
//        return findASExternalLSA(lsaKey);
//    }
//    break;
//
    default:
        //ASSERT(false);
        break;
    }
    return nullptr;
}

IPv4Address OSPFv3Area::getNewRouterLinkStateID()
{
    IPv4Address currIP = this->routerLsID;
    int newIP = currIP.getInt()+1;
    this->routerLsID = IPv4Address(newIP);
    return currIP;
}

OSPFv3RouterLSA* OSPFv3Area::originateRouterLSA()
{
    OSPFv3RouterLSA *routerLSA = new OSPFv3RouterLSA();
    OSPFv3LSAHeader& lsaHeader = routerLSA->getHeader();
    long interfaceCount = this->interfaceList.size();
    OSPFv3Options lsOptions;
    memset(&lsOptions, 0, sizeof(OSPFv3Options));


    //First set the LSA Header
    lsaHeader.setLsaAge(0);
    //The LSA Type is 0x2001
    lsaHeader.setLsaType(ROUTER_LSA);

    lsaHeader.setLinkStateID(this->getNewRouterLinkStateID());
    lsaHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    lsaHeader.setLsaSequenceNumber(this->getCurrentRouterSequence());

    //TODO - set options

    for(int i=0; i<interfaceCount; i++)
    {
        OSPFv3Interface* intf = this->interfaceList.at(i);
        int neighborCount = intf->getNeighborCount();
        for(int j=0; j<neighborCount; j++)
        {
            OSPFv3RouterLSABody routerLSABody;
            memset(&routerLSABody, 0, sizeof(OSPFv3RouterLSABody));

            switch(intf->getType())
            {
            case OSPFv3Interface::POINTTOPOINT_TYPE:
                routerLSABody.type=POINT_TO_POINT;
                break;

            case OSPFv3Interface::BROADCAST_TYPE:
                routerLSABody.type=TRANSIT_NETWORK;
                break;

            case OSPFv3Interface::VIRTUAL_TYPE:
                routerLSABody.type=VIRTUAL_LINK;
                break;
            }

            routerLSABody.interfaceID = intf->getInterfaceIndex();
            routerLSABody.metric = 1;//TODO - correct this

            OSPFv3Neighbor* neighbor = intf->getNeighbor(j);
            routerLSABody.neighborInterfaceID = neighbor->getNeighborInterfaceID();
            routerLSABody.neighborRouterID = neighbor->getNeighborID();

            routerLSA->setRoutersArraySize(j+1);
            routerLSA->setRouters(j, routerLSABody);
        }
    }

    this->addRouterLSA(routerLSA);

    //originate Intra-Area-Prefix LSA along with any Router LSA
    //this->originateIntraAreaPrefixLSA(routerLSA, intf);



//    lsOptions.E_ExternalRoutingCapability = externalRoutingCapability;
//    lsaHeader.setLsOptions(lsOptions);
//    lsaHeader.setLsType(ROUTERLSA_TYPE);
//    lsaHeader.setLinkStateID(parentRouter->getRouterID());
//    lsaHeader.setAdvertisingRouter(IPv4Address(parentRouter->getRouterID()));
//    lsaHeader.setLsSequenceNumber(INITIAL_SEQUENCE_NUMBER);
//
//    routerLSA->setB_AreaBorderRouter(parentRouter->getAreaCount() > 1);
//    routerLSA->setE_ASBoundaryRouter((externalRoutingCapability && parentRouter->getASBoundaryRouter()) ? true : false);
//    Area *backbone = parentRouter->getAreaByID(BACKBONE_AREAID);
//    routerLSA->setV_VirtualLinkEndpoint((backbone == nullptr) ? false : backbone->hasVirtualLink(areaID));
//
//    routerLSA->setNumberOfLinks(0);
//    routerLSA->setLinksArraySize(0);
//    for (i = 0; i < interfaceCount; i++) {
//        Interface *intf = associatedInterfaces[i];
//
//        if (intf->getState() == Interface::DOWN_STATE) {
//            continue;
//        }
//        if ((intf->getState() == Interface::LOOPBACK_STATE) &&
//            ((intf->getType() != Interface::POINTTOPOINT) ||
//             (intf->getAddressRange().address != NULL_IPV4ADDRESS)))
//        {
//            Link stubLink;
//            stubLink.setType(STUB_LINK);
//            stubLink.setLinkID(intf->getAddressRange().address);
//            stubLink.setLinkData(0xFFFFFFFF);
//            stubLink.setLinkCost(0);
//            stubLink.setNumberOfTOS(0);
//            stubLink.setTosDataArraySize(0);
//
//            unsigned short linkIndex = routerLSA->getLinksArraySize();
//            routerLSA->setLinksArraySize(linkIndex + 1);
//            routerLSA->setNumberOfLinks(linkIndex + 1);
//            routerLSA->setLinks(linkIndex, stubLink);
//        }
//        if (intf->getState() > Interface::LOOPBACK_STATE) {
//            switch (intf->getType()) {
//                case Interface::POINTTOPOINT: {
//                    Neighbor *neighbor = (intf->getNeighborCount() > 0) ? intf->getNeighbor(0) : nullptr;
//                    if (neighbor != nullptr) {
//                        if (neighbor->getState() == Neighbor::FULL_STATE) {
//                            Link link;
//                            link.setType(POINTTOPOINT_LINK);
//                            link.setLinkID(IPv4Address(neighbor->getNeighborID()));
//                            if (intf->getAddressRange().address != NULL_IPV4ADDRESS) {
//                                link.setLinkData(intf->getAddressRange().address.getInt());
//                            }
//                            else {
//                                link.setLinkData(intf->getIfIndex());
//                            }
//                            link.setLinkCost(intf->getOutputCost());
//                            link.setNumberOfTOS(0);
//                            link.setTosDataArraySize(0);
//
//                            unsigned short linkIndex = routerLSA->getLinksArraySize();
//                            routerLSA->setLinksArraySize(linkIndex + 1);
//                            routerLSA->setNumberOfLinks(linkIndex + 1);
//                            routerLSA->setLinks(linkIndex, link);
//                        }
//                        if (intf->getState() == Interface::POINTTOPOINT_STATE) {
//                            if (neighbor->getAddress() != NULL_IPV4ADDRESS) {
//                                Link stubLink;
//                                stubLink.setType(STUB_LINK);
//                                stubLink.setLinkID(neighbor->getAddress());
//                                stubLink.setLinkData(0xFFFFFFFF);
//                                stubLink.setLinkCost(intf->getOutputCost());
//                                stubLink.setNumberOfTOS(0);
//                                stubLink.setTosDataArraySize(0);
//
//                                unsigned short linkIndex = routerLSA->getLinksArraySize();
//                                routerLSA->setLinksArraySize(linkIndex + 1);
//                                routerLSA->setNumberOfLinks(linkIndex + 1);
//                                routerLSA->setLinks(linkIndex, stubLink);
//                            }
//                            else {
//                                if (intf->getAddressRange().mask.getInt() != 0xFFFFFFFF) {
//                                    Link stubLink;
//                                    stubLink.setType(STUB_LINK);
//                                    stubLink.setLinkID(intf->getAddressRange().address
//                                            & intf->getAddressRange().mask);
//                                    stubLink.setLinkData(intf->getAddressRange().mask.getInt());
//                                    stubLink.setLinkCost(intf->getOutputCost());
//                                    stubLink.setNumberOfTOS(0);
//                                    stubLink.setTosDataArraySize(0);
//
//                                    unsigned short linkIndex = routerLSA->getLinksArraySize();
//                                    routerLSA->setLinksArraySize(linkIndex + 1);
//                                    routerLSA->setNumberOfLinks(linkIndex + 1);
//                                    routerLSA->setLinks(linkIndex, stubLink);
//                                }
//                            }
//                        }
//                    }
//                }
//                break;
//
//                case Interface::BROADCAST:
//                case Interface::NBMA: {
//                    if (intf->getState() == Interface::WAITING_STATE) {
//                        Link stubLink;
//                        stubLink.setType(STUB_LINK);
//                        stubLink.setLinkID(intf->getAddressRange().address
//                                & intf->getAddressRange().mask);
//                        stubLink.setLinkData(intf->getAddressRange().mask.getInt());
//                        stubLink.setLinkCost(intf->getOutputCost());
//                        stubLink.setNumberOfTOS(0);
//                        stubLink.setTosDataArraySize(0);
//
//                        unsigned short linkIndex = routerLSA->getLinksArraySize();
//                        routerLSA->setLinksArraySize(linkIndex + 1);
//                        routerLSA->setNumberOfLinks(linkIndex + 1);
//                        routerLSA->setLinks(linkIndex, stubLink);
//                    }
//                    else {
//                        Neighbor *dRouter = intf->getNeighborByAddress(intf->getDesignatedRouter().ipInterfaceAddress);
//                        if (((dRouter != nullptr) && (dRouter->getState() == Neighbor::FULL_STATE)) ||
//                            ((intf->getDesignatedRouter().routerID == parentRouter->getRouterID()) &&
//                             (intf->hasAnyNeighborInStates(Neighbor::FULL_STATE))))
//                        {
//                            Link link;
//                            link.setType(TRANSIT_LINK);
//                            link.setLinkID(intf->getDesignatedRouter().ipInterfaceAddress);
//                            link.setLinkData(intf->getAddressRange().address.getInt());
//                            link.setLinkCost(intf->getOutputCost());
//                            link.setNumberOfTOS(0);
//                            link.setTosDataArraySize(0);
//
//                            unsigned short linkIndex = routerLSA->getLinksArraySize();
//                            routerLSA->setLinksArraySize(linkIndex + 1);
//                            routerLSA->setNumberOfLinks(linkIndex + 1);
//                            routerLSA->setLinks(linkIndex, link);
//                        }
//                        else {
//                            Link stubLink;
//                            stubLink.setType(STUB_LINK);
//                            stubLink.setLinkID(intf->getAddressRange().address
//                                    & intf->getAddressRange().mask);
//                            stubLink.setLinkData(intf->getAddressRange().mask.getInt());
//                            stubLink.setLinkCost(intf->getOutputCost());
//                            stubLink.setNumberOfTOS(0);
//                            stubLink.setTosDataArraySize(0);
//
//                            unsigned short linkIndex = routerLSA->getLinksArraySize();
//                            routerLSA->setLinksArraySize(linkIndex + 1);
//                            routerLSA->setNumberOfLinks(linkIndex + 1);
//                            routerLSA->setLinks(linkIndex, stubLink);
//                        }
//                    }
//                }
//                break;
//
//                case Interface::VIRTUAL: {
//                    Neighbor *neighbor = (intf->getNeighborCount() > 0) ? intf->getNeighbor(0) : nullptr;
//                    if ((neighbor != nullptr) && (neighbor->getState() == Neighbor::FULL_STATE)) {
//                        Link link;
//                        link.setType(VIRTUAL_LINK);
//                        link.setLinkID(IPv4Address(neighbor->getNeighborID()));
//                        link.setLinkData(intf->getAddressRange().address.getInt());
//                        link.setLinkCost(intf->getOutputCost());
//                        link.setNumberOfTOS(0);
//                        link.setTosDataArraySize(0);
//
//                        unsigned short linkIndex = routerLSA->getLinksArraySize();
//                        routerLSA->setLinksArraySize(linkIndex + 1);
//                        routerLSA->setNumberOfLinks(linkIndex + 1);
//                        routerLSA->setLinks(linkIndex, link);
//                    }
//                }
//                break;
//
//                case Interface::POINTTOMULTIPOINT: {
//                    Link stubLink;
//                    stubLink.setType(STUB_LINK);
//                    stubLink.setLinkID(intf->getAddressRange().address);
//                    stubLink.setLinkData(0xFFFFFFFF);
//                    stubLink.setLinkCost(0);
//                    stubLink.setNumberOfTOS(0);
//                    stubLink.setTosDataArraySize(0);
//
//                    unsigned short linkIndex = routerLSA->getLinksArraySize();
//                    routerLSA->setLinksArraySize(linkIndex + 1);
//                    routerLSA->setNumberOfLinks(linkIndex + 1);
//                    routerLSA->setLinks(linkIndex, stubLink);
//
//                    long neighborCount = intf->getNeighborCount();
//                    for (long i = 0; i < neighborCount; i++) {
//                        Neighbor *neighbor = intf->getNeighbor(i);
//                        if (neighbor->getState() == Neighbor::FULL_STATE) {
//                            Link link;
//                            link.setType(POINTTOPOINT_LINK);
//                            link.setLinkID(IPv4Address(neighbor->getNeighborID()));
//                            link.setLinkData(intf->getAddressRange().address.getInt());
//                            link.setLinkCost(intf->getOutputCost());
//                            link.setNumberOfTOS(0);
//                            link.setTosDataArraySize(0);
//
//                            unsigned short linkIndex = routerLSA->getLinksArraySize();
//                            routerLSA->setLinksArraySize(linkIndex + 1);
//                            routerLSA->setNumberOfLinks(linkIndex + 1);
//                            routerLSA->setLinks(linkIndex, stubLink);
//                        }
//                    }
//                }
//                break;
//
//                default:
//                    break;
//            }
//        }
//    }
//
//    long hostRouteCount = hostRoutes.size();
//    for (i = 0; i < hostRouteCount; i++) {
//        Link stubLink;
//        stubLink.setType(STUB_LINK);
//        stubLink.setLinkID(hostRoutes[i].address);
//        stubLink.setLinkData(0xFFFFFFFF);
//        stubLink.setLinkCost(hostRoutes[i].linkCost);
//        stubLink.setNumberOfTOS(0);
//        stubLink.setTosDataArraySize(0);
//
//        unsigned short linkIndex = routerLSA->getLinksArraySize();
//        routerLSA->setLinksArraySize(linkIndex + 1);
//        routerLSA->setNumberOfLinks(linkIndex + 1);
//        routerLSA->setLinks(linkIndex, stubLink);
//    }
//
//    routerLSA->setSource(LSATrackingInfo::ORIGINATED);

    return routerLSA;
}//originateRouterLSA


OSPFv3RouterLSA* OSPFv3Area::getRouterLSAbyKey(LSAKeyType LSAKey)
{
    EV_DEBUG << "GET ROUTER LSA BY KEY  \n";
    for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
    {
        EV_DEBUG << "FOR, routerLSAList size: " << this->routerLSAList.size() << "\n";
        if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
            EV_DEBUG << "This is OK!\n";
            return (*it);
        }
    }

    return nullptr;
}//getRouterLSAByKey


bool OSPFv3Area::installRouterLSA(OSPFv3RouterLSA *lsa)
{
    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();

    OSPFv3RouterLSA* lsaInDatabase = (OSPFv3RouterLSA*)this->getLSAbyKey(lsaKey);
    if (lsaInDatabase != nullptr) {
        this->removeFromAllRetransmissionLists(lsaKey);
        return this->updateRouterLSA(lsaInDatabase, lsa);
    }
    else {
        OSPFv3RouterLSA* lsaCopy = new OSPFv3RouterLSA(*lsa);
        this->routerLSAList.push_back(lsaCopy);
        return true;
    }
}//installRouterLSA


bool OSPFv3Area::updateRouterLSA(OSPFv3RouterLSA* currentLsa, OSPFv3RouterLSA* newLsa)
{
    bool different = routerLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->getHeader().setLsaAge(0);//reset the age
    if (different) {
//        clearNextHops();//TODO
        return true;
    }
    else {
        return false;
    }
}//updateRouterLSA


bool OSPFv3Area::routerLSADiffersFrom(OSPFv3RouterLSA* currentLsa, OSPFv3RouterLSA* newLsa)
{
    const OSPFv3LSAHeader& thisHeader = currentLsa->getHeader();
    const OSPFv3LSAHeader& lsaHeader = newLsa->getHeader();
    bool differentHeader = (((thisHeader.getLsaAge() == MAX_AGE) && (lsaHeader.getLsaAge() != MAX_AGE)) ||
                            ((thisHeader.getLsaAge() != MAX_AGE) && (lsaHeader.getLsaAge() == MAX_AGE)) ||
                            (thisHeader.getLsaLength() != lsaHeader.getLsaLength()));
    bool differentBody = false;

    if (!differentHeader) {
        differentBody = ((currentLsa->getNtBit() != newLsa->getNtBit()) ||
                         (currentLsa->getEBit() != newLsa->getEBit()) ||
                         (currentLsa->getBBit() != newLsa->getBBit()) ||
                         (currentLsa->getVBit() != newLsa->getVBit()) ||
                         (currentLsa->getXBit() != newLsa->getXBit()) ||
                         (currentLsa->getRoutersArraySize() != newLsa->getRoutersArraySize()));

        if (!differentBody) {
            unsigned int routersCount = currentLsa->getRoutersArraySize();
            for (unsigned int i = 0; i < routersCount; i++) {
                auto thisRouter = currentLsa->getRouters(i);
                auto lsaRouter = newLsa->getRouters(i);
                bool differentLink = ((thisRouter.type != lsaRouter.type) ||
                                      (thisRouter.metric != lsaRouter.metric) ||
                                      (thisRouter.interfaceID != lsaRouter.interfaceID) ||
                                      (thisRouter.neighborInterfaceID != lsaRouter.neighborInterfaceID) ||
                                      (thisRouter.neighborRouterID != lsaRouter.neighborRouterID));

                if (differentLink) {
                    differentBody = true;
                    break;
                }
            }
        }
    }

    return differentHeader || differentBody;
}//routerLSADiffersFrom


bool OSPFv3Area::floodLSA(OSPFv3LSA* lsa, OSPFv3Interface* interface, OSPFv3Neighbor* neighbor)
{
    bool floodedBackOut = false;
    long interfaceCount = this->interfaceList.size();

    for (long i = 0; i < interfaceCount; i++) {
        if (interfaceList.at(i)->floodLSA(lsa, interface, neighbor)) {
            floodedBackOut = true;
        }
    }

    return floodedBackOut;
}//floodLSA


void OSPFv3Area::removeFromAllRetransmissionLists(LSAKeyType lsaKey)
{
    long interfaceCount = this->interfaceList.size();
    for (long i = 0; i < interfaceCount; i++) {
        this->interfaceList.at(i)->removeFromAllRetransmissionLists(lsaKey);
    }
}


OSPFv3NetworkLSA* OSPFv3Area::originateNetworkLSA(OSPFv3Interface* interface)
{
    OSPFv3NetworkLSA* networkLsa = new OSPFv3NetworkLSA();
    OSPFv3LSAHeader& lsaHeader = networkLsa->getHeader();
    OSPFv3Options lsOptions;
    memset(&lsOptions, 0, sizeof(OSPFv3Options));

    //First set the LSA Header
    lsaHeader.setLsaAge(0);
    //The LSA Type is 0x2002
    lsaHeader.setLsaType(NETWORK_LSA);

    lsaHeader.setLinkStateID(IPv4Address(interface->getInterfaceIndex()));
    lsaHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    lsaHeader.setLsaSequenceNumber(this->getCurrentNetworkSequence());

    uint16_t packetLength = OSPFV3_LSA_HEADER_LENGTH + 4;//4 for options field

    //now the body
    networkLsa->setOspfOptions(lsOptions);

    int attachedCount = interface->getNeighborCount();//+1 for this router
    if(attachedCount >= 2){
        networkLsa->setAttachedRouterArraySize(attachedCount);
        for(int i=0; i<attachedCount; i++){
            OSPFv3Neighbor* neighbor = interface->getNeighbor(i);
            networkLsa->setAttachedRouter(i, neighbor->getNeighborID());
            packetLength+=4;
        }
    }

    lsaHeader.setLsaLength(packetLength);
    this->networkLSAList.push_back(networkLsa);
    return networkLsa;
}//originateNetworkLSA

bool OSPFv3Area::installNetworkLSA(OSPFv3NetworkLSA *lsa)
{
    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();

    OSPFv3NetworkLSA* lsaInDatabase = (OSPFv3NetworkLSA*)this->getLSAbyKey(lsaKey);
    if (lsaInDatabase != nullptr) {
        this->removeFromAllRetransmissionLists(lsaKey);
        return this->updateNetworkLSA(lsaInDatabase, lsa);
    }
    else {
        OSPFv3NetworkLSA* lsaCopy = new OSPFv3NetworkLSA(*lsa);
        this->networkLSAList.push_back(lsaCopy);
        return true;
    }
}//installNetworkLSA


bool OSPFv3Area::updateNetworkLSA(OSPFv3NetworkLSA* currentLsa, OSPFv3NetworkLSA* newLsa)
{
    bool different = networkLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->getHeader().setLsaAge(0);//reset the age
    if (different) {
//        clearNextHops();//TODO
        return true;
    }
    else {
        return false;
    }
}//updateNetworkLSA


bool OSPFv3Area::networkLSADiffersFrom(OSPFv3NetworkLSA* currentLsa, OSPFv3NetworkLSA* newLsa)
{
    const OSPFv3LSAHeader& thisHeader = currentLsa->getHeader();
    const OSPFv3LSAHeader& lsaHeader = newLsa->getHeader();
    bool differentHeader = (((thisHeader.getLsaAge() == MAX_AGE) && (lsaHeader.getLsaAge() != MAX_AGE)) ||
                            ((thisHeader.getLsaAge() != MAX_AGE) && (lsaHeader.getLsaAge() == MAX_AGE)) ||
                            (thisHeader.getLsaLength() != lsaHeader.getLsaLength()));
    bool differentBody = false;

    if (!differentHeader) {
        differentBody = (currentLsa->getOspfOptions() != newLsa->getOspfOptions());


        if (!differentBody) {
            unsigned int attachedCount = currentLsa->getAttachedRouterArraySize();
            for (unsigned int i = 0; i < attachedCount; i++) {
                bool differentLink = (currentLsa->getAttachedRouter(i)!=newLsa->getAttachedRouter(i));

                if (differentLink) {
                    differentBody = true;
                    break;
                }
            }
        }
    }

    return differentHeader || differentBody;
}//networkLSADiffersFrom

IPv4Address OSPFv3Area::getNewNetworkLinkStateID()
{
    IPv4Address currIP = this->networkLsID;
    int newIP = currIP.getInt()+1;
    this->networkLsID = IPv4Address(newIP);
    return currIP;
}//getNewNetworkLinkStateID

OSPFv3IntraAreaPrefixLSA* OSPFv3Area::originateIntraAreaPrefixLSA(OSPFv3LSA* lsa, OSPFv3Interface* intf)
{
    OSPFv3LSAHeader& header = lsa->getHeader();
    OSPFv3IntraAreaPrefixLSA* newLsa = new OSPFv3IntraAreaPrefixLSA();

    OSPFv3LSAHeader& newHeader = newLsa->getHeader();
    newHeader.setLsaAge(0);
    newHeader.setLsaType(INTRA_AREA_PREFIX_LSA);
    newHeader.setLinkStateID(this->getNewIntraAreaPrefixLinkStateID());
    newHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    newHeader.setLsaSequenceNumber(this->intraAreaPrefixLSASequenceNumber++);

    int packetLength = OSPFV3_LSA_HEADER_LENGTH+OSPFV3_INTRA_AREA_PREFIX_LSA_HEADER_LENGTH;

    if(header.getLsaType() == ROUTER_LSA) {

    }
    else if(header.getLsaType() == NETWORK_LSA){
        OSPFv3NetworkLSA* networkLsa = new OSPFv3NetworkLSA();
        OSPFv3LSAHeader& lsaHeader = networkLsa->getHeader();
        OSPFv3Options lsOptions;
        memset(&lsOptions, 0, sizeof(OSPFv3Options));

    }

//    lsaHeader.setLinkStateID(IPv4Address(interface->getInterfaceIndex()));
//    lsaHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
//    lsaHeader.setLsaSequenceNumber(this->getCurrentNetworkSequence());
////    #define OSPFV3_INTRA_AREA_PREFIX_LSA_PREFIX_LENGTH 20
//    uint16_t packetLength = OSPFV3_LSA_HEADER_LENGTH + OSPFV3_INTRA_AREA_PREFIX_LSA_HEADER_LENGTH;


}//originateIntraAreaPrefixLSA

bool OSPFv3Area::installIntraAreaPrefixLSA(OSPFv3IntraAreaPrefixLSA *lsa)
{
    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();

    OSPFv3IntraAreaPrefixLSA* lsaInDatabase = (OSPFv3IntraAreaPrefixLSA*)this->getLSAbyKey(lsaKey);
    if (lsaInDatabase != nullptr) {
        this->removeFromAllRetransmissionLists(lsaKey);
        return this->updateIntraAreaPrefixLSA(lsaInDatabase, lsa);
    }
    else {
        OSPFv3IntraAreaPrefixLSA* lsaCopy = new OSPFv3IntraAreaPrefixLSA(*lsa);
        this->intraAreaPrefixLSAList.push_back(lsaCopy);
        return true;
    }
}//installIntraAreaPrefixLSA


bool OSPFv3Area::updateIntraAreaPrefixLSA(OSPFv3IntraAreaPrefixLSA* currentLsa, OSPFv3IntraAreaPrefixLSA* newLsa)
{
    bool different = intraAreaPrefixLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->getHeader().setLsaAge(0);//reset the age
    if (different) {
//        clearNextHops();//TODO
        return true;
    }
    else {
        return false;
    }
}//updateIntraAreaPrefixLSA


bool OSPFv3Area::intraAreaPrefixLSADiffersFrom(OSPFv3IntraAreaPrefixLSA* currentLsa, OSPFv3IntraAreaPrefixLSA* newLsa)
{
    const OSPFv3LSAHeader& thisHeader = currentLsa->getHeader();
    const OSPFv3LSAHeader& lsaHeader = newLsa->getHeader();
    bool differentHeader = (((thisHeader.getLsaAge() == MAX_AGE) && (lsaHeader.getLsaAge() != MAX_AGE)) ||
                            ((thisHeader.getLsaAge() != MAX_AGE) && (lsaHeader.getLsaAge() == MAX_AGE)) ||
                            (thisHeader.getLsaLength() != lsaHeader.getLsaLength()));
    bool differentBody = false;

    if (!differentHeader) {
        differentBody = ((currentLsa->getNumPrefixes() != newLsa->getNumPrefixes()) ||
                         (currentLsa->getReferencedLSType() != newLsa->getReferencedLSType()) ||
                         (currentLsa->getReferencedLSID() != newLsa->getReferencedLSID()) ||
                         (currentLsa->getReferencedAdvRtr() != newLsa->getReferencedAdvRtr()));


        if (!differentBody) {
            unsigned int referenceCount = currentLsa->getNumPrefixes();
            for (unsigned int i = 0; i < referenceCount; i++) {
                OSPFv3LSAPrefix currentPrefix = currentLsa->getPrefixes(i);
                OSPFv3LSAPrefix newPrefix = newLsa->getPrefixes(i);
                bool differentLink = ((currentPrefix.addressPrefix != newPrefix.addressPrefix) ||
                                      (currentPrefix.dnBit != newPrefix.dnBit) ||
                                      (currentPrefix.laBit != newPrefix.laBit) ||
                                      (currentPrefix.metric != newPrefix.metric) ||
                                      (currentPrefix.nuBit != newPrefix.nuBit) ||
                                      (currentPrefix.pBit != newPrefix.pBit) ||
                                      (currentPrefix.prefixLen != newPrefix.prefixLen) ||
                                      (currentPrefix.xBit != newPrefix.xBit));

                if (differentLink) {
                    differentBody = true;
                    break;
                }
            }
        }
    }

    return differentHeader || differentBody;
}//intraAreaPrefixLSADiffersFrom

IPv4Address OSPFv3Area::getNewIntraAreaPrefixLinkStateID()
{
    IPv4Address currIP = this->intraAreaPrefixLsID;
    int newIP = currIP.getInt()+1;
    this->intraAreaPrefixLsID = IPv4Address(newIP);
    return currIP;
}//getNewIntraAreaPrefixStateID


OSPFv3LSA* OSPFv3Area::getLSAbyKey(LSAKeyType LSAKey)
{
    switch(LSAKey.LSType){
    case ROUTER_LSA:
        for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case NETWORK_LSA:
        for (auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case INTER_AREA_PREFIX_LSA:
        for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case INTER_AREA_ROUTER_LSA:
        for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case NSSA_LSA:
        for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case INTRA_AREA_PREFIX_LSA:
        for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case LINK_LSA:
        for (auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++)
        {
            OSPFv3LinkLSA* lsa = (*it)->getLinkLSAbyKey(LSAKey);
            if(lsa != nullptr)
                return lsa;
        }

        break;
    }
//TODO - link lsa from interface
    //TOFO - as external from top data structure


    return nullptr;
}

void OSPFv3Area::debugDump()
{
    for(auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++)
        EV_DEBUG << "\t\tinterface id: " << (*it)->getIntName() << "\n";

}//debugDump

std::string OSPFv3Area::detailedInfo() const
{//TODO - adjust so that it pnly prints LSAs that are there
    std::stringstream out;

    out << "Router Link States (Area " << this->getAreaID().str(false) << ")\n" ;
    out << "ADV Router\tAge\tSeq#\tLink State ID\n";
    for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++) {
        OSPFv3LSAHeader& header = (*it)->getHeader();
        out << header.getAdvertisingRouter()<<"\t"<<header.getLsaAge()<<"\t"<<header.getLsaSequenceNumber()<<"\t"<<header.getLinkStateID().str(false)<<"\n";
    }

    out << "\nNet Link States (Area " << this->getAreaID().str(false) << ")\n" ;
    out << "ADV Router\tAge\tSeq#\tLink State ID\tRtr count\n";
    for(auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++) {
        OSPFv3LSAHeader& header = (*it)->getHeader();
        out << header.getAdvertisingRouter()<<"\t"<<header.getLsaAge()<<"\t"<<header.getLsaSequenceNumber()<<"\t"<<header.getLinkStateID().str(false)<<"\t" << (*it)->getAttachedRouterArraySize() << "\n";
    }

    out << "\nLink (Type-8) Link States (Area " << this->getAreaID().str(false) << ")\n" ;
    out << "ADV Router\tAge\tSeq#\tLink State ID\tInterface\n";
    for(auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++) {
        int linkLSACount = (*it)->getLinkLSACount();
        for(int i = 0; i<linkLSACount; i++) {
            OSPFv3LSAHeader& header = (*it)->getLinkLSA(i)->getHeader();
            out << header.getAdvertisingRouter()<<"\t"<<header.getLsaAge()<<"\t"<<header.getLsaSequenceNumber()<<"\t"<<header.getLinkStateID().str(false)<<"\t"<< (*it)->getIntName() << "\n";
        }
    }

    out << "\nIntra Area Prefix Link States (Area" << this->getAreaID().str(false) << ")\n" ;
    out << "Router\tAge\tSeq#\tLink ID\tRef-lstype\tRef-LSID\n";
    for(auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++) {
        OSPFv3LSAHeader& header = (*it)->getHeader();
        out << header.getAdvertisingRouter() << "\t" << header.getLsaAge() << "\t" << header.getLsaSequenceNumber() << "\t" << header.getLinkStateID() << "\t" << (*it)->getReferencedLSType() << "\t" << (*it)->getReferencedLSID()<<"\n";
    }

    return out.str();

}//detailedInfo
}//namespace inet
