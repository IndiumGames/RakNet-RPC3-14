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

/// \file
/// \brief Automatically serializing and deserializing RPC system. Third generation of RPC.


#ifndef __RPC_3_H
#define __RPC_3_H

#include "RPC3_STD.h"
#include "PluginInterface2.h"
#include "PacketPriority.h"
#include "RakNetTypes.h"
#include "BitStream.h"
#include "RakString.h"
#include "NetworkIDObject.h"
#include "DS_Hash.h"
#include "DS_OrderedList.h"

#ifdef _MSC_VER
#pragma warning( push )
#endif

/// \defgroup RPC_3_GROUP RPC3
/// \brief Remote procedure calls, powered by the C++14
/// \details
/// \ingroup PLUGINS_GROUP

namespace RakNet
{
class RakPeerInterface;
class NetworkIDManager;

/// \ingroup RPC_3_GROUP
#define RPC3_REGISTER_FUNCTION(RPC3Instance, _FUNCTION_PTR_) (RPC3Instance)->RegisterFunction((#_FUNCTION_PTR_), (_FUNCTION_PTR_))

/// \brief Error codes returned by a remote system as to why an RPC function call cannot execute
/// \details Error code follows packet ID ID_RPC_REMOTE_ERROR, that is packet->data[1]<BR>
/// Name of the function will be appended starting at packet->data[2]
/// \ingroup RPC_3_GROUP
enum RPCErrorCodes
{
	/// RPC3::SetNetworkIDManager() was not called, and it must be called to call a C++ object member
	RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE,

	/// Cannot execute C++ object member call because the object specified by SetRecipientObject() does not exist on this system
	RPC_ERROR_OBJECT_DOES_NOT_EXIST,

	/// Internal error, index optimization for function lookup does not exist
	RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE,

	/// Named function was not registered with RegisterFunction(). Check your spelling.
	RPC_ERROR_FUNCTION_NOT_REGISTERED,

	/// Named function was registered, but later unregistered with UnregisterFunction() and can no longer be called.
	RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED,

	/// SetRecipientObject() was not called before Call(), but the registered pointer is a class member
	/// If you intended to call a class member function, call SetRecipientObject() with a valid object first.
	RPC_ERROR_CALLING_CPP_AS_C,

	/// SetRecipientObject() was called before Call(), but RegisterFunction() was called with isObjectMember=false
	/// If you intended to call a C function, call SetRecipientObject(UNASSIGNED_NETWORK_ID) first.
	RPC_ERROR_CALLING_C_AS_CPP,
	
	RPC_ERROR_INCORRECT_NUMBER_OF_PARAMETERS,
};

/// \brief The RPC3 plugin allows you to call remote functions as if they were local functions, using the standard function call syntax
/// \details No serialization or deserialization is needed.<BR>
/// As of this writing, the system is not threadsafe.<BR>
/// Features:<BR>
/// <LI>Pointers to classes that derive from NetworkID are automatically looked up using NetworkIDManager
/// <LI>Types are written to BitStream, meaning built-in serialization operations are performed, including endian swapping
/// <LI>Types can customize autoserialization by providing an implementation of operator << and operator >> to and from BitStream
/// \note You cannot use RPC4 at the same time as RPC3
/// \ingroup RPC_3_GROUP
class RPC3 : public PluginInterface2
{
public:
	// Constructor
	RPC3();

	// Destructor
	virtual ~RPC3();

	/// Sets the network ID manager to use for object lookup
	/// Required to call C++ object member functions via SetRecipientObject()
	/// \param[in] idMan Pointer to the network ID manager to use
	void SetNetworkIDManager(NetworkIDManager *idMan);

