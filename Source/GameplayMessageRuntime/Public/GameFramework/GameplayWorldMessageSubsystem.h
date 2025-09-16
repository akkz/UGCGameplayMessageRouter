// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageTypes2.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"

#include "GameplayWorldMessageSubsystem.generated.h"

class UGameplayWorldMessageSubsystem;
struct FFrame;

GAMEPLAYMESSAGERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogGameplayWorldMessageSubsystem, Log, All);

namespace UE::GameplayWorldMessageSubsystem
{
	GAMEPLAYMESSAGERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DefaultMessageChannel);
	
	// Grid size constant - each grid cell is 16m x 16m
	constexpr float GRID_SIZE = 1600.0f; // 16m * 100 (UE units)
	
	// Helper functions for grid coordinate conversion
	GAMEPLAYMESSAGERUNTIME_API int64 GetGridID(const FVector& WorldPosition);
	GAMEPLAYMESSAGERUNTIME_API FVector GetGridCenter(int64 GridID);
	GAMEPLAYMESSAGERUNTIME_API TArray<int64> GetGridsInRadius(const FVector& Center, float Radius);
}

class UAsyncAction_ListenForGameplayWorldMessage;

/**
 * An opaque handle that can be used to remove a previously registered spatial message listener
 * @see UGameplayWorldMessageSubsystem::RegisterListener and UGameplayWorldMessageSubsystem::UnregisterListener
 */
USTRUCT(BlueprintType)
struct GAMEPLAYMESSAGERUNTIME_API FGameplayWorldMessageListenerHandle
{
public:
	GENERATED_BODY()

	FGameplayWorldMessageListenerHandle() {}

	void Unregister();

	bool IsValid() const { return ID != 0; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UGameplayWorldMessageSubsystem> Subsystem;

	UPROPERTY(Transient)
	const UScriptStruct* StructType;

	UPROPERTY(Transient)
	int32 ID = 0;

	FDelegateHandle StateClearedHandle;

	friend UGameplayWorldMessageSubsystem;

	FGameplayWorldMessageListenerHandle(UGameplayWorldMessageSubsystem* InSubsystem, const UScriptStruct* InStructType, int32 InID) : Subsystem(InSubsystem), StructType(InStructType), ID(InID) {}
};

/** 
 * Entry information for a single registered spatial listener
 */
USTRUCT()
struct FGameplayWorldMessageListenerData
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

	// Spatial listening parameters
	FVector ListenPosition = FVector::ZeroVector;
	float ListenRadius = 0.0f;
};

/**
 * This system allows event raisers and listeners to register for spatial messages without
 * having to know about each other directly, though they must agree on the format
 * of the message (as a USTRUCT() type).
 *
 * Messages are broadcast at specific world coordinates and listeners can register to
 * receive messages within a specified radius from their listening position.
 *
 * You can get to the message router from the world:
 *    UWorld::GetSubsystem<UGameplayWorldMessageSubsystem>(World)
 * or directly from anything that has a route to a world:
 *    UGameplayWorldMessageSubsystem::Get(WorldContextObject)
 *
 * Note that call order when there are multiple listeners for the same channel is
 * not guaranteed and can change over time!
 */
UCLASS()
class GAMEPLAYMESSAGERUNTIME_API UGameplayWorldMessageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend UAsyncAction_ListenForGameplayWorldMessage;

