// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GameplayMessageTypes2.generated.h"

class UGameplayMessageRouter;

// Match rule for message listeners
UENUM(BlueprintType)
enum class EGameplayMessageMatch : uint8
{
	// An exact match will only receive messages with exactly the same channel
	// (e.g., registering for "A.B" will match a broadcast of A.B but not A.B.C)
	ExactMatch,

	// A partial match will receive any messages rooted in the same channel
	// (e.g., registering for "A.B" will match a broadcast of A.B as well as A.B.C)
	PartialMatch
};

/**
 * Struct used to specify advanced behavior when registering a listener for gameplay messages
 */
template<typename FMessageStructType>
struct FGameplayMessageListenerParams
{
	/** Whether Callback should be called for broadcasts of more derived channels or if it will only be called for exact matches. */
	EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch;

	/** If bound this callback will trigger when a message is broadcast on the specified channel. */
	TFunction<void(FGameplayTag, const FMessageStructType&)> OnMessageReceivedCallback;
	
	TWeakObjectPtr<UObject> TargetObject;

	EGameplayMessagePriority Priority;

	/** Helper to bind weak member function to OnMessageReceivedCallback */
	template<typename TOwner = UObject>
	void SetMessageReceivedCallback(TOwner* Object, void(TOwner::* Function)(FGameplayTag, const FMessageStructType&))
	{
		TWeakObjectPtr<TOwner> WeakObject(Object);
		OnMessageReceivedCallback = [WeakObject, Function](FGameplayTag Channel, const FMessageStructType& Payload)
		{
			if (TOwner* StrongObject = WeakObject.Get())
			{
				(StrongObject->*Function)(Channel, Payload);
			}
		};
	}
};

// Match rule for message listeners
UENUM(BlueprintType)
enum class EGameplayMessagePriority : uint8
{
	HIGHEST = 0,
	HIGHER = 25,
	DEFAULT = 50,
	LOWER = 75,
	LOWEST = 100,
	MONITOR = 255,
};

USTRUCT(BlueprintType)
struct GAMEPLAYMESSAGERUNTIME_API FGameplayMessageBroadcastResult
{
	GENERATED_BODY()
	
public:
	
	// Message cancel state, reset for each message broadcast. If true means message has been cancelled by gameplay logic.
	UPROPERTY(BlueprintReadOnly)
	bool bCancelled = false;
	// Message interrupted state, reset for each message broadcast. If true means the reset listener should be interrupted.
	UPROPERTY(BlueprintReadOnly)
	bool bInterrupted = false;

	void Reset()
	{
		bCancelled = false;
		bInterrupted = false;
	}
	
};

/**
 * Struct used to specify spatial listener parameters for world message system
 */
template<typename FMessageStructType>
struct FGameplayWorldMessageListenerParams
{
	/** Whether Callback should be called for broadcasts of more derived channels or if it will only be called for exact matches. */
	EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch;

	/** If bound this callback will trigger when a message is broadcast on the specified channel. */
	TFunction<void(FGameplayTag, const FMessageStructType&)> OnMessageReceivedCallback;
	
	/** The center position for listening to spatial messages */
	FVector ListenPosition = FVector::ZeroVector;
	
	/** The radius within which to listen for spatial messages */
	float ListenRadius = 0.0f;

	/** Priority of the listener */
	EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT;

	/** Helper to bind weak member function to OnMessageReceivedCallback */
	template<typename TOwner = UObject>
	void SetMessageReceivedCallback(TOwner* Object, void(TOwner::* Function)(FGameplayTag, const FMessageStructType&))
	{
		TWeakObjectPtr<TOwner> WeakObject(Object);
		OnMessageReceivedCallback = [WeakObject, Function](FGameplayTag Channel, const FMessageStructType& Payload)
		{
			if (TOwner* StrongObject = WeakObject.Get())
			{
				(StrongObject->*Function)(Channel, Payload);
			}
		};
	}
};