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

#ifndef __RPC3_STD_H
#define __RPC3_STD_H

#include <tuple>
#include <iterator>
#include <vector>

#include "NetworkIDManager.h"
#include "NetworkIDObject.h"
#include "BitStream.h"

#include "std_additions.h"

namespace RakNet
{
class RPC3;
class BitStream;


namespace _RPC3
{

enum InvokeResultCodes
{
	IRC_SUCCESS,
	IRC_NEED_BITSTREAM,
	IRC_NEED_NETWORK_ID_MANAGER,
	IRC_NEED_NETWORK_ID,
	IRC_NEED_CLASS_OBJECT,
};

struct InvokeArgs
{
	// Bitstream to use to deserialize
	RakNet::BitStream *bitStream;

	// NetworkIDManager to use to lookup objects
	NetworkIDManager *networkIDManager;

	// C++ class member object
	NetworkID classMemberObjectId;

	// The calling plugin
	RPC3 *caller;

	// The this pointer for C++
	NetworkIDObject *thisPtr;
};

typedef std::tuple<bool, std::function<InvokeResultCodes(InvokeArgs)>, int> FunctionPointer;

struct StrWithDestructor
{
	char *c;
	~StrWithDestructor() {if (c) delete c;}
};

enum RPC3TagFlag
{
	RPC3_TAG_FLAG_DEREF=1,
	RPC3_TAG_FLAG_ARRAY=2,
};

struct RPC3Tag
{
	RPC3Tag() {}
	RPC3Tag(void *_v, unsigned int _count, RPC3TagFlag _flag) : v(_v), count(_count), flag((unsigned char)_flag) {}
	void* v;
	unsigned int count;
	unsigned char flag;
};

// Track the pointers tagged with RakNet::_RPC3::Deref
static std::vector<RPC3Tag> __RPC3TagPtrs;
static int __RPC3TagHead=0;
static int __RPC3TagTail=0;

// If this assert hits, then RakNet::_RPC3::Deref was called more times than the argument was passed to the function
static void __RPC3_Tag_AddHead(const RPC3Tag &p)
{
	// Update tag if already in array
	int i;
	for (i=__RPC3TagTail; i!=__RPC3TagPtrs.size(); i++)
	{
		if (__RPC3TagPtrs[i].v==p.v)
		{
			if (p.flag==RPC3_TAG_FLAG_ARRAY)
			{
				__RPC3TagPtrs[i].count=p.count;
			}
			__RPC3TagPtrs[i].flag|=p.flag;

			return;
		}
	}

    __RPC3TagPtrs.push_back(p);
	assert(__RPC3TagPtrs.size()!=__RPC3TagTail);
}
static void __RPC3ClearTail(void) {
	while (__RPC3TagTail!=__RPC3TagPtrs.size())
	{
		if (__RPC3TagPtrs[__RPC3TagTail].v==0)
			__RPC3TagTail++;
		else
			return;
	}
}
static bool __RPC3ClearPtr(void* p, RPC3Tag *tag) {
	int i;
	for (i=__RPC3TagTail; i!=__RPC3TagPtrs.size(); i++)
	{
		if (__RPC3TagPtrs[i].v==p)
		{
			*tag=__RPC3TagPtrs[i];
			__RPC3TagPtrs[i].v=0;
			__RPC3ClearTail();
			return true;
		}
	}
	tag->flag=0;
	tag->count=1;
	return false;
}

template <class templateType>
inline const templateType& Deref(const templateType & t) {
	__RPC3_Tag_AddHead(RPC3Tag((void*)t,1,RPC3_TAG_FLAG_DEREF));
	return t;
}

template <class templateType>
inline const templateType& PtrToArray(unsigned int count, const templateType & t) {
	__RPC3_Tag_AddHead(RPC3Tag((void*)t,count,RPC3_TAG_FLAG_ARRAY));
	return t;
}

struct ReadBitstream
{
	static void applyArray(RakNet::BitStream &bitStream, RakNet::BitStream* t){apply(bitStream,t);}

	static void apply(RakNet::BitStream &bitStream, RakNet::BitStream* t)
	{
		BitSize_t numBitsUsed;
		bitStream.ReadCompressed(numBitsUsed);
		bitStream.Read(t,numBitsUsed);
		printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB%s\n", t->GetData());
	}
};

//template <typename T>
struct ReadPtr
{
	template <typename T2>
	static inline void applyArray(RakNet::BitStream &bitStream, T2 *t) {bitStream >> (*t);}
	template <typename T2>
	static inline void apply(RakNet::BitStream &bitStream, T2 *t) {bitStream >> (*t);}

