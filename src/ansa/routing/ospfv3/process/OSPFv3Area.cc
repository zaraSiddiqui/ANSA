#include "ansa/routing/ospfv3/process/OSPFv3Area.h"
#include <iostream>
#include <stdio.h>
#include <string.h>

using namespace std;

namespace inet{

OSPFv3Area::OSPFv3Area(IPv4Address areaID, OSPFv3Instance* parent, OSPFv3AreaType type)
{
    this->areaID=areaID;
    this->containingInstance=parent;
    this->externalRoutingCapability = true;
    this->areaType = type;
    this->spfTreeRoot = nullptr;
//    EV_DEBUG << "Area Type : " << type << endl;

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

OSPFv3Interface* OSPFv3Area::getNetworkLSAInterface(IPv4Address id)
{

    for (auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++)
    {
        if (IPv4Address((*it)->getInterfaceId()) == id)
            return (*it);
    }

    return nullptr;

}//getInterfaceById

OSPFv3Interface* OSPFv3Area::getInterfaceByIndex(int id)
{
    std::map<int, OSPFv3Interface*>::iterator interfaceIt = this->interfaceByIndex.find(id);
    if(interfaceIt == this->interfaceByIndex.end())
        return nullptr;

    return interfaceIt->second;
}//getInterfaceByIndex

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
        this->interfaceByIndex[(*it)->getInterfaceIndex()]=(*it);
        (*it)->processEvent(OSPFv3Interface::INTERFACE_UP_EVENT);
    }

    OSPFv3IntraAreaPrefixLSA* prefixLsa = this->originateIntraAreaPrefixLSA();
    EV_DEBUG << "Creating InterAreaPrefixLSA from IntraAreaPrefixLSA\n";
    this->originateInterAreaPrefixLSA(prefixLsa, this);         // INTER !!!
    this->installIntraAreaPrefixLSA(prefixLsa);                 // INTRA !!!

