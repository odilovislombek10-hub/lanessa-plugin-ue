#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanessaMasterMenuWidget.generated.h"

struct FLanessaIconPrim;

/**
 * Native C++ / Slate implementation of the Lanessa Master Menu HUD, built directly
 * (not through the UMG Designer) as a comparison against the Python/UMGToolSet version.
 */
UCLASS()
class LANESSA_API ULanessaMasterMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Safe factory for testing from Python: goes through the proper CreateWidget path
	 *  (unlike unreal.new_object, which skips UUserWidget initialization and crashes). */
	UFUNCTION(BlueprintCallable, Category = "Lanessa")
	static ULanessaMasterMenuWidget* CreateForTest(UObject* WorldContextObject);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedRef<SWidget> BuildBrand();
	TSharedRef<SWidget> BuildNavRail();
	TSharedRef<SWidget> BuildNavItem(const FText& Label, bool bActive, const TArray<FLanessaIconPrim>& IconPrims);
	TSharedRef<SWidget> BuildWeatherCard();
	TSharedRef<SWidget> BuildWeatherButton(const FString& TexturePath, bool bActive);
};
