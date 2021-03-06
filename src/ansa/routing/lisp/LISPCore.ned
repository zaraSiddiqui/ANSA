//
// Copyright (C) 2013, 2014 Brno University of Technology
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
//
// @author Vladimir Vesely / ivesely@fit.vutbr.cz / http://www.fit.vutbr.cz/~ivesely/
// @copyright Brno University of Technology (www.fit.vutbr.cz) under GPLv3
 

package ansa.routing.lisp;

simple LISPCore
{
    parameters:
        @display("i=block/switch");                  
        xml configData = default(xml("<LISP />"));
        bool mapServerV4 = default(false);				//Node acts as Map Server for IPv4 EIDs, overriden by XML config
        bool mapServerV6 = default(false);				//Node acts as Map Server for IPv6 EIDs, overriden by XML config
		bool mapResolverV4 = default(false);			//Node acts as Map Resolver for IPv4 EIDs, overriden by XML config
        bool mapResolverV6 = default(false);    		//Node acts as Map Resolver for IPv6 EIDs, overriden by XML config
        
        string rlocProbingAlgo = default("Cisco"); 		// Cisco OR Simple OR Sophisticated
        bool acceptMapRequestMapping = default(false);	//Whether MapReplyRecord from Map-Request should be cached
        int mapCacheTtl = default(1440); 				//CacheTTL in minutes for send Map-Replies
        
        bool echoNonceAlgo = default(false);			//Whether EchoNonceAlgorithm is on or off
        bool ciscoStartupDelays = default(false);		//Shift rloc probing of IPv4 and IPv6
        
        @signal[sigLispForward](type=bool);
        @signal[sigLispDrop](type=bool);        
        @statistic[packet-frwd](title="packet forwarded"; source=count(sigLispForward); record=last,vector?);
        @statistic[packet-drop](title="packet drop"; source=count(sigLispDrop); record=last,vector?);
        
                
    gates:
        input udpContrIn @labels(UDPControlInfo/up);
        output udpContrOut @labels(UDPControlInfo/down);
        input udpDataIn @labels(UDPControlInfo/up);
        output udpDataOut @labels(UDPControlInfo/down);
        input  ipv4In;
        output ipv4Out;
        input  ipv6In;
        output ipv6Out;
}

