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
        printf("ClassC::TestSlot\n");
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
        printf("ClassD::TestSlot\n");
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
        printf("\nBaseClassB::ClassMemberFunc called locally\n");
    }
    else {
        printf("\nBaseClassB::ClassMemberFunc called from %s\n",
                            rpcFromNetwork->GetLastSenderAddress().ToString());
    }
    
    printf("a1=%i a2=%i c1=%i\n", a1->a, a2.a, c1->c);
    printf("d1::Verify=%i\n", d1->Verify());
    
    RakNet::RakString rs1, rs2;
    bs1->Read(rs1);
    bs2.Read(rs2);
    printf("rs1=%s\n", rs1.C_String());
    printf("rs2=%s\n", rs2.C_String());
}

/*
 * ClassC and ClassD derive from NetworkIDObject, so they cannot be passed as
 * references. A pointer is required to do the object lookup.
 */
void ClassC::ClassMemberFunc(BaseClassA *a1, BaseClassA &a2, ClassC *c1,
                    ClassD *d1, RakNet::BitStream *bs1, RakNet::BitStream &bs2,
                    RakNet::RPC3 *rpcFromNetwork) {
    printf("\nClassC::ClassMemberFunc\n");
    BaseClassB::ClassMemberFunc(a1, a2, c1, d1, bs1, bs2, rpcFromNetwork);
}

void ClassC::ClassMemberFunc2(RakNet::RPC3 *rpcFromNetwork) {
    printf("\nClassC::ClassMemberFunc2\n");
}


void CFunc(RakNet::RakString rakString, int intArray[10], ClassC *c1,
                            const char *str, RakNet::RPC3 *rpcFromNetwork) {
    if (rpcFromNetwork == 0) {
        printf("\nCFunc called locally\n");
    }
    else {
        printf("\nCFunc called from %s\n",
                            rpcFromNetwork->GetLastSenderAddress().ToString());
    }
    
    printf("rakString=%s\n", rakString.C_String());
    
    printf("intArray = ");
    for (int i = 0; i < 10; i++) {
        printf("%i ", intArray[i]);
    }
    printf("\n");
    
    printf("c1=%i\n", c1->c);
    printf("str=%s\n", str);
}

int main(int argc, char *argv[]) {
    printf("Demonstration of the RPC3 plugin.\n");
    
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
        RakNet::SocketDescriptor socketDescriptor(50000, 0);
        // Only IPV4 supports broadcast on 255.255.255.255
        socketDescriptor.socketFamily = AF_INET;
        
        rakPeer->Startup(10, &socketDescriptor, 1);
        rakPeer->SetMaximumIncomingConnections(10);
        
        printf("Server started.\n");
    }
    else {
        RakNet::SocketDescriptor socketDescriptor(0, 0);
        // Only IPV4 supports broadcast on 255.255.255.255
        socketDescriptor.socketFamily = AF_INET;
        
        rakPeer->Startup(1, &socketDescriptor, 1);

        // Send out a LAN broadcast to find other instances on the same computer
        rakPeer->Ping("255.255.255.255", 50000, true, 0);

        printf("Client started. Will automatically connect to running servers.\n");
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
                    printf("ID_DISCONNECTION_NOTIFICATION\n");
                    break;
                case ID_ALREADY_CONNECTED:
                    printf("ID_ALREADY_CONNECTED\n");
                    break;
                case ID_CONNECTION_ATTEMPT_FAILED:
                    printf("Connection attempt failed\n");
                    break;
                case ID_NO_FREE_INCOMING_CONNECTIONS:
                    printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
                    break;
                case ID_UNCONNECTED_PONG:
                    // Found the server
                    rakPeer->Connect(packet->systemAddress.ToString(false),
                                    packet->systemAddress.GetPort(), 0, 0, 0);
                    break;
                case ID_CONNECTION_REQUEST_ACCEPTED:
                    // This tells the client they have connected
                    printf("ID_CONNECTION_REQUEST_ACCEPTED\n");
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
                            printf("RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE\n");
                            break;
                        case RakNet::RPC_ERROR_OBJECT_DOES_NOT_EXIST:
                            printf("RPC_ERROR_OBJECT_DOES_NOT_EXIST\n");
                            break;
                        case RakNet::RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE:
                            printf("RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE\n");
                            break;
                        case RakNet::RPC_ERROR_FUNCTION_NOT_REGISTERED:
                            printf("RPC_ERROR_FUNCTION_NOT_REGISTERED\n");
                            break;
                        case RakNet::RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED:
                            printf("RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED\n");
                            break;
                        case RakNet::RPC_ERROR_CALLING_CPP_AS_C:
                            printf("RPC_ERROR_CALLING_CPP_AS_C\n");
                            break;
                        case RakNet::RPC_ERROR_CALLING_C_AS_CPP:
                            printf("RPC_ERROR_CALLING_C_AS_CPP\n");
                            break;
                    }
                    printf("Function: %s", packet->data + 2);
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
