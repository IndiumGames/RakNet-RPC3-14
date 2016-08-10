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
#include <mutex>
#include <numeric>

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

/*
 * All time values are in microseconds.
 */
struct TestValues {
    TestValues() : allReady(false) {}
    
    void PrintTestSummary() {
        uint64_t mseconds = 0;
        uint64_t useconds = 0;
        std::cout << "Summary for the performance test:\n" << std::endl;
        
        mseconds = programRunTime / 1000;
        std::cout << "Time used to run the test program: "
                  << mseconds << " milliseconds\n" << std::endl;
        
        mseconds = callFunctionsTime / 1000;
        std::cout << "Time used to call all functions: "
                  << mseconds << " milliseconds\n" << std::endl;
        
        useconds = std::accumulate(
            cClassSlotValues.begin(), cClassSlotValues.end(), 0,
            [] (int value, const std::map<int, uint64_t>::value_type& p) {
                return value + p.second;
            }) / cClassSlotValues.size();
        std::cout << "Average for TestSlotTest call by RPC: "
                  << useconds << " microseconds\n" << std::endl;
        
        useconds = std::accumulate(
            cClassValues.begin(), cClassValues.end(), 0,
            [] (int value, const std::map<int, uint64_t>::value_type& p) {
                return value + p.second;
            }) / cClassValues.size();
        std::cout << "Average for ClassC::ClassMemberFuncTest call by RPC: "
                  << useconds << " microseconds\n" << std::endl;
        
        useconds = std::accumulate(
            cFuncValues.begin(), cFuncValues.end(), 0,
            [] (int value, const std::map<int, uint64_t>::value_type& p) {
                return value + p.second;
            }) / cFuncValues.size();
        std::cout << "Average for CFuncTest call by RPC: "
                  << useconds << " microseconds" << std::endl;
    }
    
    void AppendCClassSlotValues(std::map<int, uint64_t> values) {
        cClassSlotValues.insert(values.begin(), values.end());
    }
    
    void AppendCClassValues(std::map<int, uint64_t> values) {
        cClassValues.insert(values.begin(), values.end());
    }
    
    void AppendCFuncValues(std::map<int, uint64_t> values) {
        cFuncValues.insert(values.begin(), values.end());
    }
    
    uint64_t callFunctionsTime;
    uint64_t programRunTime;
    
    std::map<int, uint64_t> cClassSlotValues;
    std::map<int, uint64_t> cClassValues;
    std::map<int, uint64_t> cFuncValues;
    
    bool allReady;
};

void serverThread(std::recursive_mutex *mutex_, TestValues *testValues,
        BaseClassA *serverAPtr, ClassC *serverCPtr, ClassD *serverDPtr,
        RakNet::RPC3 *serverRpc, unsigned int peerCount,
        unsigned int callCount) {
    
    RakNet::RPC3 *emptyRpc = 0;
    unsigned int count = callCount;
    uint64_t startTime = RakNet::GetTimeUS();
    while (count) {
        for (size_t i = 1; i < peerCount; i++) {
            uint64_t callNumber = i * callCount + count;
        
            RakNet::BitStream testBitStream1, testBitStream2;
            testBitStream1.Write("CPP TEST STRING");
            testBitStream2.Write("CPP Remote call timestamp test string.");
            RakNet::BitStream *testBitStream1Ptr = &testBitStream1;
            
            uint64_t callTime = RakNet::GetTimeUS();
            serverCPtr->ClassMemberFuncTest(
                serverAPtr, *serverAPtr, serverCPtr, serverDPtr,
                testBitStream1Ptr, testBitStream2, callNumber, callTime, emptyRpc
            );
            serverRpc->CallCPP(
                "&ClassC::ClassMemberFuncTest", serverCPtr->GetNetworkID(),
                serverAPtr, *serverAPtr, serverCPtr, serverDPtr,
                testBitStream1Ptr, testBitStream2, callNumber, callTime, emptyRpc
            );
            
            RakNet::RakString rs("C Function call test string.");
            const char *str = "C Remote call char * test.";
            
            callTime = RakNet::GetTimeUS();
            CFuncTest(rs, serverCPtr, str, callNumber, callTime, emptyRpc);
            serverRpc->CallC("CFuncTest",
                        rs, serverCPtr, str, callNumber, callTime, emptyRpc);
            
            callTime = RakNet::GetTimeUS();
            serverRpc->Signal("TestSlotTest", callNumber, callTime);
            
        }
        count--;
        RakSleep(16);
    }
    
    mutex_->lock();
    testValues->callFunctionsTime = RakNet::GetTimeUS() - startTime;
    mutex_->unlock();
    
    delete emptyRpc;
}