    if((this->getAreaType() == OSPFv3AreaType::STUB) && (this->getInstance()->getAreaCount()>1))
        this->originateDefaultInterAreaPrefixLSA(this);
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

OSPFv3Interface* OSPFv3Area::getInterfaceByIndex(IPv4Address LinkStateID)
{
    int interfaceNum = interfaceList.size();
    for (int i = 0; i < interfaceNum; i++) {
        if ((interfaceList[i]->getType() != OSPFv3Interface::VIRTUAL_TYPE) &&
            (IPv4Address(interfaceList[i]->getInterfaceIndex()) == LinkStateID ))
        {
            return interfaceList[i];
        }
    }
    return nullptr;
}

OSPFv3LSAHeader* OSPFv3Area::findLSA(LSAKeyType lsaKey)     //asi skor hladas getLSAbyKey
{

//    EV_DEBUG << "FIND LSA:\n";

    switch (lsaKey.LSType) {
    case ROUTER_LSA: {
        RouterLSA* lsa = this->getRouterLSAbyKey(lsaKey);
        if(lsa == nullptr) {
            return nullptr;
        }
        else {
            OSPFv3LSAHeader* lsaHeader = &(lsa->getHeader());
            return lsaHeader;
        }
    }
    break;

    case NETWORK_LSA: {
        NetworkLSA* lsa = this->getNetworkLSAbyKey(lsaKey);
                if(lsa == nullptr) {
                    return nullptr;
                }
                else {
                    OSPFv3LSAHeader* lsaHeader = &(lsa->getHeader());
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

void OSPFv3Area::ageDatabase()
{
    long lsaCount = this->getRouterLSACount();
    bool shouldRebuildRoutingTable = false;
    long i;

    for (i = 0; i < lsaCount; i++) {
        RouterLSA *lsa = routerLSAList[i];
        unsigned short lsAge = lsa->getHeader().getLsaAge();
        bool selfOriginated = (lsa->getHeader().getAdvertisingRouter() == this->getInstance()->getProcess()->getRouterID());
//        TODO
//        bool unreachable = parentRouter->isDestinationUnreachable(lsa);

        if ((selfOriginated && (lsAge < (LS_REFRESH_TIME - 1))) || (!selfOriginated && (lsAge < (MAX_AGE - 1)))) {
            lsa->getHeader().setLsaAge(lsAge + 1);
            if ((lsAge + 1) % CHECK_AGE == 0) {
                if (!lsa->validateLSChecksum()) {   // always return true
                    EV_ERROR << "Invalid LS checksum. Memory error detected!\n";
                }
            }
            lsa->incrementInstallTime();
        }
        if (selfOriginated && (lsAge == (LS_REFRESH_TIME - 1))) {
//            if (unreachable) {
//                lsa->getHeader().setLsAge(MAX_AGE);
//                floodLSA(lsa);
//                lsa->incrementInstallTime();
//            }
//        else

            long sequenceNumber = lsa->getHeader().getLsaSequenceNumber();
            if (sequenceNumber == MAX_SEQUENCE_NUMBER) {
                lsa->getHeader().setLsaAge(MAX_AGE);
                floodLSA(lsa);
                lsa->incrementInstallTime();
            }
            else {
                RouterLSA *newLSA = originateRouterLSA();
                newLSA->getHeader().setLsaSequenceNumber(sequenceNumber + 1);
                shouldRebuildRoutingTable |= updateRouterLSA(lsa, newLSA);
                delete newLSA;

                floodLSA(lsa);
            }

        }
        if (!selfOriginated && (lsAge == MAX_AGE - 1)) {
            lsa->getHeader().setLsaAge(MAX_AGE);
            floodLSA(lsa);
            lsa->incrementInstallTime();
        }
        if (lsAge == MAX_AGE) {
            LSAKeyType lsaKey;

            lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
            lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();

            if (!isOnAnyRetransmissionList(lsaKey) &&
                !hasAnyNeighborInStates(OSPFv3Neighbor::EXCHANGE_STATE | OSPFv3Neighbor::LOADING_STATE))
            {
                if (!selfOriginated /*|| unreachable*/) {
                    routerLSAsByID.erase(lsa->getHeader().getLinkStateID());
                    delete lsa;
                    routerLSAList[i] = nullptr;
                    shouldRebuildRoutingTable = true;
                }
                else {
                    RouterLSA *newLSA = originateRouterLSA();
                    long sequenceNumber = lsa->getHeader().getLsaSequenceNumber();

                    newLSA->getHeader().setLsaSequenceNumber((sequenceNumber == MAX_SEQUENCE_NUMBER) ? INITIAL_SEQUENCE_NUMBER : sequenceNumber + 1);
                    shouldRebuildRoutingTable |= updateRouterLSA(lsa, newLSA);
                    delete newLSA;

                    floodLSA(lsa);
                }
            }
        }
    }

    auto routerIt = routerLSAList.begin();
    while (routerIt != routerLSAList.end()) {
        if ((*routerIt) == nullptr) {
            routerIt = routerLSAList.erase(routerIt);
        }
        else {
            routerIt++;
        }
    }

    lsaCount = networkLSAList.size();
    for (i = 0; i < lsaCount; i++) {
        unsigned short lsAge = networkLSAList[i]->getHeader().getLsaAge();
//        bool unreachable = parentRouter->isDestinationUnreachable(networkLSAs[i]);
        NetworkLSA *lsa = networkLSAList[i];
        OSPFv3Interface *localIntf = nullptr;
        if (lsa->getHeader().getAdvertisingRouter() == this->getInstance()->getProcess()->getRouterID()){
            localIntf = getNetworkLSAInterface(lsa->getHeader().getLinkStateID());
        }
        bool selfOriginated = false;

        if ((localIntf != nullptr) &&
            (localIntf->getState() == OSPFv3Interface::INTERFACE_STATE_DESIGNATED) &&
            (localIntf->getNeighborCount() > 0) &&
            (localIntf->hasAnyNeighborInStates(OSPFv3Neighbor::FULL_STATE)))
        {
            selfOriginated = true;
        }

        if ((selfOriginated && (lsAge < (LS_REFRESH_TIME - 1))) || (!selfOriginated && (lsAge < (MAX_AGE - 1)))) {
            EV_DEBUG << "lsAge + 1\n";
            cout << "lsAge + 1\n";

            lsa->getHeader().setLsaAge(lsAge + 1);
            if ((lsAge + 1) % CHECK_AGE == 0) {     // always TRUE
                if (!lsa->validateLSChecksum()) {
                    EV_ERROR << "Invalid LS checksum. Memory error detected!\n";
                }
            }
            lsa->incrementInstallTime();
        }
        if (selfOriginated && (lsAge == (LS_REFRESH_TIME - 1))) {
//            TODO
//            if (unreachable) {
//                lsa->getHeader().setLsAge(MAX_AGE);
//                floodLSA(lsa);
//                lsa->incrementInstallTime();
//            }
//            else {
            long sequenceNumber = lsa->getHeader().getLsaSequenceNumber();
            if (sequenceNumber == MAX_SEQUENCE_NUMBER) {
                lsa->getHeader().setLsaAge(MAX_AGE);
                floodLSA(lsa);
                lsa->incrementInstallTime();
            }
            else {
                NetworkLSA *newLSA = originateNetworkLSA(localIntf);

                if (newLSA != nullptr) {
                    newLSA->getHeader().setLsaSequenceNumber(sequenceNumber + 1);
                    shouldRebuildRoutingTable |= updateNetworkLSA(lsa,newLSA);
                    delete newLSA;
                }
                else {    // no neighbors on the network -> old NetworkLSA must be flushed
                    lsa->getHeader().setLsaAge(MAX_AGE);
                    lsa->incrementInstallTime();
                }

                floodLSA(lsa);
            }
        }
        if (!selfOriginated && (lsAge == MAX_AGE - 1)) {
            lsa->getHeader().setLsaAge(MAX_AGE);
            floodLSA(lsa);
            lsa->incrementInstallTime();
        }
        if (lsAge == MAX_AGE) {
            LSAKeyType lsaKey;

            lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
            lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();

            if (!isOnAnyRetransmissionList(lsaKey) &&
                !hasAnyNeighborInStates(OSPFv3Neighbor::EXCHANGE_STATE | OSPFv3Neighbor::LOADING_STATE))
            {
                if (!selfOriginated /*|| unreachable*/) {
                    networkLSAsByID.erase(lsa->getHeader().getLinkStateID());
                    delete lsa;
                    networkLSAList[i] = nullptr;
                    shouldRebuildRoutingTable = true;
                }
                else {
                    NetworkLSA *newLSA = originateNetworkLSA(localIntf);
                    long sequenceNumber = lsa->getHeader().getLsaSequenceNumber();

                    if (newLSA != nullptr) {
                        newLSA->getHeader().setLsaSequenceNumber((sequenceNumber == MAX_SEQUENCE_NUMBER) ? INITIAL_SEQUENCE_NUMBER : sequenceNumber + 1);
                        shouldRebuildRoutingTable |= updateNetworkLSA(lsa,newLSA);
                        delete newLSA;

                        floodLSA(lsa);
                    }
                    else {    // no neighbors on the network -> old NetworkLSA must be deleted
                        delete networkLSAList[i];
                    }
                }
            }
        }
    }

    auto networkIt = networkLSAList.begin();
    while (networkIt != networkLSAList.end()) {
        if ((*networkIt) == nullptr) {
            networkIt = networkLSAList.erase(networkIt);
        }
        else {
            networkIt++;
        }
    }

    lsaCount = intraAreaPrefixLSAList.size();
    for (i = 0; i < lsaCount; i++)
    {
        unsigned short lsAge = intraAreaPrefixLSAList[i]->getHeader().getLsaAge();
        //        bool unreachable = parentRouter->isDestinationUnreachable(networkLSAs[i]);
        IntraAreaPrefixLSA *lsa = intraAreaPrefixLSAList[i];
        OSPFv3Interface *localIntf = nullptr;
        if (lsa->getHeader().getAdvertisingRouter() == this->getInstance()->getProcess()->getRouterID()){
           localIntf = getNetworkLSAInterface(lsa->getReferencedLSID());
        }
        bool selfOriginated = false;

        if ((localIntf != nullptr) &&
           (localIntf->getState() == OSPFv3Interface::INTERFACE_STATE_DESIGNATED) &&
           (localIntf->getNeighborCount() > 0) &&
           (localIntf->hasAnyNeighborInStates(OSPFv3Neighbor::FULL_STATE)))
        {
           selfOriginated = true;
        }

        if ((selfOriginated && (lsAge < (LS_REFRESH_TIME - 1))) || (!selfOriginated && (lsAge < (MAX_AGE - 1)))) {
           lsa->getHeader().setLsaAge(lsAge + 1);
           if ((lsAge + 1) % CHECK_AGE == 0) {     // always TRUE
               if (!lsa->validateLSChecksum()) {
                   EV_ERROR << "Invalid LS checksum. Memory error detected!\n";
               }
           }
           lsa->incrementInstallTime();
        }
        if (selfOriginated && (lsAge == (LS_REFRESH_TIME - 1))) {
        //            TODO
        //            if (unreachable) {
        //                lsa->getHeader().setLsAge(MAX_AGE);
        //                floodLSA(lsa);
        //                lsa->incrementInstallTime();
        //            }
        //            else {
           long sequenceNumber = lsa->getHeader().getLsaSequenceNumber();
           if (sequenceNumber == MAX_SEQUENCE_NUMBER) {
               lsa->getHeader().setLsaAge(MAX_AGE);
               floodLSA(lsa);
               lsa->incrementInstallTime();
           }
           else {

               IntraAreaPrefixLSA *newLSA = nullptr;
               // If this is DR, find Network LSA from which make new IntraAreaPrefix LSA
                if (localIntf != nullptr &&
                    localIntf->getType() == OSPFv3Interface::BROADCAST_TYPE)
                    {
                    NetworkLSA *netLSA = findNetworkLSA(localIntf->getInterfaceId(), this->getInstance()->getProcess()->getRouterID());
                    newLSA = originateNetIntraAreaPrefixLSA(netLSA, localIntf);
                }
                else // OSPFv3Interface::ROUTER_TYPE
                {
                    newLSA = originateIntraAreaPrefixLSA();
                }

                if (newLSA != nullptr) {
                    newLSA->getHeader().setLsaSequenceNumber(sequenceNumber + 1);
                    shouldRebuildRoutingTable |= updateIntraAreaPrefixLSA(lsa,newLSA);
                    delete newLSA;
                }
                else {    // no neighbors on the network -> old NetworkLSA must be flushed
                    lsa->getHeader().setLsaAge(MAX_AGE);
                    lsa->incrementInstallTime();
                }
               floodLSA(lsa);
           }
        }

        if (!selfOriginated && (lsAge == MAX_AGE - 1)) {
           lsa->getHeader().setLsaAge(MAX_AGE);
           floodLSA(lsa);
           lsa->incrementInstallTime();
        }

        if (lsAge == MAX_AGE)
        {
            LSAKeyType lsaKey;

            lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
            lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();

            if (!isOnAnyRetransmissionList(lsaKey) &&
                    !hasAnyNeighborInStates(OSPFv3Neighbor::EXCHANGE_STATE | OSPFv3Neighbor::LOADING_STATE))
            {
                if (!selfOriginated /*|| unreachable*/)
                {
                   intraAreaPrefixLSAByID.erase(lsa->getHeader().getLinkStateID());
                   delete lsa;
                   intraAreaPrefixLSAList[i] = nullptr;
                   shouldRebuildRoutingTable = true;
                }
                else
                {
                    IntraAreaPrefixLSA *newLSA = nullptr;
                    if (localIntf != nullptr &&
                    localIntf->getType() == OSPFv3Interface::BROADCAST_TYPE)
                    {
                        NetworkLSA *netLSA = findNetworkLSA(localIntf->getInterfaceId(), this->getInstance()->getProcess()->getRouterID());
                        newLSA = originateNetIntraAreaPrefixLSA(netLSA, localIntf);
                    }
                    else // OSPFv3Interface::ROUTER_TYPE
                    {
                        newLSA = originateIntraAreaPrefixLSA();
                    }

                    long sequenceNumber = lsa->getHeader().getLsaSequenceNumber();

                    if (newLSA != nullptr) {
                        newLSA->getHeader().setLsaSequenceNumber((sequenceNumber == MAX_SEQUENCE_NUMBER) ? INITIAL_SEQUENCE_NUMBER : sequenceNumber + 1);
                        shouldRebuildRoutingTable |= updateIntraAreaPrefixLSA(lsa,newLSA);
                        delete newLSA;

                        floodLSA(lsa);
                    }
                    else
                    {    // no neighbors on the network -> old NetworkLSA must be flushed
                        delete intraAreaPrefixLSAList[i];
                    }
                }
            }
        }
    }

   auto intraArIt = intraAreaPrefixLSAList.begin();
   while (intraArIt != intraAreaPrefixLSAList.end()) {
       if ((*intraArIt) == nullptr) {
           intraArIt = intraAreaPrefixLSAList.erase(intraArIt);
       }
       else {
           intraArIt++;
       }
   }



    ////////////////////////////////////////////////////////////////////////////////

   for (int x = 0; x < interfaceList.size(); x++)
   {
       shouldRebuildRoutingTable |= interfaceList[x]->ageDatabase();
   }

    long interfaceCount = interfaceList.size();
    for (long m = 0; m < interfaceCount; m++) {
        interfaceList[m]->ageTransmittedLSALists();
    }

    if (shouldRebuildRoutingTable) {
        getInstance()->getProcess()->rebuildRoutingTable();
    }

    for (int d = 0; d < networkLSAList.size(); d++)
    {
        cout << "age is = " <<  networkLSAList[d]->getHeader().getLsaAge();
    }

    //TODO: add aging for missing LSAs
}

//------------------------------------- Router LSA --------------------------------------//
/*Into any given OSPF area, a router will originate several LSAs.
    Each router originates a router-LSA.  If the router is also the
    Designated Router for any of the area's networks, it will
    originate network-LSAs for those networks.

    Area border routers originate a single summary-LSA for each
    known inter-area destination.  AS boundary routers originate a
    single AS-external-LSA for each known AS external destination.*/
RouterLSA* OSPFv3Area::originateRouterLSA()
{
    EV_DEBUG << "Originating RouterLSA (Router-LSA)\n";
    cout << "////////////// " <<  this->getInstance()->getProcess()->getRouterID() << " originate Router LSA ///////////" << endl;
    RouterLSA *routerLSA = new RouterLSA;
    OSPFv3LSAHeader& lsaHeader = routerLSA->getHeader();
    long interfaceCount = this->interfaceList.size();
    OSPFv3Options lsOptions;
    memset(&lsOptions, 0, sizeof(OSPFv3Options));

    //First set the LSA Header
    lsaHeader.setLsaAge((int)simTime().dbl());
    //The LSA Type is 0x2001
    lsaHeader.setLsaType(ROUTER_LSA);

    lsaHeader.setLinkStateID(this->getInstance()->getProcess()->getRouterID());
    lsaHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
//    lsaHeader.setLsaSequenceNumber(this->getCurrentRouterSequence());  TODO: toto tu bolo povodne, ale v ospfv2 je makro na initial seq num LG
    lsaHeader.setLsaSequenceNumber(INITIAL_SEQUENCE_NUMBER);
    this->incrementRouterSequence();

    if(this->getInstance()->getAreaCount()>1)
        routerLSA->setBBit(true);
    //TODO - set options

    for(int i=0; i<interfaceCount; i++)
    {
        OSPFv3Interface* intf = this->interfaceList.at(i);

        if (intf->getState() == OSPFv3Interface::INTERFACE_STATE_DOWN) {
            continue;
        }

            cout << "intID = " << intf->getInterfaceId() << endl;
            OSPFv3RouterLSABody routerLSABody;
            memset(&routerLSABody, 0, sizeof(OSPFv3RouterLSABody));

            // if (intf->getState() > Interface::LOOPBACK_STATE) TODO
            switch(intf->getType())
            {
                case OSPFv3Interface::POINTTOPOINT_TYPE:
                    routerLSABody.type=POINT_TO_POINT;              //TODO: this body never get created LG
                    break;

                case OSPFv3Interface::BROADCAST_TYPE: {
                    routerLSABody.type=TRANSIT_NETWORK;
                    cout <<  "state = " << intf->getState() << endl;
                    cout << "intf-getDR_ID = " << intf->getDesignatedID() << endl;

                    OSPFv3Neighbor *DRouter =intf->getNeighborById(intf->getDesignatedID());

                    if ( ((DRouter != nullptr) && (DRouter->getState() == OSPFv3Neighbor::FULL_STATE)) ||
                            (intf->getDesignatedID() == this->getInstance()->getProcess()->getRouterID()) //TODO: hasAnyNeighborInStates
                             )
                    {
                        routerLSABody.interfaceID = intf->getInterfaceId();      // davam ID mojho interface
                        routerLSABody.metric = 1;//TODO - correct this

                        routerLSABody.neighborInterfaceID = intf->getDesignatedIntID();
                        routerLSABody.neighborRouterID = intf->getDesignatedID();

                        routerLSA->setRoutersArraySize(i+1);
                        routerLSA->setRouters(i, routerLSABody);
                    }
                }
                    break;

                case OSPFv3Interface::VIRTUAL_TYPE:
                    routerLSABody.type=VIRTUAL_LINK;
                    break;

//            routerLSABody.interfaceID = intf->getInterfaceIndex(); LG
//            routerLSABody.metric = 1;//TODO - correct this

            // toto je zle, TO MOZE FUNGOVAT MOZNO TAK PRE P2P
//            OSPFv3Neighbor* neighbor = intf->getNeighbor(j);
//            routerLSABody.neighborInterfaceID = neighbor->getNeighborInterfaceID();
//            routerLSABody.neighborRouterID = neighbor->getNeighborID();

//            routerLSA->setRoutersArraySize(j+1);
//            routerLSA->setRouters(j, routerLSABody);
        }
    }


    //originate Intra-Area-Prefix LSA along with any Router LSA
//    this->installIntraAreaPrefixLSA(this->originateIntraAreaPrefixLSA());

    return routerLSA;
}//originateRouterLSA


RouterLSA* OSPFv3Area::getRouterLSAbyKey(LSAKeyType LSAKey)
{
    EV_DEBUG << "GET ROUTER LSA BY KEY  \n";
    for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
    {
        EV_DEBUG << "FOR, routerLSAList size: " << this->routerLSAList.size() << "\n";
        if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
            return (*it);
        }
    }

    return nullptr;
}//getRouterLSAByKey

//add LSA message into list of all router-LSA for this area
bool OSPFv3Area::installRouterLSA(OSPFv3RouterLSA *lsa)
{
    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();

    RouterLSA* lsaInDatabase = (RouterLSA*)this->getLSAbyKey(lsaKey);
    if (lsaInDatabase != nullptr) {
        this->removeFromAllRetransmissionLists(lsaKey);
        return this->updateRouterLSA(lsaInDatabase, lsa);
    }
    else {
        RouterLSA* lsaCopy = new RouterLSA(*lsa);
        EV_DEBUG << "RouterLSA was added to routerLSAList";
        this->routerLSAList.push_back(lsaCopy);
        return true;
    }
}//installRouterLSA


bool OSPFv3Area::updateRouterLSA(RouterLSA* currentLsa, OSPFv3RouterLSA* newLsa)
{
    bool different = routerLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->resetInstallTime();
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

void OSPFv3Area::deleteRouterLSA(int index) {
    RouterLSA *delRouter = this->routerLSAList.at(index);
    OSPFv3LSAHeader &routerHeader = delRouter->getHeader();

    int prefixCount = this->intraAreaPrefixLSAList.size();
    for(int i=0; i<prefixCount; i++) {
       OSPFv3IntraAreaPrefixLSA* lsa = this->intraAreaPrefixLSAList.at(i);

       // odtrani dane LSA aj z intraAreaPrefixLSAList
       if (lsa->getReferencedAdvRtr() == routerHeader.getAdvertisingRouter() &&
               lsa->getReferencedLSID() == routerHeader.getLinkStateID() &&
               lsa->getReferencedLSType() == ROUTER_LSA) {
           this->intraAreaPrefixLSAList.erase(this->intraAreaPrefixLSAList.begin()+i);
           EV_DEBUG << "Deleting IntraAreaPrefixLSA -- deleteRouterLSA\n";
           break;
       }
    }



    this->routerLSAList.erase(this->routerLSAList.begin()+index);
}

bool OSPFv3Area::floodLSA(OSPFv3LSA* lsa, OSPFv3Interface* interface, OSPFv3Neighbor* neighbor)
{
    EV_DEBUG << "Flooding from Area to all interfaces\n";
    std::cout << this->getInstance()->getProcess()->getRouterID() << " - FLOOD LSA AREA!!" << endl;
    bool floodedBackOut = false;
    long interfaceCount = this->interfaceList.size();

    for (long i = 0; i < interfaceCount; i++) {
        if (interfaceList.at(i)->floodLSA(lsa, interface, neighbor)) {
            floodedBackOut = true;
        }
    }

    return floodedBackOut;
}//floodLSA


//bool OSPFv3Area::isLocalAddress(IPv6Address address) const
//{
//    long interfaceCount = interfaceList.size();
//    for (long i = 0; i < interfaceCount; i++) {
//        if (interfaceList[i]->getInterfaceIP() == address) {
//            return true;
//        }
//    }
//    return false;
//}

bool OSPFv3Area::hasAnyNeighborInStates(int states) const
{
    long interfaceCount = this->interfaceList.size();
    for (long i = 0; i < interfaceCount; i++) {
        if (interfaceList.at(i)->hasAnyNeighborInStates(states)) {
            return true;
        }
    }
    return false;
}

void OSPFv3Area::removeFromAllRetransmissionLists(LSAKeyType lsaKey)
{
    long interfaceCount = this->interfaceList.size();
    for (long i = 0; i < interfaceCount; i++) {
        this->interfaceList.at(i)->removeFromAllRetransmissionLists(lsaKey);
    }
}

bool OSPFv3Area::isOnAnyRetransmissionList(LSAKeyType lsaKey) const
{
    long interfaceCount = this->interfaceList.size();
    for (long i = 0; i < interfaceCount; i++) {
        if (interfaceList.at(i)->isOnAnyRetransmissionList(lsaKey)) {
            return true;
        }
    }
    return false;
}

//------------------------------------- Network LSA --------------------------------------//
NetworkLSA* OSPFv3Area::originateNetworkLSA(OSPFv3Interface* interface)
{
// TODO LG
//    if (interface->hasAnyNeighborInStates(OSPFv3Neighbor::FULL_STATE)) {

        NetworkLSA* networkLsa = new NetworkLSA();
        OSPFv3LSAHeader& lsaHeader = networkLsa->getHeader();
        OSPFv3Options lsOptions;
        memset(&lsOptions, 0, sizeof(OSPFv3Options));

        //First set the LSA Header
        lsaHeader.setLsaAge((int)simTime().dbl());
        //The LSA Type is 0x2002
        lsaHeader.setLsaType(NETWORK_LSA);

        lsaHeader.setLinkStateID(IPv4Address(interface->getInterfaceId()));
        lsaHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
//        lsaHeader.setLsaSequenceNumber(this->getCurrentNetworkSequence());    TODO: toto tu bolo povodne, ale v ospfv2 je makro na initial seq num
        lsaHeader.setLsaSequenceNumber(INITIAL_SEQUENCE_NUMBER);

        uint16_t packetLength = OSPFV3_LSA_HEADER_LENGTH + 4;//4 for options field

        //now the body
        networkLsa->setOspfOptions(lsOptions);
        int attachedCount = interface->getNeighborCount();//+1 for this router
//        cout << " ROUTER " << this->getInstance()->getProcess()->getRouterID() << " - neigborCount = " << attachedCount << endl;
        if(attachedCount >= 1){
            networkLsa->setAttachedRouterArraySize(attachedCount+1);
            for(int i=0; i<attachedCount; i++){
                OSPFv3Neighbor* neighbor = interface->getNeighbor(i);
                networkLsa->setAttachedRouter(i, neighbor->getNeighborID());
                packetLength+=4;
            }

            networkLsa->setAttachedRouter(attachedCount, this->getInstance()->getProcess()->getRouterID());
            packetLength+=4;
        }

        lsaHeader.setLsaLength(packetLength);
//        this->networkLSAList.push_back(networkLsa);
        return networkLsa;
//        FIXME:
//    }
//    else {
//        return nullptr;
//    }
}//originateNetworkLSA

IntraAreaPrefixLSA* OSPFv3Area::getNetIntraAreaPrefixLSA(L3Address prefix, int prefixLen)
{
    int intraPrefCnt = this->getIntraAreaPrefixLSACount();
    for(int i=0; i<intraPrefCnt; i++){
        IntraAreaPrefixLSA* intraLsa = this->getIntraAreaPrefixLSA(i);
        if(intraLsa->getReferencedLSType() == NETWORK_LSA){
            L3Address intraPrefix = intraLsa->getPrefixes(0).addressPrefix;
            int intraPrefLen = intraLsa->getPrefixes(0).prefixLen;
            if(prefix.getPrefix(prefixLen) == intraPrefix.getPrefix(intraPrefLen))
                return intraLsa;
        }
    }
    return nullptr;
}

NetworkLSA* OSPFv3Area::getNetworkLSAbyKey(LSAKeyType LSAKey)
{
    EV_DEBUG << "GET NETWORK LSA BY KEY  \n";
    for (auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++)
    {
        EV_DEBUG << "FOR, networkLSAList size: " << this->networkLSAList.size() << "\n";
        if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
            return (*it);
        }
    }

    return nullptr;
}//getRouterLSAByKey

bool OSPFv3Area::installNetworkLSA(OSPFv3NetworkLSA *lsa)
{
    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();

    NetworkLSA* lsaInDatabase = (NetworkLSA*)this->getLSAbyKey(lsaKey);
    if (lsaInDatabase != nullptr) {
        this->removeFromAllRetransmissionLists(lsaKey);
        return this->updateNetworkLSA(lsaInDatabase, lsa);
    }
    else {
        NetworkLSA* lsaCopy = new NetworkLSA(*lsa);
        this->networkLSAList.push_back(lsaCopy);
        return true;
    }
}//installNetworkLSA


bool OSPFv3Area::updateNetworkLSA(NetworkLSA* currentLsa, OSPFv3NetworkLSA* newLsa)
{
    bool different = networkLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->resetInstallTime();
    currentLsa->getHeader().setLsaAge(0);//reset the age
    if (different) {
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

//----------------------------------------- Inter-Area-Prefix LSA (LSA 4)------------------------------------------//
void OSPFv3Area::originateInterAreaPrefixLSA(OSPFv3IntraAreaPrefixLSA* lsa, OSPFv3Area* fromArea)
{
    int packetLength = OSPFV3_LSA_HEADER_LENGTH+OSPFV3_INTER_AREA_PREFIX_LSA_HEADER_LENGTH;
    int prefixCount = 0;

    //Only one Inter-Area-Prefix LSA for an area so only one header will suffice
    InterAreaPrefixLSA* newLsa = new InterAreaPrefixLSA();
    OSPFv3LSAHeader& newHeader = newLsa->getHeader();
    newHeader.setLsaAge((int)simTime().dbl());
    newHeader.setLsaType(INTER_AREA_PREFIX_LSA);
    newHeader.setLinkStateID(this->getNewInterAreaPrefixLinkStateID());
    newHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    newHeader.setLsaSequenceNumber(this->interAreaPrefixLSASequenceNumber++);

    OSPFv3LSAPrefix& prefix = lsa->getPrefixes(0);//TODO - this here takes only one prefix, need to make one new LSA for each prefix
    newLsa->setDnBit(prefix.dnBit);
    newLsa->setLaBit(prefix.laBit);
    newLsa->setNuBit(prefix.nuBit);
    newLsa->setPBit(prefix.pBit);
    newLsa->setXBit(prefix.xBit);
    newLsa->setMetric(prefix.metric);
    newLsa->setPrefixLen(64);//TODO - correct pref length
    newLsa->setPrefix(prefix.addressPrefix);

//    EV_DEBUG << "Setting Address Prefix in InterAreaPrefixLSA to " << prefix.addressPrefix << endl;

    for(int i = 0; i < this->getInstance()->getAreaCount(); i++)
    {
        OSPFv3Area* area = this->getInstance()->getArea(i);
        if(area->getAreaID() == fromArea->getAreaID())
            continue;

        area->installInterAreaPrefixLSA(newLsa);
    }
    //TODO - length!!!
}

void OSPFv3Area::originateInterAreaPrefixLSA(OSPFv3LSA* prefLsa, OSPFv3Area* fromArea)
{
    for(int i = 0; i < this->getInstance()->getAreaCount(); i++)
    {
        OSPFv3Area* area = this->getInstance()->getArea(i);
        if(area->getAreaID() == fromArea->getAreaID())
            continue;

        InterAreaPrefixLSA *lsa = check_and_cast<InterAreaPrefixLSA *>(prefLsa);
        int packetLength = OSPFV3_LSA_HEADER_LENGTH+OSPFV3_INTER_AREA_PREFIX_LSA_HEADER_LENGTH;
        int prefixCount = 0;

        //Only one Inter-Area-Prefix LSA for an area so only one header will suffice
        InterAreaPrefixLSA* newLsa = new InterAreaPrefixLSA();
        OSPFv3LSAHeader& newHeader = newLsa->getHeader();
        newHeader.setLsaAge(0);
        newHeader.setLsaType(INTER_AREA_PREFIX_LSA);
        newHeader.setLinkStateID(area->getNewInterAreaPrefixLinkStateID());
        newHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
        newHeader.setLsaSequenceNumber(area->getCurrentInterAreaPrefixSequence());
        area->incrementInterAreaPrefixSequence();

        newLsa->setDnBit(lsa->getDnBit());
        newLsa->setLaBit(lsa->getLaBit());
        newLsa->setNuBit(lsa->getNuBit());
        newLsa->setPBit(lsa->getPBit());
        newLsa->setXBit(lsa->getXBit());
        newLsa->setMetric(lsa->getMetric());
        newLsa->setPrefixLen(64);//TODO - correct pref length
        newLsa->setPrefix(lsa->getPrefix());

        area->installInterAreaPrefixLSA(newLsa);
    }
    //TODO - length!!!
}

void OSPFv3Area::originateDefaultInterAreaPrefixLSA(OSPFv3Area* toArea)
{
    int packetLength = OSPFV3_LSA_HEADER_LENGTH+OSPFV3_INTER_AREA_PREFIX_LSA_HEADER_LENGTH;
    int prefixCount = 0;

    //Only one Inter-Area-Prefix LSA for an area so only one header will suffice
    InterAreaPrefixLSA* newLsa = new InterAreaPrefixLSA();
    OSPFv3LSAHeader& newHeader = newLsa->getHeader();
    newHeader.setLsaAge(0);
    newHeader.setLsaType(INTER_AREA_PREFIX_LSA);
    newHeader.setLinkStateID(toArea->getNewInterAreaPrefixLinkStateID());
    newHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    newHeader.setLsaSequenceNumber(toArea->getCurrentInterAreaPrefixSequence());        // TODO: skotrolovat ci je to dobry setSqNum
    toArea->incrementInterAreaPrefixSequence();

//    OSPFv3LSAPrefix& prefix = lsa->getPrefixes(0);//TODO - this here takes only one prefix, need to make one new LSA for each prefix
    newLsa->setDnBit(false);
    newLsa->setLaBit(false);
    newLsa->setNuBit(false);
    newLsa->setPBit(false);
    newLsa->setXBit(false);
    newLsa->setMetric(1);
    newLsa->setPrefixLen(0);

    if(this->getInstance()->getAddressFamily() == IPV4INSTANCE) {
        IPv4Address defaultPref = IPv4Address("0.0.0.0");
        newLsa->setPrefix(defaultPref);
    }
    else{
        IPv6Address defaultPref = IPv6Address("::");
        newLsa->setPrefix(defaultPref);
    }

//    EV_DEBUG << "Setting Address Prefix in InterAreaPrefixLSA to " << prefix.addressPrefix << endl;
    toArea->installInterAreaPrefixLSA(newLsa);
    //TODO - length!!!
}

bool OSPFv3Area::installInterAreaPrefixLSA(OSPFv3InterAreaPrefixLSA* lsa)
{
    OSPFv3LSAHeader &header = lsa->getHeader();

    EV_DEBUG << "Installing Inter-Area-Prefix LSA:\nLink State ID: " << header.getLinkStateID() << "\nAdvertising router: " << header.getAdvertisingRouter();
    EV_DEBUG << "\nLS Seq Number: " << header.getLsaSequenceNumber() << endl;

    EV_DEBUG << "Prefix Address: " << lsa->getPrefix();
    EV_DEBUG << "\nPrefix Length: " << lsa->getPrefixLen();
    if(lsa->getDnBit())
        EV_DEBUG << "DN ";
    if(lsa->getLaBit())
        EV_DEBUG << "LA ";
    if(lsa->getNuBit())
        EV_DEBUG << "NU ";
    if(lsa->getPBit())
        EV_DEBUG << "P ";
    if(lsa->getXBit())
        EV_DEBUG << "X ";

    EV_DEBUG << ", Metric: " << lsa->getMetric() << "\n";

    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();
    InterAreaPrefixLSA* lsaInDatabase = (InterAreaPrefixLSA*)this->getLSAbyKey(lsaKey);
    if (lsaInDatabase != nullptr) {
        this->removeFromAllRetransmissionLists(lsaKey);
        EV_DEBUG << "Only updating\n";
        return this->updateInterAreaPrefixLSA(lsaInDatabase, lsa);
    }
    else {
        InterAreaPrefixLSA* lsaCopy = new InterAreaPrefixLSA(*lsa);
        this->interAreaPrefixLSAList.push_back(lsaCopy);
        EV_DEBUG << "creating new one\n";
        return true;
    }
}

bool OSPFv3Area::updateInterAreaPrefixLSA(InterAreaPrefixLSA* currentLsa, OSPFv3InterAreaPrefixLSA* newLsa)
{
    bool different = interAreaPrefixLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->getHeader().setLsaAge(0);//reset the age
    if (different) {
        return true;
    }
    else {
        return false;
    }
}

bool OSPFv3Area::interAreaPrefixLSADiffersFrom(OSPFv3InterAreaPrefixLSA* currentLsa, OSPFv3InterAreaPrefixLSA* newLsa)
{
    const OSPFv3LSAHeader& thisHeader = currentLsa->getHeader();
    const OSPFv3LSAHeader& lsaHeader = newLsa->getHeader();
    bool differentHeader = (((thisHeader.getLsaAge() == MAX_AGE) && (lsaHeader.getLsaAge() != MAX_AGE)) ||
            ((thisHeader.getLsaAge() != MAX_AGE) && (lsaHeader.getLsaAge() == MAX_AGE)) ||
            (thisHeader.getLsaLength() != lsaHeader.getLsaLength()));
    bool differentBody = false;

    if (!differentHeader) {
        differentBody = ((currentLsa->getMetric() != newLsa->getMetric()) ||
                (currentLsa->getPrefix() != newLsa->getPrefix()) ||
                (currentLsa->getPrefixLen() != newLsa->getPrefixLen()) ||
                (currentLsa->getDnBit() != newLsa->getDnBit()) ||
                (currentLsa->getLaBit() != newLsa->getLaBit()) ||
                (currentLsa->getMetric() != newLsa->getMetric()) ||
                (currentLsa->getNuBit() != newLsa->getNuBit()) ||
                (currentLsa->getPBit() != newLsa->getPBit()) ||
                (currentLsa->getPrefixLen() != newLsa->getPrefixLen()) ||
                (currentLsa->getXBit() != newLsa->getXBit()));
    }

    return differentHeader || differentBody;
}

IPv4Address OSPFv3Area::getNewInterAreaPrefixLinkStateID()
{
    IPv4Address currIP = this->interAreaPrefixLsID;
    int newIP = currIP.getInt()+1;
    this->interAreaPrefixLsID = IPv4Address(newIP);
    return currIP;
}


//----------------------------------------- Intra-Area-Prefix LSA (LSA 9) ------------------------------------------//
IntraAreaPrefixLSA* OSPFv3Area::originateIntraAreaPrefixLSA()
{
    EV_DEBUG << "calling originateIntraAreaPrefixLSA\n";
    int packetLength = OSPFV3_LSA_HEADER_LENGTH+OSPFV3_INTRA_AREA_PREFIX_LSA_HEADER_LENGTH;
    int prefixCount = 0;

    //Only one Inter-Area-Prefix LSA for an area so only one header will suffice
    IntraAreaPrefixLSA* newLsa = new IntraAreaPrefixLSA();
    OSPFv3LSAHeader& newHeader = newLsa->getHeader();
    newHeader.setLsaAge((int)simTime().dbl());
    newHeader.setLsaType(INTRA_AREA_PREFIX_LSA);
    newHeader.setLinkStateID(this->getNewIntraAreaPrefixLinkStateID());
    newHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    newHeader.setLsaSequenceNumber(this->intraAreaPrefixLSASequenceNumber++);

    //for each Router LSA there is a corresponding Inter-Area-Prefix LSA
    for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++) {
        OSPFv3LSAHeader &routerHeader = (*it)->getHeader();
        if(routerHeader.getAdvertisingRouter()!=this->getInstance()->getProcess()->getRouterID())
            continue;
        else {
            newLsa->setReferencedLSType(ROUTER_LSA);
            newLsa->setReferencedLSID(routerHeader.getLinkStateID());
            newLsa->setReferencedAdvRtr(routerHeader.getAdvertisingRouter());
        }
    }

    int currentPrefix = 0;
    for(auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++) {
//        EV_DEBUG << "(*it)->getTransitNetInt() = " << (*it)->getTransitNetInt() << "\n";
        if((*it)->getTransitNetInt()==false) {//FIXME << povodne
//        if((*it)->getType() != OSPFv3Interface::BROADCAST_TYPE) {//if this interface is not reported as transit, all its addresses belong to the prefix
            InterfaceEntry *ie = this->getInstance()->getProcess()->ift->getInterfaceByName((*it)->getIntName().c_str());
            IPv6InterfaceData* ipv6int = ie->ipv6Data();
            int ipv6Count = ipv6int->getNumAddresses();
            currentPrefix++;
            for(int i=0; i<ipv6Count; i++) {
                IPv6Address ipv6 = ipv6int->getAddress(i);
                if(ipv6.isGlobal()) {//Only all the global prefixes belong to the Intra-Area-Prefix LSA
                    OSPFv3LSAPrefix *prefix = new OSPFv3LSAPrefix();
                    prefix->prefixLen=64;//TODO - this will never work until the prefix can be gathered from IPv6Address
                    prefix->metric=1;//TODO - check
                    prefix->addressPrefix=ipv6;

                    newLsa->setPrefixesArraySize(currentPrefix);
                    newLsa->setPrefixes(currentPrefix-1, *prefix);
                    prefixCount++;
                }
            }
        }
    }

    //TODO - length!!!
    newLsa->setNumPrefixes(prefixCount);
    return newLsa;
}//originateIntraAreaPrefixLSA

IntraAreaPrefixLSA* OSPFv3Area::originateNetIntraAreaPrefixLSA(NetworkLSA* networkLSA, OSPFv3Interface* interface)
{
    OSPFv3LSAHeader &header = networkLSA->getHeader();

    IntraAreaPrefixLSA* newLsa = new IntraAreaPrefixLSA();
    OSPFv3LSAHeader& newHeader = newLsa->getHeader();
    newHeader.setLsaAge((int)simTime().dbl());
    newHeader.setLsaType(INTRA_AREA_PREFIX_LSA);
    newHeader.setLinkStateID(this->getNewNetIntraAreaPrefixLinkStateID());
    newHeader.setAdvertisingRouter(this->getInstance()->getProcess()->getRouterID());
    newHeader.setLsaSequenceNumber(this->netIntraAreaPrefixLSASequenceNumber++);

    newLsa->setReferencedLSType(NETWORK_LSA);
    newLsa->setReferencedLSID(header.getLinkStateID());
    newLsa->setReferencedAdvRtr(header.getAdvertisingRouter());

    InterfaceEntry *ie = this->getInstance()->getProcess()->ift->getInterfaceByName(interface->getIntName().c_str());
    IPv6InterfaceData* ipv6int = ie->ipv6Data();
    int ipv6Count = ipv6int->getNumAddresses();
    int currentPrefix = 1;
    int prefixCount = 0;
    for(int i=0; i<ipv6Count; i++) {
        IPv6Address ipv6 = ipv6int->getAddress(i);
        if(ipv6.isGlobal()) {//Only all the global prefixes belong to the Intra-Area-Prefix LSA
            OSPFv3LSAPrefix *prefix = new OSPFv3LSAPrefix();
            prefix->prefixLen=64;//TODO - this will never work until the prefix can be gathered from IPv6Address
            prefix->metric=0;//TODO - check
            prefix->addressPrefix=ipv6.getPrefix(64);

            newLsa->setPrefixesArraySize(currentPrefix);
            newLsa->setPrefixes(currentPrefix-1, *prefix);
            prefixCount++;
            currentPrefix++;
        }
    }

    newLsa->setNumPrefixes(prefixCount);

    int intraPrefCnt = this->getIntraAreaPrefixLSACount();
    for(int i=0; i<intraPrefCnt; i++){
        IntraAreaPrefixLSA* pref = this->getIntraAreaPrefixLSA(i);
        if((pref->getReferencedAdvRtr() == this->getInstance()->getProcess()->getRouterID()) &&
                pref->getReferencedLSType() == ROUTER_LSA){
            this->intraAreaPrefixLSAList.erase(this->intraAreaPrefixLSAList.begin()+i);
            EV_DEBUG << "Deleting IntraAreaPrefixLSA -- originateNetIntraAreaPrefixLSA\n";
        }
    }

    return newLsa;
}

bool OSPFv3Area::installIntraAreaPrefixLSA(OSPFv3IntraAreaPrefixLSA *lsa)
{
    OSPFv3LSAHeader &header = lsa->getHeader();

    EV_DEBUG << "Installing Intra-Area-Prefix LSA:\nLink State ID: " << header.getLinkStateID() << "\nAdvertising router: " << header.getAdvertisingRouter();
    EV_DEBUG << "\nLS Seq Number: " << header.getLsaSequenceNumber() << "\nReferenced LSA Type: " << lsa->getReferencedLSType();
    EV_DEBUG << "\nReferenced Link State ID: " << lsa->getReferencedLSID();
    EV_DEBUG << "\nReferenced Advertising Router: " << lsa->getReferencedAdvRtr();
    EV_DEBUG << "\nNumber of Prefixes: " << lsa->getNumPrefixes() << " and " << lsa->getPrefixesArraySize() << "\n";

    for(int i = 0; i<lsa->getNumPrefixes(); i++) {
        OSPFv3LSAPrefix &prefix = lsa->getPrefixes(i);
        EV_DEBUG << "Prefix Address: " << prefix.addressPrefix;
        EV_DEBUG << "\nPrefix Length: " << prefix.prefixLen;
        if(prefix.dnBit)
            EV_DEBUG << "DN ";
        if(prefix.laBit)
            EV_DEBUG << "LA ";
        if(prefix.nuBit)
            EV_DEBUG << "NU ";
        if(prefix.pBit)
            EV_DEBUG << "P ";
        if(prefix.xBit)
            EV_DEBUG << "X ";

        EV_DEBUG << ", Metric: " << prefix.metric << "\n";
    }

    LSAKeyType lsaKey;
    lsaKey.linkStateID = lsa->getHeader().getLinkStateID();
    lsaKey.advertisingRouter = lsa->getHeader().getAdvertisingRouter();
    lsaKey.LSType = lsa->getHeader().getLsaType();

    if(lsa->getReferencedLSType() == NETWORK_LSA)
    {
        EV_DEBUG << "Received NETWORK_LSA prefix: " << lsa->getPrefixes(0).addressPrefix << endl;

        int intraPrefCnt = this->getIntraAreaPrefixLSACount();
        for(int i=0; i<intraPrefCnt; i++){
            IntraAreaPrefixLSA* pref = this->getIntraAreaPrefixLSA(i);
            EV_DEBUG << "Examining IntraAreaPrefix LSA - " << pref->getPrefixes(0).addressPrefix << endl;

            if(pref->getReferencedLSType() == ROUTER_LSA)
            {
                IPv6Address routerPref = lsa->getPrefixes(0).addressPrefix.toIPv6();
                IPv6Address netPref = pref->getPrefixes(0).addressPrefix.toIPv6();
                // ak obdrzim LSA type 9 od DR s IPv6 adresou aku som doteraz bral ako moju LSType 1, moju LSType 1 vymazem

                if(routerPref.getPrefix(64) == netPref.getPrefix(64))
                {
                    this->intraAreaPrefixLSAList.erase(this->intraAreaPrefixLSAList.begin()+i);
                    EV_DEBUG << "Deleting IntraAreaPrefixLSA -- installIntraAreaPrefixLSA\n";
//                    IntraAreaPrefixLSA* lsaCopy = new IntraAreaPrefixLSA(*lsa); // POSLEDNA UPRAVA
//                    this->intraAreaPrefixLSAList.push_back(lsaCopy);
//                    return true;
                }
            }
            else if(pref->getReferencedLSType() == NETWORK_LSA)
            {       // TODO: Add for cycle for case, that LSA cointains more than one prefix. LG
                IPv6Address routerPref = lsa->getPrefixes(0).addressPrefix.toIPv6();
                IPv6Address netPref = pref->getPrefixes(0).addressPrefix.toIPv6();

                if((routerPref.getPrefix(64) == netPref.getPrefix(64)) &&
                  (lsa->getHeader().getAdvertisingRouter() == pref->getHeader().getAdvertisingRouter()) &&
                  (lsa->getHeader().getLsaSequenceNumber() > pref->getHeader().getLsaSequenceNumber()))
                {
//                  this->intraAreaPrefixLSAList.erase(this->intraAreaPrefixLSAList.begin()+i);
                    this->removeFromAllRetransmissionLists(lsaKey);
                    EV_DEBUG << "Only updating\n";
                    return this->updateIntraAreaPrefixLSA(pref, lsa);


                }
            }
        }

    }

    if(lsa->getReferencedLSType() == ROUTER_LSA){
        EV_DEBUG << "Received ROUTER Intra prefix: " << lsa->getPrefixes(0).addressPrefix << endl; // LG
        int intraPrefCnt = this->getIntraAreaPrefixLSACount();
        for(int i=0; i<intraPrefCnt; i++){
            OSPFv3IntraAreaPrefixLSA* pref = this->getIntraAreaPrefixLSA(i);
            if(pref->getReferencedLSType() == NETWORK_LSA) {
                EV_DEBUG << "Examining IntraAreaPrefix LSA - " << pref->getPrefixes(0).addressPrefix << endl;
                IPv6Address routerPref = lsa->getPrefixes(0).addressPrefix.toIPv6();
                IPv6Address netPref = pref->getPrefixes(0).addressPrefix.toIPv6();
                if(routerPref.getPrefix(64) == netPref.getPrefix(64)) {
                    EV_DEBUG << "Came router, have network, doing nothing - LG\n"; //TODO: This become relevant when there will be support for active changing of type of link
//                    this->intraAreaPrefixLSAList.erase(this->intraAreaPrefixLSAList.begin()+i);
//                    OSPFv3IntraAreaPrefixLSA* lsaCopy = new OSPFv3IntraAreaPrefixLSA(*lsa); // POSLEDNA UPRAVA
//                    this->intraAreaPrefixLSAList.push_back(lsaCopy);
//                    return false;
                }

            }
        }
    }
    IntraAreaPrefixLSA* lsaInDatabase = (IntraAreaPrefixLSA*)this->getLSAbyKey(lsaKey);
  /*  IntraAreaPrefixLSA* lsaInDatabase2 = findIntraAreaPrefixByAdvRouter(lsa->getHeader().getAdvertisingRouter());
    cout << "ROUTER ID = " << this->getInstance()->getProcess()->getRouterID() << "\n"; //LG

    cout << "MNE PRISLO:\n";
    cout << "advRoutet=\t\t" << lsa->getHeader().getAdvertisingRouter() << "\n";
    cout << "LSID=\t\t" << lsa->getHeader().getLinkStateID() << "\n";
    cout << "LSAge-\t\t\t"<< lsa->getHeader().getLsaAge() << "\n";
    cout << "refAdvRouter=\t" << lsa->getReferencedAdvRtr() << "\n";
    cout << "refType=\t\t" << lsa->getReferencedLSType() << "\n";
    cout << "refLSID=\t\t" << lsa->getReferencedLSID() << "\n";


    cout <<  "searching by LSID,advRouter,LSType:\t" << lsaKey.linkStateID << " / " << lsaKey.advertisingRouter << " / " << lsaKey.LSType << "\n";


    if (lsaInDatabase != nullptr)
    {
        cout << "FIND INTRA BY ADV ROUTER:\n";
        cout << "advRoutet=\t\t" << lsaInDatabase->getHeader().getAdvertisingRouter() << "\n";
        cout << "LSID=\t\t" << lsaInDatabase->getHeader().getLinkStateID() << "\n";
        cout << "LSAge-\t\t\t"<< lsaInDatabase->getHeader().getLsaAge() << "\n";
        cout << "refAdvRouter=\t" << lsaInDatabase->getReferencedAdvRtr() << "\n";
        cout << "refType=\t\t" << lsaInDatabase->getReferencedLSType() << "\n";
        cout << "refLSID=\t\t" << lsaInDatabase->getReferencedLSID() << "\n";
    }
        cout << "-----------------------------------------------------\n";
    if (lsaInDatabase2 != nullptr)
    {
        cout << "GET LSA BY KEY:\n";
        cout << "advRoutet=\t\t" << lsaInDatabase2->getHeader().getAdvertisingRouter() << "\n";
        cout << "LSID=\t\t" << lsaInDatabase2->getHeader().getLinkStateID() << "\n";
        cout << "LSAge-\t\t\t"<< lsaInDatabase2->getHeader().getLsaAge() << "\n";
        cout << "refAdvRouter=\t" << lsaInDatabase2->getReferencedAdvRtr() << "\n";
        cout << "refType=\t\t" << lsaInDatabase2->getReferencedLSType() << "\n";
        cout << "refLSID=\t\t" << lsaInDatabase2->getReferencedLSID() << "\n\n\n";
    }


    EV_DEBUG << "ROUTER ID = " << this->getInstance()->getProcess()->getRouterID() << "\n";

    EV_DEBUG << "MNE PRISLO:\n";
    EV_DEBUG << "advRoutet=\t\t" << lsa->getHeader().getAdvertisingRouter() << "\n";
    EV_DEBUG << "LSID=\t\t" << lsa->getHeader().getLinkStateID() << "\n";
    EV_DEBUG << "LSAge=\t\t\t"<< lsa->getHeader().getLsaAge() << "\n";
    EV_DEBUG << "LSSeqNum=\t\t" << lsa->getHeader().getLsaSequenceNumber() << "\n";
    EV_DEBUG << "refAdvRouter=\t" << lsa->getReferencedAdvRtr() << "\n";
    EV_DEBUG << "refType=\t\t" << lsa->getReferencedLSType() << "\n";
    EV_DEBUG << "refLSID=\t\t" << lsa->getReferencedLSID() << "\n";


    EV_DEBUG << "searching by LSID,advRouter,LSType:\t" << lsaKey.linkStateID << " / " << lsaKey.advertisingRouter << " / " << lsaKey.LSType << "\n";
    if (lsaInDatabase != nullptr)
    {
        EV_DEBUG << "FIND INTRA BY ADV ROUTER:\n";
        EV_DEBUG << "advRoutet=\t\t" << lsaInDatabase->getHeader().getAdvertisingRouter() << "\n";
        EV_DEBUG << "LSID=\t\t" << lsaInDatabase->getHeader().getLinkStateID() << "\n";
        EV_DEBUG << "LSAge-\t\t\t"<< lsaInDatabase->getHeader().getLsaAge() << "\n";
        EV_DEBUG << "LSSeqNum=\t\t" << lsaInDatabase->getHeader().getLsaSequenceNumber() << "\n";
        EV_DEBUG << "refAdvRouter=\t" << lsaInDatabase->getReferencedAdvRtr() << "\n";
        EV_DEBUG << "refType=\t\t" << lsaInDatabase->getReferencedLSType() << "\n";
        EV_DEBUG << "refLSID=\t\t" << lsaInDatabase->getReferencedLSID() << "\n";
    }
    EV_DEBUG << "-----------------------------------------------------\n";
    if (lsaInDatabase2 != nullptr)
    {
        EV_DEBUG << "GET LSA BY KEY:\n";
        EV_DEBUG << "advRoutet=\t\t" << lsaInDatabase2->getHeader().getAdvertisingRouter() << "\n";
        EV_DEBUG << "LSID=\t\t" << lsaInDatabase2->getHeader().getLinkStateID() << "\n";
        EV_DEBUG << "LSAge-\t\t\t"<< lsaInDatabase2->getHeader().getLsaAge() << "\n";
        EV_DEBUG << "LSSeqNum=\t\t" << lsaInDatabase2->getHeader().getLsaSequenceNumber() << "\n";
        EV_DEBUG << "refAdvRouter=\t" << lsaInDatabase2->getReferencedAdvRtr() << "\n";
        EV_DEBUG << "refType=\t\t" << lsaInDatabase2->getReferencedLSType() << "\n";
        EV_DEBUG << "refLSID=\t\t" << lsaInDatabase2->getReferencedLSID() << "\n\n\n";
    }
*/

    if (lsaInDatabase != nullptr &&
        lsaInDatabase->getHeader().getLsaSequenceNumber() < lsa->getHeader().getLsaSequenceNumber()) {
        this->removeFromAllRetransmissionLists(lsaKey);
        EV_DEBUG << "Only updating\n";
        return this->updateIntraAreaPrefixLSA(lsaInDatabase, lsa);
    }
    else if (lsa->getReferencedLSType() == NETWORK_LSA){
        IntraAreaPrefixLSA* lsaCopy = new IntraAreaPrefixLSA(*lsa);
        this->intraAreaPrefixLSAList.push_back(lsaCopy);
        EV_DEBUG << "creating new one\n";
        return true;
    }
    return false;
    //FIXME: this need to be reworked LG , AGE of new one is not 0.

}//installIntraAreaPrefixLSA


bool OSPFv3Area::updateIntraAreaPrefixLSA(IntraAreaPrefixLSA* currentLsa, OSPFv3IntraAreaPrefixLSA* newLsa)
{
    bool different = intraAreaPrefixLSADiffersFrom(currentLsa, newLsa);
    (*currentLsa) = (*newLsa);
    currentLsa->resetInstallTime();
    currentLsa->getHeader().setLsaAge(0);//reset the age
    if (different) {
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

IPv4Address OSPFv3Area::getNewNetIntraAreaPrefixLinkStateID()
{
    IPv4Address currIP = this->netIntraAreaPrefixLsID;
    int newIP = currIP.getInt()+1;
    this->netIntraAreaPrefixLsID = IPv4Address(newIP);
    return currIP;
}//getNewNetIntraAreaPrefixStateID


IntraAreaPrefixLSA* OSPFv3Area::findIntraAreaPrefixByAdvRouter(IPv4Address advRouter)
{
    for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++)
    {
        if ((*it)->getHeader().getAdvertisingRouter() == advRouter)
        {
            return (*it);
        }
    }

    return nullptr;
}

IntraAreaPrefixLSA* OSPFv3Area::findNetIntraAreaPrefixLSAByReference(IPv4Address refLSID, IPv4Address refAdvRouter)
{
    for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++)
    {
        if(((*it)->getReferencedLSType() == NETWORK_LSA) && ((*it)->getReferencedLSID() == refLSID) && ((*it)->getReferencedAdvRtr() == refAdvRouter))
        {
            return (*it);
        }
    }

    return nullptr;
}

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
        for (auto it=this->interAreaPrefixLSAList.begin(); it!=this->interAreaPrefixLSAList.end(); it++)
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
        for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++)
        {
            if(((*it)->getHeader().getAdvertisingRouter() == LSAKey.advertisingRouter) && (*it)->getHeader().getLinkStateID() == LSAKey.linkStateID) {
                return (*it);
            }
        }

        break;

    case LINK_LSA:
        for (auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++)
        {
            LinkLSA* lsa = (*it)->getLinkLSAbyKey(LSAKey);
            if(lsa != nullptr)
                return lsa;
        }

        break;
    }
//TODO - link lsa from interface
    //TOFO - as external from top data structure


    return nullptr;
}


// ########## pridane ########
RouterLSA *OSPFv3Area::findRouterLSA(IPv4Address routerID)
{
    for (auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++)
    {
        if( (*it)->getHeader().getAdvertisingRouter() == routerID )
        {
            return (*it);
        }
    }
    return nullptr;
}

RouterLSA *OSPFv3Area::findRouterLSAByID(IPv4Address linkStateID)
{
    for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++ )
    {
        if( (*it)->getHeader().getLinkStateID() == linkStateID)
        {
            return (*it);
        }
    }
    return nullptr;
}

NetworkLSA *OSPFv3Area::findNetworkLSA(uint32_t intID, IPv4Address routerID)
{
    for (auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++)
    {
        if( ((*it)->getHeader().getAdvertisingRouter() == routerID) && ((*it)->getHeader().getLinkStateID() == (IPv4Address)intID) )
        {
            return (*it);
        }
    }
    return nullptr;
}

NetworkLSA *OSPFv3Area::findNetworkLSAByLSID(IPv4Address linkStateID)
{
    for(auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++ )
    {
        if( (*it)->getHeader().getLinkStateID() == linkStateID)
        {
            return (*it);
        }
    }
    return nullptr;
}


void OSPFv3Area::calculateShortestPathTree(std::vector<OSPFv3RoutingTableEntry* >& newTable)
{

    // podla vsetkeho nemozem pouzit RouterLinksArraySize, pretoze to nereprezentuje pocet RouterLSA liniek

    EV_DEBUG << "Calculating SPF Tree for area " << this->getAreaID() << "\n";
    /*1)Initialize the algorithm's data structures. Clear the list
        of candidate vertices. Initialize the shortest-path tree to
        only the root (which is the router doing the calculation).
        Set Area's TransitCapability to FALSE*/
    EV_DEBUG << "SPFTREE:";


    EV_DEBUG << "********************************* SPFTREE ****************************************************" << "\n";
    EV_DEBUG << "JA SOM " <<  this->getInstance()->getProcess()->getRouterID() << " = " << this->getInstance()->getProcess()->getOwner()->getOwner()->getName() << " area = " << this->getAreaID()  << "\n";

        // ******pridane, nekopirovane******

        EV_DEBUG << "ROUTER LSA LIST = " <<  routerLSAList.size() << "\n";
        for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            EV_DEBUG << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
            EV_DEBUG << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
            EV_DEBUG << "interfaceID\tneighborIntID\tneighborRouterID\n";
            for (int i = 0; i < (*it)->getRoutersArraySize(); i++)
               EV_DEBUG << (*it)->getRouters(i).interfaceID << "\t\t\t" << (*it)->getRouters(i).neighborInterfaceID << "\t\t\t" << (*it)->getRouters(i).neighborRouterID << "\n";

            EV_DEBUG << "     NEXT HOP EV_DEBUG = " << (*it)->getNextHopCount() << "\n";
            EV_DEBUG << "\n";
        }

