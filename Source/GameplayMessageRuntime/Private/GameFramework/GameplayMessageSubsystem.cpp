// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Stack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayMessageSubsystem)

DEFINE_LOG_CATEGORY(LogGameplayMessageSubsystem);

namespace UE
{
	namespace GameplayMessageSubsystem
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_DefaultMessageChannel, "Message", "Message default channel");
		
		static int32 ShouldLogMessages = 0;
		static FAutoConsoleVariableRef CVarShouldLogMessages(TEXT("GameplayMessageSubsystem.LogMessages"),
			ShouldLogMessages,
			TEXT("Should messages broadcast through the gameplay message subsystem be logged?"));
	}
}

//////////////////////////////////////////////////////////////////////
// FGameplayMessageListenerHandle

void FGameplayMessageListenerHandle::Unregister()
{
	if (UGameplayMessageSubsystem* StrongSubsystem = Subsystem.Get())
	{
		StrongSubsystem->UnregisterListener(*this);
		Subsystem.Reset();
		StructType = nullptr;
		ID = 0;
	}
}

//////////////////////////////////////////////////////////////////////
// UGameplayMessageSubsystem

UGameplayMessageSubsystem& UGameplayMessageSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert);
	check(World);
	UGameplayMessageSubsystem* Router = UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(World->GetGameInstance());
	check(Router);
	return *Router;
}

bool UGameplayMessageSubsystem::HasInstance(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert);
	UGameplayMessageSubsystem* Router = World != nullptr ? UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(World->GetGameInstance()) : nullptr;
	return Router != nullptr;
}

void UGameplayMessageSubsystem::Deinitialize()
{
	ListenerMap.Reset();

	Super::Deinitialize();
}

FGameplayMessageBroadcastResult UGameplayMessageSubsystem::BroadcastMessageInternal(FGameplayTag Channel, const UScriptStruct* StructType, void* MessageBytes, TWeakObjectPtr<UObject> TargetObject)
{
	// Log the message if enabled
	if (UE::GameplayMessageSubsystem::ShouldLogMessages != 0)
	{
		FString* pContextString = nullptr;
#if WITH_EDITOR
		if (GIsEditor)
		{
			extern ENGINE_API FString GPlayInEditorContextString;
			pContextString = &GPlayInEditorContextString;
		}
#endif

		FString HumanReadableMessage;
		StructType->ExportText(/*out*/ HumanReadableMessage, MessageBytes, /*Defaults=*/ nullptr, /*OwnerObject=*/ nullptr, PPF_None, /*ExportRootScope=*/ nullptr);
		UE_LOG(LogGameplayMessageSubsystem, Log, TEXT("BroadcastMessage(%s, %s, %s)"), pContextString ? **pContextString : *GetPathNameSafe(this), *Channel.ToString(), *HumanReadableMessage);
	}

	// Reset State
	BroadcastResultCache.Reset();

	// Broadcast the message
	if (const FChannelListenerList* pList = ListenerMap.Find(StructType))
	{
		// Copy in case there are removals while handling callbacks
		TArray<FGameplayMessageListenerData> ListenerArray(pList->Listeners);

		for (const FGameplayMessageListenerData& Listener : ListenerArray)
		{
			if (!Listener.ListenerStructType.IsValid())
			{
				UE_LOG(LogGameplayMessageSubsystem, Warning, TEXT("Listener struct type has gone invalid on Channel %s. Removing listener from list"), *Channel.ToString());
				UnregisterListenerInternal(StructType, Listener.HandleID);
				continue;
			}

			// 检查结构体类型是否匹配
			if (StructType != Listener.ListenerStructType.Get())
			{
				continue;
			}

			// 检查 TargetObject 是否匹配。如果 TargetObject 未设置，或者TargetObject就是目标（需要确认TargetObject是Valid，不然==会误判）
			if (!Listener.TargetObject.IsExplicitlyNull() && Listener.TargetObject != TargetObject)
			{
				continue;
			}

			// 检查Tag是否匹配
			bool bMatchAny = Listener.MatchType == EGameplayMessageMatch::PartialMatch && Channel.MatchesTag(Listener.Channel);
			bool bMatchExact = Listener.MatchType == EGameplayMessageMatch::ExactMatch && Channel.MatchesTagExact(Listener.Channel);
			if (!bMatchAny && !bMatchExact)
			{
				continue;
			}

			// 执行
			Listener.ReceivedCallback(Channel, StructType, MessageBytes);

			// Check the message has been interrupted
			if (BroadcastResultCache.bInterrupted)
			{
				break;
			}
		}
	}

	return BroadcastResultCache;
}

FGameplayMessageBroadcastResult UGameplayMessageSubsystem::K2_BroadcastMessage(FGameplayTag Channel, UPARAM(ref) int32& Message)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return FGameplayMessageBroadcastResult();
}

DEFINE_FUNCTION(UGameplayMessageSubsystem::execK2_BroadcastMessage)
{
	P_GET_STRUCT(FGameplayTag, Channel);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	FGameplayMessageBroadcastResult Result;
	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		Result = P_THIS->BroadcastMessageInternal(Channel, StructProp->Struct, MessagePtr);
	}

	*(FGameplayMessageBroadcastResult*)RESULT_PARAM = Result;
}

FGameplayMessageBroadcastResult UGameplayMessageSubsystem::K2_BroadcastSimpleMessage(UPARAM(ref) int32& Message)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return FGameplayMessageBroadcastResult();
}

