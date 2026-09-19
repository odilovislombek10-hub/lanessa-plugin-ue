#include "LanessaMasterMenuWidget.h"
#include "LanessaLineIcon.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Engine/GameInstance.h"

ULanessaMasterMenuWidget* ULanessaMasterMenuWidget::CreateForTest(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	return CreateWidget<ULanessaMasterMenuWidget>(World, ULanessaMasterMenuWidget::StaticClass());
}

// Color tokens ported 1:1 from the v2 mockup's :root CSS custom properties.
namespace LanessaStyle
{
	static const FLinearColor Paper(1.f, 1.f, 1.f, 1.f);              // --paper: #ffffff
	static const FLinearColor Olive(0.6f, 0.6f, 0.4f, 1.f);           // --olive: #999966
	static const FLinearColor OliveGlow(0.788f, 0.788f, 0.604f, 1.f); // --olive-glow: #c9c99a
	static const FLinearColor TextDim(1.f, 1.f, 1.f, 0.56f);          // --text-dim: rgba(255,255,255,.56)
	static const FLinearColor LineSoft(1.f, 1.f, 1.f, 0.08f);         // --line-soft
	static const FLinearColor Line(1.f, 1.f, 1.f, 0.16f);             // --line
	static const FLinearColor BgDark(0.035f, 0.034f, 0.028f, 1.f);    // approximates .stage gradient base
	static const FLinearColor PanelSolid(0.039f, 0.039f, 0.035f, 0.92f); // --panel-solid: #0a0a09
	static const FLinearColor Transparent(0.f, 0.f, 0.f, 0.f);
	static const FLinearColor Dark(0.04f, 0.04f, 0.03f, 1.f);         // #0a0a08 (icon color on olive fill)

	static FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	static TSharedRef<SWidget> Fill(const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Color)
			.Padding(0.f)
			[
				SNew(SSpacer)
			];
	}
}

TSharedRef<SWidget> ULanessaMasterMenuWidget::RebuildWidget()
{
	using namespace LanessaStyle;

	return SNew(SOverlay)

		// .stage background (radial gradient approximated as flat dark fill - Slate has no built-in radial gradient brush)
		+ SOverlay::Slot()
		[
			Fill(BgDark)
		]

		// .brand { top:36px; left:48px; }
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(48.f, 36.f, 0.f, 0.f)
		[
			BuildBrand()
		]

		// .rail { left:0; top:176px; }
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(0.f, 176.f, 0.f, 0.f)
		[
			BuildNavRail()
		]

		// .chip-card { top:36px; right:48px; width:268px; }
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(0.f, 36.f, 48.f, 0.f)
		[
			BuildWeatherCard()
		];
}

TSharedRef<SWidget> ULanessaMasterMenuWidget::BuildBrand()
{
	using namespace LanessaStyle;

	// .brand img { height:46px; }
	UTexture2D* LogoTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/ArchVizExplorer/Blueprints/New_widgets/Textures/T_Lanessa_Logo_Horizontal.T_Lanessa_Logo_Horizontal"));
	FSlateBrush* LogoBrush = new FSlateBrush();
	if (LogoTex)
	{
		LogoBrush->SetResourceObject(LogoTex);
	}
	LogoBrush->ImageSize = FVector2D(182.f, 46.f);

	return SNew(SHorizontalBox)

		// gap:14px
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 14.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.96f, 0.955f, 0.93f, 1.f))
			.Padding(FMargin(10.f, 6.f))
			[
				SNew(SBox).WidthOverride(182.f).HeightOverride(46.f)
				[
					SNew(SImage).Image(LogoBrush)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			// .brand-word .name { font-size:20px; letter-spacing:.13em; } - display font not available, using bold sans
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("LANESSA")))
				.Font(Font(20, true))
				.ColorAndOpacity(FSlateColor(Paper))
			]

			// .brand-word .tag { margin-top:6px; font-size:9.5px; color:var(--text-dim); }
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("HAR BIR CHIZIQDA SAN'AT")))
				.Font(Font(9))
				.ColorAndOpacity(FSlateColor(TextDim))
			]
		];
}

TSharedRef<SWidget> ULanessaMasterMenuWidget::BuildNavItem(const FText& Label, bool bActive, const TArray<FLanessaIconPrim>& IconPrims)
{
	using namespace LanessaStyle;

	// .navitem.active { background: rgba(153,153,102,.10); }
	const FLinearColor BgColor = bActive ? FLinearColor(Olive.R, Olive.G, Olive.B, 0.10f) : Transparent;
	const FLinearColor TextColor = bActive ? Paper : TextDim;
	const FLinearColor StemColor = bActive ? OliveGlow : LineSoft;
	const FLinearColor IconColor = bActive ? OliveGlow : FLinearColor(Paper.R, Paper.G, Paper.B, 0.8f);

	// .navitem { width:208px; height:54px; padding-left:48px; gap:14px; } .navitem .stem { width:2px; top:8px; bottom:8px; left:0; }
	return SNew(SBox).WidthOverride(208.f).HeightOverride(54.f)
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(BgColor)
			.Padding(0.f)
			[
				SNew(SSpacer)
			]
		]

		// stem: absolutely at left edge, inset 8px top/bottom
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.Padding(0.f, 8.f, 0.f, 8.f)
		[
			SNew(SBox).WidthOverride(2.f)
			[
				Fill(StemColor)
			]
		]

		// content: padding-left 48px, icon(18) + gap(14) + label
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(48.f, 0.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SLanessaLineIcon)
				.Primitives(IconPrims)
				.IconSize(18.f)
				.StrokeWidth(1.3f)
				.StrokeColor(IconColor)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				// .navitem .label { font-size:12px; letter-spacing:.09em; text-transform:uppercase; }
				SNew(STextBlock)
				.Text(Label)
				.Font(Font(12))
				.ColorAndOpacity(FSlateColor(TextColor))
			]
		]
	];
}

