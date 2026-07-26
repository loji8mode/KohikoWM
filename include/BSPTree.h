#pragma once

#include "BSPLeaf.h"
#include "BSPSplit.h"
#include "Types.h"

#include <memory>
#include <string>
#include <vector>

namespace Kohiko
{

class ManagedWindow;

// Owns the binary space partition for one workspace.
//
// Every tiled window is a BSPLeaf; every branch is a BSPSplit with
// exactly two children and a ratio. Remove() always collapses a
// Split down to its surviving child, so there are never any empty
// nodes to trip over later.
class BSPTree
{
public:

    BSPTree();

    bool Empty() const;

    void Clear();

    BSPNode* Root() const;

    int Count() const;

    // The window at the cached "anchor" leaf - the most recently
    // focused one, or a sane fallback if nothing has been focused
    // yet. This is what Insert() attaches new windows next to.
    ManagedWindow* FocusedWindow() const;

    // Inserts `window` as a new leaf right next to the anchor leaf,
    // splitting that leaf's slot in two. The split direction is
    // chosen from the anchor's current aspect ratio (wide -> side by
    // side, tall -> stacked), matching the Hyprland-style behaviour
    // from the spec:
    //
    //   A  ->  A B  ->  (A|B)  ->  (A|(B|C))  ->  ((A|D)|(B|C))
    void Insert(ManagedWindow* window);

    // Removes `window`'s leaf and collapses its parent Split away,
    // promoting the sibling into the parent's old place.
    void Remove(ManagedWindow* window);

    // Bookkeeping only: remembers `window`'s leaf as the insert
    // anchor. Actual X input focus is WindowManager's job.
    void Focus(ManagedWindow* window);

    // Super+LMB drag target: swaps which window sits in which leaf.
    // Geometry never changes - only the window <-> leaf assignment
    // does, so the two windows trade places on screen and everything
    // else in the tree is completely undisturbed.
    void Swap(ManagedWindow* first, ManagedWindow* second);

    // Super+RMB drag target: nudges the ratio of `window`'s immediate
    // parent Split so the divider between its two children tracks the
    // mouse 1:1 on screen (dx for a left/right split, dy for a
    // top/bottom one) - dragging right/down always moves the divider
    // right/down, regardless of whether `window` is the first or
    // second child, so whichever of the two windows you actually
    // grabbed grows or shrinks exactly like dragging a normal resize
    // border would. Everything else in that subtree adjusts
    // automatically next time LayoutEngine runs.
    void Resize(ManagedWindow* window, int dx, int dy);

    // Super+h/j/k/l: finds the adjacent leaf in `direction` using
    // each node's last computed Geometry(), so it works correctly no
    // matter how lopsided the tree is.
    ManagedWindow* FindNeighbor(ManagedWindow* window, Direction direction) const;

    // Toggles `window`'s immediate parent Split direction
    // (Vertical <-> Horizontal).
    void Rotate(ManagedWindow* window);

    // Swaps the two children of `window`'s immediate parent Split
    // (mirrors the pane order without changing the split axis).
    void Flip(ManagedWindow* window);

    // Finds whichever leaf's last computed Geometry() contains `p`.
    ManagedWindow* HitTest(const Point& p) const;

    // Answers "if Insert() were called right now, would either half of
    // the split it creates come out smaller than minWidth x minHeight?"
    // - without mutating anything. Re-derives the anchor leaf's true
    // rect from `tilingArea` (rather than trusting each node's cached
    // Geometry(), which is stale for any workspace that isn't the
    // currently-arranged one) so this is safe to call against a
    // workspace other than the current one.
    bool HasSpaceForAnotherWindow(
        const Rect& tilingArea,
        int innerGap,
        int minWidth,
        int minHeight
    ) const;

    // Debug / IPC dump (`kohikoctl tree`).
    std::string Serialize() const;

private:

    BSPLeaf* FindLeaf(BSPNode* node, ManagedWindow* window) const;

    // The leaf Insert() would attach a new window next to: the cached
    // "last focused" one, or - if nothing has been focused yet - the
    // last leaf found by a plain walk of the tree.
    BSPLeaf* AnchorLeaf() const;

    // Recomputes one leaf's rect from scratch by walking down from
    // `node`, applying each split's Subdivide() in turn - the same
    // math LayoutEngine uses, just without writing the result back
    // onto the nodes. Returns false if `target` isn't under `node`.
    bool ComputeLeafGeometry(
        BSPNode* node,
        BSPLeaf* target,
        const Rect& area,
        int innerGap,
        Rect& out
    ) const;

    void CollectLeaves(BSPNode* node, std::vector<BSPLeaf*>& out) const;

    static SplitDirection DirectionForRect(const Rect& rect);

    std::string SerializeNode(const BSPNode* node) const;

private:

    std::unique_ptr<BSPNode> m_root;

    // Cache of the anchor leaf described above. Cleared whenever the
    // leaf it points at is removed, so it can never dangle.
    BSPLeaf* m_lastFocused = nullptr;

};

}
