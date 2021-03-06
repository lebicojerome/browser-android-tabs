// Copyright 2017 The Chromium Authors. All
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview using private properties isn't a Closure violation in tests.
 * @suppress {accessControls}
 */

/**
 * Many application panel tests are flaky because storage state (e.g. IndexedDB)
 * doesn't get reset between tests.
 */
ApplicationTestRunner.resetState = async function() {
  const securityOrigin = new Common.ParsedURL(TestRunner.url()).securityOrigin();
  const storageTypes =
      ['appcache', 'cache_storage', 'cookies', 'indexeddb', 'local_storage', 'service_workers', 'websql'];
  await TestRunner.mainTarget.storageAgent().clearDataForOrigin(securityOrigin, storageTypes.join(','));
};

ApplicationTestRunner.createWebSQLDatabase = function(name) {
  return TestRunner.evaluateInPageAsync(`_openWebSQLDatabase("${name}")`);
};

ApplicationTestRunner.requestURLComparer = function(r1, r2) {
  return r1.request.url.localeCompare(r2.request.url);
};

ApplicationTestRunner.runAfterCachedResourcesProcessed = function(callback) {
  if (!TestRunner.resourceTreeModel._cachedResourcesProcessed)
    TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded, callback);
  else
    callback();
};

ApplicationTestRunner.runAfterResourcesAreFinished = function(resourceURLs, callback) {
  const resourceURLsMap = new Set(resourceURLs);

  function checkResources() {
    for (const url of resourceURLsMap) {
      const resource = ApplicationTestRunner.resourceMatchingURL(url);

      if (resource)
        resourceURLsMap.delete(url);
    }

    if (!resourceURLsMap.size) {
      TestRunner.resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.ResourceAdded, checkResources);
      callback();
    }
  }

  checkResources();

  if (resourceURLsMap.size)
    TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.ResourceAdded, checkResources);
};

ApplicationTestRunner.showResource = function(resourceURL, callback) {
  let reported = false;

  function callbackWrapper(sourceFrame) {
    if (reported)
      return;

    callback(sourceFrame);
    reported = true;
  }

  function showResourceCallback() {
    const resource = ApplicationTestRunner.resourceMatchingURL(resourceURL);

    if (!resource)
      return;

    UI.panels.resources.showResource(resource, 1);
    const sourceFrame = UI.panels.resources._resourceViewForResource(resource);

    if (sourceFrame.loaded)
      callbackWrapper(sourceFrame);
    else
      TestRunner.addSniffer(sourceFrame, 'onTextEditorContentSet', callbackWrapper.bind(null, sourceFrame));
  }

  ApplicationTestRunner.runAfterResourcesAreFinished([resourceURL], showResourceCallback);
};

ApplicationTestRunner.resourceMatchingURL = function(resourceURL) {
  let result = null;
  TestRunner.resourceTreeModel.forAllResources(visit);

  function visit(resource) {
    if (resource.url.indexOf(resourceURL) !== -1) {
      result = resource;
      return true;
    }
  }

  return result;
};

ApplicationTestRunner.waitForCookies = function() {
  return new Promise(resolve => {
    TestRunner.addSniffer(CookieTable.CookiesTable.prototype, '_rebuildTable', resolve);
  });
};

ApplicationTestRunner.dumpCookieDomains = function() {
  const cookieListChildren = UI.panels.resources._sidebar.cookieListTreeElement.children();
  TestRunner.addResult('Available cookie domains:');
  for (const child of cookieListChildren)
    TestRunner.addResult(child._cookieDomain);
};

ApplicationTestRunner.dumpCookies = function() {
  if (!UI.panels.resources._cookieView || !UI.panels.resources._cookieView.isShowing()) {
    TestRunner.addResult('No cookies visible');
    return;
  }

  TestRunner.addResult('Visible cookies');
  for (const item of UI.panels.resources._cookieView._cookiesTable._data) {
    const cookies = item.cookies || [];
    for (const cookie of cookies)
      TestRunner.addResult(`${cookie.name()}=${cookie.value()}`);
  }
};

ApplicationTestRunner.databaseModel = function() {
  return TestRunner.mainTarget.model(Resources.DatabaseModel);
};

ApplicationTestRunner.domStorageModel = function() {
  return TestRunner.mainTarget.model(Resources.DOMStorageModel);
};

ApplicationTestRunner.indexedDBModel = function() {
  return TestRunner.mainTarget.model(Resources.IndexedDBModel);
};

TestRunner.deprecatedInitAsync(`
  function _openWebSQLDatabase(name) {
    return new Promise(resolve => openDatabase(name, '1.0', '', 1024 * 1024, resolve));
  }
`);
