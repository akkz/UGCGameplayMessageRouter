// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/AsyncAction_ListenForGameplayWorldMessage.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameplayWorldMessageSubsystem.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Stack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ListenForGameplayWorldMessage)

UAsyncAction_ListenForGameplayWorldMessage* UAsyncAction_ListenForGameplayWorldMessage::ListenForGameplayWorldMessages(UObject* WorldContextObject, FGameplayTag Channel, UScriptStruct* PayloadType, FVector ListenPosition, float ListenRadius, EGameplayMessageMatch MatchType, EGameplayMessagePriority Priority)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UAsyncAction_ListenForGameplayWorldMessage* Action = NewObject<UAsyncAction_ListenForGameplayWorldMessage>();
	Action->WorldPtr = World;
	Action->ChannelToRegister = Channel;
	Action->MessageStructType = PayloadType;
	Action->MessageMatchType = MatchType;
	Action->Priority = Priority;
	Action->ListenPosition = ListenPosition;
	Action->ListenRadius = ListenRadius;
	Action->RegisterWithGameInstance(World);

	return Action;
}

UAsyncAction_ListenForGameplayWorldMessage* UAsyncAction_ListenForGameplayWorldMessage::SimpleListenForGameplayWorldMessages(UObject* WorldContextObject, UScriptStruct* PayloadType, FVector ListenPosition, float ListenRadius, EGameplayMessagePriority Priority)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UAsyncAction_ListenForGameplayWorldMessage* Action = NewObject<UAsyncAction_ListenForGameplayWorldMessage>();
	Action->WorldPtr = World;
	Action->ChannelToRegister = UE::GameplayWorldMessageSubsystem::TAG_DefaultMessageChannel;
	Action->MessageStructType = PayloadType;
	Action->MessageMatchType = EGameplayMessageMatch::PartialMatch;
	Action->Priority = Priority;
	Action->ListenPosition = ListenPosition;
	Action->ListenRadius = ListenRadius;
	Action->RegisterWithGameInstance(World);

	return Action;
}

void UAsyncAction_ListenForGameplayWorldMessage::Activate()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (UGameplayWorldMessageSubsystem::HasInstance(World))
		{
			UGameplayWorldMessageSubsystem& Router = UGameplayWorldMessageSubsystem::Get(World);

			TWeakObjectPtr<UAsyncAction_ListenForGameplayWorldMessage> WeakThis(this);
			ListenerHandle = Router.RegisterListenerInternal(ChannelToRegister,
				[WeakThis](FGameplayTag Channel, const UScriptStruct* StructType, void* Payload)
				{
					if (UAsyncAction_ListenForGameplayWorldMessage* StrongThis = WeakThis.Get())
					{
						StrongThis->HandleMessageReceived(Channel, StructType, Payload);
					}
				},
				MessageStructType.Get(),
				MessageMatchType,
				static_cast<int32>(Priority),
				ListenPosition,
				ListenRadius);

			return;
		}
	}

	SetReadyToDestroy();
}

void UAsyncAction_ListenForGameplayWorldMessage::SetReadyToDestroy()
{
	ListenerHandle.Unregister();

	Super::SetReadyToDestroy();
}

bool UAsyncAction_ListenForGameplayWorldMessage::GetPayload(int32& OutPayload)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UAsyncAction_ListenForGameplayWorldMessage::execGetPayload)
{
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	P_FINISH;

	bool bSuccess = false;

	// Make sure the type we are trying to get through the blueprint node matches the type of the message payload received.
	if ((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr) && (StructProp->Struct == P_THIS->MessageStructType.Get()) && (P_THIS->ReceivedMessagePayloadPtr != nullptr))
	{
		StructProp->Struct->CopyScriptStruct(MessagePtr, P_THIS->ReceivedMessagePayloadPtr);
		bSuccess = true;
	}

	*(bool*)RESULT_PARAM = bSuccess;
}

void UAsyncAction_ListenForGameplayWorldMessage::OverridePayload(const int32& InPayload)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return;
}

DEFINE_FUNCTION(UAsyncAction_ListenForGameplayWorldMessage::execOverridePayload)
{
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		if (P_THIS->ReceivedMessagePayloadPtr && StructProp->Struct == P_THIS->MessageStructType.Get())
		{
			StructProp->Struct->CopyScriptStruct(P_THIS->ReceivedMessagePayloadPtr, MessagePtr);
		}
	}
}

void UAsyncAction_ListenForGameplayWorldMessage::HandleMessageReceived(FGameplayTag Channel, const UScriptStruct* StructType, void* Payload)
{
	if (!MessageStructType.Get() || (MessageStructType.Get() == StructType))
	{
		ReceivedMessagePayloadPtr = Payload;

		OnMessageReceived.Broadcast(this, Channel);

		ReceivedMessagePayloadPtr = nullptr;
	}

	if (!OnMessageReceived.IsBound())
	{
		// If the BP object that created the async node is destroyed, OnMessageReceived will be unbound after calling the broadcast.
		// In this case we can safely mark this receiver as ready for destruction.
		SetReadyToDestroy();
	}
}
