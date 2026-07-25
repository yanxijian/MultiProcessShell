#include "tab_embed_map.hpp"

#include <gtest/gtest.h>

using mps::tab_strip::TabEmbedMap;

TEST(TabEmbedMap, BindPeekHasTake)
{
	TabEmbedMap map;
	EXPECT_FALSE(map.has(1));
	map.bind(1, 0x100);
	EXPECT_TRUE(map.has(1));
	EXPECT_EQ(map.peek(1), 0x100u);
	EXPECT_EQ(map.take(1), 0x100u);
	EXPECT_FALSE(map.has(1));
	EXPECT_EQ(map.take(1), 0u);
}

TEST(TabEmbedMap, BindIgnoresZeroIds)
{
	TabEmbedMap map;
	map.bind(0, 0x100);
	map.bind(1, 0);
	EXPECT_EQ(map.size(), 0u);
}

TEST(TabEmbedMap, TransferBetweenMaps)
{
	TabEmbedMap source;
	TabEmbedMap target;
	source.bind(7, 0xABC);
	const auto wid = source.take(7);
	EXPECT_EQ(wid, 0xABCu);
	EXPECT_FALSE(source.has(7));
	target.bind(7, wid);
	EXPECT_TRUE(target.has(7));
	EXPECT_EQ(target.peek(7), 0xABCu);
}

TEST(TabEmbedMap, UnbindAndClear)
{
	TabEmbedMap map;
	map.bind(1, 10);
	map.bind(2, 20);
	map.unbind(1);
	EXPECT_FALSE(map.has(1));
	EXPECT_TRUE(map.has(2));
	map.clear();
	EXPECT_EQ(map.size(), 0u);
}