        EV_DEBUG << "\n" << "\n";

        EV_DEBUG <<  "NETWORK LSA LIST = " << networkLSAList.size() << "\n";
        for (auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            EV_DEBUG << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
            EV_DEBUG << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
            EV_DEBUG << "Attached Router:" << "\n";
            for (int i = 0; i < (*it)->getAttachedRouterArraySize(); i++)
                EV_DEBUG << (*it)->getAttachedRouter(i) << "\n";

            EV_DEBUG << "     NEXT HOP EV_DEBUG = " << (*it)->getNextHopCount() << "\n";
        }
        EV_DEBUG <<"\n";

        EV_DEBUG << "INTER AREA LSA LIST = " << interAreaPrefixLSAList.size() << "\n";
        for (auto it=this->interAreaPrefixLSAList.begin(); it!=this->interAreaPrefixLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            EV_DEBUG << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
            EV_DEBUG << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
            EV_DEBUG << "prefix =\t\t " << (*it)->getPrefix() << "\n";
            EV_DEBUG << "prefixLen =\t\t " << (int)(*it)->getPrefixLen() << "\n";
        }


        EV_DEBUG << "\n" << "\n";

        EV_DEBUG << "INTERFACE LIST = " << this->interfaceList.size() << "\n";
        for (int i = 0; i < this->interfaceList.size(); i++)
        {
            EV_DEBUG << "#" << i << "\n";
            OSPFv3Interface* inter =  this->getInterface(i);
            EV_DEBUG << "type = " << inter->getType() << "\n";
            EV_DEBUG << "interfaceID = " << inter->getInterfaceId() << "\n";
            EV_DEBUG << "interface IP = " << inter->getInterfaceIP() << "\n";
            EV_DEBUG << "getLinkLSACount() = " <<  inter->getLinkLSACount() << "\n";
            for (int j = 0; j < inter->getLinkLSACount(); j++) {
                LinkLSA* linkLSA = inter->getLinkLSA(j);
                EV_DEBUG << "LinkLSA" << "\n";
                OSPFv3LSAHeader& header = linkLSA->getHeader();
                EV_DEBUG << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
                EV_DEBUG << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";

                EV_DEBUG << "getLinkLocalInterfaceAdd = " << linkLSA->getLinkLocalInterfaceAdd() << "\n";
                EV_DEBUG << "getPrefixesArraySize = " << linkLSA->getPrefixesArraySize() << "\n";
                for (int k = 0; k < linkLSA->getPrefixesArraySize(); k++) {
                    // vracia  OSPFv3LinkLSAPrefix
                    OSPFv3LinkLSAPrefix pr = linkLSA->getPrefixes(k);
                    EV_DEBUG << "\taddressPrefix          = " <<  pr.addressPrefix << "\n";
                    EV_DEBUG << "\tprefixLen              = " <<  int(pr.prefixLen) << "\n";
                }
                EV_DEBUG << "\n";
            }
        }

