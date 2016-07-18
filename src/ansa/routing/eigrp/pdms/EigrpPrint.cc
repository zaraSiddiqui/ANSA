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
/**
* @file EigrpPrint.cc
* @author Vit Rek, xrekvi00@stud.fit.vutbr.cz
* @date 6. 11. 2014
* @brief EIGRP Print functions
* @detail File contains useful functions for printing EIGRP informations (CUT + PASTE from EigrpIpv4Pdm.cc)
*/

#include "ansa/routing/eigrp/pdms/EigrpPrint.h"

namespace inet {
namespace eigrp
{

// User messages
const char *UserMsgs[] =
{
  // M_OK
  "OK",
  // M_UPDATE_SEND
  "Update",
  // M_REQUEST_SEND
  "Request",
  // M_QUERY_SEND
  "Query",
  // M_REPLY_SEND
  "Reply",
  // M_HELLO_SEND
  "Hello",
  // M_DISABLED_ON_IF
  "EIGRP process isn't enabled on interface",
  // M_NEIGH_BAD_AS
  "AS number is different",
  // M_NEIGH_BAD_KVALUES
  "K-value mismatch",
  // M_NEIGH_BAD_SUBNET
  "Not on the common subnet",
  // M_SIAQUERY_SEND
  "Send SIA Query message",
  // M_SIAREPLY_SEND
  "Send SIA Reply message",
};

}; // end of namespace eigrp

std::ostream& operator<<(std::ostream& os, const EigrpNetwork<IPv4Address>& network)
{
    os << "Address:" << network.getAddress() << " Mask:" << network.getMask();
    return os;
}

std::ostream& operator<<(std::ostream& os, const EigrpNetwork<IPv6Address>& network)
{
    os << "Address:" << network.getAddress() << " Mask: /" << getNetmaskLength(network.getMask());
    return os;
}

std::ostream& operator<<(std::ostream& os, const EigrpKValues& kval)
{
    os << "K1:" << kval.K1 << " K2:" << kval.K2 << " K3:" << kval.K3;
    os << "K4:" << kval.K4 << " K5:" << kval.K5 << " K6:" << kval.K6;
    return os;
}

std::ostream& operator<<(std::ostream& os, const EigrpStub& stub)
{
    if (stub.connectedRt) os << "connected ";
    if (stub.leakMapRt) os << "leak-map ";
    if (stub.recvOnlyRt) os << "recv-only ";
    if (stub.redistributedRt) os << "redistrib ";
    if (stub.staticRt) os << "static ";
    if (stub.summaryRt) os << "summary";
    return os;
}
}
