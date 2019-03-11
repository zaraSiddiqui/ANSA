#ifndef __ANSA_OSPFV3AREA_H_
#define __ANSA_OSPFV3AREA_H_

#include <omnetpp.h>
#include "inet/common/INETDefs.h"
#include "inet/networklayer/contract/ipv4/IPv4Address.h"
#include "ansa/routing/ospfv3/interface/OSPFv3Interface.h"
#include "ansa/routing/ospfv3/OSPFv3Packet_m.h"
#include "ansa/routing/ospfv3/process/OSPFv3LSA.h"
#include "ansa/routing/ospfv3/process/OSPFv3RoutingTableEntry.h"
#include "ansa/routing/ospfv3/neighbor/OSPFv3Neighbor.h"


namespace inet{

class OSPFv3Instance;
class OSPFv3Interface;
class OSPFv3Process;

enum OSPFv3AreaType {
    NORMAL = 0,
    STUB,
    TOTALLY_STUBBY,
    NSSA,
    NSSA_TOTALLY_STUB
};

class INET_API OSPFv3Area : public cObject
{
  public:
    OSPFv3Area(IPv4Address areaID, OSPFv3Instance* containingInstance, OSPFv3AreaType type);
    virtual ~OSPFv3Area();
    IPv4Address getAreaID() const {return this->areaID;}
    OSPFv3AreaType getAreaType() const {return this->areaType;}
    bool hasInterface(std::string);
    void addInterface(OSPFv3Interface*);
    void init();
    void debugDump();
    void ageDatabase();
    int getInstanceType(){return this->instanceType;};
    void setiInstanceType(int type){this->instanceType = type;};
    void setExternalRoutingCapability(bool capable){this->externalRoutingCapability=capable;}
    void setStubDefaultCost(int newCost){this->stubDefaultCost=newCost;}
    void setTransitCapability(bool capable){this->transitCapability=capable;}
    OSPFv3Interface* getInterfaceById(int id);
    OSPFv3Interface* getNetworkLSAInterface(IPv4Address id);
    OSPFv3Interface* getInterfaceByIndex(int id);
    OSPFv3Instance* getInstance() const {return this->containingInstance;};
    bool getExternalRoutingCapability(){return this->externalRoutingCapability;}
    int getStubDefaultCost(){return this->stubDefaultCost;}
    bool getTransitCapability(){return this->transitCapability;}
    OSPFv3Interface* findVirtualLink(IPv4Address routerID);

    OSPFv3Interface* getInterface(int i) const {return this->interfaceList.at(i);}
    OSPFv3Interface* getInterfaceByIndex (IPv4Address address);
    int getInterfaceCount() const {return this->interfaceList.size();}
    OSPFv3LSA* getLSAbyKey(LSAKeyType lsaKey);

    void addAddressRange(IPv6AddressRange addressRange, bool advertise);
    bool hasAddressRange(IPv6AddressRange addressRange) const;
    void addAddressRange(IPv4AddressRange addressRange, bool advertise);
    bool hasAddressRange(IPv4AddressRange addressRange) const;


    /* ROUTER LSA */
    RouterLSA* originateRouterLSA();//this originates one router LSA for one area
    int getRouterLSACount(){return this->routerLSAList.size();}
    RouterLSA* getRouterLSA(int i){return this->routerLSAList.at(i);}
    RouterLSA* getRouterLSAbyKey(LSAKeyType lsaKey);
    bool installRouterLSA(OSPFv3RouterLSA *lsa);
    bool updateRouterLSA(RouterLSA* currentLsa, OSPFv3RouterLSA* newLsa);
    bool routerLSADiffersFrom(OSPFv3RouterLSA* currentLsa, OSPFv3RouterLSA* newLsa);
    IPv4Address getNewRouterLinkStateID();
    IPv4Address getRouterLinkStateID(){return this->routerLsID;}
    uint32_t getCurrentRouterSequence(){return this->routerLSASequenceNumber;}
    void incrementRouterSequence(){this->routerLSASequenceNumber++;}
    RouterLSA* findRouterLSAByID(IPv4Address linkStateID);
    RouterLSA* findRouterLSA(IPv4Address routerID);
    void deleteRouterLSA(int index);
    void addRouterLSA(RouterLSA* newLSA){this->routerLSAList.push_back(newLSA);}

    /*NETWORK LSA */
    void addNetworkLSA(NetworkLSA* newLSA){this->networkLSAList.push_back(newLSA);}
    NetworkLSA* originateNetworkLSA(OSPFv3Interface* interface);//this originates one router LSA for one area
    int getNetworkLSACount(){return this->networkLSAList.size();}
    NetworkLSA* getNetworkLSA(int i){return this->networkLSAList.at(i);}
    bool installNetworkLSA(OSPFv3NetworkLSA *lsa);
    bool updateNetworkLSA(NetworkLSA* currentLsa, OSPFv3NetworkLSA* newLsa);
    bool networkLSADiffersFrom(OSPFv3NetworkLSA* currentLsa, OSPFv3NetworkLSA* newLsa);
    IPv4Address getNewNetworkLinkStateID();
    IPv4Address getNetworkLinkStateID(){return this->networkLsID;}
    uint32_t getCurrentNetworkSequence(){return this->networkLSASequenceNumber;}
    void incrementNetworkSequence(){this->networkLSASequenceNumber++;}
    NetworkLSA* findNetworkLSAByLSID(IPv4Address linkStateID);
    NetworkLSA* getNetworkLSAbyKey(LSAKeyType LSAKey);
    NetworkLSA* findNetworkLSA(uint32_t intID, IPv4Address routerID);

