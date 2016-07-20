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
#include "RakMemoryOverride.h"
#include "RakAssert.h"
#include "StringCompressor.h"
#include "BitStream.h"
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "NetworkIDManager.h"
#include <stdlib.h>

using namespace RakNet;

int RakNet::RPC3::LocalSlotObjectComp( const LocalSlotObject &key, const LocalSlotObject &data )
{
	if (key.callPriority>data.callPriority)
		return -1;
	if (key.callPriority==data.callPriority)
	{
		if (key.registrationCount<data.registrationCount)
			return -1;
		if (key.registrationCount==data.registrationCount)
			return 0;
		return 1;
	}

	return 1;
}

RPC3::RPC3()
{
	currentExecution[0]=0;
	networkIdManager=0;
	outgoingTimestamp=0;
	outgoingPriority=HIGH_PRIORITY;
	outgoingReliability=RELIABLE_ORDERED;
	outgoingOrderingChannel=0;
	outgoingBroadcast=true;
	incomingTimeStamp=0;
	nextSlotRegistrationCount=0;
}

RPC3::~RPC3()
{
	Clear();
}

void RPC3::SetNetworkIDManager(NetworkIDManager *idMan)
{
	networkIdManager=idMan;
}

bool RPC3::UnregisterFunction(const char *uniqueIdentifier)
{
	return false;
}

bool RPC3::IsFunctionRegistered(const char *uniqueIdentifier)
{
	DataStructures::HashIndex i = GetLocalFunctionIndex(uniqueIdentifier);
	return i.IsInvalid()==false;
}

void RPC3::SetTimestamp(RakNet::Time timeStamp)
{
	outgoingTimestamp=timeStamp;
}

void RPC3::SetSendParams(PacketPriority priority, PacketReliability reliability, char orderingChannel)
{
	outgoingPriority=priority;
	outgoingReliability=reliability;
	outgoingOrderingChannel=orderingChannel;
}

void RPC3::SetRecipientAddress(const SystemAddress &systemAddress, bool broadcast)
{
	outgoingSystemAddress=systemAddress;
	outgoingBroadcast=broadcast;
}

void RPC3::SetRecipientObject(NetworkID networkID)
{
	outgoingNetworkID=networkID;
}

RakNet::Time RPC3::GetLastSenderTimestamp(void) const
{
	return incomingTimeStamp;
}

SystemAddress RPC3::GetLastSenderAddress(void) const
{
	return incomingSystemAddress;
}

RakPeerInterface *RPC3::GetRakPeer(void) const
{
	return rakPeerInterface;
}

const char *RPC3::GetCurrentExecution(void) const
{
	return (const char *) currentExecution;
}

