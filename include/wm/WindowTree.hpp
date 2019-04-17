#pragma once

#include <deque>
#include <iterator>

#include "wm/WindowNodeIndex.hpp"
#include "wm/WindowData.hpp"

namespace wm
{
  class WindowTree
  {

  private:
    struct WindowNode
    {
      WindowNodeIndex parent;
      WindowNodeIndex firstChild;
      WindowNodeIndex nextSibling;
      WindowData data;
    };

    WindowNodeIndex freeList;
    std::deque<WindowNode> nodes;

    WindowNode &getNode(WindowNodeIndex nodeIndex) noexcept
    {
      return nodes[nodeIndex.data];
    }

    WindowNode const &getNode(WindowNodeIndex nodeIndex) const noexcept
    {
      return const_cast<WindowTree &>(*this).getNode(nodeIndex);
    }
  public:
    WindowTree() = delete;
    WindowTree(WindowTree &&) = delete;
    WindowTree(WindowTree const &) = delete;

    WindowTree(WindowData &&screen)
      : freeList(nullNode)
    {
      nodes.emplace_back(WindowNode{nullNode, nullNode, nullNode, std::move(screen)});
    }

    WindowTree(WindowData const &screen)
      : freeList(nullNode)
    {
      nodes.emplace_back(WindowNode{nullNode, nullNode, nullNode, screen});
    }

    uint16_t getWindowCountUpperBound() const noexcept
    {
      return static_cast<uint16_t>(nodes.size());
    }

    WindowNodeIndex getRootIndex() const noexcept
    {
      return WindowNodeIndex{0};
    }

    struct Iterator
    {
      WindowTree const *windowTree;
      WindowNodeIndex nodeIndex;

      Iterator &operator++() noexcept
      {
	nodeIndex = windowTree->getSibling(nodeIndex);
	return *this;
      }

      constexpr bool operator==(Iterator const &other) const noexcept
      {
	return nodeIndex == other.nodeIndex && windowTree == other.windowTree;
      }

      constexpr bool operator!=(Iterator const &other) const noexcept
      {
	return !(*this == other);
      }

      constexpr WindowNodeIndex operator*() const noexcept
      {
	return nodeIndex;
      }
    };

    WindowNodeIndex getParent(WindowNodeIndex nodeIndex) const noexcept
    {
      return getNode(nodeIndex).parent;
    }

    WindowNodeIndex getFirstChild(WindowNodeIndex nodeIndex) const noexcept
    {
      return getNode(nodeIndex).firstChild;
    }

    WindowNodeIndex getSibling(WindowNodeIndex nodeIndex) const noexcept
    {
      return getNode(nodeIndex).nextSibling;
    }

    struct IteratorPair
    {
      Iterator _begin;
      Iterator _end;

      constexpr auto begin() const noexcept
      {
	return _begin;
      }

      constexpr auto end() const noexcept
      {
	return _end;
      }
    };


    IteratorPair getChildren(WindowNodeIndex nodeIndex) const noexcept
    {
      return {Iterator{this, getFirstChild(nodeIndex)}, Iterator{this, nullNode}};
    }
   
    WindowData &getData(WindowNodeIndex nodeIndex) noexcept
    {
      return getNode(nodeIndex).data;
    }

    WindowData const &getData(WindowNodeIndex nodeIndex) const noexcept
    {
      return const_cast<WindowTree &>(*this).getData(nodeIndex);
    }

    WindowNodeIndex allocateIndex()
    {
      if (freeList == nullNode)
	{
	  WindowNodeIndex result(static_cast<uint16_t>(nodes.size()));

	  nodes.emplace_back();
	  return result;
	}
      else
	{
	  WindowNodeIndex result(freeList);

	  freeList = getSibling(freeList);
	  return result;
	}
    }

    void removeIndex(WindowNodeIndex index) noexcept
    {
      WindowNodeIndex child(getFirstChild(getParent(index)));

      if (child == index)
	getNode(getParent(index)).firstChild = getSibling(index);
      else
	{
	  while (getSibling(child) != index)
	    child = getSibling(child);
	  getNode(child).nextSibling = getSibling(index);
	}
      getNode(index).nextSibling = freeList;
      freeList = index;
    }

    WindowNodeIndex addChild(WindowNodeIndex parent)
    {
      WindowNodeIndex result(allocateIndex());

      getNode(result).parent = parent;
      getNode(result).nextSibling = getFirstChild(parent);
      getNode(result).firstChild = nullNode;
      getNode(parent).firstChild = result;
      return result;
    }

    WindowNodeIndex addChildAfter(WindowNodeIndex parent, WindowNodeIndex index)
    {
      WindowNodeIndex result(allocateIndex());

      getNode(result).parent = parent;
      getNode(result).nextSibling = getNode(index).nextSibling;
      getNode(result).firstChild = nullNode;
      getNode(index).nextSibling = result;
      return result;
    }

  };
}