	static inline void apply(RakNet::BitStream &bitStream, char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void apply(RakNet::BitStream &bitStream, unsigned char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void apply(RakNet::BitStream &bitStream, const char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void apply(RakNet::BitStream &bitStream, const unsigned char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void applyStr(RakNet::BitStream &bitStream, char *&t)
	{
		RakNet::RakString rs;
		bitStream >> rs;
		size_t len = rs.GetLength()+1;
		
		// The caller should have already allocated memory, so we need to free
		// it and allocate a new buffer.
		RakAssert("Expected allocated array, got NULL" && (NULL != t));
		delete [] t;

		t = new char [len];
		memcpy(t,rs.C_String(),len);
	}
};


template< typename T >
struct DoRead
{
	typedef typename std::conditional<
		std::is_convertible<T*,RakNet::BitStream*>::value,
		ReadBitstream,
		ReadPtr >::type type;
};


template< typename T >
struct ReadWithoutNetworkIDNoPtr
{
	static InvokeResultCodes apply(InvokeArgs &args, T &t)
	{
//		printf("ReadWithoutNetworkIDNoPtr\n");

		DoRead< typename std::remove_pointer<T>::type >::type::apply(* (args.bitStream),&t);

		return IRC_SUCCESS;
	}

	template< typename T2 >
	static void Cleanup(T2 &t) {}
};

template< typename T >
struct ReadWithNetworkIDPtr
{
	static InvokeResultCodes apply(InvokeArgs &args, T &t)
	{
//		printf("ReadWithNetworkIDPtr\n");
		// Read the network ID

		bool isNull;
		args.bitStream->Read(isNull);
		if (isNull)
		{
			t=0;
			return IRC_SUCCESS;
		}

		bool deref, isArray;
		args.bitStream->Read(deref);
		args.bitStream->Read(isArray);
		unsigned int count;
		if (isArray)
			args.bitStream->ReadCompressed(count);
		else
			count=1;
		NetworkID networkId;
		for (unsigned int i=0; i < count; i++)
		{
			args.bitStream->Read(networkId);
			t = args.networkIDManager->GET_OBJECT_FROM_ID< T >(networkId);
			if (deref)
			{
				BitSize_t bitsUsed;
				args.bitStream->AlignReadToByteBoundary();
				args.bitStream->Read(bitsUsed);

				if (t)
				{
					DoRead< typename std::remove_pointer<T>::type >::type::apply(* (args.bitStream),t);
				}
				else
				{
					// Skip data!
					args.bitStream->IgnoreBits(bitsUsed);
				}
			}
		}
		
		return IRC_SUCCESS;
	}

	template< typename T2 >
	static void Cleanup(T2 &t) {}
};

template< typename T >
struct ReadWithoutNetworkIDPtr
{
	template <typename T2>
	static InvokeResultCodes apply(InvokeArgs &args, T2 &t)
	{
//		printf("ReadWithoutNetworkIDPtr\n");
		
		bool isNull=false;
		args.bitStream->Read(isNull);
		if (isNull)
		{
			t=0;
			return IRC_SUCCESS;
		}

		typedef typename std::remove_pointer< T >::type ActualObjectType;

		bool isArray=false;
		unsigned int count;
		args.bitStream->Read(isArray);
		if (isArray)
			args.bitStream->ReadCompressed(count);
		else
			count=1;

		t = new ActualObjectType[count]();
		if (isArray)
		{
			for (unsigned int i=0; i < count; i++)
			{
				DoRead< typename std::remove_pointer<T>::type >::type::applyArray(* (args.bitStream),t+i);
			}
		}
		else
		{
			DoRead< typename std::remove_pointer<T>::type >::type::apply(* (args.bitStream),t);
		}

		return IRC_SUCCESS;
	}

	template< typename T2 >
	static void Cleanup(T2 &t) {
		if (t)
			delete [] t;
	}
};

template< typename T >
struct SetRPC3Ptr
{
	static InvokeResultCodes apply(InvokeArgs &args, T &obj)
	{
		obj=args.caller;
		return IRC_SUCCESS;
	}

	template< typename T2 >
	static void Cleanup(T2 &t) {}
};


template< typename T >
struct ReadWithoutNetworkID
{
	typedef typename std::conditional<
		std::is_pointer<T>::value
		, ReadWithoutNetworkIDPtr<T> // true
		, ReadWithoutNetworkIDNoPtr<T>
	>::type type;
};

template< typename T >
struct identity
{
	typedef T type;
};


template< typename T >
struct GetReadFunction
{
	typedef typename std::conditional<
		std::is_convertible<T, NetworkIDObject*>::value
		, ReadWithNetworkIDPtr<T>
		, typename ReadWithoutNetworkID<T>::type
	>::type type;
};

template< typename T >
struct ProcessArgType
{
	typedef typename std::conditional<
		std::is_convertible<T, RPC3*>::value
		, SetRPC3Ptr<T>
		, typename GetReadFunction<T>::type
	>::type type;
};


template<typename F>
struct RpcInvoker;

template<typename R, typename... Args>
struct RpcInvoker<R(*)(Args...)> : public RpcInvoker<R(Args...)> {
};
 
template<typename R, typename... Args>
struct RpcInvoker<R(Args...)> {
	template <typename Function>
	static inline InvokeResultCodes applyer(Function func,
	                                                InvokeArgs functionArgs) {
		std::tuple<typename std::decay<Args>::type...> args;
		InvokeResultCodes irc = IRC_SUCCESS;
		
		RpcInvoker<decltype(func)>::apply(func, functionArgs, args, irc);
		
		return irc;
	}

	/*
	 * After all of the arguments are processed, invoke them with the function
	 * pointer.
	 */
	template<std::size_t I = 0, typename Function>
	static inline typename std::enable_if<I == sizeof...(Args), void>::type
			apply(Function func, InvokeArgs &functionArgs,
							std::tuple<typename std::decay<Args>::type...> &args, InvokeResultCodes &irc) {
		INVOKE(func, args);
		irc = IRC_SUCCESS;
	}
	
	/*
	 * Iterate arguments in the args... tuple recursively and replace them with
	 * values from functionArgs.
	 */
	template<std::size_t I = 0, typename Function>
	static inline typename std::enable_if<I < sizeof...(Args), void>::type
			apply(Function func, InvokeArgs &functionArgs,
					std::tuple<typename std::decay<Args>::type...> &args, InvokeResultCodes &irc) {
		
		typedef typename std::remove_reference<decltype(std::get<I>(args))>::type arg_type_no_ref;
		
		ProcessArgType<arg_type_no_ref>::type::apply(functionArgs, std::get<I>(args));
        
		RpcInvoker<decltype(func)>::template
									apply<I+1>(func, functionArgs, args, irc);
	  }
};


struct RpcInvokerCpp;

struct RpcInvokerCpp {
	template <typename Ret, typename C, typename... Args>
	static inline InvokeResultCodes applyer(Ret(C::*func)(Args...),
													InvokeArgs functionArgs) {
		std::tuple<typename std::decay<Args>::type...> args;
		InvokeResultCodes irc = IRC_SUCCESS;
		
		auto *objectPointer = (C *)functionArgs.thisPtr;
		RpcInvokerCpp::apply(func, objectPointer, functionArgs, args, irc);
		
		return irc;
    }
    
	template<std::size_t I = 0, typename Ret, typename C, typename... Args, typename Obj>
	static inline typename std::enable_if<I == sizeof...(Args), void>::type
			apply(Ret(C::*func)(Args...), Obj *object, InvokeArgs &functionArgs,
							std::tuple<typename std::decay<Args>::type...>& args, InvokeResultCodes &irc) {
		INVOKE(func, object, args);
		irc = IRC_SUCCESS;
	}
	
	template<std::size_t I = 0, typename Ret, typename C, typename... Args, typename Obj>
	static inline typename std::enable_if<I < sizeof...(Args), void>::type
			apply(Ret(C::*func)(Args...), Obj *object, InvokeArgs &functionArgs,
							std::tuple<typename std::decay<Args>::type...>& args, InvokeResultCodes &irc) {
		
		typedef typename std::remove_reference<decltype(std::get<I>(args))>::type arg_type_no_ref;
		
		ProcessArgType<arg_type_no_ref>::type::apply(functionArgs, std::get<I>(args));
        
		RpcInvokerCpp::template apply<I+1>(func, object, functionArgs, args, irc);
	  }
};


template <typename T>
struct DoNothing
{
	static void apply(RakNet::BitStream &bitStream, T& t)
	{
		(void) bitStream;
		(void) t;
//		printf("DoNothing\n");
	}
};

struct WriteBitstream
{
	static void applyArray(RakNet::BitStream &bitStream, RakNet::BitStream* t) {apply(bitStream,t);}
	static void apply(RakNet::BitStream &bitStream, RakNet::BitStream* t)
	{
		BitSize_t oldReadOffset = t->GetReadOffset();
		t->ResetReadPointer();
		bitStream.WriteCompressed(t->GetNumberOfBitsUsed());
		bitStream.Write(t);
		t->SetReadOffset(oldReadOffset);
	}
};

struct WritePtr
{
	template <typename T2>
	static inline void applyArray(RakNet::BitStream &bitStream, T2 *t) {bitStream << (*t);}
	template <typename T2>
	static inline void apply(RakNet::BitStream &bitStream, T2 *t) {
		bitStream << (*t);}

	static inline void apply(RakNet::BitStream &bitStream, char *t) {bitStream << t;}

	static inline void apply(RakNet::BitStream &bitStream, unsigned char *t) {bitStream << t;}

	static inline void apply(RakNet::BitStream &bitStream, const char *t) {bitStream << t;}

	static inline void apply(RakNet::BitStream &bitStream, const unsigned char *t) {bitStream << t;}
};

template< typename T >
struct DoWrite
{
	typedef typename std::conditional<
		std::is_convertible<T*,RakNet::BitStream*>::value,
		WriteBitstream,
		WritePtr >::type type;
};

template <typename T>
struct WriteWithNetworkIDPtr
{
	static void apply(RakNet::BitStream &bitStream, T& t)
	{
		bool isNull;
		isNull=(t==0);
		bitStream.Write(isNull);
		if (isNull)
			return;
		RPC3Tag tag;
		__RPC3ClearPtr(t, &tag);
		bool deref = (tag.flag & RPC3_TAG_FLAG_DEREF) !=0;
		bool isArray = (tag.flag & RPC3_TAG_FLAG_ARRAY) !=0;
		bitStream.Write(deref);
		bitStream.Write(isArray);
		if (isArray)
		{
			bitStream.WriteCompressed(tag.count);
		}
		for (unsigned int i=0; i < tag.count; i++)
		{
			NetworkID inNetworkID=t->GetNetworkID();
			bitStream << inNetworkID;
			if (deref)
			{
				// skip bytes, write data, go back, write number of bits written, reset cursor
				bitStream.AlignWriteToByteBoundary();
				BitSize_t writeOffset1 = bitStream.GetWriteOffset();
				BitSize_t bitsUsed1=bitStream.GetNumberOfBitsUsed();
				bitStream.Write(bitsUsed1);
				bitsUsed1=bitStream.GetNumberOfBitsUsed();
				DoWrite< typename std::remove_pointer<T>::type >::type::apply(bitStream,t);
				BitSize_t writeOffset2 = bitStream.GetWriteOffset();
				BitSize_t bitsUsed2=bitStream.GetNumberOfBitsUsed();
				bitStream.SetWriteOffset(writeOffset1);
				bitStream.Write(bitsUsed2-bitsUsed1);
				bitStream.SetWriteOffset(writeOffset2);
			}
		}
	}
};

template <typename T>
struct WriteWithoutNetworkIDNoPtr
{
	static void apply(RakNet::BitStream &bitStream, T& t)
	{
		typedef typename std::remove_pointer<T>::type t_type_no_ptr;
		typedef typename std::remove_const<t_type_no_ptr>::type t_type_no_const;
		
		// Remove constness of t variable.
		auto ptrOfT = const_cast<t_type_no_const *>(&t);
		DoWrite<t_type_no_const>::type::apply(bitStream,ptrOfT);
	}
};

template <typename T>
struct WriteWithoutNetworkIDPtr
{
	static void apply(RakNet::BitStream &bitStream, T& t)
	{
		bool isNull;
		isNull=(t==0);
		bitStream.Write(isNull);
		if (isNull)
			return;

		RPC3Tag tag;
		__RPC3ClearPtr((void*) t, &tag);
		bool isArray = (tag.flag & RPC3_TAG_FLAG_ARRAY) !=0;
		bitStream.Write(isArray);
		if (isArray)
		{
			bitStream.WriteCompressed(tag.count);
		}
		if (isArray)
		{
			for (unsigned int i=0; i < tag.count; i++)
				DoWrite< typename std::remove_pointer<T>::type >::type::applyArray(bitStream,t+i);
		}
		else
		{
			DoWrite< typename std::remove_pointer<T>::type >::type::apply(bitStream,t);
		}
		
	}
};

template <typename T>
struct SerializeCallParameterBranch
{
	typedef typename std::conditional<
		std::is_convertible<T,RPC3*>::value
		, DoNothing<T>
		, WriteWithoutNetworkIDPtr<T>
	>::type typeCheck1;

	typedef typename std::conditional<
		std::is_pointer<T>::value
		, typeCheck1
		, WriteWithoutNetworkIDNoPtr<T>
	>::type typeCheck2;

	typedef typename std::conditional<
		std::is_convertible<T,NetworkIDObject*>::value
		, WriteWithNetworkIDPtr<T>
		, typeCheck2
	>::type type;
};

							   
template<typename Function>
struct GetBoundPointer_C {
	static FunctionPointer GetBoundPointer(Function f) {
		using Traits = function_traits<decltype(f)>;
		int arity = Traits::arity;
		return std::make_tuple(false,
			std::bind(static_cast<InvokeResultCodes(*)(Function, InvokeArgs)>
				(&RpcInvoker<decltype(f)>::applyer), f, std::placeholders::_1),
			arity
		);
	}
};

struct GetBoundPointer_CPP {
	template <typename Ret, typename C, typename... Args>
	static FunctionPointer GetBoundPointer(Ret(C::*f)(Args...))
	{
		return std::make_tuple(true,
			std::bind(
				static_cast<InvokeResultCodes(*)(Ret(C::*)(Args...), InvokeArgs)>
				(&RpcInvokerCpp::applyer),
				f,
				std::placeholders::_1
			),
			sizeof...(Args)
		);
	}
};


template<typename Function>
FunctionPointer GetBoundPointer(Function f) {
	return std::conditional<
	std::is_member_function_pointer<Function>::value
	, GetBoundPointer_CPP
	, GetBoundPointer_C<Function>
	>::type::GetBoundPointer(f);
}

// Recursive loop to serialize all arguments into a BitStream.
// Finally, send the call or signal with BitStream.
struct RpcCall {
	template<typename Rpc, typename... Args>
	static inline bool Call(Rpc *rpc, const char *identifier, int argCount,
													bool isCall, const Args&... args) {
		RakNet::BitStream bitStream;
		bool result = false;

		RpcCall::Call(rpc, identifier, bitStream, result, argCount, isCall, args...);

		return result;
	}
    
	template<typename Rpc>
	static inline void Call(Rpc *rpc, const char *identifier,
							RakNet::BitStream &bitStream, bool &result,
							int argCount, bool isCall) {
		if (!isCall) {
			rpc->InvokeSignal(rpc->GetLocalSlotIndex(identifier), &bitStream, true);
		}

		result = rpc->SendCallOrSignal(identifier, argCount, &bitStream, isCall);
	}
	
	template<typename Rpc, typename Arg>
	static inline void Call(Rpc *rpc, const char *identifier,
					RakNet::BitStream &bitStream, bool &result, int argCount,
					bool isCall, Arg &arg) {
		typedef typename std::remove_reference<decltype(arg)>::type arg_type_no_ref;
		_RPC3::SerializeCallParameterBranch<arg_type_no_ref>::type::apply(bitStream, arg);
        
		RpcCall::Call(rpc, identifier, bitStream, result, argCount, isCall);
	}
	
	template<std::size_t I = 0, typename Rpc, typename Arg, typename... Args>
	static inline typename std::enable_if<I < sizeof...(Args), void>::type
			Call(Rpc *rpc, const char *identifier, RakNet::BitStream &bitStream,
				bool &result, int argCount, bool isCall, Arg &arg, const Args&... args) {
		typedef typename std::remove_reference<decltype(arg)>::type arg_type_no_ref;
		_RPC3::SerializeCallParameterBranch<arg_type_no_ref>::type::apply(bitStream, arg);
        
		RpcCall::Call(rpc, identifier, bitStream, result, argCount, isCall, args...);
	}
};

}
}

#endif