bool RPC3::SendCallOrSignal(RakString uniqueIdentifier, char parameterCount, RakNet::BitStream *serializedParameters, bool isCall)
{
	SystemAddress systemAddr;

	if (uniqueIdentifier.IsEmpty())
		return false;

	RakNet::BitStream bs;
	if (outgoingTimestamp!=0)
	{
		bs.Write((MessageID)ID_TIMESTAMP);
		bs.Write(outgoingTimestamp);
	}
	bs.Write((MessageID)ID_RPC_PLUGIN);
	bs.Write(parameterCount);
	if (outgoingNetworkID!=UNASSIGNED_NETWORK_ID && isCall)
	{
		bs.Write(true);
		bs.Write(outgoingNetworkID);
	}
	else
	{
		bs.Write(false);
	}
	bs.Write(isCall);
	// This is so the call SetWriteOffset works
	bs.AlignWriteToByteBoundary();
	BitSize_t writeOffset = bs.GetWriteOffset();
	if (outgoingBroadcast)
	{
		unsigned systemIndex;
		for (systemIndex=0; systemIndex < rakPeerInterface->GetMaximumNumberOfPeers(); systemIndex++)
		{
			systemAddr=rakPeerInterface->GetSystemAddressFromIndex(systemIndex);
			if (systemAddr!=RakNet::UNASSIGNED_SYSTEM_ADDRESS && systemAddr!=outgoingSystemAddress)
			{
				StringCompressor::Instance()->EncodeString(uniqueIdentifier, 512, &bs, 0);

				bs.WriteCompressed(serializedParameters->GetNumberOfBitsUsed());

				bs.WriteAlignedBytes((const unsigned char*) serializedParameters->GetData(), serializedParameters->GetNumberOfBytesUsed());
				SendUnified(&bs, outgoingPriority, outgoingReliability, outgoingOrderingChannel, systemAddr, false);

				// Start writing again after ID_AUTO_RPC_CALL
				bs.SetWriteOffset(writeOffset);
			}
		}
	}
	else
	{
		systemAddr = outgoingSystemAddress;
		if (systemAddr!=RakNet::UNASSIGNED_SYSTEM_ADDRESS)
		{
			StringCompressor::Instance()->EncodeString(uniqueIdentifier, 512, &bs, 0);

			bs.WriteCompressed(serializedParameters->GetNumberOfBitsUsed());
			bs.WriteAlignedBytes((const unsigned char*) serializedParameters->GetData(), serializedParameters->GetNumberOfBytesUsed());
			SendUnified(&bs, outgoingPriority, outgoingReliability, outgoingOrderingChannel, systemAddr, false);
		}
		else
			return false;
	}
	return true;
}

void RPC3::OnAttach(void)
{
	outgoingSystemAddress=RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	outgoingNetworkID=UNASSIGNED_NETWORK_ID;
	incomingSystemAddress=RakNet::UNASSIGNED_SYSTEM_ADDRESS;
}

PluginReceiveResult RPC3::OnReceive(Packet *packet)
{
	RakNet::Time timestamp=0;
	unsigned char packetIdentifier, packetDataOffset;
	if ( ( unsigned char ) packet->data[ 0 ] == ID_TIMESTAMP )
	{
		if ( packet->length > sizeof( unsigned char ) + sizeof( RakNet::Time ) )
		{
			packetIdentifier = ( unsigned char ) packet->data[ sizeof( unsigned char ) + sizeof( RakNet::Time ) ];
			// Required for proper endian swapping
			RakNet::BitStream tsBs(packet->data+sizeof(MessageID),packet->length-1,false);
			tsBs.Read(timestamp);
			packetDataOffset=sizeof( unsigned char )*2 + sizeof( RakNet::Time );
		}
		else
			return RR_STOP_PROCESSING_AND_DEALLOCATE;
	}
	else
	{
		packetIdentifier = ( unsigned char ) packet->data[ 0 ];
		packetDataOffset=sizeof( unsigned char );
	}

	switch (packetIdentifier)
	{
	case ID_RPC_PLUGIN:
		incomingTimeStamp=timestamp;
		incomingSystemAddress=packet->systemAddress;
		OnRPC3Call(packet->systemAddress, packet->data+packetDataOffset, packet->length-packetDataOffset);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	}

	return RR_CONTINUE_PROCESSING;
}

