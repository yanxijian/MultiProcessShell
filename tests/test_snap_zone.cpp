#include "snap_zone.hpp"

#include <gtest/gtest.h>

using mps::host::snap::Result;
using mps::host::snap::Zone;
using mps::host::snap::zoneAt;

namespace
{
	constexpr QRect kScreen{0, 0, 1920, 1040};
	constexpr int kBand = 8;
} // namespace

TEST(SnapZone, InsideTopBandMaximizes)
{
	EXPECT_EQ(zoneAt({100, 0}, kScreen, kBand).zone, Zone::Maximize);
	EXPECT_EQ(zoneAt({1919, 7}, kScreen, kBand).zone, Zone::Maximize);
}

TEST(SnapZone, BelowBandIsNone)
{
	EXPECT_EQ(zoneAt({100, 8}, kScreen, kBand).zone, Zone::None);
	EXPECT_EQ(zoneAt({100, 400}, kScreen, kBand).zone, Zone::None);
}

TEST(SnapZone, OutsideHorizontalSpanIsNone)
{
	EXPECT_EQ(zoneAt({-1, 3}, kScreen, kBand).zone, Zone::None);
	EXPECT_EQ(zoneAt({1920, 3}, kScreen, kBand).zone, Zone::None);
}

TEST(SnapZone, NegativeScreenOriginWorks)
{
	const QRect secondary{-1920, 0, 1920, 1040};
	EXPECT_EQ(zoneAt({-1920, 4}, secondary, kBand).zone, Zone::Maximize);
	EXPECT_EQ(zoneAt({-100, 9}, secondary, kBand).zone, Zone::None);
}

TEST(SnapZone, ZeroThicknessOrInvalidRectDisables)
{
	EXPECT_EQ(zoneAt({100, 0}, kScreen, 0).zone, Zone::None);
	EXPECT_EQ(zoneAt({100, 0}, QRect{}, kBand).zone, Zone::None);
}

TEST(SnapZone, TargetRectIsWorkArea)
{
	const Result r = zoneAt({500, 2}, kScreen, kBand);
	ASSERT_EQ(r.zone, Zone::Maximize);
	EXPECT_EQ(r.targetRect, kScreen);
}
