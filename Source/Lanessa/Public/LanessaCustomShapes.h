#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

/**
 * Ports `clip-path: polygon(0 0, 100% 0, 100% 100%, CUTpx 100%, 0 calc(100% - CUTpx))` - the
 * diagonal bottom-left corner cut used throughout v2 (.navitem, .chip, .poi .btn, .side-footer
 * button, .plan-foot button) - which has no Slate brush equivalent, by drawing the fill as a
 * custom pentagon (fan-triangulated) instead of a rectangle, then painting the child content on top.
 */
class SLanessaCutBorder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLanessaCutBorder)
		: _FillColor(FLinearColor::Black)
		, _CutSize(12.f)
		, _HoverColor()
		, _bAnimateHover(false)
		, _BorderColor()
		, _BorderThickness(1.f)
		{}
		SLATE_ATTRIBUTE(FLinearColor, FillColor)
		SLATE_ARGUMENT(float, CutSize)
		SLATE_ARGUMENT(TOptional<FLinearColor>, HoverColor) // .navitem:hover { background: rgba(153,153,102,.10); }
		SLATE_ARGUMENT(bool, bAnimateHover)
		// Outline traced along the same cut-pentagon edge as the fill - lets a fully/near-transparent
		// FillColor (e.g. the nav rail against a bright sky photo) still read as a distinct button.
		SLATE_ATTRIBUTE(FLinearColor, BorderColor)
		SLATE_ARGUMENT(float, BorderThickness)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FillColor = InArgs._FillColor;
		CutSize = InArgs._CutSize;
		HoverColor = InArgs._HoverColor;
		bAnimateHover = InArgs._bAnimateHover;
		BorderColor = InArgs._BorderColor;
		BorderThickness = InArgs._BorderThickness;
		OnClickedDelegate = InArgs._OnClicked;
		ChildSlot
		[
			InArgs._Content.Widget
		];
		if (bAnimateHover)
		{
			SetCanTick(true);
		}
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		// .navitem { transition: background 220ms ease; } - approximated as a linear 220ms blend
		const float Target = bIsHovered ? 1.f : 0.f;
		HoverProgress = FMath::FInterpTo(HoverProgress, Target, InDeltaTime, 1.f / 0.22f);
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
		bIsHovered = true;
	}
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseLeave(MouseEvent);
		bIsHovered = false;
	}
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			OnClickedDelegate.ExecuteIfBound();
			return FReply::Handled();
		}
		return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
	}
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return OnClickedDelegate.IsBound() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float Cut = FMath::Min(CutSize, FMath::Min(Size.X, Size.Y) * 0.9f);
		FLinearColor Color = FillColor.Get();
		if (HoverColor.IsSet() && HoverProgress > 0.f)
		{
			Color = FMath::Lerp(Color, HoverColor.GetValue(), HoverProgress);
		}
		const FColor VertColor = Color.ToFColor(true);

		// pentagon: (0,0) (W,0) (W,H) (Cut,H) (0,H-Cut)
		TArray<FVector2D> Poly = {
			FVector2D(0, 0),
			FVector2D(Size.X, 0),
			FVector2D(Size.X, Size.Y),
			FVector2D(Cut, Size.Y),
			FVector2D(0, Size.Y - Cut),
		};

		if (Color.A > 0.f)
		{
			FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
			TArray<FSlateVertex> Verts;
			for (const FVector2D& P : Poly)
			{
				Verts.Add(FSlateVertex::Make(AllottedGeometry.GetAccumulatedRenderTransform(), FVector2f(P), FVector2f(0, 0), VertColor));
			}
			TArray<SlateIndex> Indices;
			for (int32 i = 1; i < Poly.Num() - 1; i++)
			{
				Indices.Add(0); Indices.Add(i); Indices.Add(i + 1);
			}
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Verts, Indices, nullptr, 0, 0);
		}

		if (BorderColor.IsSet())
		{
			const FLinearColor Stroke = BorderColor.Get();
			if (Stroke.A > 0.f)
			{
				// Closed loop along the same pentagon edge the fill uses, so the outline follows the
				// diagonal cut corner instead of drawing a plain rectangle around a cut shape.
				TArray<FVector2D> Loop = Poly;
				Loop.Add(Poly[0]);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Loop,
					ESlateDrawEffect::None, Stroke, true, BorderThickness);
			}
		}

		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
	}

private:
	TAttribute<FLinearColor> FillColor;
	float CutSize = 12.f;
	TOptional<FLinearColor> HoverColor;
	bool bAnimateHover = false;
	bool bIsHovered = false;
	float HoverProgress = 0.f;
	TAttribute<FLinearColor> BorderColor;
	float BorderThickness = 1.f;
	FSimpleDelegate OnClickedDelegate;
};