void RPC3::OnRPC3Call(const SystemAddress &systemAddress, unsigned char *data, unsigned int lengthInBytes)
{
	RakNet::BitStream bs(data,lengthInBytes,false);

	DataStructures::HashIndex functionIndex;
	LocalRPCFunction *lrpcf;
	bool hasParameterCount=false;
	char parameterCount;
	NetworkIDObject *networkIdObject;
	NetworkID networkId;
	bool hasNetworkId=false;
	BitSize_t bitsOnStack;
	char strIdentifier[512];
	incomingExtraData.Reset();
	bs.Read(parameterCount);
	bs.Read(hasNetworkId);
	if (hasNetworkId)
	{
		bool readSuccess = bs.Read(networkId);
		RakAssert(readSuccess);
		RakAssert(networkId!=UNASSIGNED_NETWORK_ID);
		if (networkIdManager==0)
		{
			// Failed - Tried to call object member, however, networkIDManager system was never registered
			SendError(systemAddress, RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE, "");
			return;
		}
		networkIdObject = networkIdManager->GET_OBJECT_FROM_ID<NetworkIDObject*>(networkId);
		if (networkIdObject==0)
		{
			// Failed - Tried to call object member, object does not exist (deleted?)
			SendError(systemAddress, RPC_ERROR_OBJECT_DOES_NOT_EXIST, "");
			return;
		}
	}
	else
	{
		networkIdObject=0;
	}
	bool isCall;
	bs.Read(isCall);
	bs.AlignReadToByteBoundary();
		StringCompressor::Instance()->DecodeString(strIdentifier,512,&bs,0);
	bs.ReadCompressed(bitsOnStack);
	RakNet::BitStream serializedParameters;
	if (bitsOnStack>0)
	{
		serializedParameters.AddBitsAndReallocate(bitsOnStack);
		bs.ReadAlignedBytes(serializedParameters.GetData(), BITS_TO_BYTES(bitsOnStack));
		serializedParameters.SetWriteOffset(bitsOnStack);
	}
	
	// Find the registered function with this str
	if (isCall)
	{
		functionIndex = localFunctions.GetIndexOf(strIdentifier);
		if (functionIndex.IsInvalid())
		{
			SendError(systemAddress, RPC_ERROR_FUNCTION_NOT_REGISTERED, strIdentifier);
			return;
		}
		lrpcf = localFunctions.ItemAtIndex(functionIndex);

		bool isObjectMember = std::get<0>(lrpcf->functionPointer);
		if (isObjectMember==true && networkIdObject==0)
		{
			// Failed - Calling C++ function as C function
			SendError(systemAddress, RPC_ERROR_CALLING_CPP_AS_C, strIdentifier);
			return;
		}

		if (isObjectMember==false && networkIdObject!=0)
		{
			// Failed - Calling C function as C++ function
			SendError(systemAddress, RPC_ERROR_CALLING_C_AS_CPP, strIdentifier);
			return;
		}
	}
	else
	{
		functionIndex = localSlots.GetIndexOf(strIdentifier);
		if (functionIndex.IsInvalid())
		{
			SendError(systemAddress, RPC_ERROR_FUNCTION_NOT_REGISTERED, strIdentifier);
			return;
		}
	}

	if (isCall)
	{
		bool isObjectMember = std::get<0>(lrpcf->functionPointer);
		std::function<_RPC3::InvokeResultCodes (_RPC3::InvokeArgs)> functionPtr = std::get<1>(lrpcf->functionPointer);
		int arity = std::get<2>(lrpcf->functionPointer);
		//if (isObjectMember)
		//	arity--; // this pointer
		if (functionPtr==0)
		{
			// Failed - Function was previously registered, but isn't registered any longer
			SendError(systemAddress, RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED, strIdentifier);
			return;
		}

		// Boost doesn't support this for class members
		if (arity!=parameterCount)
		{
			// Failed - The number of parameters that this function has was explicitly specified, and does not match up.
			SendError(systemAddress, RPC_ERROR_INCORRECT_NUMBER_OF_PARAMETERS, strIdentifier);
			return;
		}

		_RPC3::InvokeArgs functionArgs;
		functionArgs.bitStream=&serializedParameters;
		functionArgs.networkIDManager=networkIdManager;
		functionArgs.caller=this;
		functionArgs.thisPtr=networkIdObject;
		
		// serializedParameters.PrintBits();

		_RPC3::InvokeResultCodes res2 = functionPtr(std::ref(functionArgs));
	}
	else
	{
		InvokeSignal(functionIndex, &serializedParameters, false);
	}

}
void RPC3::InterruptSignal(void)
{
	interruptSignal=true;
}
void RPC3::InvokeSignal(DataStructures::HashIndex functionIndex, RakNet::BitStream *serializedParameters, bool temporarilySetUSA)
{
	if (functionIndex.IsInvalid())
		return;

	SystemAddress lastIncomingAddress=incomingSystemAddress;
	if (temporarilySetUSA)
		incomingSystemAddress=RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	interruptSignal=false;
	LocalSlot *localSlot = localSlots.ItemAtIndex(functionIndex);
	unsigned int i;
	_RPC3::InvokeArgs functionArgs;
	functionArgs.bitStream=serializedParameters;
	functionArgs.networkIDManager=networkIdManager;
	functionArgs.caller=this;
	i=0;
	while (i < localSlot->slotObjects.Size())
	{
		if (localSlot->slotObjects[i].associatedObject!=UNASSIGNED_NETWORK_ID)
		{
			functionArgs.thisPtr = networkIdManager->GET_OBJECT_FROM_ID<NetworkIDObject*>(localSlot->slotObjects[i].associatedObject);
			if (functionArgs.thisPtr==0)
			{
				localSlot->slotObjects.RemoveAtIndex(i);
				continue;
			}
		}
		else
			functionArgs.thisPtr=0;
		functionArgs.bitStream->ResetReadPointer();

		bool isObjectMember = std::get<0>(localSlot->slotObjects[i].functionPointer);
		std::function<_RPC3::InvokeResultCodes (_RPC3::InvokeArgs)> functionPtr = std::get<1>(localSlot->slotObjects[i].functionPointer);
		if (functionPtr==0)
		{
			if (temporarilySetUSA==false)
			{
				// Failed - Function was previously registered, but isn't registered any longer
				SendError(lastIncomingAddress, RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED, localSlots.KeyAtIndex(functionIndex).C_String());
			}
			return;
		}
		_RPC3::InvokeResultCodes res2 = functionPtr(std::ref(functionArgs));

		// Not threadsafe
		if (interruptSignal==true)
			break;

		i++;
	}

	if (temporarilySetUSA)
		incomingSystemAddress=lastIncomingAddress;
}