int main(int argc, char *argv[]) {
    
    std::cout << "Performance test for the RPC314 plugin." << std::endl;
    
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

    TestValues testValues;
    testValues.allReady = false;
    
    uint64_t programStartTime = RakNet::GetTimeUS();
    
    std::unique_ptr<std::recursive_mutex> mutex_(new std::recursive_mutex());

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
    
    for (std::size_t i = 0; i < peerCount; i++) {
        rakPeers.push_back(RakNet::RakPeerInterface::GetInstance());
        rpcPlugins.push_back(new RakNet::RPC3());
        
        if (i == 0) {
            RakNet::SocketDescriptor socketDescriptorServer(60000, 0);
            // Only IPV4 supports broadcast on 255.255.255.255
            socketDescriptorServer.socketFamily = AF_INET;
            
            rakPeers[i]->Startup(clientCount, &socketDescriptorServer, 1);
            rakPeers[i]->SetMaximumIncomingConnections(clientCount);
            
            std::cout << "Server started." << std::endl;
        }
        else {
            RakNet::SocketDescriptor socketDescriptorClient(60000 + i, 0);
            // Only IPV4 supports broadcast on 255.255.255.255
            socketDescriptorClient.socketFamily = AF_INET;
            
            rakPeers[i]->Startup(1, &socketDescriptorClient, 1);

            // Send out a LAN broadcast to find the server on the same computer.
            rakPeers[i]->Ping("255.255.255.255", 60000, true, 0);

            std::cout << "Client #" << i << " started." << std::endl;
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
        rpcPlugins[i]->RegisterSlot(
                "TestSlotTest", &ClassC::TestSlotTest, c[i].GetNetworkID(), 0);
        rpcPlugins[i]->RegisterSlot(
                "TestSlotTest", &ClassD::TestSlotTest, d[i].GetNetworkID(), 0);
    }
    
    std::cout << "Clients will automatically connect to running server." << std::endl;
    std::cout << "Running " << callCount << " rounds." << std::endl;

    
    RakNet::Packet *packet;
    std::thread serverT;
    bool allready = true;
    while (!testValues.allReady) {
        for (std::size_t i = 0; i < peerCount; i++) {
            for (packet=rakPeers[i]->Receive(); packet;
                    rakPeers[i]->DeallocatePacket(packet), packet=rakPeers[i]->Receive()) {
                switch (packet->data[0]) {
                    case ID_DISCONNECTION_NOTIFICATION:
                        std::cout << "ID_DISCONNECTION_NOTIFICATION peer index:"
                                  << i << std::endl;
                        break;
                    case ID_ALREADY_CONNECTED:
                        std::cout << "ID_ALREADY_CONNECTED peer index:" << i
                                  << std::endl;
                        break;
                    case ID_CONNECTION_ATTEMPT_FAILED:
                        std::cout << "Connection attempt failed peer index:"
                                  << i << std::endl;
                        break;
                    case ID_NO_FREE_INCOMING_CONNECTIONS:
                        std::cout << "ID_NO_FREE_INCOMING_CONNECTIONS peer index:"
                                  << i << std::endl;
                        break;
                    case ID_UNCONNECTED_PONG:
                        // Found the server
                        rakPeers[i]->Connect(
                            packet->systemAddress.ToString(false),
                            packet->systemAddress.GetPort(), 0, 0, 0
                        );
                        break;
                    case ID_CONNECTION_REQUEST_ACCEPTED:
                        // This tells the client they have connected
                        std::cout << "ID_CONNECTION_REQUEST_ACCEPTED peer index:"
                                  << i << std::endl;
                        break;
                    case ID_NEW_INCOMING_CONNECTION: {
                        clientCount--;
                        std::cout << "ID_NEW_INCOMING_CONNECTION peer index:"
                                  << i << "  clients to wait:" << clientCount
                                  << std::endl;
                                  
                        if (!clientCount) {
                            // If all clients are connected.
                            serverT = std::thread(
                                serverThread, mutex_.get(), &testValues,
                                &a[0], &c[0], &d[0], rpcPlugins[0],
                                peerCount, callCount
                            );
                            
                            serverT.detach();
                        }
                        break;
                    }
                    case ID_RPC_REMOTE_ERROR: {
                        // Recipient system returned an error
                        switch (packet->data[1]) {
                            case RakNet::RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE:
                                std::cout
                                    << "RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE"
                                    << " peer index:" << i << std::endl;
                                break;
                            case RakNet::RPC_ERROR_OBJECT_DOES_NOT_EXIST:
                                std::cout
                                    << "RPC_ERROR_OBJECT_DOES_NOT_EXIST peer index:"
                                    << i << std::endl;
                                break;
                            case RakNet::RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE:
                                std::cout
                                    << "RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE "
                                    << " peer index:" << i << std::endl;
                                break;
                            case RakNet::RPC_ERROR_FUNCTION_NOT_REGISTERED:
                                std::cout
                                    << "RPC_ERROR_FUNCTION_NOT_REGISTERED"
                                    << " peer index:" << i << std::endl;
                                break;
                            case RakNet::RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED:
                                std::cout
                                    << "RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED"
                                    << " peer index:" << i << std::endl;
                                break;
                            case RakNet::RPC_ERROR_CALLING_CPP_AS_C:
                                std::cout
                                    << "RPC_ERROR_CALLING_CPP_AS_C peer index:"
                                    << i << std::endl;
                                break;
                            case RakNet::RPC_ERROR_CALLING_C_AS_CPP:
                                std::cout
                                    << "RPC_ERROR_CALLING_C_AS_CPP peer index:"
                                    << i << std::endl;
                                break;
                        }
                        
                        std::cout << "Function: " << packet->data + 2
                                  << "     peer index:" << i << std::endl;
                        break;
                    }
                    default:
                        //std::cout << "Unknown id received: "
                        //            << packet->data[0] << std::endl;
                        break;
                }
            }
            
            //std::cout << "before" << std::endl;
            uint64_t allCount = (peerCount-1) * callCount;
            if (c[i].testCalls.size() < allCount ||
                c[i].testSlots.size() < allCount ||
                d[i].testSlots.size() < allCount ||
                cFuncTestCalls.size() < allCount) {
                /*std::cout << "in "
                      << "\n\tc[" << i << "].testCalls.size(): "
                      << c[i].testCalls.size()
                      << "\n\tc[" << i << "].testSlots.size(): "
                      << c[i].testSlots.size()
                      << "\n\td[" << i << "].testSlots.size(): "
                      << d[i].testSlots.size()
                      << "\n\tcFuncTestCalls.size(): "
                      << cFuncTestCalls.size()
                      << "\n\tallCount:              "
                      << allCount << std::endl;*/
                allready = false;
            }
        }

        RakSleep(0);
        
        if (allready) {
            mutex_->lock();
            testValues.allReady = true;
            mutex_->unlock();
        }
        
        allready = true;
    }

    testValues.programRunTime = RakNet::GetTimeUS() - programStartTime;
    testValues.AppendCFuncValues(cFuncTestCalls);
    
    for (size_t i = 1; i < peerCount; i++) {
        testValues.AppendCClassValues(c[i].testCalls);
        
        testValues.AppendCClassSlotValues(c[i].testSlots);
        testValues.AppendCClassSlotValues(d[i].testSlots);
    }
    
    testValues.PrintTestSummary();

    for (std::size_t i = 0; i < peerCount; i++) {
        delete rpcPlugins[i];
        rakPeers[i]->Shutdown(0, 0);
        RakNet::RakPeerInterface::DestroyInstance(rakPeers[i]);
    }
    rpcPlugins.clear();
    
    return 1;
}