/**
 * Ports .stage's two layered backgrounds:
 *   linear-gradient(180deg, #1c1b16 0%, #0c0c0a 46%, #000 100%)   - via FSlateDrawElement::MakeGradient
 *   radial-gradient(120% 90% at 72% 30%, rgba(60,58,44,.35), transparent 60%) - approximated as
 *     concentric alpha-fading rings (Slate has no radial-gradient draw primitive), fan-triangulated
 *     per ring via MakeCustomVerts.
 */
class SLanessaStageGradient : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLanessaStageGradient) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		TArray<FSlateGradientStop> Stops;
		Stops.Add(FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0.110f, 0.106f, 0.086f, 1.f)));   // #1c1b16 @ 0%
		Stops.Add(FSlateGradientStop(FVector2D(0.f, Size.Y * 0.46f), FLinearColor(0.047f, 0.047f, 0.039f, 1.f))); // #0c0c0a @ 46%
		Stops.Add(FSlateGradientStop(FVector2D(0.f, Size.Y), FLinearColor(0.f, 0.f, 0.f, 1.f)));         // #000 @ 100%
		FSlateDrawElement::MakeGradient(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Stops, Orient_Vertical);

		// radial-gradient(120% 90% at 72% 30%, rgba(60,58,44,.35), transparent 60%)
		const FVector2D Center(Size.X * 0.72f, Size.Y * 0.30f);
		const FVector2D RadiusXY(Size.X * 1.20f * 0.6f, Size.Y * 0.90f * 0.6f); // "60% fade" point
		const FLinearColor PeakColor(60.f/255.f, 58.f/255.f, 44.f/255.f, 0.35f);
		FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));

		const int32 RingCount = 10;
		const int32 Segments = 28;
		for (int32 Ring = RingCount; Ring >= 1; Ring--)
		{
			const float T0 = (float)(Ring - 1) / RingCount;
			const float T1 = (float)Ring / RingCount;
			const float Alpha0 = PeakColor.A * (1.f - T0);
			const float Alpha1 = PeakColor.A * (1.f - T1);
			TArray<FSlateVertex> Verts;
			TArray<SlateIndex> Indices;
			for (int32 s = 0; s <= Segments; s++)
			{
				const float Angle = (float)s / Segments * 2.f * PI;
				const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				FLinearColor InnerColor = PeakColor; InnerColor.A = Alpha0;
				FLinearColor OuterColor = PeakColor; OuterColor.A = Alpha1;
				Verts.Add(FSlateVertex::Make(AllottedGeometry.GetAccumulatedRenderTransform(), FVector2f(Center + Dir * RadiusXY * T0), FVector2f(0,0), InnerColor.ToFColor(true)));
				Verts.Add(FSlateVertex::Make(AllottedGeometry.GetAccumulatedRenderTransform(), FVector2f(Center + Dir * RadiusXY * T1), FVector2f(0,0), OuterColor.ToFColor(true)));
				if (s > 0)
				{
					const int32 Base = (s - 1) * 2;
					Indices.Add(Base); Indices.Add(Base+1); Indices.Add(Base+2);
					Indices.Add(Base+1); Indices.Add(Base+3); Indices.Add(Base+2);
				}
			}
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Verts, Indices, nullptr, 0, 0);
		}

		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }
};

/**
 * A draggable 0-1 track: ports .time-track / .range-track pointer-drag behaviour, which has no
 * declarative Slate equivalent and needs real mouse-capture handling.
 */
class SLanessaDragTrack : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnPctChanged, float);

	SLATE_BEGIN_ARGS(SLanessaDragTrack) {}
		SLATE_EVENT(FOnPctChanged, OnPctChanged)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnPctChangedDelegate = InArgs._OnPctChanged;
		ChildSlot[InArgs._Content.Widget];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bDragging = true;
			UpdateFromPointer(MyGeometry, MouseEvent);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		return FReply::Unhandled();
	}
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging)
		{
			UpdateFromPointer(MyGeometry, MouseEvent);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging)
		{
			bDragging = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

private:
	void UpdateFromPointer(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const float W = MyGeometry.GetLocalSize().X;
		const float Pct = W > 0.f ? FMath::Clamp(Local.X / W, 0.f, 1.f) : 0.f;
		OnPctChangedDelegate.ExecuteIfBound(Pct);
	}

	FOnPctChanged OnPctChangedDelegate;
	bool bDragging = false;
};
