/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function maybeAsyncStack(offset, column) {
  if (
    Services.prefs.getBoolPref(
      "javascript.options.asyncstack_capture_debuggee_only"
    )
  ) {
    return "";
  }

  let stack = Error().stack.replace(/^.*?\n/, "");
  return (
    "JSActor query*" +
    stack.replace(
      /^([^\n]+?):(\d+):\d+/,
      (m0, m1, m2) => `${m1}:${+m2 + offset}:${column}`
    )
  );
}

declTest("sendQuery Error", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");

    let asyncStack = maybeAsyncStack(2, 8);
    let error = await actorParent
      .sendQuery("error", { message: "foo" })
      .catch(e => e);

    is(error.message, "foo", "Error should have the correct message");
    is(error.name, "SyntaxError", "Error should have the correct name");
    is(
      error.stack,
      "receiveMessage@resource://testing-common/TestWindowChild.jsm:33:31\n" +
        asyncStack,
      "Error should have the correct stack"
    );
  },
});

declTest("sendQuery Exception", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");

    let asyncStack = maybeAsyncStack(2, 8);
    let error = await actorParent
      .sendQuery("exception", {
        message: "foo",
        result: Cr.NS_ERROR_INVALID_ARG,
      })
      .catch(e => e);

    is(error.message, "foo", "Error should have the correct message");
    is(
      error.result,
      Cr.NS_ERROR_INVALID_ARG,
      "Error should have the correct result code"
    );
    is(
      error.stack,
      "receiveMessage@resource://testing-common/TestWindowChild.jsm:36:22\n" +
        asyncStack,
      "Error should have the correct stack"
    );
  },
});

declTest("sendQuery testing", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");
    ok(actorParent, "JSWindowActorParent should have value.");

    let { result } = await actorParent.sendQuery("asyncAdd", { a: 10, b: 20 });
    is(result, 30);
  },
});

declTest("sendQuery in-process early lifetime", {
  url: "about:mozilla",
  allFrames: true,

  async test(browser) {
    let iframe = browser.contentDocument.createElement("iframe");
    browser.contentDocument.body.appendChild(iframe);
    let wgc = iframe.contentWindow.windowGlobalChild;
    let actorChild = wgc.getActor("TestWindow");
    let { result } = await actorChild.sendQuery("asyncMul", { a: 10, b: 20 });
    is(result, 200);
  },
});
