/*
 *  Original Copyright (c) 2014, Oculus VR, Inc.
 *  Modified Copyright (c) 2016, Indium Games
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 *
 */


#ifndef __DEMOCFUNCTIONS_H
#define __DEMOCFUNCTIONS_H

#include <iostream>
#include <map>
#include <utility>

#include "BitStream.h"
#include "RPC3.h"
#include "RakPeerInterface.h"
#include "NetworkIDObject.h"
#include "GetTime.h"

#include "democlasses.h"

void CFunc(RakNet::RakString rakString, ClassC *c1,
                            const char *str, RakNet::RPC3 *rpcFromNetwork) {
    if (rpcFromNetwork == 0) {
        std::cout << "CFunc called locally" << std::endl;
    }
    else {
        std::cout << "CFunc called from " <<
                rpcFromNetwork->GetLastSenderAddress().ToString() <<
                "  timestamp: " << rpcFromNetwork->GetLastSenderTimestamp() << std::endl;
    }
    
    std::cout << "rakString=" << rakString.C_String() << std::endl;
    
    std::cout << "c1=" << c1->c << std::endl;
    
    std::cout << "str=" << str << std::endl;
}

void CFunc2(ClassC *c1, RakNet::RPC3 *rpcFromNetwork) {
    if (rpcFromNetwork == 0) {
        std::cout << "CFunc2 called locally" << std::endl;
    }
    else {
        std::cout << "CFunc2 called from " <<
                rpcFromNetwork->GetLastSenderAddress().ToString() <<
                "  timestamp: " << rpcFromNetwork->GetLastSenderTimestamp() << std::endl;
    }
    
    std::cout << "c1=" << c1->c << std::endl;
}

static std::map<int, uint64_t> cFuncTestCalls;

void CFuncTest(RakNet::RakString rakString, ClassC *c1, const char *str,
        uint64_t callNumber, uint64_t callTime, RakNet::RPC3 *rpcFromNetwork) {

    std::pair<int, uint64_t> p(callNumber, RakNet::GetTimeUS() - callTime);
    cFuncTestCalls.insert(p);
}

#endif
