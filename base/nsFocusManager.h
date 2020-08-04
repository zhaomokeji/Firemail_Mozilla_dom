/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFocusManager_h___
#define nsFocusManager_h___

#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIFocusManager.h"
#include "nsIObserver.h"
#include "nsIWidget.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

#define FOCUSMETHOD_MASK 0xF000
#define FOCUSMETHODANDRING_MASK 0xF0F000

#define FOCUSMANAGER_CONTRACTID "@mozilla.org/focus-manager;1"

class nsIContent;
class nsPIDOMWindowOuter;

namespace mozilla {
class PresShell;
namespace dom {
class Element;
struct FocusOptions;
class BrowserParent;
class ContentChild;
class ContentParent;
}  // namespace dom
}  // namespace mozilla

struct nsDelayedBlurOrFocusEvent;

/**
 * The focus manager keeps track of where the focus is, that is, the node
 * which receives key events.
 */

class nsFocusManager final : public nsIFocusManager,
                             public nsIObserver,
                             public nsSupportsWeakReference {
  typedef mozilla::widget::InputContextAction InputContextAction;
  typedef mozilla::dom::Document Document;
  friend class mozilla::dom::ContentChild;
  friend class mozilla::dom::ContentParent;

 public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFocusManager, nsIFocusManager)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIFOCUSMANAGER

  // called to initialize and stop the focus manager at startup and shutdown
  static nsresult Init();
  static void Shutdown();

  // Simple helper to call SetFocusedWindow on the instance.
  //
  // This raises the window and switches to the tab as needed.
  static void FocusWindow(nsPIDOMWindowOuter* aWindow,
                          mozilla::dom::CallerType aCallerType);

  static void PrefChanged(const char* aPref, void* aSelf);
  void PrefChanged(const char* aPref);

  /**
   * Retrieve the single focus manager.
   */
  static nsFocusManager* GetFocusManager() { return sInstance; }

  /**
   * A faster version of nsIFocusManager::GetFocusedElement, returning a
   * raw Element pointer (instead of having AddRef-ed Element
   * pointer filled in to an out-parameter).
   */
  mozilla::dom::Element* GetFocusedElement() { return mFocusedElement; }

  /**
   * Returns true if aContent currently has focus.
   */
  bool IsFocused(nsIContent* aContent);

  /**
   * Returns true if test mode is enabled.
   */
  bool IsTestMode();

  /**
   * Return a focused window. Version of nsIFocusManager::GetFocusedWindow.
   */
  nsPIDOMWindowOuter* GetFocusedWindow() const { return mFocusedWindow; }

  /**
   * In the chrome process, retrieves the BrowsingContext corresponding
   * to GetFocusedWindow(). In a content process, retrieves the
   * focused BrowsingContext, which may not belong to this process.
   */
  mozilla::dom::BrowsingContext* GetFocusedBrowsingContext() const {
    if (XRE_IsParentProcess()) {
      if (mFocusedWindow) {
        return mFocusedWindow->GetBrowsingContext();
      }
      return nullptr;
    }
    return mFocusedBrowsingContextInContent;
  }

  /**
   * Return an active window. Version of nsIFocusManager::GetActiveWindow.
   */
  nsPIDOMWindowOuter* GetActiveWindow() const { return mActiveWindow; }

  /**
   * In the chrome process, retrieves the BrowsingContext corresponding
   * to GetActiveWindow(). In a content process, retrieves the
   * BrowsingContext of the top-level Web content in the active tab if
   * in the same process as the caller or nullptr otherwise.
   */
  mozilla::dom::BrowsingContext* GetActiveBrowsingContext() const {
    if (XRE_IsParentProcess()) {
      if (mActiveWindow) {
        return mActiveWindow->GetBrowsingContext();
      }
      return nullptr;
    }
    return mActiveBrowsingContextInContent;
  }

  /**
   * Called when content has been removed.
   */
  nsresult ContentRemoved(Document* aDocument, nsIContent* aContent);

  void NeedsFlushBeforeEventHandling(mozilla::dom::Element* aElement) {
    if (mFocusedElement == aElement) {
      mEventHandlingNeedsFlush = true;
    }
  }

  bool CanSkipFocus(nsIContent* aContent);

  void FlushBeforeEventHandlingIfNeeded(nsIContent* aContent) {
    if (mEventHandlingNeedsFlush) {
      nsCOMPtr<Document> doc = aContent->GetComposedDoc();
      if (doc) {
        mEventHandlingNeedsFlush = false;
        doc->FlushPendingNotifications(mozilla::FlushType::Layout);
      }
    }
  }

  /**
   * Update the caret with current mode (whether in caret browsing mode or not).
   */
  void UpdateCaretForCaretBrowsingMode();

  /**
   * Returns the content node that would be focused if aWindow was in an
   * active window. This will traverse down the frame hierarchy, starting at
   * the given window aWindow. Sets aFocusedWindow to the window with the
   * document containing aFocusedContent. If no element is focused,
   * aFocusedWindow may be still be set -- this means that the document is
   * focused but no element within it is focused.
   *
   * aWindow, aFocusIsOutOfProcess, aFocusedWindow must all be non-null.
   */
  enum SearchRange {
    // Return focused content in aWindow.  So, aFocusedWindow is always aWindow.
    eOnlyCurrentWindow,
    // Return focused content in aWindow or one of all sub windows.
    eIncludeAllDescendants,
    // Return focused content in aWindow or one of visible sub windows.
    eIncludeVisibleDescendants,
  };
  static mozilla::dom::Element* GetFocusedDescendant(
      nsPIDOMWindowOuter* aWindow, SearchRange aSearchRange,
      bool* aFocusIsOutOfProcess, nsPIDOMWindowOuter** aFocusedWindow);
  static mozilla::dom::Element* GetFocusedDescendant(
      nsPIDOMWindowOuter* aWindow, SearchRange aSearchRange,
      nsPIDOMWindowOuter** aFocusedWindow);

  /**
   * Helper function for MoveFocus which determines the next element
   * to move the focus to and returns it in aNextContent.
   *
   * aWindow is the window to adjust the focus within, and aStart is
   * the element to start navigation from. For tab key navigation,
   * this should be the currently focused element.
   *
   * aType is the type passed to MoveFocus. If aNoParentTraversal is set,
   * navigation is not done to parent documents and iteration returns to the
   * beginning (or end) of the starting document.
   *
   * aNavigateByKey to move focus by keyboard as a side effect of computing the
   * next target.
   */
  nsresult DetermineElementToMoveFocus(nsPIDOMWindowOuter* aWindow,
                                       nsIContent* aStart, int32_t aType,
                                       bool aNoParentTraversal,
                                       bool aNavigateByKey,
                                       nsIContent** aNextContent);

  /**
   * Given an element, which must be the focused element, activate the remote
   * frame it embeds, if any.
   */
  void ActivateRemoteFrameIfNeeded(mozilla::dom::Element&);

  /**
   * Raises the top-level window aWindow at the widget level.
   */
  void RaiseWindow(nsPIDOMWindowOuter* aWindow,
                   mozilla::dom::CallerType aCallerType);

  static uint32_t FocusOptionsToFocusManagerFlags(
      const mozilla::dom::FocusOptions& aOptions);

  /**
   * Returns the content node that focus will be redirected to if aContent was
   * focused. This is used for the special case of certain XUL elements such
   * as textboxes or input number which redirect focus to an anonymous child.
   *
   * aContent must be non-null.
   *
   * XXXndeakin this should be removed eventually but I want to do that as
   * followup work.
   */
  static mozilla::dom::Element* GetRedirectedFocus(nsIContent* aContent);

  /**
   * Returns an InputContextAction cause for aFlags.
   */
  static InputContextAction::Cause GetFocusMoveActionCause(uint32_t aFlags);

  static bool sMouseFocusesFormControl;

  static void MarkUncollectableForCCGeneration(uint32_t aGeneration);

 protected:
  nsFocusManager();
  ~nsFocusManager();

  /**
   * Ensure that the widget associated with the currently focused window is
   * focused at the widget level.
   */
  void EnsureCurrentWidgetFocused(mozilla::dom::CallerType aCallerType);

  /**
   * Activate or deactivate the window and send the activate/deactivate events.
   */
  void ActivateOrDeactivate(nsPIDOMWindowOuter* aWindow, bool aActive);

  /**
   * Blur whatever is currently focused and focus aNewContent. aFlags is a
   * bitmask of the flags defined in nsIFocusManager. If aFocusChanged is
   * true, then the focus has actually shifted and the caret position will be
   * updated to the new focus, aNewContent will be scrolled into view (unless
   * a flag disables this) and the focus method for the window will be updated.
   * If aAdjustWidget is false, don't change the widget focus state.
   *
   * All actual focus changes must use this method to do so. (as opposed
   * to those that update the focus in an inactive window for instance).
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void SetFocusInner(mozilla::dom::Element* aNewContent, int32_t aFlags,
                     bool aFocusChanged, bool aAdjustWidget);

  /**
   * Returns true if aPossibleAncestor is the same as aWindow or an
   * ancestor of aWindow.
   */
  bool IsSameOrAncestor(nsPIDOMWindowOuter* aPossibleAncestor,
                        nsPIDOMWindowOuter* aWindow);
  bool IsSameOrAncestor(nsPIDOMWindowOuter* aPossibleAncestor,
                        mozilla::dom::BrowsingContext* aContext);
  bool IsSameOrAncestor(mozilla::dom::BrowsingContext* aPossibleAncestor,
                        nsPIDOMWindowOuter* aWindow);
  bool IsSameOrAncestor(mozilla::dom::BrowsingContext* aPossibleAncestor,
                        mozilla::dom::BrowsingContext* aContext);

  /**
   * Returns the window that is the lowest common ancestor of both aWindow
   * and aContext, or null if they share no common ancestor.
   */
  mozilla::dom::BrowsingContext* GetCommonAncestor(
      nsPIDOMWindowOuter* aWindow, mozilla::dom::BrowsingContext* aContext);

  /**
   * When aBrowsingContext is focused, adjust the ancestors of aBrowsingContext
   * so that they also have their corresponding frames focused. Thus, one can
   * start at the active top-level window and navigate down the currently
   * focused elements for each frame in the tree to get to aBrowsingContext.
   */
  void AdjustWindowFocus(mozilla::dom::BrowsingContext* aBrowsingContext,
                         bool aCheckPermission, bool aIsVisible);

  /**
   * Returns true if aWindow is visible.
   */
  bool IsWindowVisible(nsPIDOMWindowOuter* aWindow);

  /**
   * Returns true if aContent is a root element and not focusable.
   * I.e., even if aContent is editable root element, this returns true when
   * the document is in designMode.
   *
   * @param aContent must not be null and must be in a document.
   */
  bool IsNonFocusableRoot(nsIContent* aContent);

  /**
   * First flushes the pending notifications to ensure the PresShell and frames
   * are updated.
   * Checks and returns aElement if it may be focused, another element node if
   * the focus should be retargeted at another node, or null if the node
   * cannot be focused. aFlags are the flags passed to SetFocus and similar
   * methods.
   *
   * An element is focusable if it is in a document, the document isn't in
   * print preview mode and the element has an nsIFrame where the
   * IsFocusable method returns true. For <area> elements, there is no
   * frame, so only the IsFocusable method on the content node must be
   * true.
   */
  mozilla::dom::Element* FlushAndCheckIfFocusable(
      mozilla::dom::Element* aElement, uint32_t aFlags);

  /**
   * Blurs the currently focused element. Returns false if another element was
   * focused as a result. This would mean that the caller should not proceed
   * with a pending call to Focus. Normally, true would be returned.
   *
   * The currently focused element within aBrowsingContextToClear will be
   * cleared. aBrowsingContextToClear may be null, which means that no window is
   * cleared. This will be the case, for example, when lowering a window, as we
   * want to fire a blur, but not actually change what element would be focused,
   * so that the same element will be focused again when the window is raised.
   *
   * aAncestorBrowsingContextToFocus should be set to the common ancestor of the
   * window that is being blurred and the window that is going to focused, when
   * switching focus to a sibling window.
   *
   * aIsLeavingDocument should be set to true if the document/window is being
   * blurred as well. Document/window blur events will be fired. It should be
   * false if an element is the same document is about to be focused.
   *
   * If aAdjustWidget is false, don't change the widget focus state.
   */
  // MOZ_CAN_RUN_SCRIPT_BOUNDARY for now, until we annotate callers.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  bool Blur(mozilla::dom::BrowsingContext* aBrowsingContextToClear,
            mozilla::dom::BrowsingContext* aAncestorBrowsingContextToFocus,
            bool aIsLeavingDocument, bool aAdjustWidget,
            nsIContent* aContentToFocus = nullptr);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void BlurFromOtherProcess(
      mozilla::dom::BrowsingContext* aFocusedBrowsingContext,
      mozilla::dom::BrowsingContext* aBrowsingContextToClear,
      mozilla::dom::BrowsingContext* aAncestorBrowsingContextToFocus,
      bool aIsLeavingDocument, bool aAdjustWidget);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  bool BlurImpl(mozilla::dom::BrowsingContext* aBrowsingContextToClear,
                mozilla::dom::BrowsingContext* aAncestorBrowsingContextToFocus,
                bool aIsLeavingDocument, bool aAdjustWidget,
                nsIContent* aContentToFocus);

  /**
   * Focus an element in the active window and child frame.
   *
   * aWindow is the window containing the element aContent to focus.
   *
   * aFlags is the flags passed to the various focus methods in
   * nsIFocusManager.
   *
   * aIsNewDocument should be true if a new document is being focused.
   * Document/window focus events will be fired.
   *
   * aFocusChanged should be true if a new content node is being focused, so
   * the focused content will be scrolled into view and the caret position
   * will be updated. If false is passed, then a window is simply being
   * refocused, for instance, due to a window being raised, or a tab is being
   * switched to.
   *
   * If aFocusChanged is true, then the focus has moved to a new location.
   * Otherwise, the focus is just being updated because the window was
   * raised.
   *
   * aWindowRaised should be true if the window is being raised. In this case,
   * command updaters will not be called.
   *
   * If aAdjustWidget is false, don't change the widget focus state.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void Focus(nsPIDOMWindowOuter* aWindow, mozilla::dom::Element* aContent,
             uint32_t aFlags, bool aIsNewDocument, bool aFocusChanged,
             bool aWindowRaised, bool aAdjustWidget,
             bool aFocusInOtherContentProcess,
             nsIContent* aContentLostFocus = nullptr);

  /**
   * Send a focus or blur event at aTarget. It may be added to the delayed
   * event queue if the document is suppressing events.
   *
   * aEventMessage should be either eFocus or eBlur.
   * For blur events, aFocusMethod should normally be non-zero.
   *
   * aWindowRaised should only be true if called from WindowRaised.
   */
  void SendFocusOrBlurEvent(
      mozilla::EventMessage aEventMessage, mozilla::PresShell* aPresShell,
      Document* aDocument, nsISupports* aTarget, uint32_t aFocusMethod,
      bool aWindowRaised, bool aIsRefocus = false,
      mozilla::dom::EventTarget* aRelatedTarget = nullptr);
  /**
   * Fire a focus or blur event at aTarget.
   *
   * aEventMessage should be either eFocus or eBlur.
   * For blur events, aFocusMethod should normally be non-zero.
   *
   * aWindowRaised should only be true if called from WindowRaised.
   */
  void FireFocusOrBlurEvent(
      mozilla::EventMessage aEventMessage, mozilla::PresShell* aPresShell,
      nsISupports* aTarget, bool aWindowRaised, bool aIsRefocus = false,
      mozilla::dom::EventTarget* aRelatedTarget = nullptr);

  /**
   *  Fire a focusin or focusout event
   *
   *  aEventMessage should be either eFocusIn or eFocusOut.
   *
   *  aTarget is the content the event will fire on (the object that gained
   *  focus for focusin, the object blurred for focusout).
   *
   *  aCurrentFocusedWindow is the window focused before the focus/blur event
   *  was fired.
   *
   *  aCurrentFocusedContent is the content focused before the focus/blur event
   *  was fired.
   *
   *  aRelatedTarget is the content related to the event (the object
   *  losing focus for focusin, the object getting focus for focusout).
   */
  void FireFocusInOrOutEvent(
      mozilla::EventMessage aEventMessage, mozilla::PresShell* aPresShell,
      nsISupports* aTarget, nsPIDOMWindowOuter* aCurrentFocusedWindow,
      nsIContent* aCurrentFocusedContent,
      mozilla::dom::EventTarget* aRelatedTarget = nullptr);

  /**
   * Scrolls aContent into view unless the FLAG_NOSCROLL flag is set.
   */
  MOZ_CAN_RUN_SCRIPT
  void ScrollIntoView(mozilla::PresShell* aPresShell, nsIContent* aContent,
                      uint32_t aFlags);

  /**
   * Updates the caret positon and visibility to match the focus.
   *
   * aMoveCaretToFocus should be true to move the caret to aContent.
   *
   * aUpdateVisibility should be true to update whether the caret is
   * visible or not.
   */
  void UpdateCaret(bool aMoveCaretToFocus, bool aUpdateVisibility,
                   nsIContent* aContent);

  /**
   * Helper method to move the caret to the focused element aContent.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void MoveCaretToFocus(
      mozilla::PresShell* aPresShell, nsIContent* aContent);

  /**
   * Makes the caret visible or not, depending on aVisible.
   */
  nsresult SetCaretVisible(mozilla::PresShell* aPresShell, bool aVisible,
                           nsIContent* aContent);

  // the remaining functions are used for tab key and document-navigation

  /**
   * Retrieves the start and end points of the current selection for
   * aDocument and stores them in aStartContent and aEndContent.
   */
  nsresult GetSelectionLocation(Document* aDocument,
                                mozilla::PresShell* aPresShell,
                                nsIContent** aStartContent,
                                nsIContent** aEndContent);

  /**
   * Retrieve the next tabbable element in scope owned by aOwner, using
   * focusability and tabindex to determine the tab order.
   *
   * aOwner is the owner of scope to search in.
   *
   * aStartContent is the starting point for this call of this method.
   *
   * aOriginalStartContent is the initial starting point for sequential
   * navigation.
   *
   * aForward should be true for forward navigation or false for backward
   * navigation.
   *
   * aCurrentTabIndex is the current tabindex.
   *
   * aIgnoreTabIndex to ignore the current tabindex and find the element
   * irrespective or the tab index.
   *
   * aForDocumentNavigation informs whether we're navigating only through
   * documents.
   *
   * aSkipOwner to skip owner while searching. The flag is set when caller is
   * |GetNextTabbableContent| in order to let caller handle owner.
   *
   * NOTE:
   *   Consider the method searches downwards in flattened subtree
   *   rooted at aOwner.
   */
  nsIContent* GetNextTabbableContentInScope(
      nsIContent* aOwner, nsIContent* aStartContent,
      nsIContent* aOriginalStartContent, bool aForward,
      int32_t aCurrentTabIndex, bool aIgnoreTabIndex,
      bool aForDocumentNavigation, bool aNavigateByKey, bool aSkipOwner);

  /**
   * Retrieve the next tabbable element in scope including aStartContent
   * and the scope's ancestor scopes, using focusability and tabindex to
   * determine the tab order.
   *
   * aStartOwner is the scope owner of the aStartContent.
   *
   * aStartContent an in/out paremeter. It as input is the starting point
   * for this call of this method; as output it is the shadow host in
   * light DOM if the next tabbable element is not found in shadow DOM,
   * in order to continue searching in light DOM.
   *
   * aOriginalStartContent is the initial starting point for sequential
   * navigation.
   *
   * aForward should be true for forward navigation or false for backward
   * navigation.
   *
   * aCurrentTabIndex returns tab index of shadow host in light DOM if the
   * next tabbable element is not found in shadow DOM, in order to continue
   * searching in light DOM.
   *
   * aIgnoreTabIndex to ignore the current tabindex and find the element
   * irrespective or the tab index.
   *
   * aForDocumentNavigation informs whether we're navigating only through
   * documents.
   *
   * aNavigateByKey to move focus by keyboard as a side effect of computing the
   * next target.
   *
   * NOTE:
   *   Consider the method searches upwards in all shadow host- or slot-rooted
   *   flattened subtrees that contains aStartContent as non-root, except
   *   the flattened subtree rooted at shadow host in light DOM.
   */
  nsIContent* GetNextTabbableContentInAncestorScopes(
      nsIContent* aStartOwner, nsIContent** aStartContent,
      nsIContent* aOriginalStartContent, bool aForward,
      int32_t* aCurrentTabIndex, bool aIgnoreTabIndex,
      bool aForDocumentNavigation, bool aNavigateByKey);

  /**
   * Retrieve the next tabbable element within a document, using focusability
   * and tabindex to determine the tab order. The element is returned in
   * aResultContent.
   *
   * aRootContent is the root node -- nodes above this will not be examined.
   * Typically this will be the root node of a document, but could also be
   * a popup node.
   *
   * aOriginalStartContent is the content which was originally the starting
   * node, in the case of recursive or looping calls.
   *
   * aStartContent is the starting point for this call of this method.
   * If aStartContent doesn't have visual representation, the next content
   * object, which does have a primary frame, will be used as a start.
   * If that content object is focusable, the method may return it.
   *
   * aForward should be true for forward navigation or false for backward
   * navigation.
   *
   * aCurrentTabIndex is the current tabindex.
   *
   * aIgnoreTabIndex to ignore the current tabindex and find the element
   * irrespective or the tab index. This will be true when a selection is
   * active, since we just want to focus the next element in tree order
   * from where the selection is. Similarly, if the starting element isn't
   * focusable, since it doesn't really have a defined tab index.
   *
   * aNavigateByKey to move focus by keyboard as a side effect of computing the
   * next target.
   */
  nsresult GetNextTabbableContent(
      mozilla::PresShell* aPresShell, nsIContent* aRootContent,
      nsIContent* aOriginalStartContent, nsIContent* aStartContent,
      bool aForward, int32_t aCurrentTabIndex, bool aIgnoreTabIndex,
      bool aForDocumentNavigation, bool aNavigateByKey,
      nsIContent** aResultContent);

  /**
   * Get the next tabbable image map area and returns it.
   *
   * aForward should be true for forward navigation or false for backward
   * navigation.
   *
   * aCurrentTabIndex is the current tabindex.
   *
   * aImageContent is the image.
   *
   * aStartContent is the current image map area.
   */
  nsIContent* GetNextTabbableMapArea(bool aForward, int32_t aCurrentTabIndex,
                                     mozilla::dom::Element* aImageContent,
                                     nsIContent* aStartContent);

  /**
   * Return the next valid tabindex value after aCurrentTabIndex, if aForward
   * is true, or the previous tabindex value if aForward is false. aParent is
   * the node from which to start looking for tab indicies.
   */
  int32_t GetNextTabIndex(nsIContent* aParent, int32_t aCurrentTabIndex,
                          bool aForward);

  /**
   * Focus the first focusable content within the document with a root node of
   * aRootContent. For content documents, this will be aRootContent itself, but
   * for chrome documents, this will locate the next focusable content.
   */
  nsresult FocusFirst(mozilla::dom::Element* aRootContent,
                      nsIContent** aNextContent);

  /**
   * Retrieves and returns the root node from aDocument to be focused. Will
   * return null if the root node cannot be focused. There are several reasons
   * for this:
   *
   * - if aForDocumentNavigation is false and aWindow is a chrome shell.
   * - if aCheckVisibility is true and the aWindow is not visible.
   * - if aDocument is a frameset document.
   */
  mozilla::dom::Element* GetRootForFocus(nsPIDOMWindowOuter* aWindow,
                                         Document* aDocument,
                                         bool aForDocumentNavigation,
                                         bool aCheckVisibility);

  /**
   * Retrieves and returns the root node as with GetRootForFocus but only if
   * aContent is a frame with a valid child document.
   */
  mozilla::dom::Element* GetRootForChildDocument(nsIContent* aContent);

  /**
   * Retreives a focusable element within the current selection of aWindow.
   * Currently, this only detects links.
   *
   * This is used when MoveFocus is called with a type of MOVEFOCUS_CARET,
   * which is used, for example, to focus links as the caret is moved over
   * them.
   */
  void GetFocusInSelection(nsPIDOMWindowOuter* aWindow,
                           nsIContent* aStartSelection,
                           nsIContent* aEndSelection,
                           nsIContent** aFocusedContent);

 private:
  // Notify that the focus state of aContent has changed.  Note that
  // we need to pass in whether the window should show a focus ring
  // before the SetFocusedNode call on it happened when losing focus
  // and after the SetFocusedNode call when gaining focus, which is
  // why that information needs to be an explicit argument instead of
  // just passing in the window and asking it whether it should show
  // focus rings: in the losing focus case that information could be
  // wrong..
  static void NotifyFocusStateChange(nsIContent* aContent,
                                     nsIContent* aContentToFocus,
                                     bool aWindowShouldShowFocusRing,
                                     int32_t aFlags, bool aGettingFocus);

  void SetFocusedWindowInternal(nsPIDOMWindowOuter* aWindow);

  bool TryDocumentNavigation(nsIContent* aCurrentContent,
                             bool* aCheckSubDocument,
                             nsIContent** aResultContent);

  bool TryToMoveFocusToSubDocument(nsIContent* aCurrentContent,
                                   nsIContent* aOriginalStartContent,
                                   bool aForward, bool aForDocumentNavigation,
                                   bool aNavigateByKey,
                                   nsIContent** aResultContent);

  // Sets the focused BrowsingContext and, if appropriate, syncs it to
  // other processes.
  void SetFocusedBrowsingContext(mozilla::dom::BrowsingContext* aContext);

  // Content-only
  // Called when receiving an IPC message about another process setting
  // the focused BrowsingContext.
  void SetFocusedBrowsingContextFromOtherProcess(
      mozilla::dom::BrowsingContext* aContext);

  // Chrome-only
  // Sets the chrome process notion of what BrowsingContext is focused
  // in content.
  void SetFocusedBrowsingContextInChrome(
      mozilla::dom::BrowsingContext* aContext);

 public:
  // Chrome-only
  // Gets the chrome process notion of what BrowsingContext is focused
  // in content.
  mozilla::dom::BrowsingContext* GetFocusedBrowsingContextInChrome();

  // Chrome-only
  // Notifies the focus manager that BrowsingContext::Detach was called
  // on a BrowsingContext so that pointers to it can be forgotten.
  void BrowsingContextDetached(mozilla::dom::BrowsingContext* aContext);

 private:
  // Content-only
  // Sets the BrowsingContext corresponding to top-level Web content
  // in the frontmost tab if focus is in Web content.
  void SetActiveBrowsingContextInContent(
      mozilla::dom::BrowsingContext* aContext);

  // Content-only
  // Receives notification of another process setting the top-level Web
  // content as being in the frontmost tab with focus in Web content.
  void SetActiveBrowsingContextFromOtherProcess(
      mozilla::dom::BrowsingContext* aContext);

  // Content-only
  // Receives notification that another process determined that focus
  // moved to chrome so a particular BrowsingContext is no longer the
  // "active" one.
  void UnsetActiveBrowsingContextFromOtherProcess(
      mozilla::dom::BrowsingContext* aContext);

  // Chrome-only
  // Sets the chrome process notion of what content believes to be
  // the top-level BrowsingContext in the frontmost tab when focus
  // is in Web content.
  void SetActiveBrowsingContextInChrome(
      mozilla::dom::BrowsingContext* aContext);

 public:
  // Chrome-only
  // Gets the chrome process notion of what content believes to be
  // the top-level BrowsingContext in the frontmost tab when focus
  // is in Web content.
  mozilla::dom::BrowsingContext* GetActiveBrowsingContextInChrome();

 private:
  // In the chrome process, the currently active and front-most top-most
  // window. Not supposed to be used in a meaningful way in content
  // processes.
  nsCOMPtr<nsPIDOMWindowOuter> mActiveWindow;

  // In a content process, the BrowsingContext corresponding to top-level
  // Web content in the active tab or nullptr if focus is not in a
  // BrowsingContextGroup that this process participates in. Synced
  // across processes in a BrowsingContextGroup.
  // Not supposed to be used in a meaningful way in the chrome process.
  RefPtr<mozilla::dom::BrowsingContext> mActiveBrowsingContextInContent;

  // Whether or not mActiveBrowsingContextInContent was set from another process
  // or from this process.
  bool mActiveBrowsingContextInContentSetFromOtherProcess;

  // This is the chrome process notion of content's
  // mActiveBrowsingContextInContent. Avoiding field reuse for different
  // semantics in different process types to make it easier to catch bugs.
  RefPtr<mozilla::dom::BrowsingContext> mActiveBrowsingContextInChrome;

  // the child or top-level window that is currently focused. In the chrome
  // process, when a window isn't being raised or lowered, this window will
  // either be the same window as mActiveWindow or a descendant of it.
  // Except during shutdown use SetFocusedWindowInternal to set mFocusedWindow!
  nsCOMPtr<nsPIDOMWindowOuter> mFocusedWindow;

  // The focused BrowsingContext if this is a chrome process and focus is
  // in chrome or if this is a content process and focus is in Web content
  // in this BrowsingContextGroup. nullptr otherwise.
  // Except during shutdown, must be set via SetFocusedWindowInternal which
  // calls SetFocusedBrowsingContext or if the value is coming in via IPC
  // via SetFocusedBrowsingContextFromOtherProcess.
  RefPtr<mozilla::dom::BrowsingContext> mFocusedBrowsingContextInContent;

  // This is the chrome process notion of content's
  // mFocusedBrowsingContextInContent. Avoiding field reuse for different
  // semantics in different process types to make it easier to catch bugs.
  RefPtr<mozilla::dom::BrowsingContext> mFocusedBrowsingContextInChrome;

  // the currently focused content if in-process or the XUL browser in which
  // Web content focus resides. Always inside mFocusedWindow. When a window
  // isn't being raised or lowered, this is a cached copy of the
  // mFocusedWindow's current content. This may be null if no content is
  // focused.
  RefPtr<mozilla::dom::Element> mFocusedElement;

  // these fields store a content node temporarily while it is being focused
  // or blurred to ensure that a recursive call doesn't refire the same event.
  // They will always be cleared afterwards.
  nsCOMPtr<nsIContent> mFirstBlurEvent;
  nsCOMPtr<nsIContent> mFirstFocusEvent;

  // keep track of a window while it is being lowered
  nsCOMPtr<nsPIDOMWindowOuter> mWindowBeingLowered;

  // synchronized actions cannot be interrupted with events, so queue these up
  // and fire them later.
  nsTArray<nsDelayedBlurOrFocusEvent> mDelayedBlurFocusEvents;

  // If set to true, layout of the document of the event target should be
  // flushed before handling focus depending events.
  bool mEventHandlingNeedsFlush;

  static bool sTestMode;

  // the single focus manager
  static nsFocusManager* sInstance;
};

nsresult NS_NewFocusManager(nsIFocusManager** aResult);

#endif