public:

	/**
	 * @return the spatial message router for the world of the specified object
	 */
	static UGameplayWorldMessageSubsystem& Get(const UObject* WorldContextObject);

	/**
	 * @return true if a valid GameplayWorldMessageSubsystem is active in the provided world
	 */
	static bool HasInstance(const UObject* WorldContextObject);

	//~USubsystem interface
	virtual void Deinitialize() override;
	//~End of USubsystem interface

	/**
	 * Broadcast a spatial message at the specified world position
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 * @param Channel			The message channel to broadcast on
	 * @param WorldPosition		The world position where the message is broadcast
	 */
	template <typename FMessageStructType>
	FGameplayMessageBroadcastResult BroadcastMessage(FMessageStructType& Message, FGameplayTag Channel, const FVector& WorldPosition)
	{
		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		return BroadcastMessageInternal(Channel, StructType, &Message, WorldPosition);
	}

	/**
	 * Broadcast a simple spatial message at the specified world position
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 * @param WorldPosition		The world position where the message is broadcast
	 */
	template <typename FMessageStructType>
	FGameplayMessageBroadcastResult BroadcastSimpleMessage(FMessageStructType& Message, const FVector& WorldPosition)
	{
		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		return BroadcastMessageInternal(UE::GameplayWorldMessageSubsystem::TAG_DefaultMessageChannel, StructType, &Message, WorldPosition);
	}

	/**
	 * Register to receive spatial messages within a specified radius
	 *
	 * @param Callback			Function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 * @param ListenPosition	The world position to listen from
	 * @param ListenRadius		The radius within which to receive messages
	 * @param Priority			Priority of the listener
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayWorldMessageListenerHandle RegisterListener(TFunction<void(FGameplayTag, const FMessageStructType&)>&& Callback, const FVector& ListenPosition, float ListenRadius, EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT)
	{
		auto ThunkCallback = [InnerCallback = MoveTemp(Callback)](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, const void* SenderPayload)
		{
			InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
		};

		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		
		return RegisterListenerInternal(UE::GameplayWorldMessageSubsystem::TAG_DefaultMessageChannel, ThunkCallback, StructType, EGameplayMessageMatch::PartialMatch, static_cast<int32>(Priority), ListenPosition, ListenRadius);
	}

	/**
	 * Register to receive spatial messages on a specified channel within a specified radius
	 *
	 * @param Channel			The message channel to listen to
	 * @param Callback			Function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 * @param ListenPosition	The world position to listen from
	 * @param ListenRadius		The radius within which to receive messages
	 * @param MatchType			How to match the channel tags
	 * @param Priority			Priority of the listener
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayWorldMessageListenerHandle RegisterListener(FGameplayTag Channel, TFunction<void(FGameplayTag, const FMessageStructType&)>&& Callback, const FVector& ListenPosition, float ListenRadius, EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch, EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT)
	{
		auto ThunkCallback = [InnerCallback = MoveTemp(Callback)](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, void* SenderPayload)
		{
			InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
		};

		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		
		return RegisterListenerInternal(Channel, ThunkCallback, StructType, MatchType, static_cast<int32>(Priority), ListenPosition, ListenRadius);
	}

	/**
	 * Register to receive spatial messages on a specified channel and handle it with a specified member function
	 * Executes a weak object validity check to ensure the object registering the function still exists before triggering the callback
	 *
	 * @param Channel			The message channel to listen to
	 * @param Object			The object instance to call the function on
	 * @param Function			Member function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 * @param ListenPosition	The world position to listen from
	 * @param ListenRadius		The radius within which to receive messages
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType, typename TOwner = UObject>
	FGameplayWorldMessageListenerHandle RegisterListener(FGameplayTag Channel, TOwner* Object, void(TOwner::* Function)(FGameplayTag, const FMessageStructType&), const FVector& ListenPosition, float ListenRadius)
	{
		TWeakObjectPtr<TOwner> WeakObject(Object);
		return RegisterListener<FMessageStructType>(Channel,
													[WeakObject, Function](FGameplayTag Channel, FMessageStructType& Payload)
													{
														if (TOwner* StrongObject = WeakObject.Get())
														{
															(StrongObject->*Function)(Channel, Payload);
														}
													}, ListenPosition, ListenRadius);
	}

	/**
	 * Register to receive spatial messages on a specified channel with extra parameters to support advanced behavior
	 *
	 * @param Channel			The message channel to listen to
	 * @param Params			Structure containing details for advanced spatial behavior
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayWorldMessageListenerHandle RegisterListener(FGameplayTag Channel, FGameplayWorldMessageListenerParams<FMessageStructType>& Params)
	{
		FGameplayWorldMessageListenerHandle Handle;

		// Register to receive any future messages broadcast on this channel
		if (Params.OnMessageReceivedCallback)
		{
			auto ThunkCallback = [InnerCallback = Params.OnMessageReceivedCallback](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, void* SenderPayload)
			{
				InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
			};

			const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
			Handle = RegisterListenerInternal(Channel, ThunkCallback, StructType, Params.MatchType, static_cast<int32>(Params.Priority), Params.ListenPosition, Params.ListenRadius);
		}

		return Handle;
	}

	/**
	 * Remove a message listener previously registered by RegisterListener
	 *
	 * @param Handle	The handle returned by RegisterListener
	 */
	void UnregisterListener(FGameplayWorldMessageListenerHandle Handle);

	/**
	 * Update the listening location for a previously registered listener
	 * This efficiently moves the listener to the new grid cells based on the new position and radius
	 *
	 * @param Handle			The handle returned by RegisterListener
	 * @param NewListenPosition	The new world position to listen from
	 * @param NewListenRadius	The new radius within which to receive messages (optional, uses existing radius if not specified)
	 *
	 * @return true if the listener was successfully updated, false if the handle was invalid
	 */
	bool UpdateRegisterListenerLocation(FGameplayWorldMessageListenerHandle Handle, const FVector& NewListenPosition, float NewListenRadius = -1.0f);

	/**
	 * Mark current message context as cancelled
	 * @param WorldContext Context to get GameplayWorldMessageSubsystem
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
	 * Broadcast a spatial message at the specified world position
	 *
	 * @param Channel			The message channel to broadcast on
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 * @param WorldPosition		The world position where the message is broadcast
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Spatial Message"))
	FGameplayMessageBroadcastResult K2_BroadcastMessage(FGameplayTag Channel, UPARAM(ref) int32& Message, FVector WorldPosition);
	DECLARE_FUNCTION(execK2_BroadcastMessage);

	/**
	 * Broadcast a simple spatial message at the specified world position
	 *
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 * @param WorldPosition		The world position where the message is broadcast
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Simple Spatial Message"))
	FGameplayMessageBroadcastResult K2_BroadcastSimpleMessage(UPARAM(ref) int32& Message, FVector WorldPosition);
	DECLARE_FUNCTION(execK2_BroadcastSimpleMessage);

private:
	// Internal helper for broadcasting a spatial message
	FGameplayMessageBroadcastResult BroadcastMessageInternal(FGameplayTag Channel, const UScriptStruct* StructType, void* MessageBytes, const FVector& WorldPosition);

	// Internal helper for registering a spatial message listener
	FGameplayWorldMessageListenerHandle RegisterListenerInternal(
		FGameplayTag Channel, 
		TFunction<void(FGameplayTag, const UScriptStruct*, void*)>&& Callback,
		const UScriptStruct* StructType,
		EGameplayMessageMatch MatchType,
		int32 Priority = static_cast<int32>(EGameplayMessagePriority::DEFAULT),
		const FVector& ListenPosition = FVector::ZeroVector,
		float ListenRadius = 0.0f);
	
	void UnregisterListenerInternal(const UScriptStruct* StructType, int32 HandleID);

	// Message execute context, reset for each message broadcast
	FGameplayMessageBroadcastResult BroadcastResultCache;

private:
	// Grid-based listener storage for spatial queries
	struct FGridListenerList
	{
		TArray<FGameplayWorldMessageListenerData> Listeners;
		int32 HandleID = 0;
	};

	// Map from GridID to listeners in that grid
	TMap<int64, FGridListenerList> GridListenerMap;
	
	// Direct mapping from HandleID to spatial info for efficient cleanup
	struct FListenerSpatialInfo
	{
		FVector ListenPosition = FVector::ZeroVector;
		float ListenRadius = 0.0f;
	};
	TMap<int32, FListenerSpatialInfo> HandleToSpatialMap;
};
