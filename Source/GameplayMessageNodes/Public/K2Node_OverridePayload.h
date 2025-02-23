// 

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_OverridePayload.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYMESSAGENODES_API UK2Node_OverridePayload : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void PostReconstructNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;

	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	
	/**
	 * 刷新Context的输出类型
	 */
	void RefreshOutputContentType();
	

private:
	UEdGraphPin* GetListenerInstancePin() const;
	UEdGraphPin* GetContextPin() const;
};