DEFINE_FUNCTION(UGameplayMessageSubsystem::execK2_BroadcastSimpleMessage)
{
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	FGameplayMessageBroadcastResult Result;
	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		Result = P_THIS->BroadcastMessageInternal(UE::GameplayMessageSubsystem::TAG_DefaultMessageChannel, StructProp->Struct, MessagePtr);
	}

	*(FGameplayMessageBroadcastResult*)RESULT_PARAM = Result;
}

FGameplayMessageBroadcastResult UGameplayMessageSubsystem::K2_BroadcastObjectMessage(FGameplayTag Channel, UPARAM(ref) int32& Message, UObject* TargetObject)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return FGameplayMessageBroadcastResult();
}

DEFINE_FUNCTION(UGameplayMessageSubsystem::execK2_BroadcastObjectMessage)
{
	P_GET_STRUCT(FGameplayTag, Channel);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_GET_STRUCT(UObject*, TargetObject);
	TWeakObjectPtr<UObject> TargetObjectPtr = TWeakObjectPtr<UObject>(TargetObject);

	P_FINISH;

	FGameplayMessageBroadcastResult Result;
	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		Result = P_THIS->BroadcastMessageInternal(Channel, StructProp->Struct, MessagePtr, TargetObjectPtr);
	}

	*(FGameplayMessageBroadcastResult*)RESULT_PARAM = Result;
}

FGameplayMessageBroadcastResult UGameplayMessageSubsystem::K2_BroadcastSimpleObjectMessage(UPARAM(ref) int32& Message, UObject* TargetObject)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return FGameplayMessageBroadcastResult();
}

DEFINE_FUNCTION(UGameplayMessageSubsystem::execK2_BroadcastSimpleObjectMessage)
{
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_GET_STRUCT(UObject*, TargetObject);
	TWeakObjectPtr<UObject> TargetObjectPtr = TWeakObjectPtr<UObject>(TargetObject);

	P_FINISH;

	FGameplayMessageBroadcastResult Result;
	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		Result = P_THIS->BroadcastMessageInternal(UE::GameplayMessageSubsystem::TAG_DefaultMessageChannel, StructProp->Struct, MessagePtr, TargetObjectPtr);
	}

	*(FGameplayMessageBroadcastResult*)RESULT_PARAM = Result;
}

FGameplayMessageListenerHandle UGameplayMessageSubsystem::RegisterListenerInternal(FGameplayTag Channel, TFunction<void(FGameplayTag, const UScriptStruct*, void*)>&& Callback, const UScriptStruct* StructType, EGameplayMessageMatch MatchType, int32 Priority, TWeakObjectPtr<UObject> TargetObject)
{
	FChannelListenerList& List = ListenerMap.FindOrAdd(StructType);

	// Find index by priority to insert
	int32 Index = List.Listeners.Num();
	for (int i = List.Listeners.Num()-1; i >= 0; --i)
	{
		if (List.Listeners[i].Priority > Priority)
		{
			Index = i;
		}
		else
		{
			break;
		}
	}

	FGameplayMessageListenerData& Entry = List.Listeners.InsertDefaulted_GetRef(Index);
	Entry.ReceivedCallback = MoveTemp(Callback);
	Entry.ListenerStructType = StructType;
	Entry.Channel = Channel;
	Entry.HandleID = ++List.HandleID;
	Entry.MatchType = MatchType;
	Entry.TargetObject = TargetObject;
	Entry.Priority = Priority;

	return FGameplayMessageListenerHandle(this, StructType, Entry.HandleID);
}

void UGameplayMessageSubsystem::UnregisterListener(FGameplayMessageListenerHandle Handle)
{
	if (Handle.IsValid())
	{
		check(Handle.Subsystem == this);

		UnregisterListenerInternal(Handle.StructType, Handle.ID);
	}
	else
	{
		UE_LOG(LogGameplayMessageSubsystem, Warning, TEXT("Trying to unregister an invalid Handle."));
	}
}

void UGameplayMessageSubsystem::CancelCurrentMessage(UObject* WorldContext, bool bCancel, bool bInterrupted)
{
	if (!IsValid(WorldContext))
	{
		return;
	}
	
	UGameplayMessageSubsystem& GameplayMessageSubsystem = UGameplayMessageSubsystem::Get(WorldContext);
	GameplayMessageSubsystem.CancelMessage(bCancel, bInterrupted);
}

void UGameplayMessageSubsystem::CancelMessage(bool bCancel, bool bInterrupt)
{
	BroadcastResultCache.bCancelled = bCancel;
	BroadcastResultCache.bInterrupted = bInterrupt;
}

void UGameplayMessageSubsystem::UnregisterListenerInternal(const UScriptStruct* StructType, int32 HandleID)
{
	if (FChannelListenerList* StructMap = ListenerMap.Find(StructType))
	{
		int32 MatchIndex = StructMap->Listeners.IndexOfByPredicate([ID = HandleID](const FGameplayMessageListenerData& Other) { return Other.HandleID == ID; });
		if (MatchIndex != INDEX_NONE)
		{
			StructMap->Listeners.RemoveAtSwap(MatchIndex);
		}
		
		if (StructMap->Listeners.Num() == 0)
		{
			ListenerMap.Remove(StructType);
		}
	}
}