	/// Register a function pointer as callable using RPC()
	/// \param[in] uniqueIdentifier String identifying the function. Recommended that this is the name of the function
	/// \param[in] functionPtr Pointer to the function. For C, just pass the name of the function. For C++, use ARPC_REGISTER_CPP_FUNCTION
	/// \return True on success, false on uniqueIdentifier already used
	template<typename Function>
	bool RegisterFunction(const char *uniqueIdentifier, Function functionPtr)
	{
		if (IsFunctionRegistered(uniqueIdentifier)) return false;
		_RPC3::FunctionPointer fp;
		fp= _RPC3::GetBoundPointer(functionPtr);
		localFunctions.Push(uniqueIdentifier,RakNet::OP_NEW_1<LocalRPCFunction>( _FILE_AND_LINE_, fp ),_FILE_AND_LINE_);
		return true;
	}

	/// \internal
	// Callable object, along with priority to call relative to other objects
	struct LocalSlotObject
	{
		LocalSlotObject() {}
		LocalSlotObject(NetworkID _associatedObject,unsigned int _registrationCount,int _callPriority,_RPC3::FunctionPointer _functionPointer)
		{associatedObject=_associatedObject;registrationCount=_registrationCount;callPriority=_callPriority;functionPointer=_functionPointer;}
		~LocalSlotObject() {}

		// Used so slots are called in the order they are registered
		NetworkID associatedObject;
		unsigned int registrationCount;
		int callPriority;
		_RPC3::FunctionPointer functionPointer;
	};
	
	/// \internal
	/// Identifies an RPC function, by string identifier and if it is a C or C++ function
	typedef RakString RPCIdentifier;
	static int LocalSlotObjectComp( const LocalSlotObject &key, const LocalSlotObject &data );
	/// \internal
	struct LocalSlot
	{
//		RPCIdentifier identifier;
		DataStructures::OrderedList<LocalSlotObject,LocalSlotObject,LocalSlotObjectComp> slotObjects;
	};
	
	/// Register a slot, which is a function pointer to one or more instances of a class that supports this function signature
	/// When a signal occurs, all slots with the same identifier are called.
	/// \param[in] sharedIdentifier A string to identify the slot. Recommended to be the same as the name of the function.
	/// \param[in] functionPtr Pointer to the function. For C, just pass the name of the function. For C++, use ARPC_REGISTER_CPP_FUNCTION
	/// \param[in] objectInstance If 0, then this slot is just a regular C function. Otherwise, this is a member of the given class instance.
	/// \param[in] callPriority Slots are called by order of the highest callPriority first. For slots with the same priority, they are called in the order they are registered
	template<typename Function>
	void RegisterSlot(const char *sharedIdentifier, Function functionPtr, NetworkID objectInstanceId, int callPriority)
	{
		_RPC3::FunctionPointer fp;
		fp= _RPC3::GetBoundPointer(functionPtr);
		LocalSlotObject lso(objectInstanceId, nextSlotRegistrationCount++, callPriority, _RPC3::GetBoundPointer(functionPtr));
		DataStructures::HashIndex idx = GetLocalSlotIndex(sharedIdentifier);
		LocalSlot *localSlot;
		if (idx.IsInvalid())
		{
			localSlot = RakNet::OP_NEW<LocalSlot>(_FILE_AND_LINE_);
			localSlots.Push(sharedIdentifier, localSlot,_FILE_AND_LINE_);
		}
		else
		{
			localSlot=localSlots.ItemAtIndex(idx);
		}
		localSlot->slotObjects.Insert(lso,lso,true,_FILE_AND_LINE_);
	}

	/// Unregisters a function pointer to be callable given an identifier for the pointer
	/// \param[in] uniqueIdentifier String identifying the function.
	/// \return True on success, false on function was not previously or is not currently registered.
	bool UnregisterFunction(const char *uniqueIdentifier);

	/// Returns if a function identifier was previously registered on this system with RegisterFunction(), and not unregistered with UnregisterFunction()
	/// \param[in] uniqueIdentifier String identifying the function.
	/// \return True if the function was registered, false otherwise
	bool IsFunctionRegistered(const char *uniqueIdentifier);

	/// Send or stop sending a timestamp with all following calls to Call()
	/// Use GetLastSenderTimestamp() to read the timestamp.
	/// \param[in] timeStamp Non-zero to pass this timestamp using the ID_TIMESTAMP system. 0 to clear passing a timestamp.
	void SetTimestamp(RakNet::Time timeStamp);

