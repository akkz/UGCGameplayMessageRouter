// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/GameplayWorldMessageSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Stack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayWorldMessageSubsystem)

DEFINE_LOG_CATEGORY(LogGameplayWorldMessageSubsystem);

namespace UE
{
	namespace GameplayWorldMessageSubsystem
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_DefaultMessageChannel, "Message", "Message default channel");
		
		static int32 ShouldLogMessages = 0;
		static FAutoConsoleVariableRef CVarShouldLogMessages(TEXT("GameplayWorldMessageSubsystem.LogMessages"),
			ShouldLogMessages,
			TEXT("Should spatial messages broadcast through the gameplay world message subsystem be logged?"));
		
		// Grid coordinate conversion functions
		int64 GetGridID(const FVector& WorldPosition)
		{
			int32 GridX = FMath::FloorToInt(WorldPosition.X / GRID_SIZE);
			int32 GridY = FMath::FloorToInt(WorldPosition.Y / GRID_SIZE);
			
			// Pack X and Y into a single 64-bit integer (high 32 bits = X, low 32 bits = Y)
			return (static_cast<int64>(GridX) << 32) | (static_cast<int64>(GridY) & 0xFFFFFFFF);
		}
		
		FVector GetGridCenter(int64 GridID)
		{
			int32 GridX = static_cast<int32>(GridID >> 32);
			int32 GridY = static_cast<int32>(GridID & 0xFFFFFFFF);
			
			return FVector(
				(GridX + 0.5f) * GRID_SIZE,
				(GridY + 0.5f) * GRID_SIZE,
				0.0f
			);
		}
		
		TArray<int64> GetGridsInRadius(const FVector& Center, float Radius)
		{
			TArray<int64> GridIDs;
			
			// Calculate the grid bounds that could contain points within the radius
			int32 MinGridX = FMath::FloorToInt((Center.X - Radius) / GRID_SIZE);
			int32 MaxGridX = FMath::FloorToInt((Center.X + Radius) / GRID_SIZE);
			int32 MinGridY = FMath::FloorToInt((Center.Y - Radius) / GRID_SIZE);
			int32 MaxGridY = FMath::FloorToInt((Center.Y + Radius) / GRID_SIZE);
			
			// Check each grid cell in the bounding box
			for (int32 GridX = MinGridX; GridX <= MaxGridX; ++GridX)
			{
				for (int32 GridY = MinGridY; GridY <= MaxGridY; ++GridY)
				{
					// Calculate the closest point in this grid to the center
					FVector GridMin(GridX * GRID_SIZE, GridY * GRID_SIZE, 0.0f);
					FVector GridMax((GridX + 1) * GRID_SIZE, (GridY + 1) * GRID_SIZE, 0.0f);
					
					FVector ClosestPoint;
					ClosestPoint.X = FMath::Clamp(Center.X, GridMin.X, GridMax.X);
					ClosestPoint.Y = FMath::Clamp(Center.Y, GridMin.Y, GridMax.Y);
					ClosestPoint.Z = Center.Z; // Use center's Z for distance calculation
					
					// Check if this grid intersects with the radius
					float DistanceSquared = FVector::DistSquared(Center, ClosestPoint);
					if (DistanceSquared <= Radius * Radius)
					{
						int64 GridID = (static_cast<int64>(GridX) << 32) | (static_cast<int64>(GridY) & 0xFFFFFFFF);
						GridIDs.Add(GridID);
					}
				}
			}
			
			return GridIDs;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// FGameplayWorldMessageListenerHandle

void FGameplayWorldMessageListenerHandle::Unregister()
{
	if (UGameplayWorldMessageSubsystem* StrongSubsystem = Subsystem.Get())
	{
		StrongSubsystem->UnregisterListener(*this);
		Subsystem.Reset();
		StructType = nullptr;
		ID = 0;
	}
}

//////////////////////////////////////////////////////////////////////
// UGameplayWorldMessageSubsystem

UGameplayWorldMessageSubsystem& UGameplayWorldMessageSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert);
	check(World);
	UGameplayWorldMessageSubsystem* Router = World->GetSubsystem<UGameplayWorldMessageSubsystem>();
	check(Router);
	return *Router;
}

