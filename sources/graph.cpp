#include <algorithm>

#include "generator.h"

#include <cage-core/filesystem.h>
#include <cage-core/utility/random.h>
#include <cage-core/utility/color.h>

namespace generator
{
	graphStruct::graphStruct() : links(comparator(this)), oriented(false)
	{}

	void graphStruct::clear()
	{
		links.clear();
		nodes.clear();
	}

	void graphStruct::addRandomNodes(uint64 seed, uint32 nodesCount)
	{
		nodes.reserve(nodes.size() + nodesCount);
		randomGenerator rnd(globalSeed, seed);
		for (uint32 i = 0; i < nodesCount; i++)
		{
			nodeStruct n;
			n.position = rnd.randomDirection3() * pow(rnd.random(), 0.2);
			nodes.push_back(n);
		}
	}

	void graphStruct::addRandomLinks(uint64 seed, uint32 linksCount, real maxLength, uint32 maxAttempts)
	{
		CAGE_ASSERT_RUNTIME(nodes.size() > 0);
		CAGE_ASSERT_RUNTIME(links.size() + linksCount < nodes.size() * (nodes.size() - 1) / 2, links.size(), linksCount, nodes.size());
		randomGenerator rnd(globalSeed, seed);
		uint32 target = numeric_cast<uint32>(links.size()) + linksCount;
		for (uint32 i = 0; i < maxAttempts && links.size() < target; i++)
		{
			linkStruct l;
			l.a = rnd.random(0u, numeric_cast<uint32>(nodes.size()));
			l.b = rnd.random(0u, numeric_cast<uint32>(nodes.size()));
			if (l.a == l.b)
				continue;
			if (nodes[l.a].position.distance(nodes[l.b].position) > maxLength)
				continue;
			links.insert(l);
		}
	}

	void graphStruct::generateLinksFull()
	{
		links.clear();
		uint32 cnt = numeric_cast<uint32>(nodes.size());
		for (uint32 a = 0; a < cnt; a++)
			for (uint32 b = a + 1; b < cnt; b++)
				links.insert(linkStruct(a, b));
	}

	void graphStruct::linkCostsByLength(bool inverse)
	{
		std::set<linkStruct, comparator> ll(comparator(this));
		for (linkStruct l : links)
		{
			l.cost = distance(nodes[l.a].position, nodes[l.b].position);
			if (inverse)
				l.cost = 1 / l.cost;
			ll.insert(l);
		}
		ll.swap(links);
	}

	void graphStruct::removeNode(uint32 index)
	{
		// remove links that points to invalid node
		auto it = links.begin();
		while (it != links.end())
		{
			if (it->a == index || it->b == index)
				it = links.erase(it);
			else
				it++;
		}
		// update links indexes
		std::set<linkStruct, comparator> ll(comparator(this));
		for (const linkStruct &l : links)
		{
			linkStruct m(l);
			if (m.a > index)
				m.a--;
			if (m.b > index)
				m.b--;
			ll.insert(m);
		}
		ll.swap(links);
		// remove the actual node
		nodes.erase(nodes.begin() + index);
	}

	void graphStruct::removeNodeOverlaps()
	{
		uint32 cnt = numeric_cast<uint32>(nodes.size());
		for (uint32 a = 0; a < cnt; a++)
		{
			for (uint32 b = a + 1; b < cnt; b++)
			{
				if (nodes[a].position.distance(nodes[b].position) < nodes[a].radius + nodes[b].radius)
				{
					if (nodes[a].radius < nodes[b].radius)
						removeNode(a);
					else
						removeNode(b);
					return removeNodeOverlaps();
				}
			}
		}
	}

	void graphStruct::removeLinkOverlaps()
	{
		for (const linkStruct &a : links)
		{
			for (const linkStruct &b : links)
			{
				if (a.a == b.a || a.a == b.b || a.b == b.a || a.b == b.b)
					continue;
				if (!comparator(this)(a, b))
					continue;
				if (distance(makeSegment(nodes[a.a].position, nodes[a.b].position), makeSegment(nodes[b.a].position, nodes[b.b].position)) < a.radius + b.radius)
				{
					links.erase(a);
					return removeLinkOverlaps();
				}
			}
		}
	}

