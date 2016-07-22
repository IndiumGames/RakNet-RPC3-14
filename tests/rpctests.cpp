/*
 *  Original Copyright (c) 2014, Oculus VR, Inc.
 *  Modified Copyright (c) 2016, Indium Games
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "RPC3.h"
#include "RakPeerInterface.h"

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>

#include "Kbhit.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "StringCompressor.h"
#include "RakSleep.h"
#include "NetworkIDObject.h"
#include "NetworkIDManager.h"
#include "GetTime.h"
#include "Gets.h"

#include "democlasses.h"
#include "democfunctions.h"

//#define COUT std::cout.rdbuf(stdOut); std::cout
//#define ENDL std::endl; std::cout.rdbuf(trash.rdbuf())

#define COUT std::cout
#define ENDL std::endl

int main(int argc, char *argv[]) {
    std::ofstream trash("tests/bin/garbage.txt");
    std::streambuf* stdOut = std::cout.rdbuf();
    
    COUT << "Performance test for the RPC314 plugin." << ENDL;
    
    bool isServer = false;
    bool testAll = false;
    bool testSignal = false;
    bool testCallC = false;
    bool testCallCpp = false;
    bool useBoost = false;
    unsigned int clientCount = 1;
    unsigned int callCount = 1;
    
    int opt;
    while (1) {
        static struct option long_options[] = {
            {"use-boost",   no_argument, 0, 'b'},
            {"test",    required_argument, 0, 't'},
            {"client-count",    required_argument, 0, 'c'},
            {"call-count",    required_argument, 0, 'r'},
            {0, 0, 0, 0}
        };
        
        // getopt_long stores the option index here.
        int option_index = 0;
        
        opt = getopt_long(argc, argv, "bc:t:", long_options, &option_index);
        
        // Detect the end of the options.
        if (opt == -1) {
            break;
        }
    
        switch (opt) {
            case 0:
                break;
            case 'b':
                useBoost = true;
                break;
            case 'c':
                clientCount = atoi(optarg);
                break;
            case 'r':
                callCount = atoi(optarg);
                break;
            case 't': {
                if (optarg == "all") {
                    testAll = true;
                }
                else if (optarg == "signal") {
                    testSignal = true;
                }
                else if (optarg == "c") {
                    testCallC = true;
                }
                else if (optarg == "cpp") {
                    testCallCpp = true;
                }
                else {
                    testAll = true;
                }
            } break;
            case '?':
                return 1;
            default:
                testAll = true;
                break;
        }
    }

    unsigned int peerCount = clientCount + 1;
    std::vector<RakNet::RakPeerInterface *> rakPeers;
    //rakPeers.assign(peerCount, RakNet::RakPeerInterface::GetInstance());
    
    std::vector<RakNet::RPC3 *> rpcPlugins;
    //rpcPlugins.assign(peerCount, new RakNet::RPC3());
    
    std::vector<RakNet::NetworkIDManager> networkIdManagers;
    networkIdManagers.assign(peerCount, RakNet::NetworkIDManager());
    
    std::vector<BaseClassA> a;
    a.assign(peerCount, BaseClassA());
    
    std::vector<BaseClassB> b;
    b.assign(peerCount, BaseClassB());
    
    std::vector<ClassC> c;
    c.assign(peerCount, ClassC());
    
    std::vector<ClassD> d;
    d.assign(peerCount, ClassD());
    
    std::thread serverThread;
    
    for (std::size_t i = 0; i < peerCount; i++) {
        rakPeers.push_back(RakNet::RakPeerInterface::GetInstance());
        rpcPlugins.push_back(new RakNet::RPC3());
        
        if (i == 0) {
            RakNet::SocketDescriptor socketDescriptorServer(60000, 0);
            // Only IPV4 supports broadcast on 255.255.255.255
            socketDescriptorServer.socketFamily = AF_INET;
            
            rakPeers[i]->Startup(clientCount, &socketDescriptorServer, 1);
            rakPeers[i]->SetMaximumIncomingConnections(clientCount);
            
            COUT << "Server started." << ENDL;
        }
        else {
            RakNet::SocketDescriptor socketDescriptorClient(60000 + i, 0);
            // Only IPV4 supports broadcast on 255.255.255.255
            socketDescriptorClient.socketFamily = AF_INET;
            
            rakPeers[i]->Startup(1, &socketDescriptorClient, 1);

            // Send out a LAN broadcast to find other instances on the same computer.
            rakPeers[i]->Ping("255.255.255.255", 60000, true, 0);

            COUT << "Client #" << i << " started." << ENDL;
        }
        
        rakPeers[i]->AttachPlugin(rpcPlugins[i]);
        rpcPlugins[i]->SetNetworkIDManager(&networkIdManagers[i]);
        
        c[i].SetNetworkIDManager(&networkIdManagers[i]);
        d[i].SetNetworkIDManager(&networkIdManagers[i]);
        c[i].SetNetworkID(0);
        d[i].SetNetworkID(1);
    
        // Register a regular C function.
        RPC3_REGISTER_FUNCTION(rpcPlugins[i], CFuncTest);
        
        // Register C++ class member functions.
        RPC3_REGISTER_FUNCTION(rpcPlugins[i], &ClassC::ClassMemberFuncTest);
        
        // All equivalent local and remote slots are called when a signal is sent.
        rpcPlugins[i]->RegisterSlot("TestSlotTest", &ClassC::TestSlotTest, c[i].GetNetworkID(), 0);
        rpcPlugins[i]->RegisterSlot("TestSlotTest", &ClassD::TestSlotTest, d[i].GetNetworkID(), 0);
    }
    
    COUT << "Clients will automatically connect to running server." << ENDL;
    COUT << "Running " << callCount << " rounds." << ENDL;

    // The original version of RPC3 requires these pointers as lvalue.
    BaseClassA *serverAPtr = &a[0];
    BaseClassB *serverBPtr = &b[0];
    ClassC *serverCPtr = &c[0];
    ClassD *serverDPtr = &d[0];
    RakNet::RPC3 *emptyRpc = 0;
    
    RakNet::Packet *packet;
    while (1) {
        for (std::size_t i = 0; i < peerCount; i++) {
            for (packet=rakPeers[i]->Receive(); packet;
                    rakPeers[i]->DeallocatePacket(packet), packet=rakPeers[i]->Receive()) {
                switch (packet->data[0]) {
                    case ID_DISCONNECTION_NOTIFICATION:
                        COUT << "ID_DISCONNECTION_NOTIFICATION peer index:" << i << ENDL;
                        break;
                    case ID_ALREADY_CONNECTED:
                        COUT << "ID_ALREADY_CONNECTED peer index:" << i << ENDL;
                        break;
                    case ID_CONNECTION_ATTEMPT_FAILED:
                        COUT << "Connection attempt failed peer index:" << i << ENDL;
                        break;
                    case ID_NO_FREE_INCOMING_CONNECTIONS:
                        COUT << "ID_NO_FREE_INCOMING_CONNECTIONS peer index:" << i << ENDL;
                        break;
                    case ID_UNCONNECTED_PONG:
                        // Found the server
                        rakPeers[i]->Connect(packet->systemAddress.ToString(false),
                                        packet->systemAddress.GetPort(), 0, 0, 0);
                        break;
                    case ID_CONNECTION_REQUEST_ACCEPTED:
                        // This tells the client they have connected
                        COUT << "ID_CONNECTION_REQUEST_ACCEPTED peer index:" << i << ENDL;
                        break;
                    case ID_NEW_INCOMING_CONNECTION: {
                        clientCount--;
                        COUT << "ID_NEW_INCOMING_CONNECTION peer index:" << i << "  clients to wait:" << clientCount << ENDL;
                        if (!clientCount) {
                            // If all clients connected.
                            
                            serverThread = std::thread([&, peerCount, callCount] () {
                                COUT << "\033[0;31m     Starting server thread to do RPC calls, timestamp: " << RakNet::GetTimeUS() << "     \033[0m" << ENDL;
                                unsigned int count = peerCount - 1;
                                while (count) {
                                    for (size_t i = 0; i < peerCount - 1; i++) {
                                        COUT << "\033[0;35m    CCCCCCCCCCCCCCCC " << count << "." << i << "     \033[0m" << ENDL;
                                        RakNet::BitStream testBitStream1, testBitStream2;
                                        
                                        std::string strCpp("\033[1;32m     CPP Function call timestamp: " + std::to_string(RakNet::GetTimeUS()) + std::string("     \033[0m"));
                                        testBitStream1.Write(strCpp.c_str());
                                        testBitStream2.Write("\033[1;32m     CPP Remote call timestamp:      \033[0m");
                                        RakNet::BitStream *testBitStream1Ptr = &testBitStream1;
                                        
                                        c[0].ClassMemberFuncTest(serverAPtr, a[0], serverCPtr, serverDPtr, testBitStream1Ptr,
                                                                                testBitStream2, emptyRpc);
                                        rpcPlugins[0]->CallCPP("&ClassC::ClassMemberFuncTest", c[0].GetNetworkID(),
                                                                serverAPtr, a[0], serverCPtr, RakNet::_RPC3::Deref(serverDPtr),
                                                                testBitStream1Ptr, testBitStream2, emptyRpc);
                                        
                                        
                                        std::string strC("\033[1;32m     C Function call timestamp: " + std::to_string(RakNet::GetTimeUS()) + std::string("     \033[0m"));
                                        RakNet::RakString rs(strC.c_str());
                                        int intArray[10];
                                        for (int j = 0 ; j < sizeof(intArray)/sizeof(int) ; j++) {
                                            intArray[j] = j;
                                        }
                                        const char *str = "\033[1;32m     C Remote call timestamp:      \033[0m";
                                        
                                        CFuncTest(rs, intArray, serverCPtr, str, emptyRpc);
                                        rpcPlugins[0]->CallC("CFuncTest", rs,
                                                RakNet::_RPC3::PtrToArray(10, intArray), serverCPtr, str, emptyRpc);
                                                
                                        
                                        COUT << "\033[1;33m     TestSlot signal sent timestamp: " + std::to_string(RakNet::GetTimeUS()) + std::string("     \033[0m") << ENDL;
                                        rpcPlugins[0]->Signal("TestSlotTest");
                                    }
                                    count--;
                                    //std::this_thread::sleep_for(std::chrono::milliseconds(16));
                                }
                                
                                COUT << "\033[0;31m     All RPC calls sent, timestamp: "  << RakNet::GetTimeUS() << "     \033[0m" << ENDL;
                            });
                            serverThread.join();
                            COUT << "\033[0;35m     Server thread started     \033[0m" << ENDL;
                        }
                        break;
                    }
                    case ID_RPC_REMOTE_ERROR: {
                        // Recipient system returned an error
                        switch (packet->data[1]) {
                            case RakNet::RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE:
                                COUT << "RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE peer index:" << i << ENDL;
                                break;
                            case RakNet::RPC_ERROR_OBJECT_DOES_NOT_EXIST:
                                COUT << "RPC_ERROR_OBJECT_DOES_NOT_EXIST peer index:" << i << ENDL;
                                break;
                            case RakNet::RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE:
                                COUT << "RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE peer index:" << i << ENDL;
                                break;
                            case RakNet::RPC_ERROR_FUNCTION_NOT_REGISTERED:
                                COUT << "RPC_ERROR_FUNCTION_NOT_REGISTERED peer index:" << i << ENDL;
                                break;
                            case RakNet::RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED:
                                COUT << "RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED peer index:" << i << ENDL;
                                break;
                            case RakNet::RPC_ERROR_CALLING_CPP_AS_C:
                                COUT << "RPC_ERROR_CALLING_CPP_AS_C peer index:" << i << ENDL;
                                break;
                            case RakNet::RPC_ERROR_CALLING_C_AS_CPP:
                                COUT << "RPC_ERROR_CALLING_C_AS_CPP peer index:" << i << ENDL;
                                break;
                        }
                        
                        COUT << "Function: " << packet->data + 2 << "     peer index:" << i << ENDL;
                        break;
                    }
                    default:
                        //COUT << "Unknown id received: " << packet->data[0] << ENDL;
                        break;
                }
            }
        }

        RakSleep(0);
    }

    for (std::size_t i = 0; i < peerCount; i++) {
        delete rpcPlugins[i];
        rakPeers[i]->Shutdown(0, 0);
        RakNet::RakPeerInterface::DestroyInstance(rakPeers[i]);
    }
    rpcPlugins.clear();
    trash.close();
    
    delete emptyRpc;
    
    return 1;
}