        EV_DEBUG << "\n" << "\n";
        EV_DEBUG <<  "INTRA AREA PREFIX = " << intraAreaPrefixLSAList.size() << "\n";
        for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            EV_DEBUG << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
            EV_DEBUG << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
            EV_DEBUG << "getPrefixesArraySize = " << (*it)->getPrefixesArraySize() << "\n";
            EV_DEBUG << "getReferencedLSType = " << (*it)->getReferencedLSType() << "\n";
            EV_DEBUG << "getReferencedLSID = " << (*it)->getReferencedLSID() << "\n";
            EV_DEBUG << "getReferencedAdvRtr = " << (*it)->getReferencedAdvRtr() << "\n";
            EV_DEBUG << "prefixes :" << "\n";
            for (int i = 0; i < (*it)->getPrefixesArraySize(); i++) {
                EV_DEBUG << "addressPrefix = "<< (*it)->getPrefixes(i).addressPrefix << "\n";
                EV_DEBUG << "prefixLen = "<< int((*it)->getPrefixes(i).prefixLen) << "\n";
            }
        }
        EV_DEBUG << "\n\n";

        ////////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////

        cout << "********************************* SPFTREE ****************************************************" << "\n";
        cout << "JA SOM " <<  this->getInstance()->getProcess()->getRouterID() << " = " << this->getInstance()->getProcess()->getOwner()->getOwner()->getName() << " area = " << this->getAreaID()  << "\n";

            // ******pridane, nekopirovane******

            cout << "ROUTER LSA LIST = " <<  routerLSAList.size() << "\n";
            for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++) {
                OSPFv3LSAHeader& header = (*it)->getHeader();
                cout << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
                cout << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
                cout << "interfaceID\tneighborIntID\tneighborRouterID\n";
                for (int i = 0; i < (*it)->getRoutersArraySize(); i++)
                   cout << (*it)->getRouters(i).interfaceID << "\t\t\t" << (*it)->getRouters(i).neighborInterfaceID << "\t\t\t" << (*it)->getRouters(i).neighborRouterID << "\n";

                cout << "     NEXT HOP cout = " << (*it)->getNextHopCount() << "\n";
                cout << "\n";
            }

            cout << "\n" << "\n";

            cout <<  "NETWORK LSA LIST = " << networkLSAList.size() << "\n";
            for (auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++) {
                OSPFv3LSAHeader& header = (*it)->getHeader();
                cout << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
                cout << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
                cout << "Attached Router:" << "\n";
                for (int i = 0; i < (*it)->getAttachedRouterArraySize(); i++)
                    cout << (*it)->getAttachedRouter(i) << "\n";

                cout << "     NEXT HOP cout = " << (*it)->getNextHopCount() << "\n";
            }

            cout << "\n";

            cout << "INTER AREA LSA LIST = " << interAreaPrefixLSAList.size() << "\n";
            for (auto it=this->interAreaPrefixLSAList.begin(); it!=this->interAreaPrefixLSAList.end(); it++) {
                OSPFv3LSAHeader& header = (*it)->getHeader();
                cout << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
                cout << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
                cout << "prefix =\t\t " << (*it)->getPrefix() << "\n";
                cout << "prefixLen =\t\t " << (*it)->getPrefixLen() << "\n";
            }


            cout << "\n" << "\n";

            cout << "INTERFACE LIST = " << this->interfaceList.size() << "\n";
            for (int i = 0; i < this->interfaceList.size(); i++)
            {
                cout << "#" << i << "\n";
                OSPFv3Interface* inter =  this->getInterface(i);
                cout << "type = " << inter->getType() << "\n";
                cout << "interfaceID = " << inter->getInterfaceId() << "\n";
                cout << "interface IP = " << inter->getInterfaceIP() << "\n";
                cout << "getLinkLSACount() = " <<  inter->getLinkLSACount() << "\n";
                for (int j = 0; j < inter->getLinkLSACount(); j++) {
                    LinkLSA* linkLSA = inter->getLinkLSA(j);
                    cout << "LinkLSA" << "\n";
                    OSPFv3LSAHeader& header = linkLSA->getHeader();
                    cout << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
                    cout << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";

                    cout << "getLinkLocalInterfaceAdd = " << linkLSA->getLinkLocalInterfaceAdd() << "\n";
                    cout << "getPrefixesArraySize = " << linkLSA->getPrefixesArraySize() << "\n";
                    for (int k = 0; k < linkLSA->getPrefixesArraySize(); k++) {
                        // vracia  OSPFv3LinkLSAPrefix
                        OSPFv3LinkLSAPrefix pr = linkLSA->getPrefixes(k);
                        cout << "\taddressPrefix          = " <<  pr.addressPrefix << "\n";
                        cout << "\tprefixLen              = " <<  int(pr.prefixLen) << "\n";
                    }
                    cout << "\n";
                }
            }

            cout << "\n" << "\n";
            cout <<  "INTRA AREA PREFIX = " << intraAreaPrefixLSAList.size() << "\n";
            for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++) {
                OSPFv3LSAHeader& header = (*it)->getHeader();
                cout << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
                cout << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
                cout << "getPrefixesArraySize = " << (*it)->getPrefixesArraySize() << "\n";
                cout << "getReferencedLSType = " << (*it)->getReferencedLSType() << "\n";
                cout << "getReferencedLSID = " << (*it)->getReferencedLSID() << "\n";
                cout << "getReferencedAdvRtr = " << (*it)->getReferencedAdvRtr() << "\n";
                cout << "prefixes :" << "\n";
                for (int i = 0; i < (*it)->getPrefixesArraySize(); i++) {
                    cout << "addressPrefix = "<< (*it)->getPrefixes(i).addressPrefix << "\n";
                    cout << "prefixLen = "<< int((*it)->getPrefixes(i).prefixLen) << "\n";
                }
            }
            cout << "\n\n";

