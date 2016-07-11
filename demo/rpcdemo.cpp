/*
 *  Original Copyright (c) 2014, Oculus VR, Inc.
 *  Modified Copyright (c) 2016, Indium Games.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "RPC3.h"
#include "RakPeerInterface.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory>
#include <iostream>

#include "Kbhit.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "StringCompressor.h"
#include "RakSleep.h"
#include "NetworkIDObject.h"
#include "NetworkIDManager.h"
#include "GetTime.h"
#include "Gets.h"



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
                rpcFromNetwork->GetLastSenderAddress().ToString() << std::endl;
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

void ClassC::ClassMemberFunc2(RakNet::RPC3 *rpcFromNetwork) {
    std::cout << "ClassC::ClassMemberFunc2" << std::endl;
}


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

int main(int argc, char *argv[]) {
    std::cout << "Demonstration of the RPC3 plugin." << std::endl;
    
    bool isServer = false;
    int opt;
    
    while ((opt = getopt (argc, argv, "sc")) != -1) {
        switch (opt) {
            case 's':
                isServer = true;
                break;
            case 'c':
                isServer = false;
                break;
            case '?':
                if (isprint (optopt)) {
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                }
                else {
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                }
                
                return 1;
            default:
                break;
        }
    }

    RakNet::RakPeerInterface *rakPeer = RakNet::RakPeerInterface::GetInstance();
    
    if (isServer) {
        RakNet::SocketDescriptor socketDescriptor(60000, 0);
        // Only IPV4 supports broadcast on 255.255.255.255
        socketDescriptor.socketFamily = AF_INET;
        
        rakPeer->Startup(10, &socketDescriptor, 1);
        rakPeer->SetMaximumIncomingConnections(10);
        
        std::cout << "Server started." << std::endl;
    }
    else {
        RakNet::SocketDescriptor socketDescriptor(60001, 0);
        // Only IPV4 supports broadcast on 255.255.255.255
        socketDescriptor.socketFamily = AF_INET;
        
        rakPeer->Startup(1, &socketDescriptor, 1);

        // Send out a LAN broadcast to find other instances on the same computer
        rakPeer->Ping("255.255.255.255", 60000, true, 0);

        std::cout << "Client started. Will automatically connect to running servers." << std::endl;
    }
    
    // Add RPC3 plugin.
    std::unique_ptr<RakNet::RPC3> rpc3;
    rpc3.reset(new RakNet::RPC3);
    rakPeer->AttachPlugin(rpc3.get());
    
    RakNet::NetworkIDManager networkIDManager;
    rpc3->SetNetworkIDManager(&networkIDManager);
    
    BaseClassA a;
    BaseClassB b;
    ClassC c;
    ClassD d;

    c.SetNetworkIDManager(&networkIDManager);
    d.SetNetworkIDManager(&networkIDManager);
    c.SetNetworkID(0);
    d.SetNetworkID(1);

    // Register a regular C function.
    RPC3_REGISTER_FUNCTION(rpc3, CFunc);
    
    // Register C++ class member functions.
    RPC3_REGISTER_FUNCTION(rpc3, &ClassC::ClassMemberFunc);
    RPC3_REGISTER_FUNCTION(rpc3, &ClassC::ClassMemberFunc2);

    // All equivalent local and remote slots are called when a signal is sent.
    rpc3->RegisterSlot("TestSlot", &ClassC::TestSlot, c.GetNetworkID(), 0);
    rpc3->RegisterSlot("TestSlot", &ClassD::TestSlot, d.GetNetworkID(), 0);


    RakNet::TimeMS stage2 = 0;
    RakNet::Packet *packet;
    while (1) {
        for (packet=rakPeer->Receive(); packet;
                rakPeer->DeallocatePacket(packet), packet=rakPeer->Receive()) {
            switch (packet->data[0]) {
                case ID_DISCONNECTION_NOTIFICATION:
                    std::cout << "ID_DISCONNECTION_NOTIFICATION" << std::endl;
                    break;
                case ID_ALREADY_CONNECTED:
                    std::cout << "ID_ALREADY_CONNECTED" << std::endl;
                    break;
                case ID_CONNECTION_ATTEMPT_FAILED:
                    std::cout << "Connection attempt failed" << std::endl;
                    break;
                case ID_NO_FREE_INCOMING_CONNECTIONS:
                    std::cout << "ID_NO_FREE_INCOMING_CONNECTIONS" << std::endl;
                    break;
                case ID_UNCONNECTED_PONG:
                    // Found the server
                    rakPeer->Connect(packet->systemAddress.ToString(false),
                                    packet->systemAddress.GetPort(), 0, 0, 0);
                    break;
                case ID_CONNECTION_REQUEST_ACCEPTED:
                    // This tells the client they have connected
                    std::cout << "ID_CONNECTION_REQUEST_ACCEPTED" << std::endl;
                    break;
                case ID_NEW_INCOMING_CONNECTION: {
                    RakNet::BitStream testBitStream1, testBitStream2;
                    testBitStream1.Write("Hello World 1");
                    testBitStream2.Write("Hello World 2");
                    
                    c.ClassMemberFunc(&a, a, &c, &d, &testBitStream1,
                                                            testBitStream2, 0);
                    // By default, pointers to objects that derive from
                    // NetworkIDObject (ClassC c and ClassD d), will only
                    // transmit the NetworkID of the object.
                    //
                    // You can use RakNet::_RPC3::Deref(variable) to dereference
                    // the pointer and serialize the object itself.
                    //
                    // In this demo, parameters c and d demonstrates these
                    // behaviors:
                    //     * c will only transmit c->GetNetworkID()
                    //         (default behavior)
                    //     * d will transmit d->GetNetworkID() and also
                    //         bitStream << (*d) (contents of the pointer)
                    rpc3->CallCPP("&ClassC::ClassMemberFunc", c.GetNetworkID(),
                                            &a, a, &c, RakNet::_RPC3::Deref(&d),
                                            &testBitStream1, testBitStream2, 0);
                    
                    
                    c.ClassMemberFunc2(0);
                    rpc3->CallCPP("&ClassC::ClassMemberFunc2", c.GetNetworkID(), 0);
                        
                    
                    RakNet::RakString rs("RakString test");
                    int intArray[10];
                    for (int i = 0 ; i < sizeof(intArray)/sizeof(int) ; i++) {
                        intArray[i] = i;
                    }
                    const char *str = "Test string";
                    
                    CFunc(rs, intArray, &c, str, 0);
                
                    // The parameter "int intArray[10]" is actually a pointer
                    // due to the design of C and C++. The RakNet::_RPC3::PtrToArray()
                    // function will tell the RPC3 system that this is actually
                    // an array of n elements. Each element will be endian
                    // swapped appropriately.
                    rpc3->CallC("CFunc", rs,
                            RakNet::_RPC3::PtrToArray(10, intArray), &c, str, 0);
                    
                    stage2 = RakNet::GetTimeMS() + 1000;
                    break;
                }
                case ID_RPC_REMOTE_ERROR: {
                    // Recipient system returned an error
                    switch (packet->data[1]) {
                        case RakNet::RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE:
                            std::cout << "RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE" << std::endl;
                            break;
                        case RakNet::RPC_ERROR_OBJECT_DOES_NOT_EXIST:
                            std::cout << "RPC_ERROR_OBJECT_DOES_NOT_EXIST" << std::endl;
                            break;
                        case RakNet::RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE:
                            std::cout << "RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE" << std::endl;
                            break;
                        case RakNet::RPC_ERROR_FUNCTION_NOT_REGISTERED:
                            std::cout << "RPC_ERROR_FUNCTION_NOT_REGISTERED" << std::endl;
                            break;
                        case RakNet::RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED:
                            std::cout << "RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED" << std::endl;
                            break;
                        case RakNet::RPC_ERROR_CALLING_CPP_AS_C:
                            std::cout << "RPC_ERROR_CALLING_CPP_AS_C" << std::endl;
                            break;
                        case RakNet::RPC_ERROR_CALLING_C_AS_CPP:
                            std::cout << "RPC_ERROR_CALLING_C_AS_CPP" << std::endl;
                            break;
                    }
                    std::cout << "Function: " << packet->data + 2 << std::endl;
                }
            }
        }

        if (stage2 && stage2 < RakNet::GetTimeMS()) {
            stage2 = 0;

            RakNet::BitStream testBitStream1, testBitStream2;
            testBitStream1.Write("Hello World 1 (2)");
            testBitStream2.Write("Hello World 2 (2)");
            c.ClassMemberFunc(&a, a, &c, &d, &testBitStream1, testBitStream2, 0);
            RakNet::RakString rs("RakString test (2)");
            int intArray[10];
            for (int i = 0 ; i < sizeof(intArray)/sizeof(int) ; i++) {
                intArray[i] = i;
            }
            
            const char *str = "Test string (2)";
            CFunc(rs, intArray, &c, str, 0);
            rpc3->CallC("CFunc", rs,
                            RakNet::_RPC3::PtrToArray(10, intArray), &c, str, 0);
            
            rpc3->Signal("TestSlot");
        }

        RakSleep(0);
    }

    rakPeer->Shutdown(100, 0);
    RakNet::RakPeerInterface::DestroyInstance(rakPeer);

    return 1;
}
