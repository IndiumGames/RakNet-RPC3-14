/*
 *  Original Copyright (c) 2014, Oculus VR, Inc.
 *  Modified Copyright (c) 2016, Indium Games
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 *
 */
 
#include "democlasses.h"


/*
 * rpcFromNetwork will be automatically set to the rpc plugin instance when this
 * function is called at a remote system. When calling locally you can set it to
 * 0, so you know if it is called loacally or remotely.
 */
void BaseClassB::ClassMemberFunc(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                    ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                    RakNet::RPC3 *rpcFromNetwork) {
    
    if (rpcFromNetwork==0) {
        std::cout << "BaseClassB::ClassMemberFunc called locally" << std::endl;
    }
    else {
        std::cout << "BaseClassB::ClassMemberFunc called from " <<
                rpcFromNetwork->GetLastSenderAddress().ToString() <<
                "  timestamp: " << rpcFromNetwork->GetLastSenderTimestamp() << std::endl;
    }
    
    std::cout << "a1=" << a1->a << " a2=" << a2.a <<
                                            " c1=" << c1->c << std::endl;
    std::cout << "d1::Verify=" << d1->Verify() << std::endl;
    
    RakNet::RakString rs1, rs2;
    bs1->Read(rs1);
    bs2.Read(rs2);
    std::cout << "rs1=" << rs1.C_String() << std::endl;
    std::cout << "rs2=" << rs2.C_String() << std::endl;
}
/*void BaseClassB::ClassMemberFuncTest(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                    ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                    int callNumber, RakNet::RPC3 *rpcFromNetwork) {
    
    if (rpcFromNetwork==0) {
        //std::cout << "BaseClassB::ClassMemberFuncTest called locally" << std::endl;
    }
    else {
        //std::cout << "BaseClassB::ClassMemberFuncTest called from " <<
        //        rpcFromNetwork->GetLastSenderAddress().ToString() << std::endl;
    }
    
    RakNet::RakString rs1, rs2;
    bs1->Read(rs1);
    bs2.Read(rs2);
    //std::cout << rs1.C_String() << std::endl;
    //std::cout << rs2.C_String() << "\033[1;32m    " << RakNet::GetTimeUS() << "     \033[0m" << std::endl;
}*/

/*
 * ClassC and ClassD derive from NetworkIDObject, so they cannot be passed as
 * references. A pointer is required to do the object lookup.
 */
void ClassC::ClassMemberFunc(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                    ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                    RakNet::RPC3 *rpcFromNetwork) {
    std::cout << "ClassC::ClassMemberFunc" << std::endl;
    BaseClassB::ClassMemberFunc(a1, a2, c1, d1, bs1, bs2, rpcFromNetwork);
}
void ClassC::ClassMemberFuncTest(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                    ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                    uint64_t callNumber, uint64_t callTime,
                    RakNet::RPC3 *rpcFromNetwork) {
    
    if (testCalls.find(callNumber) != testCalls.end())
        std::cout << "ClassC::ClassMemberFuncTest already added: " << callNumber << std::endl;
    
    std::pair<int, uint64_t> p(callNumber, RakNet::GetTimeUS() - callTime);
    testCalls.insert(p);
}

void ClassC::ClassMemberFunc2(RakNet::RPC3 *rpcFromNetwork) {
    std::cout << "ClassC::ClassMemberFunc2" << std::endl;
}
