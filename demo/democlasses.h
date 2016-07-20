/*
 *  Original Copyright (c) 2014, Oculus VR, Inc.
 *  Modified Copyright (c) 2016, Indium Games
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 *
 */


#ifndef __DEMOCLASSES_H
#define __DEMOCLASSES_H

#include <iostream>

#include "BitStream.h"
#include "RPC3.h"
#include "RakPeerInterface.h"
#include "NetworkIDObject.h"

class ClassC;
class ClassD;

class BaseClassA {
    
public:
    BaseClassA() :a(1) {}
    
    int a;
};

class BaseClassB {
    
public:
    BaseClassB() : b(2) {}
    
    virtual void ClassMemberFunc(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                    ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                    RakNet::RPC3 *rpcFromNetwork);
    
    int b;
};

class ClassC
    : public BaseClassA, public BaseClassB, public RakNet::NetworkIDObject {
        
public:
    ClassC() : c(3) {}
    
    virtual void TestSlot(void) {
        std::cout << "ClassC::TestSlot" << std::endl;
    }
    
    virtual void ClassMemberFunc(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                        ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                        RakNet::RPC3 *rpcFromNetwork);
    
    void ClassMemberFunc2(RakNet::RPC3 *rpcFromNetwork);
    
    int c;
};

class ClassD : public BaseClassA, public RakNet::NetworkIDObject {
    
public:
    ClassD() {
        for (int i=0; i < 10; i++) {
            tenBytes[i]=i;
        }
    }
    
    bool Verify(void) {
        for (int i=0; i < 10; i++) {
            if (tenBytes[i]!=i) {
                return false;
            }
            else {
                return true;
            }
        }
    }
    
    virtual void TestSlot(void) {
        std::cout << "ClassD::TestSlot" << std::endl;
    }
    
    char tenBytes[10];
};

#endif