void RPC3::OnClosedConnection(const SystemAddress &systemAddress, RakNetGUID rakNetGUID, PI2_LostConnectionReason lostConnectionReason )
{

}

void RPC3::OnShutdown(void)
{
	// Not needed, and if the user calls Shutdown inadvertantly, it unregisters his functions
	// Clear();
}

void RPC3::Clear(void)
{
	unsigned j;

	DataStructures::List<RakNet::RakString> keyList;
	DataStructures::List<LocalSlot*> outputList;
	localSlots.GetAsList(outputList,keyList,_FILE_AND_LINE_);
	for (j=0; j < outputList.Size(); j++)
	{
		RakNet::OP_DELETE(outputList[j],_FILE_AND_LINE_);
	}
	localSlots.Clear(_FILE_AND_LINE_);

	DataStructures::List<LocalRPCFunction*> outputList2;
	localFunctions.GetAsList(outputList2,keyList,_FILE_AND_LINE_);
	for (j=0; j < outputList2.Size(); j++)
	{
		RakNet::OP_DELETE(outputList2[j],_FILE_AND_LINE_);
	}
	localFunctions.Clear(_FILE_AND_LINE_);
	outgoingExtraData.Reset();
	incomingExtraData.Reset();
}

void RPC3::SendError(SystemAddress target, unsigned char errorCode, const char *functionName)
{
	RakNet::BitStream bs;
	bs.Write((MessageID)ID_RPC_REMOTE_ERROR);
	bs.Write(errorCode);
	bs.WriteAlignedBytes((const unsigned char*) functionName,(const unsigned int) strlen(functionName)+1);
	SendUnified(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, target, false);
}

DataStructures::HashIndex RPC3::GetLocalSlotIndex(const char *sharedIdentifier)
{
	return localSlots.GetIndexOf(sharedIdentifier);
}
DataStructures::HashIndex RPC3::GetLocalFunctionIndex(RPC3::RPCIdentifier identifier)
{
	return localFunctions.GetIndexOf(identifier.C_String());
}
