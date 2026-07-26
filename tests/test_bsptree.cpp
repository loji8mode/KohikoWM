// Standalone correctness test for BSPTree - no X11 display needed,
// since BSPTree/ManagedWindow only depend on the WindowID *type*
// (an unsigned long alias), never on an actual X connection.
//
// Build & run: see the "test" target in the Makefile.

#include "BSPTree.h"
#include "LayoutEngine.h"
#include "ManagedWindow.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

using namespace Kohiko;

namespace
{

int g_pass = 0;

void Check(bool condition, const char* what)
{
    if (condition)
    {
        ++g_pass;
        std::printf("  PASS: %s\n", what);
    }
    else
    {
        std::printf("  FAIL: %s\n", what);
        std::exit(1);
    }
}

}

int main()
{
    std::vector<std::unique_ptr<ManagedWindow>> windows;

    auto makeWindow = [&](WindowID id)
    {
        windows.push_back(std::make_unique<ManagedWindow>(id));
        return windows.back().get();
    };

    ManagedWindow* A = makeWindow(1);
    ManagedWindow* B = makeWindow(2);
    ManagedWindow* C = makeWindow(3);
    ManagedWindow* D = makeWindow(4);

    BSPTree tree;
    LayoutEngine layout;

    LayoutEngine::Params params;
    params.innerGap = 4;
    params.outerGap = 4;
    params.borderWidth = 2;
    params.smartGaps = false;
    params.smartBorders = false;

    Rect area{0, 0, 1920, 1080};

    std::printf("-- Insert (Hyprland-style anchor insertion) --\n");

    tree.Insert(A);
    tree.Focus(A);
    layout.Apply(tree.Root(), area, params);
    Check(tree.Count() == 1, "count == 1 after inserting A");

    tree.Insert(B);
    tree.Focus(B);
    layout.Apply(tree.Root(), area, params);
    Check(tree.Count() == 2, "count == 2 after inserting B");

    tree.Insert(C); // near B (focused)
    tree.Focus(C);
    layout.Apply(tree.Root(), area, params);
    Check(tree.Count() == 3, "count == 3 after inserting C near B");

    tree.Focus(A);
    tree.Insert(D); // near A (re-focused) -> ((A|D)|(B|C)), matching the spec's diagram exactly
    layout.Apply(tree.Root(), area, params);
    Check(tree.Count() == 4, "count == 4 after inserting D near A");

    BSPNode* root = tree.Root();
    Check(!root->IsLeaf(), "root is a split");

    auto* rootSplit = static_cast<BSPSplit*>(root);
    auto* leftSplit  = static_cast<BSPSplit*>(rootSplit->Left());
    auto* rightSplit = static_cast<BSPSplit*>(rootSplit->Right());

    Check(leftSplit->Left()->IsLeaf() && static_cast<BSPLeaf*>(leftSplit->Left())->Window() == A,
          "left split's first child is A");
    Check(static_cast<BSPLeaf*>(leftSplit->Right())->Window() == D,
          "left split's second child is D");
    Check(static_cast<BSPLeaf*>(rightSplit->Left())->Window() == B,
          "right split's first child is B");
    Check(static_cast<BSPLeaf*>(rightSplit->Right())->Window() == C,
          "right split's second child is C  ->  tree is exactly ((A|D)|(B|C))");

    std::printf("\n-- Swap (Super+LMB): the core bug fix --\n");

    Rect geomA_before = A->Geometry();
    Rect geomD_before = D->Geometry();

    tree.Swap(A, D);

    // This is the crucial regression check: the ORIGINAL implementation
    // swapped Geometry() rects directly, which LayoutEngine would just
    // overwrite on the very next pass. A correct Swap() must survive a
    // fresh relayout because it changed the *tree*, not just cached
    // coordinates.
    layout.Apply(tree.Root(), area, params);

    Check(D->Geometry().x == geomA_before.x && D->Geometry().y == geomA_before.y,
          "after Swap()+relayout, D now renders where A used to be");
    Check(A->Geometry().x == geomD_before.x && A->Geometry().y == geomD_before.y,
          "after Swap()+relayout, A now renders where D used to be");
    Check(static_cast<BSPLeaf*>(leftSplit->Left())->Window() == D,
          "left split's first child is now D (leaf position unchanged, window swapped)");
    Check(static_cast<BSPLeaf*>(leftSplit->Right())->Window() == A,
          "left split's second child is now A");

    std::printf("\n-- Resize (Super+RMB): the divider always tracks the mouse, not the grabbed window --\n");

    // The split holding D/A could have ended up Vertical or Horizontal
    // depending on the anchor's aspect ratio at insert time (that's
    // the adaptive DirectionForRect() logic working as intended) - so
    // drive whichever axis this particular split actually uses.
    bool leftIsVertical = (leftSplit->Direction() == SplitDirection::Vertical);
    int dx = leftIsVertical ? 100 : 0;
    int dy = leftIsVertical ? 0   : 100;

    // Same drag (same dx/dy, same sign) regardless of which of the two
    // windows is under the cursor: the divider between them - and
    // therefore the ratio, which is the FIRST child's share - must
    // move the same way both times. That's the actual regression this
    // test guards: an earlier version flipped the sign whenever
    // `window` was the second child, which moved the divider backwards
    // relative to the mouse and read as inverted dragging for whatever
    // window happened to be on that side of the split.
    float ratio0 = leftSplit->Ratio();
    tree.Resize(D, dx, dy); // D is the FIRST child here
    float ratio1 = leftSplit->Ratio();
    Check(ratio1 > ratio0, "dragging in the positive direction increases the first child's ratio (grabbed the FIRST child)");

    tree.Resize(A, dx, dy); // A is the SECOND child - same dx/dy as above
    float ratio2 = leftSplit->Ratio();
    Check(ratio2 > ratio1, "the same drag increases the first child's ratio again, even though this time the SECOND child was grabbed");

    std::printf("\n-- Rotate / Flip --\n");

    SplitDirection dirBefore = leftSplit->Direction();
    tree.Rotate(D);
    Check(leftSplit->Direction() != dirBefore, "Rotate() flips the split direction");

    ManagedWindow* leftBefore  = static_cast<BSPLeaf*>(leftSplit->Left())->Window();
    ManagedWindow* rightBefore = static_cast<BSPLeaf*>(leftSplit->Right())->Window();
    tree.Flip(D);
    Check(static_cast<BSPLeaf*>(leftSplit->Left())->Window() == rightBefore, "Flip() swaps child order (1/2)");
    Check(static_cast<BSPLeaf*>(leftSplit->Right())->Window() == leftBefore, "Flip() swaps child order (2/2)");

    std::printf("\n-- FindNeighbor / HitTest --\n");

    layout.Apply(tree.Root(), area, params);

    ManagedWindow* neighborRight = tree.FindNeighbor(A, Direction::Right);
    Check(neighborRight != nullptr, "FindNeighbor(A, Right) finds something");

    ManagedWindow* hit = tree.HitTest(Point{C->Geometry().CenterX(), C->Geometry().CenterY()});
    Check(hit == C, "HitTest() at C's own center finds C");

    std::printf("\n-- HasSpaceForAnotherWindow (bug #4: minimum tile size) --\n");

    Check(BSPTree().HasSpaceForAnotherWindow(area, 4, 100, 60),
          "a brand-new empty tree always has room for the first window");

    Check(tree.HasSpaceForAnotherWindow(area, params.innerGap, 50, 50),
          "the existing 4-window 1920x1080 tree still has room for a modest 50x50 minimum");
    Check(!tree.HasSpaceForAnotherWindow(area, params.innerGap, 2000, 2000),
          "the existing 4-window tree has no room left for an unreasonably large minimum");

    // Precise boundary check on a tree that has *never* been laid out
    // (Arrange() only lays out the current workspace, so a tree living
    // on some other workspace can easily be in exactly this state) -
    // HasSpaceForAnotherWindow() must still get the right answer by
    // recomputing from `tilingArea` itself rather than trusting any
    // node's (here, nonexistent) cached Geometry().
    //
    // A 220x100 area is wider than it is tall, so DirectionForRect()
    // picks a Vertical (left/right) split: with a 4px gap that divides
    // into two 108px-wide children (Insert() always uses a fresh 50/50
    // ratio), so a 100px minimum should just clear and a 110px minimum
    // should not.
    ManagedWindow* E = makeWindow(5);
    BSPTree soloTree;
    soloTree.Insert(E);

    Rect soloArea{0, 0, 220, 100};
    Check(soloTree.HasSpaceForAnotherWindow(soloArea, 4, 100, 60),
          "never-laid-out tree: 108px-wide half still clears a 100px minimum width");
    Check(!soloTree.HasSpaceForAnotherWindow(soloArea, 4, 110, 60),
          "never-laid-out tree: 108px-wide half no longer clears a 110px minimum width");

    std::printf("\n-- Remove (no empty nodes left behind) --\n");

    tree.Remove(B);
    Check(tree.Count() == 3, "count == 3 after removing B");

    tree.Remove(C);
    tree.Remove(D);
    tree.Remove(A);
    Check(tree.Empty(), "tree is empty after removing every window");

    std::printf(
        "\n-- OccupiesTreeSlot (regression: a tiled window that went "
        "fullscreen must still free its slot when it closes) --\n");

    // Reproduces the exact bug report: two ordinary tiled windows
    // (Discord, Telegram) plus a third (Telegram's Media Viewer) that
    // gets tiled too, then asks for real EWMH fullscreen (which -
    // correctly - leaves it in the tree so un-fullscreening can
    // restore it to the same slot) and is then closed *while still
    // fullscreen*, without ever un-fullscreening first.
    ManagedWindow* discord = makeWindow(10);
    ManagedWindow* telegram = makeWindow(11);
    ManagedWindow* mediaViewer = makeWindow(12);

    BSPTree wsTree;
    wsTree.Insert(discord);
    wsTree.Focus(discord);
    wsTree.Insert(telegram);
    wsTree.Focus(telegram);
    wsTree.Insert(mediaViewer); // lands near Telegram, the focused window
    wsTree.Focus(mediaViewer);

    discord->SetState(WindowState::Tiled);
    telegram->SetState(WindowState::Tiled);
    mediaViewer->SetState(WindowState::Tiled);

    layout.Apply(wsTree.Root(), area, params);
    Check(wsTree.Count() == 3, "3 leaves once the Media Viewer tiles alongside Discord/Telegram");

    // The Media Viewer asks for real fullscreen - Manage()/
    // HandleClientMessage() record this exactly as WindowState::Tiled
    // (PreviousState) -> WindowState::Fullscreen (State), and
    // deliberately do NOT touch the tree.
    mediaViewer->SetPreviousState(mediaViewer->State());
    mediaViewer->SetState(WindowState::Fullscreen);

    Check(!mediaViewer->IsTiled(), "Media Viewer's current State() is Fullscreen, not Tiled");
    Check(mediaViewer->OccupiesTreeSlot(),
          "...but it still logically occupies a tree slot (PreviousState() == Tiled)");
    Check(wsTree.Count() == 3, "tree still has 3 leaves while it's fullscreen (by design, for restoring later)");

    // It closes right here, still fullscreen - this is exactly what
    // WindowManager::Unmanage() does now:
    if (mediaViewer->OccupiesTreeSlot())
        wsTree.Remove(mediaViewer);

    Check(wsTree.Count() == 2,
          "FIXED: closing it while fullscreen still frees its slot (the bug: "
          "checking IsTiled() instead of OccupiesTreeSlot() left this at 3 forever)");

    Rect discordBefore = discord->Geometry();
    Rect telegramBefore = telegram->Geometry();

    layout.Apply(wsTree.Root(), area, params);

    Check(discord->Geometry().width  != discordBefore.width ||
          telegram->Geometry().width != telegramBefore.width ||
          discord->Geometry().height  != discordBefore.height ||
          telegram->Geometry().height != telegramBefore.height,
          "Discord and/or Telegram actually grow into the freed space on relayout "
          "(this is the \"black empty area\"/\"stuck at its old size\" symptom - "
          "with the bug, these rects would be unchanged since the tree still "
          "thought there were 3 windows sharing the screen)");

    // No dead space left at all: the two survivors' tiles should
    // account for the *entire* tiling area between them (minus gaps),
    // not just "some" of it.
    long long survivorsArea =
        static_cast<long long>(discord->Geometry().width)  * discord->Geometry().height +
        static_cast<long long>(telegram->Geometry().width) * telegram->Geometry().height;
    long long totalArea = static_cast<long long>(area.width) * area.height;

    // Allow for the gaps/border insets LayoutEngine subtracts - this
    // just needs to be "the overwhelming majority of the screen", not
    // pixel-exact, to rule out a leftover reserved-but-unfilled slot.
    Check(survivorsArea > (totalArea * 9) / 10,
          "no large reserved-but-empty area is left over - the two survivors "
          "between them account for essentially the whole screen");

    std::printf("\nALL %d CHECKS PASSED.\n", g_pass);
    return 0;
}
