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

#include "ansa/linklayer/lldp/tables/LLDPNeighbourTable.h"
#include <algorithm>


namespace inet {


/*LLDPNeighbour::LLDPNeighbour(const char *cId, const char *pId) {
    chassisId = cId;
    portId = pId;

    std::string m;
    m = c;
    m += p;
    msap = m.c_str();
}*/
LLDPNeighbour::LLDPNeighbour(){}
/*
LLDPNeighbour::LLDPNeighbour()
{
    chassisId = nullptr;
    portId = nullptr;
    msap = nullptr;
}*/

std::string LLDPNeighbour::info() const
{
    std::stringstream string;

    string << chassisId;
    return string.str();
}



///************************ LLDP AGENT TABLE ****************************///


LLDPNeighbour * LLDPNeighbourTable::findNeighbourByMSAP(const char* msap)
{
    std::vector<LLDPNeighbour *>::iterator it;

    if(msap == nullptr)
        return nullptr;

    // through all neighbours search for same MSAP
    for (it = neighbours.begin(); it != neighbours.end(); ++it)
        if((*it)->getMsap() != nullptr && strcmp((*it)->getMsap(), msap) == 0)
            return (*it);

    return nullptr;
}


void LLDPNeighbourTable::addNeighbour(LLDPNeighbour * neighbour)
{
    if(findNeighbourByMSAP(neighbour->getMsap()) != NULL)
    {// neighbour already in table
        throw cRuntimeError("Adding to LLDPNeighbourTable neighbour, which is already in it - name %s, port %d", neighbour->getChassisId(), neighbour->getPortId());
    }

    neighbours.push_back(neighbour);
}

/**
 * Remove neighbour
 *
 * @param   neighbour   neighbour to delete
 *
 */
void LLDPNeighbourTable::removeNeighbour(LLDPNeighbour * neighbour)
{
    auto n = find(neighbours.begin(), neighbours.end(), neighbour);
    if (n != neighbours.end())
    {
        delete *n;
        neighbours.erase(n);
    }
}

/**
 * Removes neighbour
 *
 * @param   chassisId   chassis ID
 * @param   portId      port ID
 */
void LLDPNeighbourTable::removeNeighbour(const char* msap)
{
    std::vector<LLDPNeighbour *>::iterator it;

    if(msap == nullptr)
        return;

    for (it = neighbours.begin(); it != neighbours.end();)
    {// through all neighbours
        if((*it)->getMsap() != nullptr && strcmp((*it)->getMsap(), msap) == 0)
        {// found same
            delete (*it);
            it = neighbours.erase(it);
            return;
        }        else
        {// do not delete -> get next
            ++it;
        }
    }
}

std::string LLDPNeighbourTable::printStats()
{
    std::stringstream string;
    std::vector<LLDPNeighbour *>::iterator it;

    //for (it = neighbours.begin(); it != neighbours.end(); ++it)
    //{// through all neighbours
        //if((*it)->getAdminStatus() != AS::disabled)
        //{
            //string << (*it)->getIfaceName() << " interface statistics:" << endl;
/*
            string << "Received " << (*it)->rxStat.str();
            if((*it)->rxStat.tlv[tlvT::UPDATE].getCount() > 0) string << "Update avg. size: " << ((*it)->rxStat.tlv[tlvT::ROUTERID].getSum() + (*it)->rxStat.tlv[tlvT::NEXTHOP].getSum() + (*it)->rxStat.tlv[tlvT::UPDATE].getSum()) / static_cast<double>((*it)->rxStat.tlv[tlvT::UPDATE].getCount()) << " B/TLV" << endl;

            string << endl << "Transmitted " << (*it)->txStat.str();
            if((*it)->txStat.tlv[tlvT::UPDATE].getCount() > 0) string << "Update avg. size: " << ((*it)->txStat.tlv[tlvT::ROUTERID].getSum() + (*it)->txStat.tlv[tlvT::NEXTHOP].getSum() + (*it)->txStat.tlv[tlvT::UPDATE].getSum()) / static_cast<double>((*it)->txStat.tlv[tlvT::UPDATE].getCount()) << " B/TLV" << endl;
            */
      //  }
    //}
    return string.str();
}

LLDPNeighbourTable::LLDPNeighbourTable() {

}

LLDPNeighbourTable::~LLDPNeighbourTable() {
    std::vector<LLDPNeighbour *>::iterator it;

    for (it = neighbours.begin(); it != neighbours.end(); ++it)
    {// through all interfaces
        delete (*it);
    }
    neighbours.clear();
}

} /* namespace inet */
