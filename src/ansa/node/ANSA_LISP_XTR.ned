//
// Copyright (C) 2009 - today, Brno University of Technology, Czech Republic
// Copyright (C) 2005 Andras Babos
// Copyright (C) 2003 Andras Varga; CTIE, Monash University, Australia
//               2010 Zoltan Bojthe
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
//@author Vladimir Vesely (<a href="mailto:ivesely@fit.vutbr.cz">ivesely@fit.vutbr.cz</a>)
// @copyright Brno University of Technology (www.fit.vutbr.cz) under GPLv3

package ansa.node;

import ansa.node.ANSA_Router;
import ansa.routing.lisp.LISPRouting;

module ANSA_xTR extends ANSA_Router
{
    parameters:
        hasLISP = true;
        lisp.configData = configData; 
}
