// Pure geometry rule tests for the frameless caption hit module.
// Links Qt6::Core + GTest only: no Host, no widgets, no windows.h.

#include "caption_hit.hpp"

#include <gtest/gtest.h>

using mps::host::CaptionHitInput;
using mps::host::CaptionHitPart;
using mps::host::captionHitTest;

namespace
{
	/// Mirrors the real layout: 8px gutter, 40px title bar, tabs on the left,
	/// trailing blank in the middle, 28x24 min/max/close on the right.
	CaptionHitInput baseInput()
	{
		CaptionHitInput in;
		in.point = QPoint(500, 28); // trailing blank, inside the title bar
		in.windowSize = QSize(960, 640);
		in.titleBarRect = QRect(8, 8, 944, 40);
		in.minButtonRect = QRect(860, 16, 28, 24);
		in.maxButtonRect = QRect(892, 16, 28, 24);
		in.closeButtonRect = QRect(924, 16, 28, 24);
		in.interactiveRects = {QRect(16, 12, 200, 32)};
		in.resizeBandThickness = 8;
		return in;
	}

	void setPoint(CaptionHitInput& in, int x, int y)
	{
		in.point = QPoint(x, y);
	}
} // namespace

TEST(CaptionHit, TitleBarBlankAndTrailingAreaAreCaption)
{
	CaptionHitInput in = baseInput();
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Caption); // trailing blank
	setPoint(in, 300, 20);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Caption); // other blank
	setPoint(in, 940, 10);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Caption); // blank above buttons
}

TEST(CaptionHit, SystemButtonsBeatTitleBar)
{
	CaptionHitInput in = baseInput();
	setPoint(in, 870, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Minimize);
	setPoint(in, 900, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Maximize);
	setPoint(in, 935, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Close);
}

TEST(CaptionHit, InteractiveRectsBeatTitleBarButNotSystemButtons)
{
	CaptionHitInput in = baseInput();
	setPoint(in, 100, 28); // inside the tab rect
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	// An interactive rect overlapping the close button must not steal it.
	in.interactiveRects.push_back(in.closeButtonRect);
	setPoint(in, 935, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Close);
}

TEST(CaptionHit, ResizeBandBeatsTitleBarAndButtons)
{
	CaptionHitInput in = baseInput();
	setPoint(in, 5, 28); // left gutter next to the title bar
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::EdgeLeft);
	setPoint(in, 500, 5); // top gutter
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::EdgeTop);
	// Crafted overlap: a system button inside the band still loses to the band.
	in.minButtonRect = QRect(0, 16, 28, 24);
	setPoint(in, 3, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::EdgeLeft);
}

TEST(CaptionHit, CornersBeatEdges)
{
	CaptionHitInput in = baseInput();
	setPoint(in, 5, 5);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::CornerTopLeft);
	setPoint(in, 955, 5);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::CornerTopRight);
	setPoint(in, 5, 635);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::CornerBottomLeft);
	setPoint(in, 955, 635);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::CornerBottomRight);
	// Pure edge, just outside both corner zones.
	setPoint(in, 5, 100);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::EdgeLeft);
}

TEST(CaptionHit, MaximizedForcesBandToZero)
{
	CaptionHitInput in = baseInput();
	in.maximized = true;
	in.titleBarRect = QRect(0, 0, 960, 40); // zero gutter when maximized
	in.resizeBandThickness = 8;				// stale value must be ignored
	setPoint(in, 3, 20);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Caption); // not EdgeLeft
	setPoint(in, 500, 5);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Caption); // not EdgeTop
}

TEST(CaptionHit, MaximizedButtonsStillBeatCaption)
{
	CaptionHitInput in = baseInput();
	in.maximized = true;
	setPoint(in, 935, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Close);
	setPoint(in, 900, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Maximize);
	setPoint(in, 870, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Minimize);
}

TEST(CaptionHit, FullScreenBlankIsNoneButButtonsStay)
{
	CaptionHitInput in = baseInput();
	in.fullScreen = true;
	setPoint(in, 500, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None); // blank not draggable
	setPoint(in, 935, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Close);
	setPoint(in, 900, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Maximize);
	setPoint(in, 870, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Minimize);
}

TEST(CaptionHit, FullScreenDiffersFromNormalAndMaximized)
{
	CaptionHitInput in = baseInput();
	setPoint(in, 500, 28);
	const CaptionHitPart normal = captionHitTest(in);
	in.fullScreen = true;
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	in.fullScreen = false;
	in.maximized = true;
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::Caption);
	EXPECT_EQ(normal, CaptionHitPart::Caption);
}

TEST(CaptionHit, PausedReportsNoneEverywhere)
{
	CaptionHitInput in = baseInput();
	in.paused = true;
	setPoint(in, 500, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	setPoint(in, 935, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	setPoint(in, 5, 5);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	setPoint(in, 100, 28);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
}

TEST(CaptionHit, BelowTitleBarIsNone)
{
	CaptionHitInput in = baseInput();
	setPoint(in, 500, 100);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	setPoint(in, 480, 300);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
}

TEST(CaptionHit, OutsideWindowIsNone)
{
	CaptionHitInput in = baseInput();
	setPoint(in, -5, 20);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
	setPoint(in, 1000, 20);
	EXPECT_EQ(captionHitTest(in), CaptionHitPart::None);
}