bool UGameplayWorldMessageSubsystem::HasInstance(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert);
	UGameplayWorldMessageSubsystem* Router = World != nullptr ? World->GetSubsystem<UGameplayWorldMessageSubsystem>() : nullptr;
	return Router != nullptr;
}

void UGameplayWorldMessageSubsystem::Deinitialize()
{
	GridListenerMap.Reset();
	HandleToSpatialMap.Reset();

	Super::Deinitialize();
}

FGameplayMessageBroadcastResult UGameplayWorldMessageSubsystem::BroadcastMessageInternal(FGameplayTag Channel, const UScriptStruct* StructType, void* MessageBytes, const FVector& WorldPosition)
{
	// Log the message if enabled
	if (UE::GameplayWorldMessageSubsystem::ShouldLogMessages != 0)
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
		UE_LOG(LogGameplayWorldMessageSubsystem, Log, TEXT("BroadcastSpatialMessage(%s, %s, %s, %s)"), 
			pContextString ? **pContextString : *GetPathNameSafe(this), 
			*Channel.ToString(), 
			*HumanReadableMessage,
			*WorldPosition.ToString());
	}

	// Reset State
	BroadcastResultCache.Reset();

	// 优化：只查找广播位置所在的网格，因为监听者已经在注册时覆盖了其监听半径内的所有网格
	int64 BroadcastGridID = UE::GameplayWorldMessageSubsystem::GetGridID(WorldPosition);

	// 从广播位置所在的网格中获取所有监听者
	const FGridListenerList* pList = GridListenerMap.Find(BroadcastGridID);
	if (!pList)
	{
		// 没有监听者在这个网格中，直接返回
		return BroadcastResultCache;
	}

	// 复制监听者列表以防在回调过程中发生修改
	TArray<FGameplayWorldMessageListenerData> ListenerArray(pList->Listeners);

	// 处理监听者（已经按优先级排序）
	for (const FGameplayWorldMessageListenerData& Listener : ListenerArray)
	{
		if (!Listener.ListenerStructType.IsValid())
		{
			UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Listener struct type has gone invalid on Channel %s. Removing listener from list"), *Channel.ToString());
			UnregisterListenerInternal(StructType, Listener.HandleID);
			continue;
		}

		// 检查结构体类型是否匹配
		if (StructType != Listener.ListenerStructType.Get())
		{
			continue;
		}

		// 检查广播位置是否在监听者的监听半径内（精确距离检查）
		float DistanceSquared = FVector::DistSquared(WorldPosition, Listener.ListenPosition);
		if (DistanceSquared > Listener.ListenRadius * Listener.ListenRadius)
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

		// 执行回调
		Listener.ReceivedCallback(Channel, StructType, MessageBytes);

		// 检查消息是否被中断
		if (BroadcastResultCache.bInterrupted)
		{
			break;
		}
	}

	return BroadcastResultCache;
}

FGameplayMessageBroadcastResult UGameplayWorldMessageSubsystem::K2_BroadcastMessage(FGameplayTag Channel, UPARAM(ref) int32& Message, FVector WorldPosition)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return FGameplayMessageBroadcastResult();
}

DEFINE_FUNCTION(UGameplayWorldMessageSubsystem::execK2_BroadcastMessage)
{
	P_GET_STRUCT(FGameplayTag, Channel);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_GET_STRUCT(FVector, WorldPosition);

	P_FINISH;

	FGameplayMessageBroadcastResult Result;
	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		Result = P_THIS->BroadcastMessageInternal(Channel, StructProp->Struct, MessagePtr, WorldPosition);
	}

	*(FGameplayMessageBroadcastResult*)RESULT_PARAM = Result;
}

