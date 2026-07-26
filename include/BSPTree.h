#pragma once

#include "BSPLeaf.h"
#include "BSPSplit.h"
#include "Types.h"

#include <memory>
#include <string>
#include <utility>
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
    //
    // Unconditional: always creates a plain 50/50 split at the anchor,
    // exactly as above, regardless of how small either half ends up.
    // Kept for callers that don't care (tests, ad-hoc tree building);
    // WindowManager's real placement path uses the overload below.
    void Insert(ManagedWindow* window);

    // Placement-aware insert: only actually inserts (and returns true)
    // if a valid slot exists for `window` without shrinking any
    // *existing* tile below floorWidth x floorHeight
    // (MIN_USABLE_TILE_WIDTH/HEIGHT - see general.min_tile_width and
    // general.min_tile_height), and without giving `window` itself a
    // slot smaller than its own declared minimum (ManagedWindow::
    // MinWidth()/MinHeight(), from WM_NORMAL_HINTS - falling back to
    // floorWidth x floorHeight if it hasn't declared one). Those two
    // floors are deliberately kept separate: a client with an unusually
    // large declared minimum only demands that much room for *itself*,
    // never forces every other tile to also shrink-protect up to that
    // same size.
    //
    // Follows the same escalating strategy HasSpaceForAnotherWindow()
    // checks:
    //
    //   1. Try the anchor's natural split direction (its own aspect
    //      ratio - "Try Current Layout").
    //   2. Try the other split direction instead ("Try Alternative
    //      Layouts") - a wide-but-shallow or tall-but-narrow anchor
    //      can easily be feasible one way and not the other.
    //   3. Progressively give the anchor more room by shrinking its
    //      ancestors' *other* branches, nearest ancestor first, never
    //      past floorWidth x floorHeight (or that branch's own leaves'
    //      individually declared minimums, if larger) anywhere in the
    //      shrinking side ("Shrink Existing Tiles"), and retry 1-2
    //      after each step.
    //
    // Touches nothing and returns false if none of that finds a legal
    // placement anywhere along the anchor's ancestor chain - the
    // caller (WindowManager::TryTile) is expected to fall back to
    // another workspace or floating in that case, per
    // general.tiling_misbehavior_fallback.
    bool Insert(
        ManagedWindow* window,
        const Rect& tilingArea,
        int innerGap,
        int floorWidth,
        int floorHeight
    );

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

    // Answers "if Insert(window, ...) were called right now, would it
    // find a legal placement?" - without mutating anything. See
    // Insert() above for exactly what floorWidth/floorHeight protect
    // versus `window`'s own declared minimum. Re-derives the anchor
    // leaf's true rect from `tilingArea` (rather than trusting each
    // node's cached Geometry(), which is stale for any workspace that
    // isn't the currently-arranged one) so this is safe to call
    // against a workspace other than the current one.
    bool HasSpaceForAnotherWindow(
        ManagedWindow* window,
        const Rect& tilingArea,
        int innerGap,
        int floorWidth,
        int floorHeight
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

    // As above, but for any node (leaf or split) rather than only a
    // leaf - ComputeLeafGeometry() is just this with `target` narrowed
    // to a BSPLeaf*. Used by PlanReclaim() to find an ancestor split's
    // own rect.
    bool ComputeNodeGeometry(
        BSPNode* node,
        BSPNode* target,
        const Rect& area,
        int innerGap,
        Rect& out
    ) const;

    void CollectLeaves(BSPNode* node, std::vector<BSPLeaf*>& out) const;

    static SplitDirection DirectionForRect(const Rect& rect);

    // `window`'s own declared minimum (ManagedWindow::MinWidth()/
    // MinHeight(), from WM_NORMAL_HINTS), floored at floorWidth x
    // floorHeight - or just the floor itself for a null `window` / one
    // that never declared a preference (MinWidth()/MinHeight() default
    // to 0).
    static void EffectiveMinSize(
        ManagedWindow* window,
        int floorWidth,
        int floorHeight,
        int& outMinWidth,
        int& outMinHeight
    );

    // True if splitting `rect` would leave the first (anchor/existing)
    // half at least firstMinWidth x firstMinHeight and the second (new
    // window) half at least secondMinWidth x secondMinHeight - tried in
    // the anchor's natural direction first (keeps the common case's
    // aesthetics unchanged), falling back to the other direction ("Try
    // Alternative Layouts") only if the natural one doesn't fit.
    // `outDirection` is set to whichever direction actually worked.
    static bool FeasibleSplitDirection(
        const Rect& rect,
        int innerGap,
        int firstMinWidth,
        int firstMinHeight,
        int secondMinWidth,
        int secondMinHeight,
        SplitDirection& outDirection
    );

    // True if every leaf in the subtree rooted at `node` would still
    // meet its own minimum size if laid out into `area` - each leaf's
    // own declared minimum (ManagedWindow::MinWidth()/MinHeight()) if
    // it has one, otherwise floorWidth x floorHeight
    // (MIN_USABLE_TILE_WIDTH/HEIGHT). Used to verify that shrinking a
    // branch in PlanReclaim()'s favour never pushes any *other* window
    // below whichever of those two floors actually applies to it.
    static bool SubtreeFits(
        BSPNode* node,
        const Rect& area,
        int innerGap,
        int floorWidth,
        int floorHeight
    );

    // "Shrink Existing Tiles": walks from `leaf` up toward the root,
    // one ancestor split at a time, and works out how far that split's
    // ratio could move in `leaf`'s favour - via bisection, since
    // BSPSplit::Subdivide is linear in ratio - without dropping any
    // leaf on the *other* side of that split below its own minimum
    // (checked with SubtreeFits() against the real, unmodified tree -
    // nothing here is applied until the whole plan succeeds). Stops at
    // the first ancestor whose shrink would - on top of every nearer
    // one already chosen - finally leave `leaf` big enough to split in
    // either direction for `window`.
    //
    // Read-only: never calls BSPSplit::SetRatio() itself. On success,
    // fills `outDirection` and `outSteps` (nearest ancestor first) for
    // the caller to actually apply; returns false (leaving both
    // untouched) if no combination of shrinks, however extreme, would
    // create enough room anywhere up to the root.
    bool PlanReclaim(
        BSPLeaf* leaf,
        ManagedWindow* window,
        const Rect& tilingArea,
        int innerGap,
        int floorWidth,
        int floorHeight,
        SplitDirection& outDirection,
        std::vector<std::pair<BSPSplit*, float>>& outSteps
    ) const;

    // Actually performs the tree surgery Insert() does once a
    // direction (natural, alternate, or found via PlanReclaim) has
    // been decided - shared by both Insert() overloads so the splice
    // logic itself can never drift between them.
    void SpliceIn(
        ManagedWindow* window,
        BSPLeaf* anchor,
        SplitDirection direction
    );

    std::string SerializeNode(const BSPNode* node) const;

private:

    std::unique_ptr<BSPNode> m_root;

    // Cache of the anchor leaf described above. Cleared whenever the
    // leaf it points at is removed, so it can never dangle.
    BSPLeaf* m_lastFocused = nullptr;

};

}