    /* INTER AREA PREFIX LSA */
    void addInterAreaPrefixLSA(InterAreaPrefixLSA* newLSA){this->interAreaPrefixLSAList.push_back(newLSA);};
    int getInterAreaPrefixLSACount(){return this->interAreaPrefixLSAList.size();}
    InterAreaPrefixLSA* getInterAreaPrefixLSA(int i){return this->interAreaPrefixLSAList.at(i);}
    void originateDefaultInterAreaPrefixLSA(OSPFv3Area* toArea);
    void originateInterAreaPrefixLSA(OSPFv3IntraAreaPrefixLSA* lsa, OSPFv3Area* fromArea);
    void originateInterAreaPrefixLSA(OSPFv3LSA* prefLsa, OSPFv3Area* fromArea);
    bool installInterAreaPrefixLSA(OSPFv3InterAreaPrefixLSA* lsa);
    bool updateInterAreaPrefixLSA(InterAreaPrefixLSA* currentLsa, OSPFv3InterAreaPrefixLSA* newLsa);      // TODO: resetInstallTime
    bool interAreaPrefixLSADiffersFrom(OSPFv3InterAreaPrefixLSA* currentLsa, OSPFv3InterAreaPrefixLSA* newLsa);
    IPv4Address getNewInterAreaPrefixLinkStateID();
    uint32_t getCurrentInterAreaPrefixSequence(){return this->interAreaPrefixLSASequenceNumber;}
    void incrementInterAreaPrefixSequence(){this->interAreaPrefixLSASequenceNumber++;}


    //* INTRA AREA PREFIX LSA */
    void addIntraAreaPrefixLSA(IntraAreaPrefixLSA* newLSA){this->intraAreaPrefixLSAList.push_back(newLSA);}
    IntraAreaPrefixLSA* originateIntraAreaPrefixLSA();//this is for non-BROADCAST links
    int getIntraAreaPrefixLSACount(){return this->intraAreaPrefixLSAList.size();}
    IntraAreaPrefixLSA* getIntraAreaPrefixLSA(int i){return this->intraAreaPrefixLSAList.at(i);}
    IntraAreaPrefixLSA* getNetIntraAreaPrefixLSA(L3Address prefix, int prefLen);
    bool installIntraAreaPrefixLSA(OSPFv3IntraAreaPrefixLSA *lsa);
    bool updateIntraAreaPrefixLSA(IntraAreaPrefixLSA* currentLsa, OSPFv3IntraAreaPrefixLSA* newLsa);
    bool intraAreaPrefixLSADiffersFrom(OSPFv3IntraAreaPrefixLSA* currentLsa, OSPFv3IntraAreaPrefixLSA* newLsa);
    IPv4Address getNewIntraAreaPrefixLinkStateID();
    IPv4Address getIntraAreaPrefixLinkStateID(){return this->intraAreaPrefixLsID;}
    uint32_t getCurrentIntraAreaPrefixSequence(){return this->intraAreaPrefixLSASequenceNumber;}
    void incrementIntraAreaPrefixSequence(){this->intraAreaPrefixLSASequenceNumber++;}

    IntraAreaPrefixLSA* originateNetIntraAreaPrefixLSA(NetworkLSA* networkLSA, OSPFv3Interface* interface);//this originates one router LSA for one area
    IPv4Address getNewNetIntraAreaPrefixLinkStateID();
    IPv4Address getNetIntraAreaPrefixLinkStateID(){return this->netIntraAreaPrefixLsID;}
    uint32_t getCurrentNetIntraAreaPrefixSequence(){return this->netIntraAreaPrefixLSASequenceNumber;}
    void incrementNetIntraAreaPrefixSequence(){this->netIntraAreaPrefixLSASequenceNumber++;}
    IntraAreaPrefixLSA*  findIntraAreaPrefixByAdvRouter(IPv4Address advRouter);
    IntraAreaPrefixLSA* findNetIntraAreaPrefixLSAByReference(IPv4Address refLSID, IPv4Address refAdvRouter);

    OSPFv3LSAHeader* findLSA(LSAKeyType lsaKey);
    bool floodLSA(OSPFv3LSA* lsa, OSPFv3Interface* interface=nullptr, OSPFv3Neighbor* neighbor=nullptr);

    void removeFromAllRetransmissionLists(LSAKeyType lsaKey);
    bool isOnAnyRetransmissionList(LSAKeyType lsaKey) const;
    bool hasAnyNeighborInStates(int states) const;