FGameplayMessageBroadcastResult UGameplayWorldMessageSubsystem::K2_BroadcastSimpleMessage(UPARAM(ref) int32& Message, FVector WorldPosition)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();

	return FGameplayMessageBroadcastResult();
}

DEFINE_FUNCTION(UGameplayWorldMessageSubsystem::execK2_BroadcastSimpleMessage)
{
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MessagePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_GET_STRUCT(FVector, WorldPosition);

	P_FINISH;

	FGameplayMessageBroadcastResult Result;
	if (ensure((StructProp != nullptr) && (StructProp->Struct != nullptr) && (MessagePtr != nullptr)))
	{
		Result = P_THIS->BroadcastMessageInternal(UE::GameplayWorldMessageSubsystem::TAG_DefaultMessageChannel, StructProp->Struct, MessagePtr, WorldPosition);
	}

	*(FGameplayMessageBroadcastResult*)RESULT_PARAM = Result;
}

FGameplayWorldMessageListenerHandle UGameplayWorldMessageSubsystem::RegisterListenerInternal(
	FGameplayTag Channel, 
	TFunction<void(FGameplayTag, const UScriptStruct*, void*)>&& Callback,
	const UScriptStruct* StructType,
	EGameplayMessageMatch MatchType,
	int32 Priority,
	const FVector& ListenPosition,
	float ListenRadius)
{
	// Get all grids that this listener could potentially receive messages from
	TArray<int64> RelevantGrids = UE::GameplayWorldMessageSubsystem::GetGridsInRadius(ListenPosition, ListenRadius);

	// If no grids are found, add at least the grid containing the listen position
	if (RelevantGrids.Num() == 0)
	{
		RelevantGrids.Add(UE::GameplayWorldMessageSubsystem::GetGridID(ListenPosition));
	}

	// Create listener data
	FGameplayWorldMessageListenerData ListenerData;
	ListenerData.ReceivedCallback = MoveTemp(Callback);
	ListenerData.ListenerStructType = StructType;
	ListenerData.Channel = Channel;
	ListenerData.MatchType = MatchType;
	ListenerData.Priority = Priority;
	ListenerData.ListenPosition = ListenPosition;
	ListenerData.ListenRadius = ListenRadius;

	// Generate unique handle ID
	static int32 GlobalHandleID = 0;
	ListenerData.HandleID = ++GlobalHandleID;

	// Add listener to all relevant grids
	for (int64 GridID : RelevantGrids)
	{
		FGridListenerList& GridList = GridListenerMap.FindOrAdd(GridID);
		
		// Insert at the correct position based on priority
		int32 InsertIndex = GridList.Listeners.Num();
		for (int32 i = GridList.Listeners.Num() - 1; i >= 0; --i)
		{
			if (GridList.Listeners[i].Priority > Priority)
			{
				InsertIndex = i;
			}
			else
			{
				break;
			}
		}
		
		GridList.Listeners.Insert(ListenerData, InsertIndex);
	}

	// Track the listener's spatial info for efficient cleanup
	FListenerSpatialInfo SpatialInfo;
	SpatialInfo.ListenPosition = ListenPosition;
	SpatialInfo.ListenRadius = ListenRadius;
	HandleToSpatialMap.Add(ListenerData.HandleID, SpatialInfo);

	return FGameplayWorldMessageListenerHandle(this, StructType, ListenerData.HandleID);
}

void UGameplayWorldMessageSubsystem::UnregisterListener(FGameplayWorldMessageListenerHandle Handle)
{
	if (Handle.IsValid())
	{
		check(Handle.Subsystem == this);
		UnregisterListenerInternal(Handle.StructType, Handle.ID);
	}
	else
	{
		UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Trying to unregister an invalid Handle."));
	}
}

