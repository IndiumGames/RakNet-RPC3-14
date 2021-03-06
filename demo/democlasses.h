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
#include <map>
#include <utility>

#include "BitStream.h"
#include "RPC3.h"
#include "RakPeerInterface.h"
#include "NetworkIDObject.h"
#include "GetTime.h"

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
    ClassC() :BaseClassA(), BaseClassB(), RakNet::NetworkIDObject(), c(3) {}
    
    virtual void TestSlot();
    
    virtual void TestSlotTest(uint64_t callNumber, uint64_t callTime);
    
    virtual void ClassMemberFunc(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                        ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                        RakNet::RPC3 *rpcFromNetwork);
                        
    virtual void ClassMemberFuncTest(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                        ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                        uint64_t callNumber, uint64_t callTime,
                        RakNet::RPC3 *rpcFromNetwork);
    
    void ClassMemberFunc2(RakNet::RPC3 *rpcFromNetwork);
    
    int c;
    
    std::map<int, uint64_t> testCalls;
    std::map<int, uint64_t> testSlots;
};

class ClassD : public BaseClassA, public RakNet::NetworkIDObject {
    
public:
    ClassD();
    
    bool Verify();
    
    virtual void TestSlot();
    
    virtual void TestSlotTest(uint64_t callNumber, uint64_t callTime);
    
    char tenBytes[10];
    
    std::map<int, uint64_t> testSlots;
};

#endif