//        cout << "intraAreaPrefixLSAList.size()" << intraAreaPrefixLSAList.size() << endl;
//        for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++) {
//                   OSPFv3LSAHeader& header = (*it)->getHeader();
//                   cout << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< endl;
//                   cout << "LinkStateID =\t\t" << header. () << endl;
//                   for (int i = 0; i < (*it)->get; i++)
//                       cout << (*it)->getAttachedRouter(i) << endl;
//               }


    //**********************************

    IPv4Address routerID = this->getInstance()->getProcess()->getRouterID();
    bool finished = false;
    std::vector<OSPFv3LSA*> treeVertices;
    OSPFv3LSA *justAddedVertex;
    std::vector<OSPFv3LSA *> candidateVertices;
    unsigned long i, j, k;
    unsigned long lsaCount;

//    spfTreeRoot->getRouters()
    if (spfTreeRoot == nullptr) {
        RouterLSA *newLSA = originateRouterLSA();   //vytvori Router-LSA spravu

        if (installRouterLSA(newLSA))
        {
            RouterLSA *routerLSA = findRouterLSA(routerID);
            spfTreeRoot = routerLSA;
            floodLSA(newLSA);       //spread LSA to whole network
            delete newLSA;
        }
    }

    if (spfTreeRoot == nullptr) {
        cout <<  "spfTreeRoot je stale null";
        return;
    }

    lsaCount = routerLSAList.size();
    cout << "lsaCount = " << lsaCount << endl;

    for (i = 0; i < lsaCount; i++) {
        routerLSAList[i]->clearNextHops();
    }
    lsaCount = networkLSAList.size();
    for (i = 0; i < lsaCount; i++) {
        networkLSAList[i]->clearNextHops();
    }

    spfTreeRoot->setDistance(0);
    treeVertices.push_back(spfTreeRoot);   // root is first vertex in dijkstra alg
    justAddedVertex = spfTreeRoot;    // (1)

    cout << endl << endl;

    // int pocitadlo = 0; // LG
    do {
//        EV_DEBUG << "\n\n***************************************\niteracia c. " << pocitadlo++ << "\n";
        OSPFv3LSAFunctionCode vertexType = static_cast<OSPFv3LSAFunctionCode>(justAddedVertex->getHeader().getLsaType());
//        EV_DEBUG << "vertexType = " << vertexType << "\n";

        if (vertexType == ROUTER_LSA) {
            RouterLSA *routerVertex = check_and_cast<RouterLSA *>(justAddedVertex);

            if (routerVertex->getVBit()) {      //prebrate z OSPFv2
                transitCapability = true;
            }

            int testCount = routerVertex->getRoutersArraySize();
            EV_DEBUG << "routerVertex->getRoutersArraySize() = " <<  routerVertex->getRoutersArraySize() << "\n";
//            for (k = 0; k < routerVertex->getRoutersArraySize(); k++)
            for (int iteration = 0; iteration < testCount; iteration++)
            {
                EV_DEBUG << "testCount n." << iteration << "\n";
                OSPFv3RouterLSABody router = routerVertex->getRouters(iteration);

//                EV_DEBUG << "\n*****************************\nrouter is:\n";
                EV_DEBUG << "interfaceID = " << router.interfaceID << "\n"; //LG
                EV_DEBUG << "neighborInterfaceID = " << router.neighborInterfaceID << "\n";
                EV_DEBUG << "neighborRouterID = " << router.neighborRouterID << "\n";



                OSPFv3LSA* joiningVertex;           // joiningVertex is source vertex
                OSPFv3LSAFunctionCode joiningVertexType;
                /*The Vertex ID for a router is the OSPF Router ID.  The Vertex ID
                  for a transit network is a combination of the Interface ID and
                  OSPF Router ID of the network's Designated Router.*/

                if (router.type == TRANSIT_NETWORK) {
//                    cout <<  this->getInstance()->getProcess()->getRouterID() << " = som v TRANSIT_NETWORK" << endl;
                    joiningVertex = findNetworkLSA(router.neighborInterfaceID, router.neighborRouterID);
                    joiningVertexType = NETWORK_LSA;
                    EV_DEBUG <<  this->getInstance()->getProcess()->getRouterID() << " = som v TRANSIT_NETWORK\n";
                }
                else {  // P2P
                    EV_DEBUG <<  this->getInstance()->getProcess()->getRouterID() << " = som v P2P type = " << router.type << "\n"; // LG
//                    cout <<  this->getInstance()->getProcess()->getRouterID() << " = som v P2P" << endl;
                    joiningVertex = findRouterLSA(router.neighborRouterID);
                    joiningVertexType = ROUTER_LSA;

                }

                if ((joiningVertex == nullptr) ||
                (joiningVertex->getHeader().getLsaAge() == MAX_AGE)
                 || (!hasLink(joiningVertex, justAddedVertex)))    // (from, to)     (2) (b) // uncommented line 15.01.2019 LG
                {
                    EV_DEBUG << "continue\n";
                    continue;
                }
                if (joiningVertexType == NETWORK_LSA)
                    EV_DEBUG <<  "TRANSIT_NETWORK pre joiningVertex - " << joiningVertex->getHeader().getAdvertisingRouter() << " / " << joiningVertex->getHeader().getLinkStateID() << "\n"; // LG

                unsigned int treeSize = treeVertices.size();    // already visited vertices (at the beginning, only root)
                bool alreadyOnTree = false;

                for (j = 0; j < treeSize; j++) {                // if vertex, which was found is already in set of visited vertices, go to another one
                    if (treeVertices[j] == joiningVertex) {
                      alreadyOnTree = true;
                      break;
                    }
                }
                if (alreadyOnTree) {    // (2) (c)
                    EV_DEBUG << "continue\n";
                    continue;
                }

                unsigned long linkStateCost = routerVertex->getDistance() + routerVertex->getRouters(iteration).metric;
                unsigned int candidateCount = candidateVertices.size();     //candidateVertices na zaciatku nula

                OSPFv3LSA *candidate = nullptr;
                for (j = 0; j < candidateCount; j++) {
                    if (candidateVertices[j] == joiningVertex) {
                        candidate = candidateVertices[j];
                    }
                }
                if (candidate != nullptr) { // (2) (d)               // first iteration, candidate is nullptr
                    EV_DEBUG << "candidate != nullptr === TRUE\n";
                    RoutingInfo *routingInfo = check_and_cast<RoutingInfo *>(candidate);
                    unsigned long candidateDistance = routingInfo->getDistance();

                    if (linkStateCost > candidateDistance) {
                       continue;
                    }
                    if (linkStateCost < candidateDistance) {
                       routingInfo->setDistance(linkStateCost);
                       routingInfo->clearNextHops();
                    }
                    EV_DEBUG << "Calculate NEXT HOP  ADVERT-ROUTER with JV = " << joiningVertex->getHeader().getAdvertisingRouter() << " JAV = " << justAddedVertex->getHeader().getAdvertisingRouter() <<"\n";
                    std::vector<NextHop> *newNextHops = calculateNextHops(joiningVertex, justAddedVertex);    // (destination, parent)
                    unsigned int nextHopCount = newNextHops->size();
                    for (k = 0; k < nextHopCount; k++) {
                       routingInfo->addNextHop((*newNextHops)[k]);
                    }
                    delete newNextHops;
                }
                else
                {
                   if (joiningVertexType == ROUTER_LSA)
                   {
                       EV_DEBUG << "joiningVertexType == ROUTERLSA_TYPE === false\n";
//                           cout << "joiningVertexType == ROUTERLSA_TYPE === false" << endl;
                   }
                   else // V OSPFv2 JE TO line 1640
                   {    //joiningVertexType == NETWORK_LSA
                        NetworkLSA *joiningNetworkVertex = check_and_cast<NetworkLSA *>(joiningVertex);
                        joiningNetworkVertex->setDistance(linkStateCost);
                        EV_DEBUG << "candidate == nullptr  joiningVertexType == NETWORK LSA_TYPE\n";
                        EV_DEBUG << "Calculate NEXT HOP  ADVERT-ROUTER with JV = " << joiningVertex->getHeader().getAdvertisingRouter() << " JAV = " << justAddedVertex->getHeader().getAdvertisingRouter() << "\n";
                        std::vector<NextHop> *newNextHops = calculateNextHops(joiningVertex, justAddedVertex);    // (destination, parent)
//                        IPv6Address *addr = new IPv6Address(); LG
//                        int index = 101;
//                        NextHop nextHop;
//                        nextHop.advertisingRouter = joiningVertex->getHeader().getAdvertisingRouter();
//                        nextHop.ifIndex = index;
//                        nextHop.hopAddress = *addr;
//
//                        std::vector<NextHop> *newNextHops = new std::vector<NextHop>;
//                        newNextHops->push_back(nextHop);
//                        cout << "newNextHops->size() = " << newNextHops->size() << endl;
                        EV_DEBUG << "newNextHops->size() = " << newNextHops->size() << "\n";
                        for(int u = 0; u <  newNextHops->size(); u++){
                            EV_DEBUG << "#" << u << " - " << (*newNextHops)[u].ifIndex  << "   " << (*newNextHops)[u].hopAddress << " " << (*newNextHops)[u].advertisingRouter << "\n";
                        }
//                        return;

                        unsigned int nextHopCount = newNextHops->size();
                        for (k = 0; k < nextHopCount; k++)
                        {
                            joiningNetworkVertex->addNextHop((*newNextHops)[k]);
                        }
                        delete newNextHops;
                        // justAdded is source vertex
                        // joining is destination vertex
                        // joiningNetworkVertex == joiningVertex
                        RoutingInfo *vertexRoutingInfo = check_and_cast<RoutingInfo *>(joiningNetworkVertex);
                        vertexRoutingInfo->setParent(justAddedVertex);

                        candidateVertices.push_back(joiningNetworkVertex);
                   }

                }
              }  // end of for
         }    //   (vertexType == ROUTER_LSA)

        if (vertexType == NETWORK_LSA) {
            EV_DEBUG << "som vo vertexType == NETWORK_LSA\n"; // LG

            NetworkLSA *networkVertex = check_and_cast<NetworkLSA *>(justAddedVertex);
            unsigned int routerCount = networkVertex->getAttachedRouterArraySize();

            for (i = 0; i < routerCount; i++) {    // (2)
                RouterLSA *joiningVertex = findRouterLSA(networkVertex->getAttachedRouter(i));
                if ((joiningVertex == nullptr) ||
                    (joiningVertex->getHeader().getLsaAge() == MAX_AGE) ||
                    (!hasLink(joiningVertex, justAddedVertex)))    // (from, to)     (2) (b)
                {
                    EV_DEBUG << "triple if is true, continue\n"; //LG
                    continue;
                }

                unsigned int treeSize = treeVertices.size();
                bool alreadyOnTree = false;

                for (j = 0; j < treeSize; j++) {
                    if (treeVertices[j] == joiningVertex) {
                        alreadyOnTree = true;
                        break;
                    }
                }
                if (alreadyOnTree) {    // (2) (c) already on tree, continue
                    EV_DEBUG << "already on tree, continue\n"; // LG
                    continue;
                }

                unsigned long linkStateCost = networkVertex->getDistance();    // link cost from network to router is always 0
                unsigned int candidateCount = candidateVertices.size();
                OSPFv3LSA *candidate = nullptr;

                for (j = 0; j < candidateCount; j++) {
                    if (candidateVertices[j] == joiningVertex) {
                        candidate = candidateVertices[j];
                    }
                }
                if (candidate != nullptr) {    // (2) (d)
                    RoutingInfo *routingInfo = check_and_cast<RoutingInfo *>(candidate);
                    unsigned long candidateDistance = routingInfo->getDistance();

                    if (linkStateCost > candidateDistance) {
                        continue;
                    }
                    if (linkStateCost < candidateDistance) {
                        routingInfo->setDistance(linkStateCost);
                        routingInfo->clearNextHops();
                    }
                    EV_DEBUG << "vertexType == NETWORKLSA_TYPE   candidate != nullptr  joiningVertexType == NETWORK LSA_TYPE\n"; // LG
                    EV_DEBUG << "Calculate NEXT HOP ADVERT-ROUTER with JV = " << joiningVertex->getHeader().getAdvertisingRouter() << " JAV = " << justAddedVertex->getHeader().getAdvertisingRouter() << "\n";
                    std::vector<NextHop> *newNextHops = calculateNextHops(joiningVertex, justAddedVertex);    // (destination, parent)
                    unsigned int nextHopCount = newNextHops->size();
                    for (k = 0; k < nextHopCount; k++) {
                        routingInfo->addNextHop((*newNextHops)[k]);
                    }
                    delete newNextHops;
                }
                else {
                    joiningVertex->setDistance(linkStateCost);
                    EV_DEBUG << "vertexType == NETWORKLSA_TYPE   candidate == nullptr  joiningVertexType == NETWORK LSA_TYPE\n"; // LG
                    EV_DEBUG << "Calculate NEXT HOP  ADVERT-ROUTER with JV = " << joiningVertex->getHeader().getAdvertisingRouter() << " JAV = " << justAddedVertex->getHeader().getAdvertisingRouter() << "\n";
                    std::vector<NextHop> *newNextHops = calculateNextHops(joiningVertex, justAddedVertex);    // (destination, parent)

                    EV_DEBUG << "newNextHops->size() = " << newNextHops->size() << "\n";
                   for(int u = 0; u <  newNextHops->size(); u++){ // LG
                       EV_DEBUG << "#" << u << " - " << (*newNextHops)[u].ifIndex  << "   " << (*newNextHops)[u].hopAddress << " " << (*newNextHops)[u].advertisingRouter << "\n";
//                     cout << "#" << u << " - " << (*newNextHops)[u].ifIndex  << "   " << (*newNextHops)[u].hopAddress << " " << (*newNextHops)[u].advertisingRouter << endl;
                   }

                    unsigned int nextHopCount = newNextHops->size();
                    for (k = 0; k < nextHopCount; k++) {
                        joiningVertex->addNextHop((*newNextHops)[k]);
                    }
                    delete newNextHops;
                    RoutingInfo *vertexRoutingInfo = check_and_cast<RoutingInfo *>(joiningVertex);
                    vertexRoutingInfo->setParent(justAddedVertex);

                    candidateVertices.push_back(joiningVertex);
                }
            }
        }

        EV_DEBUG << "candidateVertices.size*() = " << candidateVertices.size() << "\n"; // LG
        if (candidateVertices.empty()) {    // (3)
            finished = true;
        }
        else
        {
            unsigned int candidateCount = candidateVertices.size();
            unsigned long minDistance = LS_INFINITY;
            OSPFv3LSA *closestVertex = candidateVertices[0];

            // this for-cycle edit distane from source vertex to all others adjacent vertices by dijstra algorithm
            for (i = 0; i < candidateCount; i++) {
                RoutingInfo *routingInfo = check_and_cast<RoutingInfo *>(candidateVertices[i]);
                unsigned long currentDistance = routingInfo->getDistance();

                if (currentDistance < minDistance) {
                    closestVertex = candidateVertices[i];
                    minDistance = currentDistance;
                }
                else {      // TENTO ELSE SA NEVYKONAL LG
                    if (currentDistance == minDistance) {
                        if ((closestVertex->getHeader().getLsaType() == ROUTER_LSA) &&
                            (candidateVertices[i]->getHeader().getLsaType() == NETWORK_LSA))
                        {
                            EV_DEBUG << "tento else sa nevykonal closestVertex = candidateVertices[i]\n"; //LG
                            closestVertex = candidateVertices[i];
                        }
                    }
                }
            }

            treeVertices.push_back(closestVertex);      // treeVertices je hlavny SPF tree, ktory budujem

            // vymaz vybrany closestVertex  z candidateVertices
            for (auto it = candidateVertices.begin(); it != candidateVertices.end(); it++) {
                if ((*it) == closestVertex) {
                    candidateVertices.erase(it);
                    break;
                }
            }


            if (closestVertex->getHeader().getLsaType() == ROUTER_LSA) {
                EV_DEBUG << "closestVertex->getHeader().getLsType() == ROUTERLSA_TYPE  ---- nemalo by navstivit\n";
//                RouterLSA *routerLSA = check_and_cast<RouterLSA *>(closestVertex);
//                if (routerLSA->getBBit() || routerLSA->getEBit()) {
//
//                    EV_DEBUG << "som aj za ifom\n";
//                    IPv4Address destinationID = routerLSA->getHeader().getLinkStateID();
//                    OSPFv3RoutingTableEntry *entry = (this->getInstance()->ift, destinationID, prefixLen, IRoute::OSPF)
//                    unsigned int nextHopCount = routerLSA->getNextHopCount();
//                    OSPFv3RoutingTableEntry::RoutingDestinationType destinationType = OSPFv3RoutingTableEntry::NETWORK_DESTINATION;
//
//                    entry->setDestination(destinationID);
//                    entry->setLinkStateOrigin(routerLSA);
//                    entry->setArea(areaID);
//                    entry->setPathType(RoutingTableEntry::INTRAAREA);
//                    entry->setCost(routerLSA->getDistance());
//                    if (routerLSA->getBBit()) {
//                        destinationType |= OSPFv3RoutingTableEntry::AREA_BORDER_ROUTER_DESTINATION;
//                    }
//                    if (routerLSA->getEBit()) {
//                        destinationType |= OSPFv3RoutingTableEntry::AS_BOUNDARY_ROUTER_DESTINATION;
//                    }
//                    entry->setDestinationType(destinationType);
//                    entry->setOptionalCapabilities(routerLSA->getHeader().getLsOptions());
//                    for (i = 0; i < nextHopCount; i++) {
//                        entry->addNextHop(routerLSA->getNextHop(i));
//                    }
//
//                    newRoutingTable.push_back(entry);
//
//                    OSPFv3Area *backbone;
//                    if (areaID != BACKBONE_AREAID) {
//                        backbone = parentRouter->getAreaByID(BACKBONE_AREAID);
//                    }
//                    else {
//                        backbone = this;
//                    }
//                    if (backbone != nullptr) {
//                        OSPFv3Interface *virtualIntf = backbone->findVirtualLink(destinationID);
//                        if ((virtualIntf != nullptr) && (virtualIntf->getTransitAreaID() == areaID)) {
//                            cout << "VIRTUAL LINK SHITS\n"; // LG
//                            EV_DEBUG<< "VIRTUAL LINK SHITS\n";
//                            /*
//                        }
//
//                            IPv4AddressRange range;
//                            range.address = getInterface(routerLSA->getNextHop(0).ifIndex)->getAddressRange().address;
//                            range.mask = IPv4Address::ALLONES_ADDRESS;
//                            virtualIntf->setAddressRange(range);
//                            virtualIntf->setIfIndex(ift, routerLSA->getNextHop(0).ifIndex);
//                            virtualIntf->setOutputCost(routerLSA->getDistance());
//                            Neighbor *virtualNeighbor = virtualIntf->getNeighbor(0);
//                            if (virtualNeighbor != nullptr) {
//                                unsigned int linkCount = routerLSA->getLinksArraySize();
//                                RouterLSA *toRouterLSA = dynamic_cast<RouterLSA *>(justAddedVertex);
//                                if (toRouterLSA != nullptr) {
//                                    for (i = 0; i < linkCount; i++) {
//                                        Link& link = routerLSA->getLinks(i);
//
//                                        if ((link.getType() == POINTTOPOINT_LINK) &&
//                                            (link.getLinkID() == toRouterLSA->getHeader().getLinkStateID()) &&
//                                            (virtualIntf->getState() < Interface::WAITING_STATE))
//                                        {
//                                            virtualNeighbor->setAddress(IPv4Address(link.getLinkData()));
//                                            virtualIntf->processEvent(Interface::INTERFACE_UP);
//                                            break;
//                                        }
//                                    }
//                                }
//                                else {
//                                    NetworkLSA *toNetworkLSA = dynamic_cast<NetworkLSA *>(justAddedVertex);
//                                    if (toNetworkLSA != nullptr) {
//                                        for (i = 0; i < linkCount; i++) {
//                                            Link& link = routerLSA->getLinks(i);
//
//                                            if ((link.getType() == TRANSIT_LINK) &&
//                                                (link.getLinkID() == toNetworkLSA->getHeader().getLinkStateID()) &&
//                                                (virtualIntf->getState() < Interface::WAITING_STATE))
//                                            {
//                                                virtualNeighbor->setAddress(IPv4Address(link.getLinkData()));
//                                                virtualIntf->processEvent(Interface::INTERFACE_UP);
//                                                break;
//                                            }
//                                        }
//                                    }
//                                }
//                            }*/
//                        }
//                    }
//                }

            }

            if (closestVertex->getHeader().getLsaType() == NETWORK_LSA) {
                NetworkLSA *networkLSA = check_and_cast<NetworkLSA *>(closestVertex);
//                        IPv4Address destinationID = (networkLSA->getHeader().getLinkStateID() & networkLSA->getNetworkMask());

                // address is extracted from Intra-Area-Prefix-LSA
                LSAKeyType lsaKey;
                lsaKey.linkStateID = networkLSA->getHeader().getLinkStateID();
                lsaKey.advertisingRouter = networkLSA->getHeader().getAdvertisingRouter();
                lsaKey.LSType = INTRA_AREA_PREFIX_LSA;  //navyse

                IPv6Address destinationID;
                uint8_t prefixLen;
                OSPFv3IntraAreaPrefixLSA *iapLSA = findNetIntraAreaPrefixLSAByReference(lsaKey.linkStateID, lsaKey.advertisingRouter);
                if (iapLSA != nullptr){
                    for (int i = 0; i < iapLSA->getPrefixesArraySize(); i++) {
                        EV_DEBUG << "addressPrefix = "<< iapLSA->getPrefixes(i).addressPrefix << "\n";  //this is my destinationID
                        EV_DEBUG << "prefixLen = "<< int(iapLSA->getPrefixes(i).prefixLen) << "\n";
                        destinationID = iapLSA->getPrefixes(i).addressPrefix.toIPv6();
                        prefixLen = iapLSA->getPrefixes(i).prefixLen;
                        if(i > 0){
                            EV_DEBUG << "Mam tam viac adries, WTF?\n"; //LG
                            break;
                        }
                    }
                }
                else {
                    continue; // there is no LSA type 9 which would care a IP addresses for this calculating route, so skip to next one.
                }


                unsigned int nextHopCount = networkLSA->getNextHopCount();
                bool overWrite = false;
                OSPFv3RoutingTableEntry *entry = nullptr;
                unsigned long routeCount = newTable.size();
                IPv6Address longestMatch(IPv6Address::UNSPECIFIED_ADDRESS);

                for (i = 0; i < routeCount; i++) {
                    EV_DEBUG << "for (i = 0; i < routeCount; i++)\n";
                    if (newTable[i]->getDestinationType() == OSPFv3RoutingTableEntry::NETWORK_DESTINATION) {
                        OSPFv3RoutingTableEntry *routingEntry = newTable[i];
                        IPv6Address entryAddress = routingEntry->getDestPrefix();

                        if (entryAddress == destinationID)
                        {
                            if (destinationID > longestMatch) {
                                longestMatch = destinationID;
                                entry = routingEntry;
                            }
                        }
                    }
                }
                if (entry != nullptr) {
                    EV_DEBUG << "entry != nullptr - TOTO JE NOVE !\n";
                    const OSPFv3LSA *entryOrigin = entry->getLinkStateOrigin();
                    if ((entry->getCost() != networkLSA->getDistance()) ||
                        (entryOrigin->getHeader().getLinkStateID() >= networkLSA->getHeader().getLinkStateID()))
                    {
                        overWrite = true;
                    }
                }

                if ((entry == nullptr) || (overWrite)) {
                    if (entry == nullptr) {
                        EV_DEBUG << "v Area destinationID = " << destinationID << "\n";
//                      OSPFv3RoutingTableEntry(IInterfaceTable *ift, IPv6Address destPrefix, int prefixLength, SourceType sourceType)
                        entry = new OSPFv3RoutingTableEntry(this->getInstance()->ift, destinationID, prefixLen, IRoute::OSPF);
                    }
                    entry->setLinkStateOrigin(networkLSA);
                    entry->setArea(areaID);
                    entry->setPathType(OSPFv3RoutingTableEntry::INTRAAREA);
                    entry->setCost(networkLSA->getDistance());
                    entry->setDestinationType(OSPFv3RoutingTableEntry::NETWORK_DESTINATION);
                    entry->setOptionalCapabilities(networkLSA->getOspfOptions());
                    for (i = 0; i < nextHopCount; i++) {
                        entry->addNextHop(networkLSA->getNextHop(i));
                    }

                    if (!overWrite) {
                        newTable.push_back(entry);
                    }

                }
            }

            justAddedVertex = closestVertex;
            }// end of else not empty()
    } while (!finished);




        //FAR FAR AWAY LG
