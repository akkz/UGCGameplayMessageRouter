// 


#include "K2Node_OverridePayload.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_AsyncAction_ListenForGameplayMessages.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "GameFramework/AsyncAction_ListenForGameplayMessage.h"

#define LOCTEXT_NAMESPACE "K2Node_OverridePayload"

namespace UK2Node_OverridePayloadHelper
{
    const FName PIN_Exec = UEdGraphSchema_K2::PN_Execute;
    const FName PIN_Then = UEdGraphSchema_K2::PN_Then;
    const FName PIN_ListenerInstance = TEXT("ListenerInstance");
    const FName PIN_Context = TEXT("Context");
}

void UK2Node_OverridePayload::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Input Pins
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UK2Node_OverridePayloadHelper::PIN_Exec);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UAsyncAction_ListenForGameplayMessage::StaticClass(), UK2Node_OverridePayloadHelper::PIN_ListenerInstance);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, UK2Node_OverridePayloadHelper::PIN_Context);
	
	// Output Pins
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UK2Node_OverridePayloadHelper::PIN_Then);
}

void UK2Node_OverridePayload::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* OverridePayloadNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	OverridePayloadNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UAsyncAction_ListenForGameplayMessage, OverridePayload), UAsyncAction_ListenForGameplayMessage::StaticClass());
	OverridePayloadNode->AllocateDefaultPins();

	// Connect the execution pins
	UEdGraphPin* ExecPin = GetExecPin();
	UEdGraphPin* ThenPin = GetThenPin();
	UEdGraphPin* OverrideExecPin = OverridePayloadNode->GetExecPin();
	UEdGraphPin* OverrideThenPin = OverridePayloadNode->GetThenPin();

	if (ExecPin && OverrideExecPin && ThenPin && OverrideThenPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*ExecPin, *OverrideExecPin);
		CompilerContext.MovePinLinksToIntermediate(*ThenPin, *OverrideThenPin);
	}

	// Connect the ListenerInstance pin
	UEdGraphPin* ListenerInstancePin = GetListenerInstancePin();
	UEdGraphPin* OverrideListenerInstancePin = OverridePayloadNode->FindPinChecked(TEXT("self"));
	CompilerContext.MovePinLinksToIntermediate(*ListenerInstancePin, *OverrideListenerInstancePin);

	// Connect the Context (NewPayload) pin
	UEdGraphPin* ContextPin = GetContextPin();
	UEdGraphPin* OverrideNewPayloadPin = OverridePayloadNode->FindPinChecked(TEXT("InPayload"));
	// Update the OverrideNewPayloadPin type to match the ContextPin
	OverrideNewPayloadPin->PinType = ContextPin->PinType;
	OverrideNewPayloadPin->PinType.PinSubCategoryObject = ContextPin->PinType.PinSubCategoryObject;
	CompilerContext.MovePinLinksToIntermediate(*ContextPin, *OverrideNewPayloadPin);

	// Break any links to the original node
	BreakAllNodeLinks();
}

FText UK2Node_OverridePayload::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Override Gameplay Message Payload");
}

FText UK2Node_OverridePayload::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Overrides the payload of a Gameplay Message Listener with a new context");
}

void UK2Node_OverridePayload::PostReconstructNode()
{
	Super::PostReconstructNode();
	RefreshOutputContentType();
}

void UK2Node_OverridePayload::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	if (ChangedPin == GetListenerInstancePin())
	{
		RefreshOutputContentType();
	}
}

void UK2Node_OverridePayload::RefreshOutputContentType()
{
	UEdGraphPin* ListenerInstancePin = GetListenerInstancePin();
	UEdGraphPin* ContextPin = GetContextPin();

	if (ListenerInstancePin && ContextPin)
	{
		if (ContextPin->SubPins.Num() > 0)
		{
			GetSchema()->RecombinePin(ContextPin);
		}
		
		UScriptStruct* PayloadType = nullptr;

		// Check if the ListenerInstancePin is connected
		if (ListenerInstancePin->LinkedTo.Num() > 0)
		{
			// Get the connected pin
			UEdGraphPin* ConnectedPin = ListenerInstancePin->LinkedTo[0];
			
			// Check if the connected pin is from a K2Node_AsyncAction_ListenForGameplayMessages
			if (UK2Node_AsyncAction_ListenForGameplayMessages* ListenNode = Cast<UK2Node_AsyncAction_ListenForGameplayMessages>(ConnectedPin->GetOwningNode()))
			{
				// Find the PayloadType pin
				UEdGraphPin* PayloadTypePin = ListenNode->GetPayloadTypePin();
				
				// Get the PayloadType
				if (PayloadTypePin->DefaultObject && PayloadTypePin->DefaultObject->IsA<UScriptStruct>())
				{
					PayloadType = Cast<UScriptStruct>(PayloadTypePin->DefaultObject);
				}
			}
		}

		// Update the ContextPin type
		if (PayloadType)
		{
			bool bTypesMatch = (ContextPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) &&
							   (ContextPin->PinType.PinSubCategoryObject == PayloadType);
			
			if (!bTypesMatch)
			{
				ContextPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				ContextPin->PinType.PinSubCategoryObject = PayloadType;
				ContextPin->PinType.PinSubCategory = NAME_None;
			}
		}
		else
		{
			ContextPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			ContextPin->PinType.PinSubCategoryObject = nullptr;
			ContextPin->PinType.PinSubCategory = NAME_None;
		}
	}
}

UEdGraphPin* UK2Node_OverridePayload::GetListenerInstancePin() const
{
	return FindPinChecked(UK2Node_OverridePayloadHelper::PIN_ListenerInstance);
}

UEdGraphPin* UK2Node_OverridePayload::GetContextPin() const
{
	return FindPinChecked(UK2Node_OverridePayloadHelper::PIN_Context);
}

void UK2Node_OverridePayload::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_OverridePayload::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "Gameplay Messages");
}

#undef LOCTEXT_NAMESPACE
