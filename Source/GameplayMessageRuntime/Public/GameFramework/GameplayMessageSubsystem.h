// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageTypes2.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/WeakObjectPtr.h"

#include "GameplayMessageSubsystem.generated.h"

class UGameplayMessageSubsystem;
struct FFrame;

GAMEPLAYMESSAGERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogGameplayMessageSubsystem, Log, All);

namespace UE::GameplayMessageSubsystem
{
	GAMEPLAYMESSAGERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DefaultMessageChannel);
}

class UAsyncAction_ListenForGameplayMessage;

/**
 * An opaque handle that can be used to remove a previously registered message listener
 * @see UGameplayMessageSubsystem::RegisterListener and UGameplayMessageSubsystem::UnregisterListener
 */
USTRUCT(BlueprintType)
struct GAMEPLAYMESSAGERUNTIME_API FGameplayMessageListenerHandle
{
public:
	GENERATED_BODY()

	FGameplayMessageListenerHandle() {}

	void Unregister();

	bool IsValid() const { return ID != 0; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UGameplayMessageSubsystem> Subsystem;

	UPROPERTY(Transient)
	const UScriptStruct* StructType;

	UPROPERTY(Transient)
	int32 ID = 0;

	FDelegateHandle StateClearedHandle;

	friend UGameplayMessageSubsystem;

	FGameplayMessageListenerHandle(UGameplayMessageSubsystem* InSubsystem, const UScriptStruct* InStructType, int32 InID) : Subsystem(InSubsystem), StructType(InStructType), ID(InID) {}
};

/** 
 * Entry information for a single registered listener
 */
USTRUCT()
struct FGameplayMessageListenerData
{
	GENERATED_BODY()

	// Callback for when a message has been received
	TFunction<void(FGameplayTag, const UScriptStruct*, void*)> ReceivedCallback;

	int32 HandleID;
	EGameplayMessageMatch MatchType;

	// Adding some logging and extra variables around some potential problems with this
	TWeakObjectPtr<const UScriptStruct> ListenerStructType = nullptr;

	// 注册时的Channel
	FGameplayTag Channel = FGameplayTag::EmptyTag;

	int32 Priority;

	// Listen Object
	TWeakObjectPtr<UObject> TargetObject = nullptr;
};

/**
 * This system allows event raisers and listeners to register for messages without
 * having to know about each other directly, though they must agree on the format
 * of the message (as a USTRUCT() type).
 *
 *
 * You can get to the message router from the game instance:
 *    UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(GameInstance)
 * or directly from anything that has a route to a world:
 *    UGameplayMessageSubsystem::Get(WorldContextObject)
 *
 * Note that call order when there are multiple listeners for the same channel is
 * not guaranteed and can change over time!
 */
UCLASS()
class GAMEPLAYMESSAGERUNTIME_API UGameplayMessageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	friend UAsyncAction_ListenForGameplayMessage;

public:

	/**
	 * @return the message router for the game instance associated with the world of the specified object
	 */
	static UGameplayMessageSubsystem& Get(const UObject* WorldContextObject);

	/**
	 * @return true if a valid GameplayMessageRouter subsystem if active in the provided world
	 */
	static bool HasInstance(const UObject* WorldContextObject);

	//~USubsystem interface
	virtual void Deinitialize() override;
	//~End of USubsystem interface

	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 * @param Channel			The message channel to broadcast on
	 */
	template <typename FMessageStructType>
	FGameplayMessageBroadcastResult BroadcastMessage(FMessageStructType& Message, FGameplayTag Channel, TWeakObjectPtr<UObject> TargetObject = nullptr)
	{
		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		return BroadcastMessageInternal(Channel, StructType, &Message, TargetObject);
	}