bool UGameplayWorldMessageSubsystem::UpdateRegisterListenerLocation(FGameplayWorldMessageListenerHandle Handle, const FVector& NewListenPosition, float NewListenRadius)
{
	if (!Handle.IsValid() || Handle.Subsystem != this)
	{
		UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Trying to update location for an invalid Handle."));
		return false;
	}

	// Find the existing spatial info
	FListenerSpatialInfo* SpatialInfoPtr = HandleToSpatialMap.Find(Handle.ID);
	if (!SpatialInfoPtr)
	{
		UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Trying to update location for listener with unknown HandleID %d"), Handle.ID);
		return false;
	}

	// Use existing radius if new radius is not specified (negative value)
	float ActualNewRadius = NewListenRadius >= 0.0f ? NewListenRadius : SpatialInfoPtr->ListenRadius;

	// Calculate old and new grid sets
	TArray<int64> OldGrids = UE::GameplayWorldMessageSubsystem::GetGridsInRadius(SpatialInfoPtr->ListenPosition, SpatialInfoPtr->ListenRadius);
	TArray<int64> NewGrids = UE::GameplayWorldMessageSubsystem::GetGridsInRadius(NewListenPosition, ActualNewRadius);

	// Ensure we have at least the grid containing the position
	if (OldGrids.Num() == 0)
	{
		OldGrids.Add(UE::GameplayWorldMessageSubsystem::GetGridID(SpatialInfoPtr->ListenPosition));
	}
	if (NewGrids.Num() == 0)
	{
		NewGrids.Add(UE::GameplayWorldMessageSubsystem::GetGridID(NewListenPosition));
	}

	// Find grids to remove from and grids to add to
	TArray<int64> GridsToRemoveFrom;
	TArray<int64> GridsToAddTo;

	// Find grids that are in old but not in new (remove from these)
	for (int64 OldGrid : OldGrids)
	{
		if (!NewGrids.Contains(OldGrid))
		{
			GridsToRemoveFrom.Add(OldGrid);
		}
	}

	// Find grids that are in new but not in old (add to these)
	for (int64 NewGrid : NewGrids)
	{
		if (!OldGrids.Contains(NewGrid))
		{
			GridsToAddTo.Add(NewGrid);
		}
	}

	// Find the listener data to copy (from any grid where it exists)
	FGameplayWorldMessageListenerData* ListenerDataPtr = nullptr;
	for (int64 GridID : OldGrids)
	{
		if (FGridListenerList* GridList = GridListenerMap.Find(GridID))
		{
			for (FGameplayWorldMessageListenerData& Listener : GridList->Listeners)
			{
				if (Listener.HandleID == Handle.ID)
				{
					ListenerDataPtr = &Listener;
					break;
				}
			}
			if (ListenerDataPtr)
			{
				break;
			}
		}
	}

	if (!ListenerDataPtr)
	{
		UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Could not find listener data for HandleID %d during location update"), Handle.ID);
		return false;
	}

	// Create updated listener data
	FGameplayWorldMessageListenerData UpdatedListenerData = *ListenerDataPtr;
	UpdatedListenerData.ListenPosition = NewListenPosition;
	UpdatedListenerData.ListenRadius = ActualNewRadius;

	// Remove from old grids
	TArray<int64> GridIDsToClean;
	for (int64 GridID : GridsToRemoveFrom)
	{
		if (FGridListenerList* GridList = GridListenerMap.Find(GridID))
		{
			int32 RemovedCount = GridList->Listeners.RemoveAll([HandleID = Handle.ID](const FGameplayWorldMessageListenerData& Listener)
			{
				return Listener.HandleID == HandleID;
			});

			// If the grid is now empty, mark for cleanup
			if (GridList->Listeners.Num() == 0)
			{
				GridIDsToClean.Add(GridID);
			}
		}
	}

	// Clean up empty grids
	for (int64 GridID : GridIDsToClean)
	{
		GridListenerMap.Remove(GridID);
	}

	// Add to new grids
	for (int64 GridID : GridsToAddTo)
	{
		FGridListenerList& GridList = GridListenerMap.FindOrAdd(GridID);
		
		// Insert at the correct position based on priority
		int32 InsertIndex = GridList.Listeners.Num();
		for (int32 i = GridList.Listeners.Num() - 1; i >= 0; --i)
		{
			if (GridList.Listeners[i].Priority > UpdatedListenerData.Priority)
			{
				InsertIndex = i;
			}
			else
			{
				break;
			}
		}
		
		GridList.Listeners.Insert(UpdatedListenerData, InsertIndex);
	}

	// Update existing listeners in grids that remain the same
	for (int64 GridID : OldGrids)
	{
		if (NewGrids.Contains(GridID))
		{
			if (FGridListenerList* GridList = GridListenerMap.Find(GridID))
			{
				for (FGameplayWorldMessageListenerData& Listener : GridList->Listeners)
				{
					if (Listener.HandleID == Handle.ID)
					{
						Listener.ListenPosition = NewListenPosition;
						Listener.ListenRadius = ActualNewRadius;
						break;
					}
				}
			}
		}
	}

	// Update the spatial mapping
	SpatialInfoPtr->ListenPosition = NewListenPosition;
	SpatialInfoPtr->ListenRadius = ActualNewRadius;

	return true;
}