//        unsigned int treeSize = treeVertices.size();
//        for (i = 0; i < treeSize; i++) {
//        RouterLSA *routerVertex = dynamic_cast<RouterLSA *>(treeVertices[i]);
//            if (routerVertex == nullptr) {
//                continue;
//            }
//        }


    EV_DEBUG << "SPFTree calculation finished\n";
}

void OSPFv3Area::calculateInterAreaRoutes(std::vector<OSPFv3RoutingTableEntry* > newTable)
{
//    EV_DEBUG << "Calculating Inter-Area Routes for Backbone\n";
}

void OSPFv3Area::recheckSummaryLSAs(std::vector<OSPFv3RoutingTableEntry* > newTable)
{
//    EV_DEBUG << "Rechecking Summary LSA\n";
}

bool OSPFv3Area::hasLink(OSPFv3LSA *fromLSA, OSPFv3LSA *toLSA) const
{
    unsigned int i;

    RouterLSA *fromRouterLSA = dynamic_cast<RouterLSA *>(fromLSA);
    if (fromRouterLSA != nullptr) {
        unsigned int linkCount = fromRouterLSA->getRoutersArraySize();
        RouterLSA *toRouterLSA = dynamic_cast<RouterLSA *>(toLSA);
        if (toRouterLSA != nullptr) {
            for (i = 0; i < linkCount; i++) {
                OSPFv3RouterLSABody& link = fromRouterLSA->getRouters(i);
                OSPFv3RouterLSAType linkType = static_cast<OSPFv3RouterLSAType>(link.type);

                if (((linkType == POINT_TO_POINT) ||
                     (linkType == VIRTUAL_LINK)) &&
                    (link.neighborRouterID == toRouterLSA->getHeader().getAdvertisingRouter()))
                {
                    return true;
                }
            }
        }
        else {
            NetworkLSA *toNetworkLSA = dynamic_cast<NetworkLSA *>(toLSA);
            if (toNetworkLSA != nullptr) {
                for (i = 0; i < linkCount; i++) {
                    OSPFv3RouterLSABody& link = fromRouterLSA->getRouters(i);

                    if ((link.type == TRANSIT_NETWORK) &&
                        (link.neighborRouterID == toNetworkLSA->getHeader().getAdvertisingRouter()))
                    {
                        return true;
                    }
                    if (link.type == RESERVED)
                    {
                        cout << "return RESERVED HAS LINK" << endl;
                        EV_DEBUG << "return RESERVED HAS LINK\n";
                        return true;
                    }
                }
            }
        }
    }
    else {
        NetworkLSA *fromNetworkLSA = dynamic_cast<NetworkLSA *>(fromLSA);
        if (fromNetworkLSA != nullptr) {
            unsigned int routerCount = fromNetworkLSA->getAttachedRouterArraySize();
            RouterLSA *toRouterLSA = dynamic_cast<RouterLSA *>(toLSA);
            if (toRouterLSA != nullptr) {
                for (i = 0; i < routerCount; i++) {
                    if (fromNetworkLSA->getAttachedRouter(i) == toRouterLSA->getHeader().getAdvertisingRouter()) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

std::vector<NextHop> *OSPFv3Area::calculateNextHops(OSPFv3SPFVertex* destination, OSPFv3SPFVertex *parent) const
{
    EV_DEBUG << "Calculating Next Hops VERTEX NOT IMPLEMENTED NIY\n"; // LG
    return nullptr;
}

std::vector<NextHop> *OSPFv3Area::calculateNextHops(OSPFv3LSA *destination, OSPFv3LSA *parent) const
{
    EV_DEBUG << "Calculating Next Hops\n";
    std::vector<NextHop> *hops = new std::vector<NextHop>;
    unsigned long i, j;

    RouterLSA *routerLSA = dynamic_cast<RouterLSA *>(parent); // OSPFLSA -> RouterLSA
    if (routerLSA != nullptr) { // if parrent is ROUTER_LSA
        if (routerLSA != spfTreeRoot) {
            unsigned int nextHopCount = routerLSA->getNextHopCount();
            for (i = 0; i < nextHopCount; i++) {
                hops->push_back(routerLSA->getNextHop(i));
            }
            return hops;
        }
        else {
            RouterLSA *destinationRouterLSA = dynamic_cast<RouterLSA *>(destination);
            if (destinationRouterLSA != nullptr) {  // if destination is ROUTER_LSA  (for P2P connection (?) LG )
                EV_DEBUG << "TENTO KOD SA NEVYKONAL NIY\n";
//                cout << "TENTO KOD SA NEVYKONAL NIY" << endl;
            }
            else {          //  else destination is NETWORK_LSA
                NetworkLSA *destinationNetworkLSA = dynamic_cast<NetworkLSA *>(destination);
                if (destinationNetworkLSA != nullptr) {
//                    IPv4Address networkDesignatedRouter = destinationNetworkLSA->getHeader().getLinkStateID(); LG
                    IPv4Address networkDesignatedRouter = destinationNetworkLSA->getHeader().getAdvertisingRouter();
                    IPv4Address networkDRintID = destinationNetworkLSA->getHeader().getLinkStateID();
                    unsigned long interfaceNum = interfaceList.size();
                    for (i = 0; i < interfaceNum; i++) {        // jedna iteracia

                        EV_DEBUG << "interfaceList[i]->getDesignatedID() = " << interfaceList[i]->getDesignatedID() << "\n"; //LG
                        EV_DEBUG << "networkDesignatedRouter = " << networkDesignatedRouter << "     intf-DRID = " << interfaceList[i]->getDesignatedID() << "\n";
                        EV_DEBUG << "networkDRintID = " << networkDRintID << "    intf-DRintID = " << (IPv4Address)interfaceList[i]->getDesignatedIntID() << "\n";

                        OSPFv3Interface::OSPFv3InterfaceType intfType = interfaceList[i]->getType();
                        if (((intfType == OSPFv3Interface::BROADCAST_TYPE) ||
                             (intfType == OSPFv3Interface::NBMA_TYPE)) &&
                            (interfaceList[i]->getDesignatedID() == networkDesignatedRouter)
                            && ((IPv4Address)interfaceList[i]->getDesignatedIntID() == networkDRintID))
                        {
                            NextHop nextHop;

                            nextHop.ifIndex = interfaceList[i]->getInterfaceId();
                            nextHop.hopAddress = IPv6Address::UNSPECIFIED_ADDRESS;    //TODO revise it!
                            nextHop.advertisingRouter = destinationNetworkLSA->getHeader().getAdvertisingRouter();
                            hops->push_back(nextHop);
                        }
                    }
                }
            }
        }

    }
    else { // if parent is NETWORK_LSA
        EV_DEBUG <<  " PARENT JE NETWORK_LSA\n"; //LG
        //           cout <<  "AK PARENT JE NETWORK_LSA" << endl;
        NetworkLSA *networkLSA = dynamic_cast<NetworkLSA *>(parent);
        if (networkLSA != nullptr)
        {
           if (networkLSA->getParent() != spfTreeRoot) { //ak som network a moj parent nie je spfTreeRoot, vrat v�etky nexthopy
               unsigned int nextHopCount = networkLSA->getNextHopCount();
               for (i = 0; i < nextHopCount; i++) {
                   hops->push_back(networkLSA->getNextHop(i));
               }
               return hops;
           }
           else
           {
               // for Network-LSA, Link State ID is ID of interface by which it is connected into network LG
               IPv4Address parentLinkStateID = parent->getHeader().getAdvertisingRouter();

               RouterLSA *destinationRouterLSA = dynamic_cast<RouterLSA *>(destination);
               if (destinationRouterLSA != nullptr) {
                   IPv4Address& destinationRouterID = destinationRouterLSA->getHeader().getLinkStateID();
                   unsigned int linkCount = destinationRouterLSA->getRoutersArraySize();
                   for (i = 0; i < linkCount; i++) {
                       OSPFv3RouterLSABody& link = destinationRouterLSA->getRouters(i);
                       NextHop nextHop;

                       if (((link.type == TRANSIT_NETWORK) &&
                            (link.neighborRouterID == parentLinkStateID))
//                               ||  ((link.getType() == STUB_LINK) &&       LG
//                            ((link.getLinkID() & IPv4Address(link.getLinkData())) == (parentLinkStateID & networkLSA->getNetworkMask())))
                            )
                       {
                           unsigned long interfaceNum = interfaceList.size();
                           for (j = 0; j < interfaceNum; j++) {
                               OSPFv3Interface::OSPFv3InterfaceType intfType = interfaceList[j]->getType();
                               if (((intfType == OSPFv3Interface::BROADCAST_TYPE) ||
                                    (intfType == OSPFv3Interface::NBMA_TYPE)) &&
                                   (interfaceList[j]->getDesignatedID() == parentLinkStateID))
                               {
                                   OSPFv3Neighbor *nextHopNeighbor = interfaceList[j]->getNeighborById(destinationRouterID);
                                   if (nextHopNeighbor != nullptr) {
                                       nextHop.ifIndex = interfaceList[j]->getInterfaceId();
                                       nextHop.hopAddress = nextHopNeighbor->getNeighborIP();
                                       nextHop.advertisingRouter = destinationRouterLSA->getHeader().getAdvertisingRouter();
                                       hops->push_back(nextHop);
                                   }
                               }
                           }
                       }
                   }
               }
               else
               {
                   EV_DEBUG << "DESTINATION IS NOT ROUTER, nemalo by nastat NIY\n";  // LG
                   cout << "DESTINATION IS NOT ROUTER, nemalo by nastat NIY\n";
               }
           }
        }
    }


    return hops;

}

void OSPFv3Area::debugDump()
{
    for(auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++)
        EV_DEBUG << "\t\tinterface id: " << (*it)->getIntName() << "\n";

}//debugDump

std::string OSPFv3Area::detailedInfo() const
{//TODO - adjust so that it pnly prints LSAs that are there
    std::stringstream out;

    out << "OSPFv3 1 address-family ";
    if(this->getInstance()->getAddressFamily()==IPV4INSTANCE)
        out << "ipv4 (router-id ";
    else
        out << "ipv6 (router-id ";

    out << this->getInstance()->getProcess()->getRouterID();
    out << ")\n\n";

    if(this->routerLSAList.size()>0) {
        out << "Router Link States (Area " << this->getAreaID().str(false) << ")\n" ;
        out << "ADV Router\tAge\tSeq#\t\tFragment ID\tLink count\tBits\n";
        for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            bool bitsEmpty = true;
            out << header.getAdvertisingRouter()<<"\t";
            out << header.getLsaAge() <<"\t0x8000000"<<header.getLsaSequenceNumber()<<"\t0\t\t1\t\t";
            if((*it)->getNtBit()) {
                out << "Nt ";
                bitsEmpty = false;
            }
            if((*it)->getXBit()){
                out << "x ";
                bitsEmpty = false;
            }
            if((*it)->getVBit()){
                out << "V ";
                bitsEmpty = false;
            }
            if((*it)->getEBit()){
                out << "E ";
                bitsEmpty = false;
            }
            if((*it)->getBBit()){
                out << "B";
                bitsEmpty = false;
            }
            if(bitsEmpty)
                out << "None";//TODO Link count and Bits

            out << endl;
        }
    }

    if(this->networkLSAList.size()>0) {
        out << "\nNet Link States (Area " << this->getAreaID().str(false) << ")\n" ;
        out << "ADV Router\tAge\tSeq#\t\tLink State ID\tRtr count\n";
        for(auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            out << header.getAdvertisingRouter()<<"\t";
            out << header.getLsaAge()<<"\t0x8000000"<<header.getLsaSequenceNumber()<<"\t"<<header.getLinkStateID().str(false)<<"\t\t" << (*it)->getAttachedRouterArraySize() << "\n";
        }
    }

    if(this->interAreaPrefixLSAList.size()>0) {
        out << "\nInter Area Prefix Link States (Area " << this->getAreaID().str(false) << ")\n" ;
        out << "ADV Router\tAge\tSeq#\t\tPrefix\n";
        for(auto it = this->interAreaPrefixLSAList.begin(); it != this->interAreaPrefixLSAList.end(); it++){
            OSPFv3LSAHeader& header = (*it)->getHeader();
            out << header.getAdvertisingRouter()<<"\t";
            out << (int)simTime().dbl() - header.getLsaAge()<<"\t0x8000000"<<header.getLsaSequenceNumber()<<"\t";

            L3Address addrPref = (*it)->getPrefix();
            if(this->getInstance()->getAddressFamily()==IPV4INSTANCE) {
                IPv4Address ipv4addr = addrPref.toIPv4();
                ipv4addr = ipv4addr.getNetwork();
                out << ipv4addr << "/" << (*it)->getPrefixLen() << endl;
            }
            else if(this->getInstance()->getAddressFamily()==IPV6INSTANCE) {
                IPv6Address ipv6addr = addrPref.toIPv6();
                ipv6addr = ipv6addr.getPrefix((*it)->getPrefixLen());
                if(ipv6addr == IPv6Address::UNSPECIFIED_ADDRESS)
                    out << "::/0" << endl; //TODO - this needs to be changed to the actual length
                else
                    out << ipv6addr << "/64" << endl;
            }
        }
    }

    out << "\nLink (Type-8) Link States (Area " << this->getAreaID().str(false) << ")\n" ;
    out << "ADV Router\tAge\tSeq#\t\tLink State ID\tInterface\n";
    for(auto it=this->interfaceList.begin(); it!=this->interfaceList.end(); it++) {
        int linkLSACount = (*it)->getLinkLSACount();
        for(int i = 0; i<linkLSACount; i++) {
            OSPFv3LSAHeader& header = (*it)->getLinkLSA(i)->getHeader();
            out << header.getAdvertisingRouter()<<"\t";
            out << header.getLsaAge()<<"\t0x8000000"<<header.getLsaSequenceNumber()<<"\t"<<header.getLinkStateID().str(false)<<"\t\t"<< (*it)->getIntName() << "\n";
        }
    }

    if(this->intraAreaPrefixLSAList.size()>0) {
        out << "\nIntra Area Prefix Link States (Area" << this->getAreaID().str(false) << ")\n" ;
        out << "ADV Router\tAge\tSeq#\t\tLink ID\t\tRef-lstype\tRef-LSID\n";
        for(auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++) {
            OSPFv3LSAHeader& header = (*it)->getHeader();
            out << header.getAdvertisingRouter() << "\t";
            out << header.getLsaAge() << "\t0x8000000" << header.getLsaSequenceNumber() << "\t" << header.getLinkStateID().str(false) << "\t\t0x200" << (*it)->getReferencedLSType() << "\t\t" << (*it)->getReferencedLSID().str(false)<<"\n\n";
        }
    }

    // LG out stream for DEV
    out << "ROUTER LSA LIST .size = " <<  routerLSAList.size() << "\n";
    for(auto it=this->routerLSAList.begin(); it!=this->routerLSAList.end(); it++) {
        OSPFv3LSAHeader& header = (*it)->getHeader();
        out << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< endl;
        out << "LinkStateID =\t\t" << header.getLinkStateID() << endl;
        out << "Age = \t\t\t" << header.getLsaAge() << endl;
        out << "interfaceID\tneighborIntID\tneighborRouterID\n";
        for (int i = 0; i < (*it)->getRoutersArraySize(); i++)
           out << (*it)->getRouters(i).interfaceID << "\t\t\t" << (*it)->getRouters(i).neighborInterfaceID << "\t\t\t" << (*it)->getRouters(i).neighborRouterID << "\n";
        out << endl;
    }

    out << endl;

    out <<  "NETWORK LSA LIST .size() = " << networkLSAList.size() << endl;
    for (auto it=this->networkLSAList.begin(); it!=this->networkLSAList.end(); it++) {
        OSPFv3LSAHeader& header = (*it)->getHeader();
        out << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< endl;
        out << "LinkStateID =\t\t" << header.getLinkStateID() << endl;
        out << "Age = \t\t\t" << header.getLsaAge() << endl;
        out << "Attached Router:" << endl;
        for (int i = 0; i < (*it)->getAttachedRouterArraySize(); i++)
            out << (*it)->getAttachedRouter(i) << endl;
    }
    out << endl;

    out << "INTER AREA LSA LIST = " << interAreaPrefixLSAList.size() << "\n";
    for (auto it=this->interAreaPrefixLSAList.begin(); it!=this->interAreaPrefixLSAList.end(); it++) {
       OSPFv3LSAHeader& header = (*it)->getHeader();
       out << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
       out << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
       out << "Age = \t\t\t" << header.getLsaAge() << endl;
       out << "prefix =\t\t " << (*it)->getPrefix() << "\n";
       out << "prefixLen =\t\t " << (int)(*it)->getPrefixLen() << "\n";
    }
    out << endl;
    out <<  "INTRA AREA PREFIX = " << intraAreaPrefixLSAList.size() << "\n";
    for (auto it=this->intraAreaPrefixLSAList.begin(); it!=this->intraAreaPrefixLSAList.end(); it++) {
        OSPFv3LSAHeader& header = (*it)->getHeader();
        out << "AdvertisingRouter =\t" << header.getAdvertisingRouter()<< "\n";
        out << "LinkStateID =\t\t" << header.getLinkStateID() << "\n";
        out << "Age = \t\t\t" << header.getLsaAge() << endl;
        out << "getPrefixesArraySize = " << (*it)->getPrefixesArraySize() << "\n";
        out << "getReferencedLSType = " << (*it)->getReferencedLSType() << "\n";
        out << "getReferencedLSID = " << (*it)->getReferencedLSID() << "\n";
        out << "getReferencedAdvRtr = " << (*it)->getReferencedAdvRtr() << "\n";
        out << "prefixes :" << "\n";
        for (int i = 0; i < (*it)->getPrefixesArraySize(); i++) {
            out << "addressPrefix = "<< (*it)->getPrefixes(i).addressPrefix << "\n";
            out << "prefixLen = "<< int((*it)->getPrefixes(i).prefixLen) << "\n";
        }
        out << "\n";
    }
    out << "\n\n";

    for (int t = 0 ; t < interfaceList.size(); t++)
    {
        OSPFv3Interface *intf = interfaceList[t];
        out << "intfID = " << intf->getInterfaceId() <<  "\tTYPE " << intf->getType() << "\n";
    }



    return out.str();

}//detailedInfo
}//namespace inet