	/// Set parameters to pass to RakPeer::Send() for all following calls to Call()
	/// Deafults to HIGH_PRIORITY, RELIABLE_ORDERED, ordering channel 0
	/// \param[in] priority See RakPeer::Send()
	/// \param[in] reliability See RakPeer::Send()
	/// \param[in] orderingChannel See RakPeer::Send()
	void SetSendParams(PacketPriority priority, PacketReliability reliability, char orderingChannel);

	/// Set system to send to for all following calls to Call()
	/// Defaults to RakNet::UNASSIGNED_SYSTEM_ADDRESS, broadcast=true
	/// \param[in] systemAddress See RakPeer::Send()
	/// \param[in] broadcast See RakPeer::Send()
	void SetRecipientAddress(const SystemAddress &systemAddress, bool broadcast);

	/// Set the NetworkID to pass for all following calls to Call()
	/// Defaults to UNASSIGNED_NETWORK_ID (none)
	/// If set, the remote function will be considered a C++ function, e.g. an object member function
	/// If set to UNASSIGNED_NETWORK_ID (none), the remote function will be considered a C function
	/// If this is set incorrectly, you will get back either RPC_ERROR_CALLING_C_AS_CPP or RPC_ERROR_CALLING_CPP_AS_C
	/// \sa NetworkIDManager
	/// \param[in] networkID Returned from NetworkIDObject::GetNetworkID()
	void SetRecipientObject(NetworkID networkID);

	/// If the last received function call has a timestamp included, it is stored and can be retrieved with this function.
	/// \return 0 if the last call did not have a timestamp, else non-zero
	RakNet::Time GetLastSenderTimestamp(void) const;

	/// Returns the system address of the last system to send us a received function call
	/// Equivalent to the old system RPCParameters::sender
	/// \return Last system to send an RPC call using this system
	SystemAddress GetLastSenderAddress(void) const;

	/// If called while processing a slot, no further slots for the currently executing signal will be executed
	void InterruptSignal(void);

	/// Returns the instance of RakPeer this plugin was attached to
	RakPeerInterface *GetRakPeer(void) const;

	/// Returns the currently running RPC call identifier, set from RegisterFunction::uniqueIdentifier
	/// Returns an empty string "" if none
	/// \return which RPC call is currently running
	const char *GetCurrentExecution(void) const;

	/// Calls a remote function, using as send parameters whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// If you call a C++ class member function, don't forget to first call SetRecipientObject(). You can use CallExplicit() instead of Call() to force yourself not to forget.
	///
	/// Parameters passed to Call are processed as follows:
	/// 1. If the parameter is not a pointer
	/// 2. - And you overloaded RakNet::BitStream& operator<<(RakNet::BitStream& out, MyClass& in) then that will be used to do the serialization
	/// 3. - Otherwise, it will use bitStream.Write(myClass); BitStream already defines specializations for NetworkIDObject, SystemAddress, other BitStreams
	/// 4. If the parameter is a pointer
	/// 5. - And the pointer can be converted to NetworkIDObject, then it will write bitStream.Write(myClass->GetNetworkID()); To make it also dereference the pointer, use RakNet::_RPC3::Deref(myClass)
	/// 6. - And the pointer can not be converted to NetworkID, but it is a pointer to RakNet::RPC3, then it is skipped
	/// 7. Otherwise, the pointer is dereferenced and written as in step 2 and 3.
	///
	/// \note If you need endian swapping (Mac talking to PC for example), you pretty much need to define operator << and operator >> for all classes you want to serialize. Otherwise the member variables will not be endian swapped.
	/// \note If the call fails on the remote system, you will get back ID_RPC_REMOTE_ERROR. packet->data[1] will contain one of the values of RPCErrorCodes. packet->data[2] and on will contain the name of the function.
	///
	/// \param[in] uniqueIdentifier parameter of the same name passed to RegisterFunction() on the remote system
	template<typename... Args>
	bool Call(const char *uniqueIdentifier, const Args&... args) {
		return _RPC3::RpcCall::Call(this, uniqueIdentifier, sizeof...(Args), true, args...);
	}