	void graphStruct::minimumSpanningTree()
	{
		CAGE_ASSERT_RUNTIME(componentsCount() == 1, nodes.size(), links.size());
		std::vector<linkStruct> ls;
		ls.reserve(links.size());
		for (const linkStruct &l : links)
			ls.push_back(l);
		std::sort(ls.begin(), ls.end(), [](const linkStruct &a, const linkStruct &b) { return a.cost < b.cost; });
		links.clear();
		uint32 csPrev = numeric_cast<uint32>(nodes.size());
		for (const linkStruct &l : ls)
		{
			links.insert(l);
			uint32 csNow = componentsCount();
			if (csPrev == csNow)
			{
				// the link did not decrement the components count
				links.erase(l);
				continue;
			}
			CAGE_ASSERT_RUNTIME(csNow == csPrev - 1);
			if (csNow == 1)
				break;
			else
				csPrev = csNow;
		}
		CAGE_ASSERT_RUNTIME(componentsCount() == 1, nodes.size(), links.size());
	}

	namespace
	{
		void removeUnusedNodes(graphStruct &g, const aabb &box)
		{
			uint32 cnt = numeric_cast<uint32>(g.nodes.size());
			for (uint32 i = 0; i < cnt; i++)
			{
				graphStruct::nodeStruct &n = g.nodes[i];
				if (distance(box, n.position) < n.radius)
					continue;
				bool used = false;
				for (const graphStruct::linkStruct &l : g.links)
				{
					if (l.a == i || l.b == i)
					{
						used = true;
						break;
					}
				}
				if (used)
					continue;
				g.removeNode(i);
				return removeUnusedNodes(g, box);
			}
		}
	}

	void graphStruct::clamp(const aabb &box)
	{
		std::set<linkStruct, comparator> ll(comparator(this));
		for (const linkStruct &l : links)
		{
			vec3 a = nodes[l.a].position, b = nodes[l.b].position;
			aabb bb = aabb(box.a - l.radius, box.b + l.radius);
			if (intersects(bb, makeSegment(a, b)))
				ll.insert(l);
		}
		ll.swap(links);
		removeUnusedNodes(*this, box);
	}

	uint32 graphStruct::componentsCount() const
	{
		uint32 cnt = numeric_cast<uint32>(nodes.size());
		std::vector<uint32> c;
		c.resize(cnt);
		for (uint32 i = 0; i < cnt; i++)
			c[i] = i;
		while (true)
		{
			bool change = false;
			for (const linkStruct &l : links)
			{
				uint32 &a = c[l.a];
				uint32 &b = c[l.b];
				if (a != b)
				{
					a = b = min(a, b);
					change = true;
				}
			}
			if (!change)
				break;
		}
		std::set<uint32> cc;
		for (uint32 i : c)
			cc.insert(i);
		return numeric_cast<uint32>(cc.size());
	}

	graphStruct::linkStruct::linkStruct() : a(-1), b(-1)
	{}

	graphStruct::linkStruct::linkStruct(uint32 a, uint32 b) : a(a), b(b)
	{}

	graphStruct::comparator::comparator(graphStruct *graph) : graph(graph)
	{}

	bool graphStruct::comparator::operator () (const linkStruct &a, const linkStruct &b) const
	{
		uint32 a1, a2, b1, b2;
		if (graph->oriented)
		{
			a1 = a.a;
			a2 = a.b;
			b1 = b.a;
			b2 = b.b;
		}
		else
		{
			a1 = min(a.a, a.b);
			a2 = max(a.a, a.b);
			b1 = min(b.a, b.b);
			b2 = max(b.a, b.b);
		}
		if (a1 == b1)
			return a2 < b2;
		return a1 < b1;
	}

	void graphStruct::colorizeNodes()
	{
		uint32 cnt = numeric_cast<uint32>(nodes.size());
		uint32 i = 0;
		for (nodeStruct &n : nodes)
			n.color = convertHsvToRgb(vec3(real(i++) / cnt, 1, 1));
	}
}