TSharedRef<SWidget> ULanessaMasterMenuWidget::BuildNavRail()
{
	using namespace LanessaIcons;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	struct FNavEntry { FString Label; bool bActive; TArray<FLanessaIconPrim> Icon; };
	TArray<FNavEntry> Items = {
		{TEXT("BOSH SAHIFA"), true, Home()},
		{TEXT("ATROFI"), false, Atrofi()},
		{TEXT("QULAYLIKLAR"), false, Qulay()},
		{TEXT("QIDIRUV"), false, Qidiruv()},
		{TEXT("QIRQIM"), false, Qirqim()},
		{TEXT("VR PROGULKA"), false, VRProgulka()},
		{TEXT("PROGULKA"), false, Progulka()},
		{TEXT("INTERYER"), false, Interyer()},
		{TEXT("GALEREYA"), false, Galereya()},
	};

	for (const FNavEntry& Item : Items)
	{
		Box->AddSlot().AutoHeight()
		[
			BuildNavItem(FText::FromString(Item.Label), Item.bActive, Item.Icon)
		];
	}

	return Box;
}

TSharedRef<SWidget> ULanessaMasterMenuWidget::BuildWeatherButton(const FString& TexturePath, bool bActive)
{
	using namespace LanessaStyle;

	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexturePath);
	FSlateBrush* IconBrush = new FSlateBrush();
	if (Tex)
	{
		IconBrush->SetResourceObject(Tex);
	}
	IconBrush->ImageSize = FVector2D(16.f, 16.f);
	IconBrush->TintColor = FSlateColor(bActive ? Dark : TextDim);

	// .weather-row button { height:38px; border:1px solid var(--line); }
	return SNew(SBox).HeightOverride(38.f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(bActive ? Olive : Transparent)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f))
		[
			SNew(SImage).Image(IconBrush)
		]
	];
}

TSharedRef<SWidget> ULanessaMasterMenuWidget::BuildWeatherCard()
{
	using namespace LanessaStyle;

	// .weather-row { gap:6px; margin-top:18px; }
	TSharedRef<SHorizontalBox> WeatherRow = SNew(SHorizontalBox);
	const TCHAR* TexBase = TEXT("/Game/ArchVizExplorer/Blueprints/New_widgets/Textures/T_Weather_");
	TArray<TPair<FString, bool>> Icons = {
		{TEXT("Sun"), true},
		{TEXT("Cloud"), false},
		{TEXT("Partly"), false},
		{TEXT("Rain"), false},
		{TEXT("Snow"), false},
	};
	for (int32 i = 0; i < Icons.Num(); i++)
	{
		WeatherRow->AddSlot().FillWidth(1.f).Padding(i == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f)
		[
			BuildWeatherButton(FString(TexBase) + Icons[i].Key + TEXT(".T_Weather_") + Icons[i].Key, Icons[i].Value)
		];
	}

	// .chip-card { width:268px; padding:18px 20px 20px; }
	return SNew(SBox).WidthOverride(268.f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(PanelSolid)
		.Padding(FMargin(20.f, 18.f, 20.f, 20.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				// .chip-card .time { font-size:28px; }
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("16:24"))).Font(Font(28)).ColorAndOpacity(FSlateColor(Paper))
				]
				// .chip-card .temp { font-size:12px; color:var(--olive-glow); }
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("+27°C"))).Font(Font(12)).ColorAndOpacity(FSlateColor(OliveGlow))
				]
			]

			// .chip-card .sub { margin-top:3px; font-size:9.5px; }
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("IYUL · KUNDUZI"))).Font(Font(9)).ColorAndOpacity(FSlateColor(TextDim))
			]

			// .time-track { margin:20px 3px 0; height:2px; }
			+ SVerticalBox::Slot().AutoHeight().Padding(3.f, 20.f, 3.f, 0.f)
			[
				SNew(SBox).HeightOverride(2.f)
				[
					Fill(Line)
				]
			]

			// .ticks { margin-top:8px; font-size:8.5px; color: rgba(255,255,255,.3); }
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Text(FText::FromString(TEXT("06:00"))).Font(Font(8))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.f,1.f,1.f,0.3f)))]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("14:00"))).Font(Font(8))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.f,1.f,1.f,0.3f)))]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[SNew(STextBlock).Text(FText::FromString(TEXT("22:00"))).Font(Font(8))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.f,1.f,1.f,0.3f)))]
			]

			// .weather-row { margin-top:18px; }
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
			[
				WeatherRow
			]
		]
	];
}