	struct CallExplicitParameters
	{
		CallExplicitParameters(
			NetworkID _networkID=UNASSIGNED_NETWORK_ID, SystemAddress _systemAddress=RakNet::UNASSIGNED_SYSTEM_ADDRESS,
			bool _broadcast=true, RakNet::Time _timeStamp=0, PacketPriority _priority=HIGH_PRIORITY,
			PacketReliability _reliability=RELIABLE_ORDERED, char _orderingChannel=0
			) : networkID(_networkID), systemAddress(_systemAddress), broadcast(_broadcast), timeStamp(_timeStamp), priority(_priority), reliability(_reliability), orderingChannel(_orderingChannel)
		{}
		NetworkID networkID;
		SystemAddress systemAddress;
		bool broadcast;
		RakNet::Time timeStamp;
		PacketPriority priority;
		PacketReliability reliability;
		char orderingChannel;
	};

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are serialized using operator << with RakNet::BitStream. If you provide an overload it will be used, otherwise the seriailzation is equivalent to memcpy except for native RakNet types (NetworkIDObject, SystemAddress, etc.)
	/// If the type is a pointer to a type deriving from NetworkIDObject, then only the NetworkID is sent, and the object looked up on the remote system. Otherwise, the pointer is dereferenced and the contents serialized as usual.
	/// \note The this pointer, for this instance of RPC3, is pushed as the last parameter on the stack. See RPC3Sample.cpp for an example of this
	/// \note If the call fails on the remote system, you will get back ID_RPC_REMOTE_ERROR. packet->data[1] will contain one of the values of RPCErrorCodes. packet->data[2] and on will contain the name of the function.
	/// \param[in] uniqueIdentifier parameter of the same name passed to RegisterFunction() on the remote system
	/// \param[in] timeStamp See SetTimestamp()
	/// \param[in] priority See SetSendParams()
	/// \param[in] reliability See SetSendParams()
	/// \param[in] orderingChannel See SetSendParams()
	/// \param[in] systemAddress See SetRecipientAddress()
	/// \param[in] broadcast See SetRecipientAddress()
	/// \param[in] networkID See SetRecipientObject()
	template<typename... Args>
	bool CallExplicit(const char *uniqueIdentifier, const CallExplicitParameters * const callExplicitParameters, const Args&... args) {
		SetTimestamp(callExplicitParameters->timeStamp);
		SetSendParams(callExplicitParameters->priority, callExplicitParameters->reliability, callExplicitParameters->orderingChannel);
		SetRecipientAddress(callExplicitParameters->systemAddress, callExplicitParameters->broadcast);
		SetRecipientObject(callExplicitParameters->networkID);
		return Call(uniqueIdentifier, args...);
	}

	template<typename... Args>
	bool CallC(const char *uniqueIdentifier, const Args&... args) {
		SetRecipientObject(UNASSIGNED_NETWORK_ID);
		return Call(uniqueIdentifier, args...);
	}

	template<typename... Args>
	bool CallCPP(const char *uniqueIdentifier, NetworkID nid, const Args&... args) {
		SetRecipientObject(nid);
		return Call(uniqueIdentifier, args...);
	}


	// ---------------------------- Signals and slots ----------------------------------

	/// Calls zero or more functions identified by sharedIdentifier.
	/// Uses as send parameters whatever was last passed to SetTimestamp(), SetSendParams(), and SetRecipientAddress()
	/// You can use CallExplicit() instead of Call() to force yourself not to forget to set parameters
	///
	/// See the Call() function for a description of parameters
	///
	/// \param[in] sharedIdentifier parameter of the same name passed to RegisterSlot() on the remote system
	
	template<typename... Args>
	bool Signal(const char *sharedIdentifier, const Args&... args) {
		return _RPC3::RpcCall::Call(this, sharedIdentifier, sizeof...(Args), false, args...);
	}
	

