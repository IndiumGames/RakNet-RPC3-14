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

void CFunc(RakNet::RakString rakString, int intArray[10], ClassC *c1,
                            const char *str, RakNet::RPC3 *rpcFromNetwork) {
    if (rpcFromNetwork == 0) {
        std::cout << "CFunc called locally" << std::endl;
    }
    else {
        std::cout << "CFunc called from " <<
                rpcFromNetwork->GetLastSenderAddress().ToString() << std::endl;
    }
    
    std::cout << "rakString=" << rakString.C_String() << std::endl;
    
    std::cout << "intArray = ";
    for (int i = 0; i < 10; i++) {
        std::cout << intArray[i] << " ";
    }
    std::cout << "" << std::endl;
    
    std::cout << "c1=" << c1->c << std::endl;
    std::cout << "str=" << str << std::endl;
}

#endif