	/**
	 * Broadcast a message
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	template <typename FMessageStructType>
	FGameplayMessageBroadcastResult BroadcastSimpleMessage(FMessageStructType& Message, TWeakObjectPtr<UObject> TargetObject = nullptr)
	{
		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		return BroadcastMessageInternal(UE::GameplayMessageSubsystem::TAG_DefaultMessageChannel, StructType, &Message, TargetObject);
	}

	/**
	 * Register to receive messages
	 *
	 * @param Callback			Function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayMessageListenerHandle RegisterListener(TFunction<void(FGameplayTag, const FMessageStructType&)>&& Callback, EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT, TWeakObjectPtr<UObject> TargetObject = nullptr)
	{
		auto ThunkCallback = [InnerCallback = MoveTemp(Callback)](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, const void* SenderPayload)
		{
			InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
		};

		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		
		return RegisterListenerInternal(UE::GameplayMessageSubsystem::TAG_DefaultMessageChannel, ThunkCallback, StructType, EGameplayMessageMatch::PartialMatch, static_cast<int32>(Priority), TargetObject);
	}

	/**
	 * Register to receive messages on a specified channel
	 *
	 * @param Channel			The message channel to listen to
	 * @param Callback			Function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, TFunction<void(FGameplayTag, const FMessageStructType&)>&& Callback, EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch, EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT, TWeakObjectPtr<UObject> TargetObject = nullptr)
	{
		auto ThunkCallback = [InnerCallback = MoveTemp(Callback)](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, void* SenderPayload)
		{
			InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
		};

		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		
		return RegisterListenerInternal(Channel, ThunkCallback, StructType, MatchType, static_cast<int32>(Priority), TargetObject);
	}

	/**
	 * Register to receive messages on a specified channel and handle it with a specified member function
	 * Executes a weak object validity check to ensure the object registering the function still exists before triggering the callback
	 *
	 * @param Channel			The message channel to listen to
	 * @param Object			The object instance to call the function on
	 * @param Function			Member function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType, typename TOwner = UObject>
	FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, TOwner* Object, void(TOwner::* Function)(FGameplayTag, const FMessageStructType&))
	{
		TWeakObjectPtr<TOwner> WeakObject(Object);
		return RegisterListener<FMessageStructType>(Channel,
													[WeakObject, Function](FGameplayTag Channel, FMessageStructType& Payload)
													{
														if (TOwner* StrongObject = WeakObject.Get())
														{
															(StrongObject->*Function)(Channel, Payload);
														}
													});
	}

	/**
	 * Register to receive messages on a specified channel with extra parameters to support advanced behavior
	 * The stateful part of this logic should probably be separated out to a separate system
	 *
	 * @param Channel			The message channel to listen to
	 * @param Params			Structure containing details for advanced behavior
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, FGameplayMessageListenerParams<FMessageStructType>& Params)
	{
		FGameplayMessageListenerHandle Handle;

		// Register to receive any future messages broadcast on this channel
		if (Params.OnMessageReceivedCallback)
		{
			auto ThunkCallback = [InnerCallback = Params.OnMessageReceivedCallback](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, void* SenderPayload)
			{
				InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
			};

			const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
			Handle = RegisterListenerInternal(Channel, ThunkCallback, StructType, Params.MatchType, static_cast<int32>(Params.Priority), Params.TargetObject);
		}

		return Handle;
	}

	/**
	 * Remove a message listener previously registered by RegisterListener
	 *
	 * @param Handle	The handle returned by RegisterListener
	 */
	void UnregisterListener(FGameplayMessageListenerHandle Handle);

	/**
	 * Mark current message context as cancelled
	 * @param WorldContext Context to get GameplayMessageSubsystem
	 * @param bCancel Should cancel current message
	 * @param bInterrupted Should interrupt current message
	 */
	UFUNCTION(BlueprintCallable, Category=Messaging, meta=(HidePin = "WorldContext", DefaultToSelf = "WorldContext"))
	static void CancelCurrentMessage(UObject* WorldContext, bool bCancel = true, bool bInterrupted = true);

	/**
	 * Mark current message context as cancelled
	 * @param bCancel Should cancel current message
	 * @param bInterrupt Should interrupt current message
	 */
	UFUNCTION(BlueprintCallable, Category=Messaging)
	void CancelMessage(bool bCancel = true, bool bInterrupt = true);

protected:
	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Channel			The message channel to broadcast on
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Message"))
	FGameplayMessageBroadcastResult K2_BroadcastMessage(FGameplayTag Channel, UPARAM(ref) int32& Message);
	DECLARE_FUNCTION(execK2_BroadcastMessage);

	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Simple Message"))
	FGameplayMessageBroadcastResult K2_BroadcastSimpleMessage(UPARAM(ref) int32& Message);
	DECLARE_FUNCTION(execK2_BroadcastSimpleMessage);
	
	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Channel			The message channel to broadcast on
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Object Message"))
	FGameplayMessageBroadcastResult K2_BroadcastObjectMessage(FGameplayTag Channel, UPARAM(ref) int32& Message, UObject* TargetObject = nullptr);
	DECLARE_FUNCTION(execK2_BroadcastObjectMessage);

	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Simple Object Message"))
	FGameplayMessageBroadcastResult K2_BroadcastSimpleObjectMessage(UPARAM(ref) int32& Message, UObject* TargetObject = nullptr);
	DECLARE_FUNCTION(execK2_BroadcastSimpleObjectMessage);

private:
	// Internal helper for broadcasting a message
	FGameplayMessageBroadcastResult BroadcastMessageInternal(FGameplayTag Channel, const UScriptStruct* StructType, void* MessageBytes, TWeakObjectPtr<UObject> TargetObject = nullptr);

	// Internal helper for registering a message listener
	FGameplayMessageListenerHandle RegisterListenerInternal(
		FGameplayTag Channel, 
		TFunction<void(FGameplayTag, const UScriptStruct*, void*)>&& Callback,
		const UScriptStruct* StructType,
		EGameplayMessageMatch MatchType,
		int32 Priority = static_cast<int32>(EGameplayMessagePriority::DEFAULT),
		TWeakObjectPtr<UObject> TargetObject = nullptr);
	
	void UnregisterListenerInternal(const UScriptStruct* StructType, int32 HandleID);

	// Message execute context, reset for each message broadcast
	FGameplayMessageBroadcastResult BroadcastResultCache;

private:
	// List of all entries for a given channel
	struct FChannelListenerList
	{
		TArray<FGameplayMessageListenerData> Listeners;
		int32 HandleID = 0;
	};

private:
	TMap<const UScriptStruct*, FChannelListenerList> ListenerMap;
};