void UGameplayWorldMessageSubsystem::CancelCurrentMessage(UObject* WorldContext, bool bCancel, bool bInterrupted)
{
	if (!IsValid(WorldContext))
	{
		return;
	}
	
	UGameplayWorldMessageSubsystem& GameplayWorldMessageSubsystem = UGameplayWorldMessageSubsystem::Get(WorldContext);
	GameplayWorldMessageSubsystem.CancelMessage(bCancel, bInterrupted);
}

void UGameplayWorldMessageSubsystem::CancelMessage(bool bCancel, bool bInterrupt)
{
	BroadcastResultCache.bCancelled = bCancel;
	BroadcastResultCache.bInterrupted = bInterrupt;
}

void UGameplayWorldMessageSubsystem::UnregisterListenerInternal(const UScriptStruct* StructType, int32 HandleID)
{
	// Direct lookup of spatial info by HandleID
	FListenerSpatialInfo* SpatialInfoPtr = HandleToSpatialMap.Find(HandleID);
	if (!SpatialInfoPtr)
	{
		UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Trying to unregister listener with unknown HandleID %d"), HandleID);
		return;
	}
	
	// Recalculate which grids this listener was registered in
	TArray<int64> RelevantGrids = UE::GameplayWorldMessageSubsystem::GetGridsInRadius(SpatialInfoPtr->ListenPosition, SpatialInfoPtr->ListenRadius);
	
	// If no grids are found, add at least the grid containing the listen position
	if (RelevantGrids.Num() == 0)
	{
		RelevantGrids.Add(UE::GameplayWorldMessageSubsystem::GetGridID(SpatialInfoPtr->ListenPosition));
	}
	
	TArray<int64> GridIDsToClean;
	
	// Remove the listener from all grids where it was registered
	for (int64 GridID : RelevantGrids)
	{
		if (FGridListenerList* GridList = GridListenerMap.Find(GridID))
		{
			int32 RemovedCount = GridList->Listeners.RemoveAll([HandleID](const FGameplayWorldMessageListenerData& Listener)
			{
				return Listener.HandleID == HandleID;
			});
			
			if (RemovedCount == 0)
			{
				UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Listener with HandleID %d should be in grid %lld but not found in grid's listener list"), HandleID, GridID);
			}
			
			// If the grid is now empty, mark for cleanup
			if (GridList->Listeners.Num() == 0)
			{
				GridIDsToClean.Add(GridID);
			}
		}
		else
		{
			UE_LOG(LogGameplayWorldMessageSubsystem, Warning, TEXT("Listener with HandleID %d should be in grid %lld but grid not found"), HandleID, GridID);
		}
	}
	
	// Clean up empty grids
	for (int64 GridID : GridIDsToClean)
	{
		GridListenerMap.Remove(GridID);
	}
	
	// Clean up the spatial mapping
	HandleToSpatialMap.Remove(HandleID);
}