    void calculateShortestPathTree(std::vector<OSPFv3RoutingTableEntry* >& newTableIPv6, std::vector<OSPFv3IPv4RoutingTableEntry* >& newTableIPv4);
    void calculateInterAreaRoutes(std::vector<OSPFv3RoutingTableEntry* >& newTable, std::vector<OSPFv3IPv4RoutingTableEntry* >& newTableIPv4);

    bool findSameOrWorseCostRoute(const std::vector<OSPFv3RoutingTableEntry *>& newTable, // TODO >  PRE IPV4 SPRAVIT SAMOSTATNU METODU
            const InterAreaPrefixLSA& interAreaPrefixLSA,
            unsigned short currentCost,
            bool& destinationInRoutingTable,
            std::list<OSPFv3RoutingTableEntry *>& sameOrWorseCost) const;
    bool findSameOrWorseCostRoute(const std::vector<OSPFv3IPv4RoutingTableEntry *>& newTable,
            const InterAreaPrefixLSA& interAreaPrefixLSA,
            unsigned short currentCost,
            bool& destinationInRoutingTable,
            std::list<OSPFv3IPv4RoutingTableEntry *>& sameOrWorseCost) const;
    OSPFv3RoutingTableEntry *createRoutingTableEntryFromInterAreaPrefixLSA(const InterAreaPrefixLSA& interAreaPrefixLSA,
            unsigned short entryCost,
            const OSPFv3RoutingTableEntry& borderRouterEntry) const;
    OSPFv3IPv4RoutingTableEntry *createRoutingTableEntryFromInterAreaPrefixLSA(const InterAreaPrefixLSA& interAreaPrefixLSA,
            unsigned short entryCost,
            const OSPFv3IPv4RoutingTableEntry& borderRouterEntry) const;
    void recheckInterAreaPrefixLSAs(std::vector<OSPFv3RoutingTableEntry* >& newTable, std::vector<OSPFv3IPv4RoutingTableEntry* >& newTableIPv4);
    bool hasLink(OSPFv3LSA *fromLSA, OSPFv3LSA *toLSA) const;
    std::vector<NextHop> *calculateNextHops(OSPFv3SPFVertex* destination, OSPFv3SPFVertex *parent) const;
    std::vector<NextHop> *calculateNextHops(OSPFv3LSA *destination, OSPFv3LSA *parent) const;


    std::string detailedInfo() const override;

    void setSpfTreeRoot(RouterLSA* routerLSA){spfTreeRoot = routerLSA;};

  private:
    bool v6; // for IPv6 AF is this set to true, for IPv4 to false

    IPv4Address areaID;
    OSPFv3AreaType areaType;
    std::vector<OSPFv3Interface*> interfaceList;//associated router interfaces

    //address ranges - networks where router within this area have a direct connection
    std::vector<IPv6AddressRange> IPv6areaAddressRanges;
    std::map<IPv6AddressRange, bool> IPv6advertiseAddressRanges;
    std::vector<IPv4AddressRange> IPv4areaAddressRanges;
    std::map<IPv4AddressRange, bool> IPv4advertiseAddressRanges;

    std::map<std::string, OSPFv3Interface*> interfaceByName;//interfaces by ids
    std::map<int, OSPFv3Interface*> interfaceById;
    std::map<int, OSPFv3Interface*> interfaceByIndex;
    int instanceType;
    OSPFv3Instance* containingInstance;
    bool externalRoutingCapability;
    int stubDefaultCost;
    bool transitCapability;

    std::map<IPv4Address, RouterLSA *> routerLSAsByID;
    std::map<IPv4Address, NetworkLSA *> networkLSAsByID;
    std::map<IPv4Address, IntraAreaPrefixLSA *> intraAreaPrefixLSAByID;

    std::vector<RouterLSA* > routerLSAList;
    IPv4Address routerLsID = IPv4Address::UNSPECIFIED_ADDRESS;
    uint32_t routerLSASequenceNumber = 1;

    std::vector<NetworkLSA* > networkLSAList;
    IPv4Address networkLsID = IPv4Address::UNSPECIFIED_ADDRESS;
    uint32_t networkLSASequenceNumber = 1;

    std::vector<InterAreaPrefixLSA* > interAreaPrefixLSAList;
    uint32_t interAreaPrefixLSASequenceNumber = 1;

    std::vector<IntraAreaPrefixLSA*> intraAreaPrefixLSAList;
    IPv4Address intraAreaPrefixLsID = (IPv4Address) 1; //in simulaiton, zero print as <unspec>
    uint32_t intraAreaPrefixLSASequenceNumber = 1;

    IPv4Address netIntraAreaPrefixLsID = IPv4Address::UNSPECIFIED_ADDRESS;
    uint32_t netIntraAreaPrefixLSASequenceNumber = 1;

    RouterLSA* spfTreeRoot=nullptr;
    //list of summary lsas
    //shortest path tree

};

inline std::ostream& operator<<(std::ostream& ostr, const OSPFv3Area& area)
{
    ostr << area.detailedInfo();
    return ostr;
}

}//namespace inet

#endif