	struct SignalExplicitParameters
	{
		SignalExplicitParameters(
			SystemAddress _systemAddress=RakNet::UNASSIGNED_SYSTEM_ADDRESS,
			bool _broadcast=true, RakNet::Time _timeStamp=0, PacketPriority _priority=HIGH_PRIORITY,
			PacketReliability _reliability=RELIABLE_ORDERED, char _orderingChannel=0
			) : systemAddress(_systemAddress), broadcast(_broadcast), timeStamp(_timeStamp), priority(_priority), reliability(_reliability), orderingChannel(_orderingChannel)
		{}
		SystemAddress systemAddress;
		bool broadcast;
		RakNet::Time timeStamp;
		PacketPriority priority;
		PacketReliability reliability;
		char orderingChannel;
	};

	/// Same as Signal(), but you are forced to specify the remote system parameters
	template<typename... Args>
	bool SignalExplicit(const char *sharedIdentifier, const SignalExplicitParameters * const signalExplicitParameters, const Args&... args){
		SetTimestamp(signalExplicitParameters->timeStamp);
		SetSendParams(signalExplicitParameters->priority, signalExplicitParameters->reliability, signalExplicitParameters->orderingChannel);
		SetRecipientAddress(signalExplicitParameters->systemAddress, signalExplicitParameters->broadcast);
		return Signal(sharedIdentifier, args...);
	}
	
	// ---------------------------- ALL INTERNAL AFTER HERE ----------------------------

	/// \internal
	/// The RPC identifier, and a pointer to the function
	struct LocalRPCFunction
	{
		LocalRPCFunction() {}
		LocalRPCFunction(_RPC3::FunctionPointer _functionPointer) {functionPointer=_functionPointer;};
		_RPC3::FunctionPointer functionPointer;
	};

	/// \internal
	/// Sends the RPC call, with a given serialized function
	bool SendCallOrSignal(RakString uniqueIdentifier, char parameterCount, RakNet::BitStream *serializedParameters, bool isCall);

	/// Call a given signal with a bitstream representing the parameter list
	void InvokeSignal(DataStructures::HashIndex functionIndex, RakNet::BitStream *serializedParameters, bool temporarilySetUSA);


	protected:

	// --------------------------------------------------------------------------------------------
	// Packet handling functions
	// --------------------------------------------------------------------------------------------
	void OnAttach(void);
	virtual PluginReceiveResult OnReceive(Packet *packet);
	virtual void OnRPC3Call(const SystemAddress &systemAddress, unsigned char *data, unsigned int lengthInBytes);
	virtual void OnClosedConnection(const SystemAddress &systemAddress, RakNetGUID rakNetGUID, PI2_LostConnectionReason lostConnectionReason );
	virtual void OnShutdown(void);

	void Clear(void);

	void SendError(SystemAddress target, unsigned char errorCode, const char *functionName);
	DataStructures::HashIndex GetLocalFunctionIndex(RPCIdentifier identifier);
	DataStructures::HashIndex GetLocalSlotIndex(const char *sharedIdentifier);

	DataStructures::Hash<RakNet::RakString, LocalSlot*,256, RakNet::RakString::ToInteger> localSlots;
	DataStructures::Hash<RakNet::RakString, LocalRPCFunction*,256, RakNet::RakString::ToInteger> localFunctions;

	RakNet::Time outgoingTimestamp;
	PacketPriority outgoingPriority;
	PacketReliability outgoingReliability;
	char outgoingOrderingChannel;
	SystemAddress outgoingSystemAddress;
	bool outgoingBroadcast;
	NetworkID outgoingNetworkID;
	RakNet::BitStream outgoingExtraData;

	RakNet::Time incomingTimeStamp;
	SystemAddress incomingSystemAddress;
	RakNet::BitStream incomingExtraData;

	NetworkIDManager *networkIdManager;
	char currentExecution[512];

	/// Used so slots are called in the order they are registered
	unsigned int nextSlotRegistrationCount;

	bool interruptSignal;
	
	friend _RPC3::RpcCall;
};

} // End namespace

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif
