#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/** A single 24x24-space line-icon primitive, mirroring an SVG <path>/<circle> from the v2 mockup. */
struct FLanessaIconPrim
{
	enum class EType { Line, Circle };
	EType Type;
	TArray<FVector2D> Points; // for Line: polyline points in 24x24 space
	FVector2D Center = FVector2D::ZeroVector; // for Circle
	float Radius = 0.f; // for Circle

	static FLanessaIconPrim MakeLine(std::initializer_list<FVector2D> InPoints)
	{
		FLanessaIconPrim P;
		P.Type = EType::Line;
		P.Points = InPoints;
		return P;
	}
	static FLanessaIconPrim MakeCircle(FVector2D InCenter, float InRadius)
	{
		FLanessaIconPrim P;
		P.Type = EType::Circle;
		P.Center = InCenter;
		P.Radius = InRadius;
		return P;
	}
	static FLanessaIconPrim MakePolyline(TArray<FVector2D> InPoints)
	{
		FLanessaIconPrim P;
		P.Type = EType::Line;
		P.Points = MoveTemp(InPoints);
		return P;
	}
};

/**
 * Minimal SVG path-data tessellator: parses M/L/H/V/A/Z commands (the only ones used by the
 * v2 mockup's icon set) into one FLanessaIconPrim polyline per subpath, so icon shapes -
 * including elliptical arcs (clouds, sun rays) - are geometrically exact, not hand-approximated.
 * Multi-command sequences without a repeated letter (SVG's implicit-command-repeat grammar) are
 * supported for A specifically, since the v2 paths rely on it (e.g. "A4 4 0 0 0 x y A5.5 5.5 0 0 0 x y").
 */
namespace LanessaSvgPath
{
	inline TArray<float> ParseNumbers(const FString& S)
	{
		TArray<float> Out;
		FString Cur;
		auto Flush = [&]() { if (!Cur.IsEmpty()) { Out.Add(FCString::Atof(*Cur)); Cur.Empty(); } };
		for (int32 i = 0; i < S.Len(); i++)
		{
			TCHAR C = S[i];
			if (C == '-' && !Cur.IsEmpty() && Cur[Cur.Len()-1] != 'e' && Cur[Cur.Len()-1] != 'E')
			{
				Flush();
				Cur.AppendChar(C);
			}
			else if (FChar::IsDigit(C) || C == '.' || C == '-' || C == '+' || C == 'e' || C == 'E')
			{
				Cur.AppendChar(C);
			}
			else
			{
				Flush();
			}
		}
		Flush();
		return Out;
	}

	// Tessellates a circular arc (rx==ry, the only case the v2 icon set uses) from Start to End.
	inline void TessellateArc(TArray<FVector2D>& Out, FVector2D Start, FVector2D End, float Rx, float Ry, bool bLargeArc, bool bSweep, int32 Segments = 16)
	{
		const float R = (Rx + Ry) * 0.5f;
		const FVector2D Mid = (Start + End) * 0.5f;
		const FVector2D Delta = End - Start;
		const float DistHalf = Delta.Size() * 0.5f;
		float H = FMath::Sqrt(FMath::Max(0.f, R * R - DistHalf * DistHalf));
		FVector2D Dir = Delta.GetSafeNormal();
		FVector2D Perp(-Dir.Y, Dir.X);
		// Per the SVG endpoint-to-center formula (rx=ry case): center = mid + sign*h*perp, where
		// sign is +1 when largeArcFlag != sweepFlag. This was inverted (== instead of !=), which
		// picked the wrong one of the two possible circle centers - the resulting arc still connected
		// Start to End, but swept the WRONG side of the chord (e.g. large arc when a small one was
		// asked for), producing the scrambled cloud/partly/rain icon shapes seen in-game.
		const bool bPickPositive = (bLargeArc != bSweep);
		FVector2D Center = Mid + Perp * (bPickPositive ? H : -H);

		float AngleStart = FMath::Atan2(Start.Y - Center.Y, Start.X - Center.X);
		float AngleEnd = FMath::Atan2(End.Y - Center.Y, End.X - Center.X);

		float Delta_ = AngleEnd - AngleStart;
		if (bSweep && Delta_ < 0) Delta_ += 2.f * PI;
		if (!bSweep && Delta_ > 0) Delta_ -= 2.f * PI;

		for (int32 i = 1; i <= Segments; i++)
		{
			float T = (float)i / (float)Segments;
			float A = AngleStart + Delta_ * T;
			Out.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
	}

	inline TArray<FLanessaIconPrim> Tessellate(const FString& D)
	{
		TArray<FLanessaIconPrim> Result;
		TArray<FVector2D> Current;
		FVector2D Pos(0, 0);
		TCHAR LastCmd = 0;

		int32 i = 0;
		while (i < D.Len())
		{
			TCHAR C = D[i];
			if (FChar::IsWhitespace(C)) { i++; continue; }

			TCHAR Cmd = LastCmd;
			if (FChar::IsAlpha(C))
			{
				Cmd = C;
				i++;
			}

			// gather the numeric token(s) for this command instance
			auto ReadNum = [&]() -> float
			{
				while (i < D.Len() && (FChar::IsWhitespace(D[i]))) i++;
				int32 Start = i;
				if (i < D.Len() && D[i] == '-') i++;
				while (i < D.Len() && (FChar::IsDigit(D[i]) || D[i] == '.')) i++;
				return FCString::Atof(*D.Mid(Start, i - Start));
			};

			switch (Cmd)
			{
				case 'M':
				{
					if (!Current.IsEmpty()) { Result.Add(FLanessaIconPrim::MakePolyline(Current)); Current.Empty(); }
					{
						// FVector2D(ReadNum(), ReadNum()) has unspecified argument evaluation order in
						// C++ (MSVC evaluates right-to-left), silently swapping X/Y since both calls
						// mutate the shared parse cursor `i` - read into named locals to force order.
						const float NX = ReadNum();
						const float NY = ReadNum();
						Pos = FVector2D(NX, NY);
					}
					Current.Add(Pos);
					LastCmd = 'L'; // subsequent bare coordinate pairs after M are implicit lineto
					break;
				}
				case 'L':
				{
					{
						const float NX = ReadNum();
						const float NY = ReadNum();
						Pos = FVector2D(NX, NY);
					}
					Current.Add(Pos);
					LastCmd = 'L';
					break;
				}
				case 'H':
				{
					Pos.X = ReadNum();
					Current.Add(Pos);
					LastCmd = 'H';
					break;
				}
				case 'V':
				{
					Pos.Y = ReadNum();
					Current.Add(Pos);
					LastCmd = 'V';
					break;
				}
				case 'A':
				{
					const float Rx = ReadNum();
					const float Ry = ReadNum();
					ReadNum(); // x-axis-rotation (unused, always 0 in this icon set)
					const bool bLargeArc = ReadNum() != 0.f;
					const bool bSweep = ReadNum() != 0.f;
					const float EndX = ReadNum();
					const float EndY = ReadNum();
					FVector2D End(EndX, EndY);
					LanessaSvgPath::TessellateArc(Current, Pos, End, Rx, Ry, bLargeArc, bSweep);
					Pos = End;
					LastCmd = 'A';
					break;
				}
				case 'Z':
				case 'z':
				{
					if (!Current.IsEmpty()) { const FVector2D First = Current[0]; Current.Add(First); }
					LastCmd = 0;
					break;
				}
				default:
					i++;
					break;
			}
		}
		if (!Current.IsEmpty()) { Result.Add(FLanessaIconPrim::MakePolyline(Current)); }
		return Result;
	}
}

/** Renders a set of 24x24-space line primitives (ported directly from the v2 SVG icons) as a small vector icon. */
class SLanessaLineIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLanessaLineIcon)
		: _IconSize(18.f)
		, _StrokeColor(FLinearColor::White)
		, _StrokeWidth(1.3f)
		{}
		SLATE_ARGUMENT(TArray<FLanessaIconPrim>, Primitives)
		SLATE_ATTRIBUTE(float, IconSize)
		SLATE_ATTRIBUTE(FLinearColor, StrokeColor)
		SLATE_ARGUMENT(float, StrokeWidth)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Primitives = InArgs._Primitives;
		IconSize = InArgs._IconSize;
		StrokeColor = InArgs._StrokeColor;
		StrokeWidth = InArgs._StrokeWidth;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const float Size = IconSize.Get();
		const float Scale = AllottedGeometry.GetLocalSize().X / 24.f;
		const FLinearColor Color = StrokeColor.Get();

		for (const FLanessaIconPrim& Prim : Primitives)
		{
			if (Prim.Type == FLanessaIconPrim::EType::Line)
			{
				TArray<FVector2D> ScaledPoints;
				ScaledPoints.Reserve(Prim.Points.Num());
				for (const FVector2D& P : Prim.Points)
				{
					ScaledPoints.Add(P * Scale);
				}
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ScaledPoints, ESlateDrawEffect::None, Color, true, StrokeWidth);
			}
			else
			{
				const int32 Segments = 24;
				TArray<FVector2D> CirclePoints;
				CirclePoints.Reserve(Segments + 1);
				for (int32 i = 0; i <= Segments; i++)
				{
					const float Angle = (float)i / (float)Segments * 2.f * PI;
					CirclePoints.Add((Prim.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Prim.Radius) * Scale);
				}
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CirclePoints, ESlateDrawEffect::None, Color, true, StrokeWidth);
			}
		}

		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Size = IconSize.Get();
		return FVector2D(Size, Size);
	}

private:
	TArray<FLanessaIconPrim> Primitives;
	TAttribute<float> IconSize;
	TAttribute<FLinearColor> StrokeColor;
	float StrokeWidth = 1.3f;
};

/**
 * Icon sets ported EXACTLY from the v2 mockup's <svg> markup: every d="..." string here is
 * copied verbatim from the artifact HTML and run through LanessaSvgPath::Tessellate, and every
 * circle uses the same cx/cy/r as the source <circle> element. Nothing here is hand-approximated.
 * Source: https://claude.ai/code/artifact/46c1b5b3-b961-4725-b312-62cb482b6140
 */
namespace LanessaIcons
{
	// <svg class="ic" viewBox="0 0 24 24"><path d="M4 11 L12 4 L20 11 M6 10 V20 H18 V10"/></svg>
	inline TArray<FLanessaIconPrim> Home()
	{
		return LanessaSvgPath::Tessellate(TEXT("M4 11 L12 4 L20 11 M6 10 V20 H18 V10"));
	}
	// <circle cx="12" cy="12" r="8"/><path d="M12 4 V2 M12 22 V20 M4 12 H2 M22 12 H20"/>
	inline TArray<FLanessaIconPrim> Atrofi()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M12 4 V2 M12 22 V20 M4 12 H2 M22 12 H20"));
		R.Add(FLanessaIconPrim::MakeCircle({12,12}, 8.f));
		return R;
	}
	// <rect x="4" y="4" width="7" height="7"/><rect x="13" y="4".../><rect x="4" y="13".../><rect x="13" y="13".../>
	inline TArray<FLanessaIconPrim> Qulay()
	{
		return {
			FLanessaIconPrim::MakeLine({{4,4},{11,4},{11,11},{4,11},{4,4}}),
			FLanessaIconPrim::MakeLine({{13,4},{20,4},{20,11},{13,11},{13,4}}),
			FLanessaIconPrim::MakeLine({{4,13},{11,13},{11,20},{4,20},{4,13}}),
			FLanessaIconPrim::MakeLine({{13,13},{20,13},{20,20},{13,20},{13,13}}),
		};
	}
	// <circle cx="10" cy="10" r="6.5"/><path d="M15 15 L21 21"/>
	inline TArray<FLanessaIconPrim> Qidiruv()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M15 15 L21 21"));
		R.Add(FLanessaIconPrim::MakeCircle({10,10}, 6.5f));
		return R;
	}
	// <path d="M3 6 H21 M3 12 H21 M3 18 H21"/><path d="M7 6 V18 M15 6 V18" opacity=".4"/>
	inline TArray<FLanessaIconPrim> Qirqim()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M3 6 H21 M3 12 H21 M3 18 H21"));
		R.Append(LanessaSvgPath::Tessellate(TEXT("M7 6 V18 M15 6 V18")));
		return R;
	}
	// <rect x="3" y="4" width="18" height="14"/><path d="M3 15 L9 9 L14 14 L18 10 L21 13"/>
	inline TArray<FLanessaIconPrim> Galereya()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M3 15 L9 9 L14 14 L18 10 L21 13"));
		R.Add(FLanessaIconPrim::MakeLine({{3,4},{21,4},{21,18},{3,18},{3,4}}));
		return R;
	}
	// rail-foot "Sozlamalar": <circle cx="12" cy="12" r="3"/><path d="M12 3 V6 M12 18 V21 M3 12 H6 M18 12 H21 M5.6 5.6 L7.8 7.8 M16.2 16.2 L18.4 18.4 M18.4 5.6 L16.2 7.8 M7.8 16.2 L5.6 18.4"/>
	inline TArray<FLanessaIconPrim> Settings()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M12 3 V6 M12 18 V21 M3 12 H6 M18 12 H21 M5.6 5.6 L7.8 7.8 M16.2 16.2 L18.4 18.4 M18.4 5.6 L16.2 7.8 M7.8 16.2 L5.6 18.4"));
		R.Add(FLanessaIconPrim::MakeCircle({12,12}, 3.f));
		return R;
	}
	// utility "Sozlamalar" (simpler 4-tick variant): <circle cx="12" cy="12" r="3"/><path d="M12 3 V6 M12 18 V21 M3 12 H6 M18 12 H21"/>
	inline TArray<FLanessaIconPrim> UtilitySettings()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M12 3 V6 M12 18 V21 M3 12 H6 M18 12 H21"));
		R.Add(FLanessaIconPrim::MakeCircle({12,12}, 3.f));
		return R;
	}
	// <path d="M15 4 H7 V20 H15 M19 12 H10 M15 8 L19 12 L15 16"/>
	inline TArray<FLanessaIconPrim> Exit()
	{
		return LanessaSvgPath::Tessellate(TEXT("M15 4 H7 V20 H15 M19 12 H10 M15 8 L19 12 L15 16"));
	}

	// ---- these three have no counterpart in v2 (added on the user's separate, explicit request
	// to extend the nav rail) - kept out of ULanessaV2Widget, used only by ULanessaMasterMenuWidget.
	inline TArray<FLanessaIconPrim> VRProgulka()
	{
		return {
			FLanessaIconPrim::MakeLine({{3,9},{21,9},{21,17},{15,17},{13,14},{11,14},{9,17},{3,17},{3,9}}),
			FLanessaIconPrim::MakeCircle({8,13}, 1.6f),
			FLanessaIconPrim::MakeCircle({16,13}, 1.6f),
		};
	}
	inline TArray<FLanessaIconPrim> Progulka()
	{
		return {
			FLanessaIconPrim::MakeLine({{5,7},{12,12},{5,17}}),
			FLanessaIconPrim::MakeLine({{13,7},{20,12},{13,17}}),
		};
	}
	inline TArray<FLanessaIconPrim> Interyer()
	{
		return {
			FLanessaIconPrim::MakeLine({{4,20},{4,6},{12,3},{20,6},{20,20},{4,20}}),
			FLanessaIconPrim::MakeLine({{4,13},{20,13}}),
		};
	}

	// ---- weather-row icons, tessellated exactly from the v2 <svg> markup (including arcs) ----
	// <circle cx="12" cy="12" r="4.5"/><path d="M12 3 V5 M12 19 V21 M3 12 H5 M19 12 H21 M5.6 5.6 L7 7 M17 17 L18.4 18.4 M18.4 5.6 L17 7 M7 17 L5.6 18.4"/>
	inline TArray<FLanessaIconPrim> WeatherSun()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M12 3 V5 M12 19 V21 M3 12 H5 M19 12 H21 M5.6 5.6 L7 7 M17 17 L18.4 18.4 M18.4 5.6 L17 7 M7 17 L5.6 18.4"));
		R.Add(FLanessaIconPrim::MakeCircle({12,12}, 4.5f));
		return R;
	}
	// <path d="M7 18 H17 A4 4 0 0 0 16.2 10.2 A5.5 5.5 0 0 0 6 12.5 A3.5 3.5 0 0 0 7 18Z"/>
	inline TArray<FLanessaIconPrim> WeatherCloud()
	{
		return LanessaSvgPath::Tessellate(TEXT("M7 18 H17 A4 4 0 0 0 16.2 10.2 A5.5 5.5 0 0 0 6 12.5 A3.5 3.5 0 0 0 7 18Z"));
	}
	// <circle cx="8" cy="9" r="3.4"/><path d="M11 18 H18 A3.6 3.6 0 0 0 17.3 11 A4.6 4.6 0 0 0 9.3 12.6"/>
	inline TArray<FLanessaIconPrim> WeatherPartly()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M11 18 H18 A3.6 3.6 0 0 0 17.3 11 A4.6 4.6 0 0 0 9.3 12.6"));
		R.Add(FLanessaIconPrim::MakeCircle({8,9}, 3.4f));
		return R;
	}
	// <path d="M6 14 H16 A4 4 0 0 0 15.2 6.2 A5.5 5.5 0 0 0 5 8.5 A3.5 3.5 0 0 0 6 14Z"/><path d="M8 18 L7 20 M12 18 L11 20 M16 18 L15 20"/>
	inline TArray<FLanessaIconPrim> WeatherRain()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M6 14 H16 A4 4 0 0 0 15.2 6.2 A5.5 5.5 0 0 0 5 8.5 A3.5 3.5 0 0 0 6 14Z"));
		R.Append(LanessaSvgPath::Tessellate(TEXT("M8 18 L7 20 M12 18 L11 20 M16 18 L15 20")));
		return R;
	}
	// no v2 counterpart - added on the user's separate, explicit request to extend the weather
	// row with a winter option; a plain 6-point asterisk/snowflake in the same minimal line style.
	inline TArray<FLanessaIconPrim> WeatherSnow()
	{
		return {
			FLanessaIconPrim::MakeLine({{4.f,12.f},{20.f,12.f}}),
			FLanessaIconPrim::MakeLine({{8.f,5.07f},{16.f,18.93f}}),
			FLanessaIconPrim::MakeLine({{16.f,5.07f},{8.f,18.93f}}),
		};
	}

	// ---- season-row icons, same minimal 24x24 line language as the weather row above.
	// No v2 mockup counterpart: the season row is an addition, so these are drawn in the
	// established style (single-stroke paths, 24x24 box) rather than ported from markup.
	// A leaf/branch metaphor keeps them readable against the weather row's sky metaphor.

	// Bahor - a sprout: one stem with two young leaves opening off it.
	inline TArray<FLanessaIconPrim> SeasonSpring()
	{
		return LanessaSvgPath::Tessellate(TEXT("M12 21 V10 M12 14 C8 14 6 11.5 6 8.5 C9.5 8.5 12 11 12 14 Z M12 11.5 C12 8.5 14.5 6 18 6 C18 9 15.5 11.5 12 11.5 Z"));
	}
	// Yoz - a full broadleaf with its midrib.
	inline TArray<FLanessaIconPrim> SeasonSummer()
	{
		return LanessaSvgPath::Tessellate(TEXT("M5 19 C5 11 11 5 19 5 C19 13 13 19 5 19 Z M5 19 L19 5"));
	}
	// Kuz - the same leaf tipped over, with drifting marks falling away beneath it.
	inline TArray<FLanessaIconPrim> SeasonAutumn()
	{
		TArray<FLanessaIconPrim> R = LanessaSvgPath::Tessellate(TEXT("M4 13 C4 7.5 8.5 3 14 3 C14 8.5 9.5 13 4 13 Z M4 13 L14 3"));
		R.Append(LanessaSvgPath::Tessellate(TEXT("M9 17 L8 19.5 M14 16 L13 18.5 M18.5 14.5 L17.5 17")));
		return R;
	}
	// Qish - a bare branch, every leaf gone.
	inline TArray<FLanessaIconPrim> SeasonWinter()
	{
		return LanessaSvgPath::Tessellate(TEXT("M12 21 V4 M12 15 L7 10.5 M12 17 L17 12.5 M12 10.5 L8 6.5 M12 12 L16 8"));
	}

	// tour-start .core svg: <path d="M8 5 L19 12 L8 19 Z"/>
	inline TArray<FLanessaIconPrim> PlayTriangle()
	{
		return LanessaSvgPath::Tessellate(TEXT("M8 5 L19 12 L8 19 Z"));
	}
}
