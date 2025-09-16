// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CancellableAsyncAction.h"
#include "GameplayWorldMessageSubsystem.h"
#include "GameplayMessageTypes2.h"

#include "AsyncAction_ListenForGameplayWorldMessage.generated.h"

class UScriptStruct;
class UWorld;
struct FFrame;

/**
 * Proxy object pin will be hidden in UK2Node_AsyncAction_ListenForGameplayWorldMessages. Is used to get a reference to the object triggering the delegate for the follow up call of 'GetPayload'.
 *
 * @param ActualChannel		The actual message channel that we received Payload from (will always start with Channel, but may be more specific if partial matches were enabled)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAsyncGameplayWorldMessageDelegate, UAsyncAction_ListenForGameplayWorldMessage*, ProxyObject, FGameplayTag, ActualChannel);

UCLASS(BlueprintType, meta=(HasDedicatedAsyncNode))
class GAMEPLAYMESSAGERUNTIME_API UAsyncAction_ListenForGameplayWorldMessage : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	/**
	 * Asynchronously waits for a spatial gameplay message to be broadcast on the specified channel within a radius.
	 *
	 * @param Channel			The message channel to listen for
	 * @param PayloadType		The kind of message structure to use (this must match the same type that the sender is broadcasting)
	 * @param ListenPosition	The world position to listen from
	 * @param ListenRadius		The radius within which to receive messages
	 * @param MatchType			The rule used for matching the channel with broadcasted messages
	 * @param Priority			Priority of the listener
	 */
	UFUNCTION(BlueprintCallable, Category = Messaging, meta = (WorldContext = "WorldContextObject", DefaultToSelf="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UAsyncAction_ListenForGameplayWorldMessage* ListenForGameplayWorldMessages(UObject* WorldContextObject, FGameplayTag Channel, UScriptStruct* PayloadType, FVector ListenPosition, float ListenRadius, EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch, EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT);

	/**
	 * Asynchronously waits for a simple spatial gameplay message to be broadcast within a radius.
	 *
	 * @param PayloadType		The kind of message structure to use (this must match the same type that the sender is broadcasting)
	 * @param ListenPosition	The world position to listen from
	 * @param ListenRadius		The radius within which to receive messages
	 * @param Priority			Priority of the listener
	 */
	UFUNCTION(BlueprintCallable, Category = Messaging, meta = (WorldContext = "WorldContextObject", DefaultToSelf="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UAsyncAction_ListenForGameplayWorldMessage* SimpleListenForGameplayWorldMessages(UObject* WorldContextObject, UScriptStruct* PayloadType, FVector ListenPosition, float ListenRadius, EGameplayMessagePriority Priority = EGameplayMessagePriority::DEFAULT);

	/**
	 * Attempt to copy the payload received from the broadcasted spatial gameplay message into the specified wildcard.
	 * The wildcard's type must match the type from the received message.
	 *
	 * @param OutPayload	The wildcard reference the payload should be copied into
	 * @return				If the copy was a success
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Messaging", meta = (CustomStructureParam = "OutPayload"))
	bool GetPayload(UPARAM(ref) int32& OutPayload);
	DECLARE_FUNCTION(execGetPayload);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Messaging", meta = (CustomStructureParam = "inPayload", BlueprintInternalUseOnly = "true"))
	void OverridePayload(const int32& InPayload);
	DECLARE_FUNCTION(execOverridePayload);

	virtual void Activate() override;
	virtual void SetReadyToDestroy() override;

public:
	/** Called when a spatial message is broadcast on the specified channel within the listen radius. Use GetPayload() to request the message payload. */
	UPROPERTY(BlueprintAssignable)
	FAsyncGameplayWorldMessageDelegate OnMessageReceived;

private:
	void HandleMessageReceived(FGameplayTag Channel, const UScriptStruct* StructType, void* Payload);

private:
	void* ReceivedMessagePayloadPtr = nullptr;

	TWeakObjectPtr<UWorld> WorldPtr;
	FGameplayTag ChannelToRegister;
	TWeakObjectPtr<UScriptStruct> MessageStructType = nullptr;
	EGameplayMessageMatch MessageMatchType = EGameplayMessageMatch::ExactMatch;
	EGameplayMessagePriority Priority;

	// Spatial listening parameters
	FVector ListenPosition = FVector::ZeroVector;
	float ListenRadius = 0.0f;

	FGameplayWorldMessageListenerHandle ListenerHandle;
};
