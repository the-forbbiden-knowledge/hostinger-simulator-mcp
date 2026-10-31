/* Original source module: 00-ui-overlay-manager */
(function () {
  "use strict";

  if (window.UIOverlayManager) {
    return;
  }

  var registry = new Map();
  var active = new Set();
  var stack = [];
  var openerByName = new Map();
  var bodyOverflowSnapshot = null;

  function getDescriptor(name) {
    return registry.get(name) || null;
  }

  function removeFromStack(name) {
    stack = stack.filter(function (entry) {
      return entry !== name;
    });
  }

  function touchStack(name) {
    removeFromStack(name);
    stack.push(name);
  }

  function getActiveInGroup(group) {
    var index;

    for (index = stack.length - 1; index >= 0; index -= 1) {
      var name = stack[index];
      var descriptor = getDescriptor(name);

      if (active.has(name) && descriptor && descriptor.group === group) {
        return name;
      }
    }

    return null;
  }

  function getTop(predicate) {
    var index;

    for (index = stack.length - 1; index >= 0; index -= 1) {
      var name = stack[index];
      var descriptor = getDescriptor(name);

      if (!active.has(name) || !descriptor) {
        continue;
      }

      if (!predicate || predicate(descriptor, name)) {
        return name;
      }
    }

    return null;
  }

  function getState() {
    var groups = {};

    registry.forEach(function (descriptor) {
      if (descriptor.group && !Object.prototype.hasOwnProperty.call(groups, descriptor.group)) {
        groups[descriptor.group] = getActiveInGroup(descriptor.group);
      }
    });

    return {
      active: stack.filter(function (name) { return active.has(name); }),
      groups: groups,
      top: getTop(),
      blocking: Array.from(active).some(function (name) {
        var descriptor = getDescriptor(name);
        return Boolean(descriptor && descriptor.blocksScroll);
      }),
    };
  }

  function syncDocumentState() {
    var root = document.documentElement;
    var state = getState();

    if (!root) {
      return;
    }

    Object.keys(state.groups).forEach(function (group) {
      var attribute = "data-ui-overlay-" + group.replace(/[^a-z0-9_-]/gi, "-").toLowerCase();
      var value = state.groups[group];

      if (value) {
        root.setAttribute(attribute, value);
      } else {
        root.removeAttribute(attribute);
      }
    });

    if (state.active.length) {
      root.setAttribute("data-ui-overlay-stack", state.active.join(" "));
    } else {
      root.removeAttribute("data-ui-overlay-stack");
    }

    if (state.top) {
      root.setAttribute("data-ui-overlay-top", state.top);
    } else {
      root.removeAttribute("data-ui-overlay-top");
    }
  }

  function syncBodyScrollLock() {
    var body = document.body;
    var shouldLock = getState().blocking;

    if (!body) {
      return;
    }

    if (shouldLock && bodyOverflowSnapshot === null) {
      bodyOverflowSnapshot = body.style.overflow || "";
      body.style.overflow = "hidden";
      body.setAttribute("data-ui-overlay-scroll-lock", "true");
      return;
    }

    if (!shouldLock && bodyOverflowSnapshot !== null) {
      body.style.overflow = bodyOverflowSnapshot;
      bodyOverflowSnapshot = null;
      body.removeAttribute("data-ui-overlay-scroll-lock");
    }
  }

  function emitChange(reason, name, payload) {
    var detail = {
      reason: reason,
      name: name || null,
      payload: payload || null,
      state: getState(),
    };

    syncDocumentState();
    syncBodyScrollLock();

    if (typeof window.CustomEvent === "function") {
      document.dispatchEvent(new window.CustomEvent("ui-overlay:change", { detail: detail }));
    }
  }

  function resolveOpener(name, payload) {
    var descriptor = getDescriptor(name);
    var explicit = payload && payload.opener;
    var fallback = null;

    if (explicit && typeof explicit.focus === "function") {
      return explicit;
    }

    if (descriptor && typeof descriptor.getTrigger === "function") {
      fallback = descriptor.getTrigger(payload || {});
      if (fallback && typeof fallback.focus === "function") {
        return fallback;
      }
    }

    if (document.activeElement && typeof document.activeElement.focus === "function") {
      return document.activeElement;
    }

    return null;
  }

  function restoreFocus(name, payload) {
    var descriptor = getDescriptor(name);
    var target;

    if (!payload || payload.restoreFocus !== true) {
      return;
    }

    target = openerByName.get(name);

    if ((!target || typeof target.focus !== "function") && descriptor && typeof descriptor.getTrigger === "function") {
      target = descriptor.getTrigger(payload);
    }

    if (target && typeof target.focus === "function" && target.isConnected !== false) {
      try {
        target.focus();
      } catch (error) {}
    }
  }

  function close(name, payload) {
    var descriptor = getDescriptor(name);
    var result;

    if (!descriptor || !active.has(name)) {
      return false;
    }

    result = typeof descriptor.close === "function"
      ? descriptor.close(payload || {})
      : true;

    if (result === false) {
      return false;
    }

    active.delete(name);
    removeFromStack(name);
    emitChange("close", name, payload);
    restoreFocus(name, payload || {});
    openerByName.delete(name);
    return true;
  }

  function closeGroup(group, payload) {
    var names = stack.slice().reverse();
    var didClose = false;

    names.forEach(function (name) {
      var descriptor = getDescriptor(name);

      if (descriptor && descriptor.group === group && active.has(name)) {
        didClose = close(name, payload) || didClose;
      }
    });

    return didClose;
  }

  function closeAll(payload) {
    var names = stack.slice().reverse();
    var didClose = false;

    names.forEach(function (name) {
      if (active.has(name)) {
        didClose = close(name, payload) || didClose;
      }
    });

    return didClose;
  }

  function closeForOpen(name, descriptor, payload) {
    var names = stack.slice().reverse();
    var closeGroups = Array.isArray(descriptor.closeGroupsOnOpen)
      ? descriptor.closeGroupsOnOpen
      : [];
    var conflicts = Array.isArray(descriptor.conflicts)
      ? descriptor.conflicts
      : [];

    names.forEach(function (activeName) {
      var activeDescriptor = getDescriptor(activeName);
      var sameGroup = descriptor.group && activeDescriptor && descriptor.group === activeDescriptor.group;
      var groupedConflict = activeDescriptor && closeGroups.indexOf(activeDescriptor.group) !== -1;
      var namedConflict = conflicts.indexOf(activeName) !== -1;

      if (activeName !== name && (sameGroup || groupedConflict || namedConflict)) {
        close(activeName, {
          reason: "replaced-by:" + name,
          source: payload && payload.source,
          restoreFocus: false,
        });
      }
    });
  }

  function open(name, payload) {
    var descriptor = getDescriptor(name);
    var wasActive;
    var result;

    if (!descriptor) {
      return false;
    }

    wasActive = active.has(name);

    if (!wasActive) {
      closeForOpen(name, descriptor, payload || {});
      openerByName.set(name, resolveOpener(name, payload || {}));
    } else if (payload && payload.opener) {
      openerByName.set(name, payload.opener);
    }

    result = typeof descriptor.open === "function"
      ? descriptor.open(payload || {}, { refresh: wasActive })
      : true;

    if (result === false) {
      if (!wasActive) {
        openerByName.delete(name);
      }
      return false;
    }

    active.add(name);
    touchStack(name);
    emitChange(wasActive ? "refresh" : "open", name, payload);
    return true;
  }

  function toggle(name, payload) {
    if (active.has(name)) {
      return close(name, payload);
    }

    return open(name, payload);
  }

  function closeTop(payload) {
    var name = getTop(function (descriptor) {
      return descriptor.dismissOnEscape !== false;
    });

    if (!name) {
      return false;
    }

    return close(name, Object.assign({ reason: "escape", restoreFocus: true }, payload || {}));
  }

  function register(name, options) {
    if (!name || !options || typeof options !== "object") {
      throw new TypeError("UIOverlayManager.register requer nome e configuração.");
    }

    if (registry.has(name)) {
      throw new Error("Overlay já registrado: " + name);
    }

    registry.set(name, {
      group: options.group || null,
      blocksScroll: options.blocksScroll === true,
      dismissOnEscape: options.dismissOnEscape !== false,
      dismissOnOutsidePointer: options.dismissOnOutsidePointer === true,
      dismissOnScroll: options.dismissOnScroll === true,
      dismissOnResize: options.dismissOnResize === true,
      closeGroupsOnOpen: options.closeGroupsOnOpen || [],
      conflicts: options.conflicts || [],
      getBoundary: options.getBoundary,
      getTrigger: options.getTrigger,
      open: options.open,
      close: options.close,
    });

    syncDocumentState();
    return name;
  }

  function isActive(name) {
    return active.has(name);
  }

  function dismissByPolicy(policy, reason, event) {
    var names = stack.slice().reverse();

    names.forEach(function (name) {
      var descriptor = getDescriptor(name);

      if (active.has(name) && descriptor && descriptor[policy] === true) {
        close(name, { reason: reason, event: event, restoreFocus: false });
      }
    });
  }

  function containsTarget(element, target) {
    return Boolean(element && target && typeof element.contains === "function" && element.contains(target));
  }

  function onDocumentPointerDown(event) {
    var name = getTop(function (descriptor) {
      return descriptor.dismissOnOutsidePointer === true;
    });
    var descriptor;
    var boundary;
    var trigger;

    if (!name) {
      return;
    }

    descriptor = getDescriptor(name);
    boundary = descriptor && typeof descriptor.getBoundary === "function"
      ? descriptor.getBoundary()
      : null;
    trigger = descriptor && typeof descriptor.getTrigger === "function"
      ? descriptor.getTrigger()
      : null;

    if (containsTarget(boundary, event.target) || containsTarget(trigger, event.target)) {
      return;
    }

    close(name, { reason: "outside-pointer", event: event, restoreFocus: false });
  }

  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape") {
      closeTop({ event: event });
    }
  });

  document.addEventListener("pointerdown", onDocumentPointerDown, true);

  if (typeof window.addEventListener === "function") {
    window.addEventListener("resize", function (event) {
      dismissByPolicy("dismissOnResize", "resize", event);
    });
    window.addEventListener("scroll", function (event) {
      dismissByPolicy("dismissOnScroll", "scroll", event);
    }, true);
  }

  window.UIOverlayManager = Object.freeze({
    register: register,
    open: open,
    close: close,
    toggle: toggle,
    closeGroup: closeGroup,
    closeAll: closeAll,
    closeTop: closeTop,
    getTop: getTop,
    isActive: isActive,
    getState: getState,
  });
})();


/* Original source module: 01-theme */
(function () {
  var themeStorageKey = "zeit-theme";
  var media = window.matchMedia("(prefers-color-scheme: light)");

  function resolvedTheme(theme) {
    return theme === "system" ? (media.matches ? "light" : "dark") : theme;
  }

  function syncThemeControls(theme) {
    document.querySelectorAll('.theme-switcher_root__DrzBE input[type="radio"]').forEach(function (input) {
      input.checked = input.value === theme;
    });
  }

  function applyTheme(theme, persist) {
    var selectedTheme = theme === "light" || theme === "dark" ? theme : "system";
    var resolved = resolvedTheme(selectedTheme);
    var root = document.documentElement;

    root.classList.add("theme-switching-instant");
    root.classList.remove("light-theme", "dark-theme", "light", "dark");
    root.classList.add(resolved === "light" ? "light-theme" : "dark-theme");
    root.classList.add(resolved);
    root.style.colorScheme = resolved;
    root.style.backgroundColor = resolved === "light" ? "rgb(255, 255, 255)" : "rgb(0, 0, 0)";

    if (persist !== false) {
      try {
        localStorage.setItem(themeStorageKey, selectedTheme);
      } catch (error) {}
    }

    syncThemeControls(selectedTheme);
    window.requestAnimationFrame(function () {
      window.requestAnimationFrame(function () {
        root.classList.remove("theme-switching-instant");
      });
    });
  }

  function customizeCourseList() {
    var heading = Array.from(document.querySelectorAll("main h2")).find(function (element) {
      return element.textContent.trim() === "Cursos";
    });
    var courseColumn = heading && heading.parentElement && heading.parentElement.nextElementSibling;

    if (!courseColumn) {
      return;
    }

    Array.from(courseColumn.querySelectorAll(":scope > a")).forEach(function (course, index) {
      if (index > 0) {
        course.remove();
      }
    });
  }

  function init() {
    var savedTheme = "system";

    try {
      savedTheme = localStorage.getItem(themeStorageKey) || "light";
    } catch (error) {}

    customizeCourseList();
    applyTheme(savedTheme, false);

    document.addEventListener("change", function (event) {
      var input = event.target;

      if (input.matches('.theme-switcher_root__DrzBE input[type="radio"]')) {
        applyTheme(input.value, true);
      }
    });

    media.addEventListener("change", function () {
      var selectedTheme = "system";

      try {
        selectedTheme = localStorage.getItem(themeStorageKey) || "light";
      } catch (error) {}

      if (selectedTheme === "system") {
        applyTheme("system", false);
      }
    });
  }

  var style = document.createElement("style");
  style.textContent =
    "html.theme-switching-instant,html.theme-switching-instant *,html.theme-switching-instant *::before,html.theme-switching-instant *::after{" +
    "transition:none!important;animation:none!important;animation-duration:0s!important;" +
    "}" +
    "html.light-theme .navigation-menu_menuSubLink__jde_U:hover .navigation-menu_icon__xsndA{" +
    "background:#7a1116!important;" +
    "}" +
    "html.light-theme .navigation-menu_menuSubLink__jde_U:hover .navigation-menu_menuItemText__l6KEM{" +
    "--text-color:#7a1116!important;color:#7a1116!important;" +
    "}";
  document.head.appendChild(style);
  window.setSiteTheme = applyTheme;

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();


/* Original source module: 05-footer */
(function () {
  function initFooterInteractions() {
    var footerMark = document.querySelector("[data-footer-back-to-top]");
    var status = document.querySelector('[data-testid="footer/status"]');
    var statusDot = status && status.querySelector("[data-footer-status-dot]");
    var statusText = status && status.querySelector("[data-footer-status-text]");

    if (footerMark) {
      footerMark.addEventListener("click", function (event) {
        event.preventDefault();
        window.scrollTo({ top: 0, left: 0, behavior: "smooth" });
      });
    }

    if (status && statusDot && statusText) {
      window.setTimeout(function () {
        status.setAttribute("data-status-phase", "ready");
        statusDot.setAttribute("data-testid", "footer/status/success");
        statusText.textContent = "Todos os sistemas normais.";
      }, 900);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initFooterInteractions);
  } else {
    initFooterInteractions();
  }
})();


/* Original source module: 06-academy-command */
(function () {
  var portalId = "academy-command-menu";
  var launcherSelector = ".cmdklaunch_launcher__f9k1l";
  var overlayManager = window.UIOverlayManager;

  function normalize(text) {
    return (text || "").replace(/\s+/g, " ").trim();
  }

  function getCourses() {
    var seen = new Set();
    return Array.from(document.querySelectorAll('a[data-public-project="true"]'))
      .map(function (link) {
        var title = normalize(link.querySelector("h3") && link.querySelector("h3").textContent);
        var description = normalize(link.querySelector("p") && link.querySelector("p").textContent);
        var href = link.getAttribute("href");

        if (!title || !description || !href || seen.has(href)) {
          return null;
        }

        seen.add(href);
        return {
          title: title,
          description: description,
          href: href,
          value: ("static-" + title + description).toLowerCase(),
        };
      })
      .filter(Boolean);
  }

  function createElement(tagName, attributes, text) {
    var element = document.createElement(tagName);

    Object.keys(attributes || {}).forEach(function (name) {
      element.setAttribute(name, attributes[name]);
    });

    if (text) {
      element.textContent = text;
    }

    return element;
  }

  function selectItem(root, selectedItem) {
    Array.from(root.querySelectorAll("[cmdk-item]")).forEach(function (item) {
      var isSelected = item === selectedItem;

      item.setAttribute("aria-selected", isSelected ? "true" : "false");

      if (isSelected) {
        item.setAttribute("data-selected", "true");
      } else {
        item.removeAttribute("data-selected");
      }
    });
  }

  function createCourseItem(course, index) {
    var item = createElement("div", {
      class: "style_item__flc5Q w-full mb-3 text-left hover:bg-(--ds-gray-alpha-100) transition-colors",
      "cmdk-item": "",
      "data-course-item": "",
      role: "option",
      "data-value": course.value,
      "aria-selected": index === 0 ? "true" : "false",
    });

    if (index === 0) {
      item.setAttribute("data-selected", "true");
    }

    var content = createElement("div", {
      class: "flex flex-col gap-2 rounded-md p-3",
    });
    var title = createElement("div", {
      class: "text-base font-semibold text-(--ds-gray-1000)",
    }, course.title);
    var description = createElement("p", {
      class: "text-sm text-(--ds-gray-700) line-clamp-3",
    }, course.description);

    content.appendChild(title);
    content.appendChild(description);
    item.appendChild(content);

    item.addEventListener("click", function () {
      window.location.href = course.href;
    });
    item.addEventListener("mouseenter", function () {
      var root = item.closest('[role="dialog"]');

      if (root) {
        selectItem(root, item);
      }
    });

    return item;
  }

  function createShortQueryItem() {
    return createElement("div", {
      class: "style_item__flc5Q",
      "cmdk-item": "",
      "data-short-query-item": "",
      role: "option",
      "data-value": "static-digite pelo menos 3 caracteres para pesquisar",
      "aria-selected": "false",
    }, "Digite pelo menos 3 caracteres para pesquisar");
  }

  function updateResults(root) {
    var input = root.querySelector("[cmdk-input]");
    var list = root.querySelector("[cmdk-list]");
    var courseGroup = root.querySelector("[data-course-group]");
    var shortQueryItem = root.querySelector("[data-short-query-item]");
    var items = Array.from(root.querySelectorAll("[data-course-item]"));
    var query = normalize(input.value).toLowerCase();
    var aiButton = root.querySelector("[data-academy-ai]");
    if (aiButton) aiButton.hidden = query.length < 3;
    var selected = false;
    var visibleCount = 0;

    if (query.length > 0 && query.length < 3) {
      if (courseGroup) {
        courseGroup.hidden = true;
      }

      if (shortQueryItem) {
        shortQueryItem.hidden = false;
        selectItem(root, shortQueryItem);
      }

      items.forEach(function (item) {
        item.hidden = true;
        item.removeAttribute("data-selected");
        item.setAttribute("aria-selected", "false");
      });

      list.style.setProperty("--cmdk-list-height", "40.0px");
      return;
    }

    if (courseGroup) {
      courseGroup.hidden = false;
    }

    if (shortQueryItem) {
      shortQueryItem.hidden = true;
      shortQueryItem.removeAttribute("data-selected");
      shortQueryItem.setAttribute("aria-selected", "false");
    }

    items.forEach(function (item) {
      var isVisible = !query || item.getAttribute("data-value").indexOf(query) !== -1;
      item.hidden = !isVisible;
      item.removeAttribute("data-selected");
      item.setAttribute("aria-selected", "false");

      if (isVisible) {
        visibleCount += 1;
      }

      if (isVisible && !selected) {
        selectItem(root, item);
        selected = true;
      }
    });

    list.style.setProperty("--cmdk-list-height", Math.max(60, visibleCount * 106) + "px");
  }

  async function askAiFromSearch(root) {
    var input = root.querySelector("[cmdk-input]");
    var answer = root.querySelector("[data-academy-ai-answer]");
    var button = root.querySelector("[data-academy-ai]");
    var query = normalize(input && input.value);
    if (!answer || query.length < 3) return;
    answer.hidden = false;
    answer.innerHTML = "<strong>Avaliação assistida por IA</strong>Analisando a consulta…";
    if (button) button.disabled = true;
    try {
      throw new Error("Sem IA conectada neste website p?blico.");
      var body = await response.json().catch(function () { return {}; });
      if (!response.ok) throw new Error(body.error || "Falha ao consultar a IA.");
      answer.textContent = "";
      var title = createElement("strong", {}, "Avaliação assistida por IA");
      answer.appendChild(title);
      answer.appendChild(document.createTextNode(body.content || "Sem resposta disponível."));
    } catch (error) {
      answer.textContent = "";
      var errorTitle = createElement("strong", {}, "IA indisponível");
      answer.appendChild(errorTitle);
      answer.appendChild(document.createTextNode(error.message || "Não foi possível concluir a consulta."));
    } finally { if (button) button.disabled = false; }
  }

  function closeMenu() {
    var portal = document.getElementById(portalId);

    if (portal) {
      portal.remove();
    }

    return true;
  }

  function isMobileViewport() {
    return window.matchMedia("(max-width: 600px)").matches;
  }

  function openMenu() {
    closeMenu();

    var courses = getCourses();
    var isMobile = isMobileViewport();
    var labelId = isMobile ? "_r_2p_" : "_r_u_";
    var inputId = isMobile ? "_r_2q_" : "_r_v_";
    var listId = isMobile ? "_r_2o_" : "_r_t_";
    var groupId = isMobile ? "_r_2u_" : "_r_13_";
    var portal = createElement("div", { id: portalId });

    var overlay = isMobile
      ? createElement("div", {
          "data-state": "open",
          class: "new-dialog_overlay___0a9q new-dialog_open__DTLMi new-dialog_notSafari__UQu2r",
          style: "--animation-duration: 300ms; --curve: cubic-bezier(0.32, 0.72, 0, 1); pointer-events: auto;",
          "data-aria-hidden": "true",
          "aria-hidden": "true",
        })
      : createElement("div", {
          "data-state": "open",
          "cmdk-overlay": "",
          "geist-overlay": "",
          style: "pointer-events: auto;",
          "data-aria-hidden": "true",
          "aria-hidden": "true",
        });

    var dialog = isMobile
      ? createElement("div", {
          role: "dialog",
          id: "radix-_r_2l_",
          "aria-describedby": "radix-_r_2n_",
          "aria-labelledby": "radix-_r_2m_",
          "data-state": "open",
          "data-vaul-drawer-direction": "bottom",
          "data-vaul-drawer": "",
          "data-vaul-delayed-snap-points": "false",
          "data-vaul-snap-points": "false",
          "data-vaul-custom-container": "false",
          "data-vaul-animate": "true",
          class: "new-dialog_drawer__ykt7Q geist-dialog",
          tabindex: "-1",
          style: "height: 80vh; pointer-events: auto;",
        })
      : createElement("div", {
          role: "dialog",
          id: "radix-_r_q_",
          "aria-describedby": "radix-_r_s_",
          "aria-labelledby": "radix-_r_r_",
          "data-state": "open",
          "aria-label": "Menu global de comandos",
          "cmdk-dialog": "",
          "geist-dialog": "",
          tabindex: "-1",
          style: "pointer-events: auto;",
        });

    dialog.innerHTML =
      (isMobile
        ? '<div class="new-dialog_inner__FYIY1 tailwind" style="height: 100%; display: flex; flex-direction: column; overflow: hidden;"><div cmdk-root="" style="height: 100%; display: flex; flex-direction: column; min-height: 0;">'
        : '<h2 id="radix-_r_r_" class="sr-only">Menu de comandos</h2>' +
          '<p id="radix-_r_s_" class="sr-only">Use o menu de comandos para navegar.</p>' +
          '<div data-testid="cmdk/root" cmdk-root="">') +
      '<label cmdk-label="" for="' + inputId + '" id="' + labelId + '" style="position: absolute; width: 1px; height: 1px; padding: 0px; margin: -1px; overflow: hidden; clip: rect(0px, 0px, 0px, 0px); white-space: nowrap; border-width: 0px;">Menu global de comandos</label>' +
      '<div class="style_topSection__VAC9w"' + (isMobile ? ' style="position: sticky; top: 0; z-index: 2; flex: 0 0 auto;"' : '') + '><div class="style_inputWrapper__U5UlZ">' +
      '<input class="style_input__t1XJR" placeholder="O que você está procurando?" aria-autocomplete="list" aria-controls="' + listId + '" aria-expanded="true" aria-labelledby="' + labelId + '" autocomplete="off" autocorrect="off" cmdk-input="" id="' + inputId + '" role="combobox" spellcheck="false" type="text" value="">' +
      '<button class="academy-ai-query-button" data-academy-ai type="button" hidden aria-label="Perguntar à IA sobre esta busca" title="Perguntar à IA"><img src="/favicon.png" alt=""></button>' +
      '<button class="style_escButton__v426S" type="button">Esc</button>' +
      '</div><div class="academy-ai-query-answer" data-academy-ai-answer hidden></div></div>' +
      '<div class="uncontained tailwind tailwind-no-preflight h-full cmdk_cmdk__uyUyV"' + (isMobile ? ' style="flex: 1 1 auto; min-height: 0; overflow: hidden;"' : '') + '>' +
      '<div class="style_list__P7QQw" aria-label="Sugestões" aria-labelledby="' + inputId + '" cmdk-list="" id="' + listId + '" role="listbox"' + (isMobile ? ' style="height: 100%; max-height: none; overflow-y: auto;"' : '') + '>' +
      '<div cmdk-list-sizer=""><div class="style_group__zRcxf" cmdk-group="" role="presentation" data-group-id="' + (isMobile ? "_r_2r_" : "_r_10_") + '" data-value=""><div data-short-query-items="" cmdk-group-items="" role="group"></div></div>' +
      '<div class="style_group__zRcxf" data-course-group="" cmdk-group="" role="presentation" data-group-id="' + (isMobile ? "_r_2t_" : "_r_12_") + '" data-value="recommended courses">' +
      '<div class="flex items-center justify-between gap-3" cmdk-group-heading=""><div aria-hidden="true" id="' + groupId + '">Cursos recomendados</div><button type="button" data-academy-ask-ai="" aria-label="Pergunte à IA" title="Pergunte à IA" class="outline-none m-0 border-0 bg-transparent cursor-pointer inline-flex size-8 items-center justify-center p-0 text-gray-900 hover:text-gray-1000"><svg viewBox="0 0 16 16" width="16" height="16" aria-hidden="true"><path fill="currentColor" d="M8 1.25l1.25 3.5L12.75 6 9.25 7.25 8 10.75 6.75 7.25 3.25 6l3.5-1.25L8 1.25Zm4.25 8.5.7 1.95 1.8.7-1.8.7-.7 1.9-.7-1.9-1.8-.7 1.8-.7.7-1.95Z"/></svg></button></div>' +
      '<div aria-labelledby="' + groupId + '" cmdk-group-items="" role="group"></div>' +
      '</div></div></div></div></div>' +
      (isMobile ? '</div></div>' : '');

    var itemsRoot = dialog.querySelector('[aria-labelledby="' + groupId + '"]');
    var shortQueryRoot = dialog.querySelector("[data-short-query-items]");
    shortQueryRoot.appendChild(createShortQueryItem());
    courses.forEach(function (course, index) {
      itemsRoot.appendChild(createCourseItem(course, index));
    });

    portal.appendChild(overlay);
    portal.appendChild(dialog);
    document.body.appendChild(portal);

    var input = dialog.querySelector("[cmdk-input]");
    var escButton = dialog.querySelector(".style_escButton__v426S");

    overlay.addEventListener("click", function () {
      overlayManager.close("academy", { source: "backdrop", restoreFocus: true });
    });
    escButton.addEventListener("click", function () {
      overlayManager.close("academy", { source: "esc-button", restoreFocus: true });
    });
    var aiButton = dialog.querySelector("[data-academy-ai]");
    if (aiButton) aiButton.addEventListener("click", function () { askAiFromSearch(dialog); });
    input.addEventListener("input", function () {
      var answer = dialog.querySelector("[data-academy-ai-answer]");
      if (answer) answer.hidden = true;
      updateResults(dialog);
    });
    updateResults(dialog);
    dialog.focus();
    input.focus();
    return true;
  }

  overlayManager.register("academy", {
    group: "modal",
    blocksScroll: true,
    dismissOnEscape: true,
    closeGroupsOnOpen: ["header"],
    open: openMenu,
    close: closeMenu,
  });

  document.addEventListener("click", function (event) {
    var launcher = event.target.closest && event.target.closest(launcherSelector);

    if (!launcher) {
      return;
    }

    event.preventDefault();
    overlayManager.open("academy", { source: "launcher", opener: launcher });
  });

  document.addEventListener("keydown", function (event) {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") {
      event.preventDefault();
      overlayManager.open("academy", { source: "keyboard" });
    }
  });
})();


/* Original source module: 07-desktop-navigation */
(function () {
  var rootSelector = ".navigation-menu_root__HT1ea";
  var triggerSelector = ".navigation-menu_trigger__AmZ48";
  var viewportPositionSelector = ".navigation-menu_viewportPosition__U0Mm6";
  var viewportClass = "navigation-menu_viewport__6i_54";
  var closeTimer = null;
  var defaultActiveTrigger = null;
  var overlayManager = window.UIOverlayManager;

  function getRoot() {
    return document.querySelector(rootSelector);
  }

  function getHeader() {
    return document.querySelector("[data-navigation-header]");
  }

  function getScrollTop() {
    return window.pageYOffset || document.documentElement.scrollTop || document.body.scrollTop || 0;
  }

  function getTriggers(root) {
    return Array.from(root.querySelectorAll(triggerSelector));
  }

  function getViewportPosition(root) {
    return root.querySelector(viewportPositionSelector);
  }

  function getContent(trigger) {
    var contentId = trigger.getAttribute("aria-controls");
    return contentId ? document.getElementById(contentId) : null;
  }

  function clearTimer() {
    if (closeTimer) {
      window.clearTimeout(closeTimer);
      closeTimer = null;
    }
  }

  function setTriggerState(root, activeTrigger, suppressDefault) {
    getTriggers(root).forEach(function (trigger) {
      var isOpen = trigger === activeTrigger;
      var isRestingActive = !activeTrigger && !suppressDefault && trigger === defaultActiveTrigger;

      trigger.setAttribute("data-state", isOpen ? "open" : "closed");
      trigger.setAttribute("data-active", isOpen || isRestingActive ? "open" : "closed");
      trigger.setAttribute("aria-expanded", isOpen ? "true" : "false");
    });
  }

  function setSourceContentState(root, activeContent) {
    getTriggers(root).forEach(function (trigger) {
      var content = getContent(trigger);
      var isActive = content === activeContent;

      if (!content) {
        return;
      }

      content.setAttribute("data-state", isActive ? "open" : "closed");
      content.setAttribute("aria-hidden", isActive ? "false" : "true");
      content.style.pointerEvents = "none";
    });
  }

  function makeContentInteractive(content) {
    content.setAttribute("data-state", "open");
    content.setAttribute("aria-hidden", "false");
    content.style.pointerEvents = "auto";
    content.querySelectorAll('[tabindex="-1"]').forEach(function (element) {
      element.removeAttribute("tabindex");
    });
  }

  function closeMenu(root, suppressDefault) {
    var viewportPosition;

    if (!root) {
      return false;
    }

    viewportPosition = getViewportPosition(root);
    clearTimer();
    setTriggerState(root, null, Boolean(suppressDefault));
    setSourceContentState(root, null);

    if (viewportPosition) {
      viewportPosition.innerHTML = "";
    }

    return true;
  }

  function dismissContextMenusForHeader(reason) {
    overlayManager.closeGroup("context-menu", {
      reason: reason || "header-hover",
      restoreFocus: false,
    });
  }

  function activateStandaloneLink(root) {
    dismissContextMenusForHeader("standalone-header-link");
    overlayManager.closeGroup("header", {
      reason: "standalone-header-link",
      suppressDefault: true,
    });
    closeMenu(root, true);
  }

  function updateHeaderScrollState() {
    var header = getHeader();
    var hasScrolled = getScrollTop() > 0;

    if (!header) {
      return;
    }

    header.classList.toggle("hasScrolled", hasScrolled);
    header.setAttribute("data-scrolled", hasScrolled ? "true" : "false");

    if (hasScrolled) {
      header.setAttribute("data-expand", "false");
    } else {
      header.removeAttribute("data-expand");
    }
  }

  function scheduleHeaderScrollUpdate() {
    updateHeaderScrollState();
  }

  function scheduleClose() {
    clearTimer();
    closeTimer = window.setTimeout(function () {
      overlayManager.close("desktop-navigation", { source: "hover-leave" });
    }, 120);
  }

  function openMenu(root, trigger) {
    var viewportPosition;
    var sourceContent;

    if (!root || !trigger) {
      return false;
    }

    viewportPosition = getViewportPosition(root);
    sourceContent = getContent(trigger);
    var viewport =
      viewportPosition && viewportPosition.querySelector("." + viewportClass);
    var content = sourceContent && sourceContent.cloneNode(true);

    if (!viewportPosition || !sourceContent || !content) {
      return false;
    }

    clearTimer();
    setTriggerState(root, trigger);
    setSourceContentState(root, sourceContent);
    makeContentInteractive(content);

    if (!viewport) {
      viewport = document.createElement("div");
      viewport.setAttribute("data-state", "open");
      viewport.setAttribute("data-orientation", "horizontal");
      viewport.className = viewportClass;
      viewport.style.setProperty("--radix-navigation-menu-viewport-width", "760px");
      viewport.style.setProperty("--radix-navigation-menu-viewport-height", "356px");

      viewport.addEventListener("mouseenter", clearTimer);
      viewport.addEventListener("mouseleave", function () {
        scheduleClose();
      });

      viewportPosition.appendChild(viewport);
    }

    viewport.replaceChildren(content);
    viewport.style.setProperty(
      "--radix-navigation-menu-viewport-width",
      Math.max(320, content.scrollWidth) + "px"
    );
    viewport.style.setProperty(
      "--radix-navigation-menu-viewport-height",
      Math.max(120, content.scrollHeight) + "px"
    );
    return true;
  }

  overlayManager.register("desktop-navigation", {
    group: "header",
    blocksScroll: false,
    dismissOnEscape: true,
    closeGroupsOnOpen: ["context-menu"],
    open: function (payload) {
      return openMenu(getRoot(), payload && payload.trigger);
    },
    close: function (payload) {
      return closeMenu(getRoot(), payload && payload.suppressDefault);
    },
  });

  function init() {
    var root = getRoot();

    updateHeaderScrollState();
    window.addEventListener("scroll", function () {
      scheduleHeaderScrollUpdate();
      }, { passive: true });
    window.addEventListener("resize", scheduleHeaderScrollUpdate);

    if (!root) {
      return;
    }

    defaultActiveTrigger =
      getTriggers(root).find(function (trigger) {
        return trigger.getAttribute("data-active") === "open";
      }) || null;

    setTriggerState(root, null);

    getTriggers(root).forEach(function (trigger) {
      trigger.addEventListener("mouseenter", function () {
        dismissContextMenusForHeader("header-trigger-hover");
        overlayManager.open("desktop-navigation", { source: "hover", trigger: trigger });
      });
      trigger.addEventListener("focus", function () {
        dismissContextMenusForHeader("header-trigger-focus");
        overlayManager.open("desktop-navigation", { source: "focus", trigger: trigger });
      });
      trigger.addEventListener("mouseleave", function () {
        scheduleClose();
      });
    });

    Array.from(
      root.querySelectorAll(
        ".navigation-menu_list__1vwjh > li > .navigation-menu_link__vSiGj"
      )
    ).forEach(function (link) {
      link.addEventListener("mouseenter", function () {
        activateStandaloneLink(root);
      });
      link.addEventListener("focus", function () {
        activateStandaloneLink(root);
      });
      link.addEventListener("mouseleave", function () {
        scheduleClose();
      });
      link.addEventListener("blur", function () {
        scheduleClose();
      });
    });

    root.addEventListener("mouseenter", clearTimer);
    root.addEventListener("mouseleave", function () {
      scheduleClose();
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();


/* Original source module: 08-context-sliding-highlight */
(function () {
  var groupAttribute = "data-context-sliding-highlight";
  var visibleAttribute = "data-context-highlight-visible";

  function isInside(target, node) {
    return Boolean(
      target &&
        node &&
        node.nodeType &&
        (target === node || target.contains(node))
    );
  }

  function setHighlight(group, target, immediate) {
    var geometryTarget;
    var groupRect;
    var padding;
    var targetRect;

    if (!group || !target || !target.isConnected) {
      group && group.setAttribute(visibleAttribute, "false");
      return;
    }

    geometryTarget = target;
    if (group.getAttribute("data-context-highlight-geometry") === "icon") {
      geometryTarget =
        target.querySelector(".navigation-menu_icons__8b2lC") ||
        target.querySelector(".navigation-menu_icon__xsndA") ||
        target;
    }

    groupRect = group.getBoundingClientRect();
    padding = Number(group.getAttribute("data-context-highlight-padding")) || 0;
    targetRect = geometryTarget.getBoundingClientRect();

    if (immediate) {
      group.style.setProperty("--context-highlight-duration", "0ms");
    }

    group.style.setProperty(
      "--context-highlight-x",
      targetRect.left - groupRect.left + group.scrollLeft - padding + "px"
    );
    group.style.setProperty(
      "--context-highlight-y",
      targetRect.top - groupRect.top + group.scrollTop - padding + "px"
    );
    group.style.setProperty(
      "--context-highlight-width",
      targetRect.width + padding * 2 + "px"
    );
    group.style.setProperty(
      "--context-highlight-height",
      targetRect.height + padding * 2 + "px"
    );
    group.setAttribute(visibleAttribute, "true");

    if (immediate) {
      window.requestAnimationFrame(function () {
        group.style.setProperty("--context-highlight-duration", "240ms");
      });
    }
  }

  function getNavigationSpinMotion(previousTarget, nextTarget) {
    var previousIcon;
    var previousRect;
    var nextIcon;
    var nextRect;
    var deltaX;
    var deltaY;
    var primaryDelta;

    if (!previousTarget || !nextTarget) {
      return { axis: "y", distance: 1800 };
    }

    previousIcon =
      previousTarget.querySelector(".navigation-menu_icon__xsndA") ||
      previousTarget;
    nextIcon =
      nextTarget.querySelector(".navigation-menu_icon__xsndA") || nextTarget;
    previousRect = previousIcon.getBoundingClientRect();
    nextRect = nextIcon.getBoundingClientRect();
    deltaX =
      nextRect.left +
      nextRect.width / 2 -
      (previousRect.left + previousRect.width / 2);
    deltaY =
      nextRect.top +
      nextRect.height / 2 -
      (previousRect.top + previousRect.height / 2);

    primaryDelta =
      Math.abs(deltaY) > Math.abs(deltaX) ? deltaY : deltaX;

    return {
      axis: "y",
      distance: primaryDelta >= 0 ? 1800 : -1800,
    };
  }

  function getNavigationCurrentRotation(host) {
    var rotationTarget =
      host.querySelector(".navigation-menu_icon__xsndA") || host;
    var computedRotation = window.getComputedStyle(rotationTarget).rotate;
    var angleMatch =
      computedRotation && computedRotation.match(/(-?\d+(?:\.\d+)?)deg/);
    var axisMatch =
      computedRotation &&
      computedRotation.match(/(?:^|\s)([xyz])\s/i);

    return {
      angle: angleMatch ? Number(angleMatch[1]) : 0,
      axis:
        (axisMatch && axisMatch[1].toLowerCase()) ||
        host.getAttribute("data-context-icon-spin-axis") ||
        "y",
    };
  }

  function getNavigationSpinEnd(currentRotation, spinDistance) {
    var direction = spinDistance >= 0 ? 1 : -1;
    var normalizedRotation =
      ((currentRotation % 360) + 360) % 360;
    var frontFacingCorrection =
      direction > 0
        ? (360 - normalizedRotation) % 360
        : normalizedRotation;

    return (
      currentRotation +
      direction * (1800 + frontFacingCorrection)
    );
  }

  function animateNavigationOriginalIcon(previousTarget, nextTarget) {
    var currentRotation;
    var host;
    var rotationTarget;
    var spinMotion;
    var spinEnd;
    var wasSpinning;

    if (
      !nextTarget ||
      window.matchMedia("(prefers-reduced-motion: reduce)").matches
    ) {
      return;
    }

    host =
      nextTarget.querySelector(".navigation-menu_icons__8b2lC") ||
      nextTarget.querySelector(".navigation-menu_icon__xsndA");
    if (!host) {
      return;
    }

    rotationTarget =
      host.querySelector(".navigation-menu_icon__xsndA") || host;
    wasSpinning =
      host.getAttribute("data-context-icon-spinning") === "true" ||
      host.getAttribute("data-context-icon-microphone-spinning") === "true";
    currentRotation = getNavigationCurrentRotation(host);
    spinMotion = getNavigationSpinMotion(previousTarget, nextTarget);

    if (wasSpinning && typeof host._contextIconSpinReset === "function") {
      host._contextIconSpinReset();
    } else {
      currentRotation.angle = 0;
    }

    currentRotation.axis = spinMotion.axis;
    spinEnd = getNavigationSpinEnd(
      currentRotation.angle,
      spinMotion.distance
    );

    host.setAttribute(
      "data-context-icon-spin-direction",
      spinMotion.distance >= 0 ? "1" : "-1"
    );
    host.setAttribute("data-context-icon-spin-axis", spinMotion.axis);
    host.style.setProperty(
      "--context-icon-spin-start",
      spinMotion.axis + " " + currentRotation.angle + "deg"
    );
    host.style.setProperty(
      "--context-icon-spin-end",
      spinMotion.axis + " " + spinEnd + "deg"
    );
    rotationTarget.getBoundingClientRect();
    host.setAttribute("data-context-icon-spinning", "true");

    function resetNavigationIconSpin() {
      host.removeEventListener("animationend", finishNavigationIconSpin);
      host.removeAttribute("data-context-icon-spinning");
      host._contextIconSpinReset = null;
    }

    function finishNavigationIconSpin(event) {
      if (
        event.target !== rotationTarget ||
        event.animationName !== "context-navigation-original-icon-spin"
      ) {
        return;
      }

      resetNavigationIconSpin();
    }

    host._contextIconSpinReset = resetNavigationIconSpin;
    host.addEventListener("animationend", finishNavigationIconSpin);
  }

  function microphoneSvgMarkup() {
    return (
      '<svg data-testid="geist-icon" aria-hidden="true" width="16" height="16" ' +
      'viewBox="0 0 16 16" fill="none" stroke="currentColor" ' +
      'stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">' +
      '<rect x="5.25" y="1.25" width="5.5" height="8.5" rx="2.75"></rect>' +
      '<path d="M3.5 7.75a4.5 4.5 0 0 0 9 0"></path>' +
      '<path d="M8 12.25v2.5"></path>' +
      '<path d="M5.5 14.75h5"></path>' +
      "</svg>"
    );
  }

  function animateNavigationMicrophone(host) {
    var currentRotation;
    var normalizedRotation;
    var rotationTarget;
    var spinEnd;

    if (
      !host ||
      window.matchMedia("(prefers-reduced-motion: reduce)").matches
    ) {
      return;
    }

    rotationTarget =
      host.querySelector(".navigation-menu_icon__xsndA") || host;
    currentRotation = getNavigationCurrentRotation(host);

    if (typeof host._contextIconSpinReset === "function") {
      host._contextIconSpinReset();
    }

    normalizedRotation =
      ((currentRotation.angle % 360) + 360) % 360;
    spinEnd =
      currentRotation.angle - 1080 - normalizedRotation;

    host.setAttribute("data-context-icon-spin-axis", "y");
    host.setAttribute("data-context-icon-spin-direction", "-1");
    host.style.setProperty(
      "--context-icon-microphone-start",
      "y " + currentRotation.angle + "deg"
    );
    host.style.setProperty(
      "--context-icon-microphone-end",
      "y " + spinEnd + "deg"
    );
    rotationTarget.getBoundingClientRect();
    host.setAttribute(
      "data-context-icon-microphone-spinning",
      "true"
    );

    function resetMicrophoneSpin() {
      host.removeEventListener("animationend", finishMicrophoneSpin);
      host.removeAttribute("data-context-icon-microphone-spinning");
      host._contextIconSpinReset = null;
    }

    function finishMicrophoneSpin(event) {
      if (
        event.target !== rotationTarget ||
        event.animationName !==
          "context-navigation-microphone-reverse-spin"
      ) {
        return;
      }

      resetMicrophoneSpin();
    }

    host._contextIconSpinReset = resetMicrophoneSpin;
    host.addEventListener("animationend", finishMicrophoneSpin);
  }

  function setupNavigationMicrophoneAction(panel) {
    if (
      !panel ||
      panel.getAttribute("data-context-microphone-ready") === "true"
    ) {
      return;
    }

    panel.setAttribute("data-context-microphone-ready", "true");
    panel.addEventListener("contextmenu", function (event) {
      var host;
      var icon;
      var svg =
        event.target &&
        event.target.closest &&
        event.target.closest("svg");
      var target =
        svg &&
        svg.closest(".navigation-menu_menuSubLink__jde_U");

      if (!target || !panel.contains(target)) {
        return;
      }

      icon = svg.closest(".navigation-menu_icon__xsndA");
      host =
        target.querySelector(".navigation-menu_icons__8b2lC") ||
        icon;

      if (!icon || !host) {
        return;
      }

      event.preventDefault();
      event.stopPropagation();

      icon.innerHTML = microphoneSvgMarkup();
      host.setAttribute("data-context-icon-mode", "microphone");
      animateNavigationMicrophone(host);
    });
  }

  function setupGroup(group, targets, options) {
    var activeTarget = null;
    var hoveredTarget = null;
    var previousTarget = null;
    var resizeFrame = null;
    var selectedTarget = null;
    var existingController;

    if (!group || !targets.length) {
      return;
    }

    existingController = group._contextHighlightController;
    if (existingController) {
      if (typeof existingController.updateTargets === "function") {
        existingController.updateTargets(targets);
      } else {
        existingController.refreshSelectedTarget();
      }
      return;
    }

    options = options || {};
    group.setAttribute(groupAttribute, "");

    function findSelectedTarget() {
      return (
        targets.find(function (target) {
          var current = target.getAttribute("aria-current");
          return current === "page" || current === "true";
        }) || null
      );
    }

    selectedTarget = findSelectedTarget();

    function targetFromNode(node) {
      return targets.find(function (target) {
        return isInside(target, node);
      }) || null;
    }

    function focusedTarget() {
      return targetFromNode(document.activeElement);
    }

    function hideHighlight() {
      group.setAttribute(visibleAttribute, "false");
    }

    function transitionToTarget(target, immediate) {
      if (options.animateIcons && previousTarget !== target) {
        animateNavigationOriginalIcon(previousTarget, target);
      }

      targets.forEach(function (candidate) {
        if (candidate !== target) {
          candidate.removeAttribute("data-context-highlight-active");
        }
      });

      activeTarget = target;
      activeTarget.setAttribute("data-context-highlight-active", "true");
      previousTarget = target;
      setHighlight(group, target, Boolean(immediate));
    }

    function restoreSelectedTarget() {
      if (!options.restoreSelected || !selectedTarget) {
        return false;
      }

      transitionToTarget(selectedTarget, false);
      return true;
    }

    function refreshSelectedTarget() {
      var focused = focusedTarget();

      selectedTarget = findSelectedTarget();
      if (hoveredTarget || focused) {
        return;
      }

      if (selectedTarget) {
        transitionToTarget(selectedTarget, true);
        return;
      }

      targets.forEach(function (target) {
        target.removeAttribute("data-context-highlight-active");
      });
      activeTarget = null;
      previousTarget = null;
      hideHighlight();
    }

    function restoreAfterPointerLeave() {
      hoveredTarget = null;

      if (restoreSelectedTarget()) {
        return;
      }

      if (options.persist && activeTarget) {
        setHighlight(group, activeTarget, false);
        return;
      }

      if (options.clearActiveOnLeave && activeTarget) {
        activeTarget.removeAttribute("data-context-highlight-active");
        activeTarget = null;
      }

      previousTarget = null;
      hideHighlight();
    }

    function updateTargets(nextTargets) {
      var next = Array.isArray(nextTargets) ? nextTargets.filter(Boolean) : [];
      if (activeTarget && next.indexOf(activeTarget) === -1) {
        activeTarget.removeAttribute("data-context-highlight-active");
        activeTarget = null;
      }
      if (hoveredTarget && next.indexOf(hoveredTarget) === -1) hoveredTarget = null;
      if (previousTarget && next.indexOf(previousTarget) === -1) previousTarget = null;
      targets = next;
      refreshSelectedTarget();
    }

    function scheduleRefresh() {
      if (resizeFrame) {
        return;
      }

      resizeFrame = window.requestAnimationFrame(function () {
        resizeFrame = null;
        if (
          hoveredTarget ||
          focusedTarget() ||
          (options.restoreSelected && selectedTarget) ||
          (options.persist && activeTarget)
        ) {
          setHighlight(
            group,
            hoveredTarget ||
              focusedTarget() ||
              (options.restoreSelected && selectedTarget) ||
              activeTarget,
            true
          );
        } else {
          hideHighlight();
        }
      });
    }

    group.addEventListener("pointerover", function (event) {
      var target = targetFromNode(event.target);

      if (!target || isInside(target, event.relatedTarget)) {
        return;
      }

      hoveredTarget = target;
      transitionToTarget(target);
    });

    if (options.trackPointerPosition) {
      group.addEventListener(
        "pointermove",
        function (event) {
          var pointNode = document.elementFromPoint(
            event.clientX,
            event.clientY
          );
          var target =
            targetFromNode(pointNode) || targetFromNode(event.target);

          if (!target || target === hoveredTarget) {
            return;
          }

          hoveredTarget = target;
          transitionToTarget(target);
        },
        { passive: true }
      );
    }

    /*
     * Restore the selected page only when the pointer exits the entire list.
     * Adjacent rows have a small visual gap; pointerout on each row would
     * otherwise snap the highlight back to aria-current while crossing it.
     */
    group.addEventListener("pointerleave", restoreAfterPointerLeave);

    group.addEventListener("focusin", function (event) {
      var target = targetFromNode(event.target);

      if (target) {
        transitionToTarget(target);
      }
    });

    group.addEventListener("focusout", function (event) {
      if (!group.contains(event.relatedTarget)) {
        if (restoreSelectedTarget()) {
          return;
        }

        if (options.persist && activeTarget) {
          setHighlight(group, activeTarget, false);
          return;
        }

        if (options.clearActiveOnLeave && activeTarget) {
          activeTarget.removeAttribute("data-context-highlight-active");
          activeTarget = null;
        }

        previousTarget = null;
        hideHighlight();
      }
    });

    group.addEventListener("scroll", scheduleRefresh, { passive: true });
    if (options.trackResize !== false) {
      window.addEventListener("resize", scheduleRefresh);
    }

    group._contextHighlightRefresh = scheduleRefresh;
    group._contextHighlightController = {
      refreshSelectedTarget: refreshSelectedTarget,
      scheduleRefresh: scheduleRefresh,
      updateTargets: updateTargets,
    };

    if (options.initializeFromCurrent && selectedTarget) {
      window.requestAnimationFrame(function () {
        transitionToTarget(selectedTarget, true);
      });
    } else {
      hideHighlight();
    }
  }

  function ensureDocsSidebarActiveVisible(docsSidebar, immediate) {
    var activeItem;
    var activeRect;
    var maxScroll;
    var navRect;
    var nextScroll;

    if (!docsSidebar || !docsSidebar.isConnected) {
      return;
    }

    var activeItems = Array.from(docsSidebar.querySelectorAll(
      'a[aria-current="page"], a[aria-current="true"], button[aria-current="page"], button[aria-current="true"]'
    ));
    activeItem = activeItems.find(function (item) {
      return !item.closest("[inert]") && item.getClientRects().length > 0;
    }) || activeItems[0];
    if (!activeItem) {
      return;
    }

    navRect = docsSidebar.getBoundingClientRect();
    activeRect = activeItem.getBoundingClientRect();

    // Acompanha o item selecionado mantendo-o no centro vertical da sidebar.
    // Assim a barra de rolagem lateral não fica presa no topo ou no rodapé
    // quando o usuário alterna entre itens distantes da documentação.
    nextScroll = docsSidebar.scrollTop +
      (activeRect.top + activeRect.height / 2) -
      (navRect.top + navRect.height / 2);

    maxScroll = Math.max(0, docsSidebar.scrollHeight - docsSidebar.clientHeight);
    nextScroll = Math.max(0, Math.min(maxScroll, nextScroll));

    if (Math.abs(nextScroll - docsSidebar.scrollTop) < 1) return;

    if (typeof docsSidebar.scrollTo === "function") {
      docsSidebar.scrollTo({
        top: nextScroll,
        behavior: immediate ? "auto" : "smooth",
      });
    } else {
      docsSidebar.scrollTop = nextScroll;
    }
  }

  function scheduleDocsSidebarActiveFollow(docsSidebar, immediate) {
    if (!docsSidebar || docsSidebar._docsActiveFollowFrame) {
      return;
    }

    docsSidebar._docsActiveFollowFrame = window.requestAnimationFrame(function () {
      docsSidebar._docsActiveFollowFrame = null;
      ensureDocsSidebarActiveVisible(docsSidebar, immediate);
    });
  }

  function setupDocsSidebarActiveFollow(docsSidebar) {
    if (!docsSidebar || docsSidebar._docsActiveFollowObserver) {
      scheduleDocsSidebarActiveFollow(docsSidebar, true);
      return;
    }

    docsSidebar._docsActiveFollowObserver = new MutationObserver(function (mutations) {
      var changedCurrent = mutations.some(function (mutation) {
        return mutation.type === "attributes" && mutation.attributeName === "aria-current";
      });

      if (changedCurrent) {
        scheduleDocsSidebarActiveFollow(docsSidebar, false);
      }
    });
    docsSidebar._docsActiveFollowObserver.observe(docsSidebar, {
      subtree: true,
      attributes: true,
      attributeFilter: ["aria-current"],
    });

    scheduleDocsSidebarActiveFollow(docsSidebar, true);
  }

  var docsSidebarSkeletonState = {
    primed: false,
    siteReady: false,
    siteReadyAt: 0,
    windowLoaded: document.readyState === "complete",
    releaseTimer: null,
    sidebarObserver: null,
    pageSkeletonObserver: null,
    sidebarHadInert: false,
    headerRightHadInert: false,
  };

  function getDocsSidebar() {
    return document.querySelector(
      'body[data-page="docs"] [data-sidebar-nav="true"]'
    );
  }

  function getDocsHeaderRight() {
    return document.querySelector(
      'body[data-page="docs"] [data-navigation-header] .header_right__RkxkJ'
    );
  }

  function setSkeletonVisible(node, visible) {
    if (!node || !node.setAttribute) return;
    if (visible) {
      node.setAttribute("data-skeleton-visible", "true");
    } else {
      node.removeAttribute("data-skeleton-visible");
    }
  }

  function setupSidebarSkeletonVisibility(docsSidebar) {
    var targets;

    if (!docsSidebar) return;
    if (docsSidebarSkeletonState.sidebarObserver) {
      docsSidebarSkeletonState.sidebarObserver.disconnect();
      docsSidebarSkeletonState.sidebarObserver = null;
    }

    targets = Array.from(
      docsSidebar.querySelectorAll(
        'button:has([data-search-icon="true"]), [data-sidebar-category="true"] > div:first-child, ul a, ul button'
      )
    );

    if (typeof window.IntersectionObserver !== "function") {
      targets.forEach(function (node) { setSkeletonVisible(node, true); });
      return;
    }

    docsSidebarSkeletonState.sidebarObserver = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          setSkeletonVisible(
            entry.target,
            entry.isIntersecting && entry.intersectionRatio > 0
          );
        });
      },
      { root: docsSidebar, rootMargin: "0px", threshold: 0.01 }
    );

    targets.forEach(function (node) {
      docsSidebarSkeletonState.sidebarObserver.observe(node);
    });
  }

  function setupPageSkeletonVisibility() {
    var cards = Array.from(
      document.querySelectorAll(
        'body[data-page="docs"] [data-system-utilities-card-grid="true"] > [data-system-utility-card]'
      )
    );

    if (docsSidebarSkeletonState.pageSkeletonObserver) {
      docsSidebarSkeletonState.pageSkeletonObserver.disconnect();
      docsSidebarSkeletonState.pageSkeletonObserver = null;
    }

    if (!cards.length) return;

    if (typeof window.IntersectionObserver !== "function") {
      cards.forEach(function (card) { setSkeletonVisible(card, true); });
      return;
    }

    docsSidebarSkeletonState.pageSkeletonObserver = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          setSkeletonVisible(
            entry.target,
            entry.isIntersecting && entry.intersectionRatio > 0
          );
        });
      },
      { root: null, rootMargin: "0px", threshold: 0.01 }
    );

    cards.forEach(function (card) {
      docsSidebarSkeletonState.pageSkeletonObserver.observe(card);
    });
  }

  function lockSkeletonInteractions(docsSidebar) {
    var headerRight = getDocsHeaderRight();

    if (docsSidebar) {
      docsSidebarSkeletonState.sidebarHadInert = docsSidebar.hasAttribute("inert");
      if (!docsSidebarSkeletonState.sidebarHadInert) docsSidebar.setAttribute("inert", "");
      docsSidebar.setAttribute("data-skeleton-interaction-lock", "true");
    }

    if (headerRight) {
      docsSidebarSkeletonState.headerRightHadInert = headerRight.hasAttribute("inert");
      if (!docsSidebarSkeletonState.headerRightHadInert) headerRight.setAttribute("inert", "");
      headerRight.setAttribute("data-skeleton-interaction-lock", "true");
    }
  }

  function unlockSkeletonInteractions(docsSidebar) {
    var headerRight = getDocsHeaderRight();

    if (docsSidebar) {
      if (!docsSidebarSkeletonState.sidebarHadInert) docsSidebar.removeAttribute("inert");
      docsSidebar.removeAttribute("data-skeleton-interaction-lock");
    }

    if (headerRight) {
      if (!docsSidebarSkeletonState.headerRightHadInert) headerRight.removeAttribute("inert");
      headerRight.removeAttribute("data-skeleton-interaction-lock");
    }
  }

  function primeDocsSidebarSkeleton() {
    var docsSidebar = getDocsSidebar();

    if (!docsSidebar || docsSidebarSkeletonState.primed) {
      return;
    }

    docsSidebarSkeletonState.primed = true;
    if (document.body) {
      // Um único estado de boot é aplicado no mesmo task ao header, sidebar e
      // conteúdo central; o browser só pode pintar depois que todos estiverem
      // marcados, evitando o header entrar um frame depois do sidebar.
      document.body.setAttribute("data-docs-chrome-loading", "true");
      document.body.setAttribute("data-docs-content-loading", "true");
    }
    docsSidebar.setAttribute("data-sidebar-loading", "true");
    docsSidebar.setAttribute("aria-busy", "true");
    lockSkeletonInteractions(docsSidebar);
    setupSidebarSkeletonVisibility(docsSidebar);
    setupPageSkeletonVisibility();
  }

  function releaseDocsSidebarSkeleton() {
    var docsSidebar = getDocsSidebar();

    if (!docsSidebar) {
      return;
    }

    if (docsSidebarSkeletonState.sidebarObserver) {
      docsSidebarSkeletonState.sidebarObserver.disconnect();
      docsSidebarSkeletonState.sidebarObserver = null;
    }
    Array.from(docsSidebar.querySelectorAll("[data-skeleton-visible]")).forEach(function (node) {
      node.removeAttribute("data-skeleton-visible");
    });

    unlockSkeletonInteractions(docsSidebar);
    docsSidebar.removeAttribute("data-sidebar-loading");
    docsSidebar.removeAttribute("aria-busy");
    docsSidebar.setAttribute("data-sidebar-loaded", "true");
    if (document.body) {
      document.body.removeAttribute("data-docs-chrome-loading");
      document.body.removeAttribute("data-docs-content-loading");
    }
    window.dispatchEvent(new Event("docs:sidebar-ready"));
    window.dispatchEvent(new Event("docs:content-ready"));
  }

  function maybeReleaseDocsSidebarSkeleton() {
    var elapsedSinceSiteReady;
    var delay;

    if (
      !docsSidebarSkeletonState.primed ||
      !docsSidebarSkeletonState.siteReady ||
      !docsSidebarSkeletonState.windowLoaded ||
      docsSidebarSkeletonState.releaseTimer
    ) {
      return;
    }

    elapsedSinceSiteReady = Math.max(
      0,
      performance.now() - docsSidebarSkeletonState.siteReadyAt
    );
    delay = Math.max(0, 220 - elapsedSinceSiteReady);
    docsSidebarSkeletonState.releaseTimer = window.setTimeout(function () {
      docsSidebarSkeletonState.releaseTimer = null;
      window.requestAnimationFrame(function () {
        releaseDocsSidebarSkeleton();
      });
    }, delay);
  }

  function setupDocsSidebarSkeletonLifecycle() {
    primeDocsSidebarSkeleton();

    if (document.documentElement.classList.contains("site-ready")) {
      docsSidebarSkeletonState.siteReady = true;
      docsSidebarSkeletonState.siteReadyAt = performance.now();
    } else {
      window.addEventListener(
        "site:ready",
        function () {
          docsSidebarSkeletonState.siteReady = true;
          docsSidebarSkeletonState.siteReadyAt = performance.now();
          maybeReleaseDocsSidebarSkeleton();
        },
        { once: true }
      );
    }

    if (document.readyState === "complete") {
      docsSidebarSkeletonState.windowLoaded = true;
    } else {
      window.addEventListener(
        "load",
        function () {
          docsSidebarSkeletonState.windowLoaded = true;
          maybeReleaseDocsSidebarSkeleton();
        },
        { once: true }
      );
    }

    maybeReleaseDocsSidebarSkeleton();
  }

  function setupDocsSidebar() {
    var docsSidebar = getDocsSidebar();
    var docsSections;

    if (!docsSidebar) {
      return;
    }

    setupDocsSidebarActiveFollow(docsSidebar);

    if (!docsSidebar.hasAttribute("data-docs-brand-snap-ready")) {
      docsSidebar.setAttribute("data-docs-brand-snap-ready", "true");
      docsSidebar.addEventListener("pointerover", function (event) {
        var target = event.target.closest("a, button");
        var label;
        var previousLabel;

        if (
          !target ||
          !docsSidebar.contains(target) ||
          (event.relatedTarget && target.contains(event.relatedTarget))
        ) {
          return;
        }

        // Itens que abrem um segundo nível e o cabeçalho de retorno precisam
        // permanecer integralmente legíveis; não aplique o scan/fade neles.
        if (
          target.matches('[data-revo-has-submenu="true"]') ||
          target.closest('[data-revo-sidebar-detail="true"] [data-testid="docs-sidebar/section-header"]')
        ) {
          return;
        }

        label = target.querySelector(":scope > span.text-heading-14");
        if (!label) {
          return;
        }

        previousLabel = docsSidebar._docsBrandSnapLabel;
        if (previousLabel && previousLabel !== label) {
          previousLabel.removeAttribute("data-docs-brand-snap");
        }

        docsSidebar._docsBrandSnapLabel = label;
        label.removeAttribute("data-docs-brand-snap");
        void label.offsetWidth;
        label.setAttribute("data-docs-brand-snap", "true");
      });
      docsSidebar.addEventListener("animationend", function (event) {
        if (
          event.animationName === "brand-fade-scan" &&
          event.target.hasAttribute("data-docs-brand-snap")
        ) {
          event.target.removeAttribute("data-docs-brand-snap");
          if (docsSidebar._docsBrandSnapLabel === event.target) {
            docsSidebar._docsBrandSnapLabel = null;
          }
        }
      });
      docsSidebar.addEventListener("animationcancel", function (event) {
        if (event.target.hasAttribute("data-docs-brand-snap")) {
          event.target.removeAttribute("data-docs-brand-snap");
          if (docsSidebar._docsBrandSnapLabel === event.target) {
            docsSidebar._docsBrandSnapLabel = null;
          }
        }
      });
      docsSidebar.addEventListener("pointerleave", function () {
        var label = docsSidebar._docsBrandSnapLabel;
        if (label) {
          label.removeAttribute("data-docs-brand-snap");
          docsSidebar._docsBrandSnapLabel = null;
        }
      });
    }

    docsSections = Array.from(docsSidebar.querySelectorAll("ul")).filter(
      function (section) {
        return section.querySelector("a");
      }
    );
    docsSidebar.setAttribute(
      "data-context-highlight-sections",
      String(docsSections.length)
    );
    docsSections.forEach(function (section) {
      section.setAttribute(
        "data-context-highlight-variant",
        "docs-sidebar"
      );
      setupGroup(
        section,
        Array.from(section.querySelectorAll("a, button")),
        {
          persist: true,
          initializeFromCurrent: true,
          restoreSelected: true,
          trackResize: false,
        }
      );
    });
  }

  document.addEventListener("docs:content-updated", function () {
    // Conteúdo SPA também pode substituir os itens do segundo nível da sidebar.
    // Atualiza a lista de alvos do mesmo controlador para manter o Sliding Highlight
    // funcional em Limpador de Histórico, Desinstalador e outros painéis dinâmicos.
    setupDocsSidebar();
    setupPageSkeletonVisibility();
  });

  function initContextSlidingHighlight() {
    setupDocsSidebar();
  }

  window.initContextSlidingDocsSidebar = setupDocsSidebar;

  setupDocsSidebarSkeletonLifecycle();

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initContextSlidingHighlight);
  } else {
    initContextSlidingHighlight();
  }
})();


/* Original source module: 09-mobile-menu */
(function () {
  var rootSelector = "#mobile-menu-toggle";
  var buttonSelector = ".toggle_mobileMenuToggle__W5y02";
  var menuId = "mobile-menu-content";
  var overlayManager = window.UIOverlayManager;

  function icon(name) {
    var paths = {
      home: '<path fill-rule="evenodd" clip-rule="evenodd" d="M8 1L0 15H16L8 1ZM8 4.02335L2.58477 13.5H13.4152L8 4.02335Z" fill="currentColor"></path>',
      system: '<path fill-rule="evenodd" clip-rule="evenodd" d="M1 3.25C1 1.45507 2.45507 0 4.25 0H11.75C13.5449 0 15 1.45507 15 3.25V15.25V16H14.25H1.75H1V15.25V3.25ZM4.25 1.5C3.2835 1.5 2.5 2.2835 2.5 3.25V14.5H13.5V3.25C13.5 2.2835 12.7165 1.5 11.75 1.5H4.25ZM4 4C4 3.44772 4.44772 3 5 3H11C11.5523 3 12 3.44772 12 4V10H4V4ZM9 13H12V11.5H9V13Z" fill="currentColor"></path>',
      light: '<path fill-rule="evenodd" clip-rule="evenodd" d="M8.75 0.75V0H7.25V0.75V2V2.75H8.75V2V0.75ZM11.182 3.75732L11.7123 3.22699L12.0659 2.87344L12.5962 2.34311L13.6569 3.40377L13.1265 3.9341L12.773 4.28765L12.2426 4.81798L11.182 3.75732ZM8 10.5C9.38071 10.5 10.5 9.38071 10.5 8C10.5 6.61929 9.38071 5.5 8 5.5C6.61929 5.5 5.5 6.61929 5.5 8C5.5 9.38071 6.61929 10.5 8 10.5ZM8 12C10.2091 12 12 10.2091 12 8C12 5.79086 10.2091 4 8 4C5.79086 4 4 5.79086 4 8C4 10.2091 5.79086 12 8 12ZM13.25 7.25H14H15.25H16V8.75H15.25H14H13.25V7.25ZM0.75 7.25H0V8.75H0.75H2H2.75V7.25H2H0.75ZM2.87348 12.0659L2.34315 12.5962L3.40381 13.6569L3.93414 13.1265L4.28769 12.773L4.81802 12.2426L3.75736 11.182L3.22703 11.7123L2.87348 12.0659ZM3.75735 4.81798L3.22702 4.28765L2.87347 3.9341L2.34314 3.40377L3.4038 2.34311L3.93413 2.87344L4.28768 3.22699L4.81802 3.75732L3.75735 4.81798ZM12.0659 13.1265L12.5962 13.6569L13.6569 12.5962L13.1265 12.0659L12.773 11.7123L12.2426 11.182L11.182 12.2426L11.7123 12.773L12.0659 13.1265ZM8.75 13.25V14V15.25V16H7.25V15.25V14V13.25H8.75Z" fill="currentColor"></path>',
      dark: '<path fill-rule="evenodd" clip-rule="evenodd" d="M1.5 8.00005C1.5 5.53089 2.99198 3.40932 5.12349 2.48889C4.88136 3.19858 4.75 3.95936 4.75 4.7501C4.75 8.61609 7.88401 11.7501 11.75 11.7501C11.8995 11.7501 12.048 11.7454 12.1953 11.7361C11.0955 13.1164 9.40047 14.0001 7.5 14.0001C4.18629 14.0001 1.5 11.3138 1.5 8.00005ZM6.41706 0.577759C2.78784 1.1031 0 4.22536 0 8.00005C0 12.1422 3.35786 15.5001 7.5 15.5001C10.5798 15.5001 13.2244 13.6438 14.3792 10.9921L13.4588 9.9797C12.9218 10.155 12.3478 10.2501 11.75 10.2501C8.71243 10.2501 6.25 7.78767 6.25 4.7501C6.25 3.63431 6.58146 2.59823 7.15111 1.73217L6.41706 0.577759ZM13.25 1V1.75V2.75L14.25 2.75H15V4.25H14.25H13.25V5.25V6H11.75V5.25V4.25H10.75L10 4.25V2.75H10.75L11.75 2.75V1.75V1H13.25Z" fill="currentColor"></path>',
    };

    return '<svg data-testid="geist-icon" height="16" stroke-linejoin="round" viewBox="0 0 16 16" width="16" style="color: currentcolor;">' + paths[name] + "</svg>";
  }

  function getButton() {
    var root = document.querySelector(rootSelector);
    return root && root.querySelector(buttonSelector);
  }

  function closestWithClassText(element, text) {
    var current = element;

    while (current && current !== document.body) {
      if (typeof current.className === "string" && current.className.indexOf(text) !== -1) {
        return current;
      }

      current = current.parentElement;
    }

    return null;
  }

  function getHeaderTargets() {
    var logoNavigation = document.querySelector(".header_header__z_e5g > .flex.items-center.gap-8");
    var mobileSearchLauncher = document.querySelector(".header_right__RkxkJ .cmdklaunch_wrapper__JQ6t7");
    var mobileSearch = mobileSearchLauncher && closestWithClassText(mobileSearchLauncher, "min-[1150px]:hidden");

    return [logoNavigation, mobileSearch].filter(Boolean);
  }

  function hideHeaderTargets() {
    getHeaderTargets().forEach(function (element) {
      if (!element.hasAttribute("data-mobile-menu-original-visibility")) {
        element.setAttribute("data-mobile-menu-original-visibility", element.style.visibility || "");
      }

      element.style.visibility = "hidden";
    });
  }

  function restoreHeaderTargets() {
    document.querySelectorAll("[data-mobile-menu-original-visibility]").forEach(function (element) {
      element.style.visibility = element.getAttribute("data-mobile-menu-original-visibility");
      element.removeAttribute("data-mobile-menu-original-visibility");
    });
  }

  function setMobileMenuOpen(isOpen) {
    document.documentElement.toggleAttribute("data-mobile-menu-open", isOpen);
    document.body.toggleAttribute("data-mobile-menu-open", isOpen);
  }

  function getSelectedTheme() {
    try {
      return localStorage.getItem("zeit-theme") || "system";
    } catch (error) {
      return "system";
    }
  }

  function setTheme(theme) {
    if (typeof window.setSiteTheme === "function") {
      window.setSiteTheme(theme, true);
      return;
    }

    var resolvedTheme =
      theme === "system"
        ? (window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark")
        : theme;

    document.documentElement.classList.remove("light-theme", "dark-theme", "light", "dark");
    document.documentElement.classList.add(resolvedTheme === "light" ? "light-theme" : "dark-theme");
    document.documentElement.classList.add(resolvedTheme);
    document.documentElement.style.colorScheme = resolvedTheme;
    try {
      localStorage.setItem("zeit-theme", theme);
    } catch (error) {}
  }

  function closeMenu() {
    var menu = document.getElementById(menuId);
    var closeButton = document.querySelector('[data-mobile-header-menu-close="true"]');
    var button = getButton();

    if (button) {
      button.setAttribute("data-expanded", "false");
      button.setAttribute("aria-expanded", "false");
      button.setAttribute("aria-label", "Abrir menu");
    }

    setMobileMenuOpen(false);
    restoreHeaderTargets();

    if (menu) {
      menu.remove();
    }

    if (closeButton) {
      closeButton.remove();
    }

    return true;
  }

  function createCloseButton(anchorRect) {
    var closeButton = document.createElement("button");

    closeButton.type = "button";
    closeButton.setAttribute("data-mobile-header-menu-close", "true");
    closeButton.setAttribute("aria-label", "Close menu");
    closeButton.setAttribute("data-geist-button", "");
    closeButton.setAttribute("data-prefix", "false");
    closeButton.setAttribute("data-suffix", "false");
    closeButton.setAttribute("data-version", "v1");
    closeButton.className =
      "outline-none m-0 border-0 align-baseline no-underline group/trigger relative cursor-pointer select-none flex items-center justify-center rounded-full";
    closeButton.style.setProperty("--geist-icon-size", "16px");

    if (anchorRect) {
      var closeButtonSize = 32;
      var left = Math.max(0, Math.min(anchorRect.left, window.innerWidth - closeButtonSize));
      var top = Math.max(0, anchorRect.top);

      closeButton.style.setProperty("top", Math.round(top) + "px", "important");
      closeButton.style.setProperty("right", "auto", "important");
      closeButton.style.setProperty("left", Math.round(left) + "px", "important");
    }

    closeButton.innerHTML =
      '<span data-mobile-header-menu-close-icon class="shrink-0 flex items-center justify-center">' +
      '<svg data-testid="geist-icon" height="16" stroke-linejoin="round" style="color: currentcolor;" viewBox="0 0 16 16" width="16">' +
      '<path clip-rule="evenodd" d="M12.4697 13.5303L13 14.0607L14.0607 13L13.5303 12.4697L9.06065 7.99999L13.5303 3.53032L14.0607 2.99999L13 1.93933L12.4697 2.46966L7.99999 6.93933L3.53032 2.46966L2.99999 1.93933L1.93933 2.99999L2.46966 3.53032L6.93933 7.99999L2.46966 12.4697L1.93933 13L2.99999 14.0607L3.53032 13.5303L7.99999 9.06065L12.4697 13.5303Z" fill="currentColor" fill-rule="evenodd"></path>' +
      "</svg></span>";
    closeButton.addEventListener("click", function () {
      overlayManager.close("mobile-menu", { source: "close-button", restoreFocus: true });
    });

    return closeButton;
  }

  function openMenu() {
    var button = getButton();
    var buttonRect = button && button.getBoundingClientRect();
    var menu = document.createElement("div");

    closeMenu();

    if (button) {
      button.setAttribute("data-expanded", "true");
      button.setAttribute("aria-expanded", "true");
      button.setAttribute("aria-label", "Fechar menu");
    }

    menu.id = menuId;
    menu.className = "popover_scrollWrapper__HNGBM";
    menu.style.setProperty("--background-color", "var(--ds-background-100)");
    menu.innerHTML =
      '<nav class="popover_wrapper__Rxdol">' +
      '<section><ul>' +
      '<li><a href="/docs" data-zone="667155" class="link_link__hbWKh no-underline popover_navigationListItemLink__mO5gr" data-prefetch="true">Documentação</a></li>' +
      '</ul></section>' +
      '<section><ul>' +
      '<li class="popover_themeSwitcher__HWbbC"><p>Tema</p><fieldset class="tailwind theme-switcher_root__DrzBE popover_themeSwitcherComponent__KfdcZ"><legend class="sr-only">Selecione um tema de exibição:</legend>' +
      '<span style="height: 100%;"><input aria-label="sistema" type="radio" value="system" id="theme-switch-system-mobile"><label for="theme-switch-system-mobile"><span class="sr-only">sistema</span>' + icon("system") + "</label></span>" +
      '<span style="height: 100%;"><input aria-label="claro" type="radio" value="light" id="theme-switch-light-mobile"><label for="theme-switch-light-mobile"><span class="sr-only">claro</span>' + icon("light") + "</label></span>" +
      '<span style="height: 100%;"><input aria-label="escuro" type="radio" value="dark" id="theme-switch-dark-mobile"><label for="theme-switch-dark-mobile"><span class="sr-only">escuro</span>' + icon("dark") + "</label></span>" +
      "</fieldset></li>" +
      "</ul></section></nav>";

    menu.addEventListener("change", function (event) {
      if (event.target.matches('input[name="mobile-theme"], input[type="radio"][value]')) {
        var value = event.target.value;
        setTheme(value);
      }
    });

    var selectedTheme = getSelectedTheme();

    menu.querySelectorAll('input[type="radio"]').forEach(function (input) {
      input.setAttribute("name", "mobile-theme");
      input.checked = input.value === selectedTheme;
    });

    document.body.appendChild(menu);
    var closeButton = createCloseButton(buttonRect);
    document.body.appendChild(closeButton);
    setMobileMenuOpen(true);
    hideHeaderTargets();
    closeButton.focus();
    return true;
  }

  function toggleMenu(event) {
    event.preventDefault();
    overlayManager.toggle("mobile-menu", { source: "launcher", opener: event.currentTarget || getButton() });
  }

  overlayManager.register("mobile-menu", {
    group: "modal",
    blocksScroll: true,
    dismissOnEscape: true,
    closeGroupsOnOpen: ["header"],
    open: openMenu,
    close: closeMenu,
  });

  function init() {
    var button = getButton();

    if (!button) {
      return;
    }

    button.addEventListener("click", toggleMenu);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();


/* Original source module: 10-profile-menu */
(function () {
  "use strict";

  var buttonSelector = '[data-geist-menu-button][aria-label="Opções da conta"]';
  var portalId = "profile-menu-portal";
  var menuId = "profile-menu";
  var adminMenuId = "avatar-popover-menu";
  var currentSession = { authenticated: false, role: "guest", user: null };
  var overlayManager = window.UIOverlayManager;

  function getButton() {
    return document.querySelector(buttonSelector);
  }

  function escapeHtml(value) {
    return String(value == null ? "" : value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function refreshSession() { return Promise.resolve(currentSession); }

  function closeMenu() {
    var portal = document.getElementById(portalId);
    var adminMenu = document.getElementById(adminMenuId);
    var button = getButton();
    if (button) {
      button.setAttribute("aria-expanded", "false");
      button.removeAttribute("data-is-open");
    }
    if (adminMenu && typeof adminMenu.hidePopover === "function") {
      try { adminMenu.hidePopover(); } catch (error) {}
    }
    if (portal) portal.remove();
    return true;
  }

  function createMenuLink(href, id, text, action) {
    return (
      '<li data-geist-menu-item="" data-geist-menu-link="" role="none">' +
      '<a href="' + href + '" aria-labelledby="' + id + '" data-geist-menu-item="" role="menuitem" tabindex="-1" data-zone="2d9dc9" class="link_link__hbWKh no-underline outline-none cursor-pointer flex items-center px-2 h-10 rounded-[6px] text-[var(--ds-gray-1000)] w-full data-selected:bg-[var(--ds-gray-alpha-100)] data-highlighted:bg-[var(--ds-gray-alpha-100)] aria-disabled:cursor-default aria-disabled:text-[var(--ds-gray-700)] aria-disabled:pointer-none disabled:cursor-default disabled:text-[var(--ds-gray-700)] disabled:pointer-none max-sm:h-12 max-sm:text-base" style="--geist-icon-size: 18px;" data-prefetch="true"' +
      (action ? ' data-profile-action="' + action + '"' : "") + '>' +
      '<span id="' + id + '">' + text + "</span>" +
      "</a></li>"
    );
  }

  function simpleMenuMarkup(button) {
    var items = "";
    if (currentSession.authenticated) {
      items += createMenuLink("#", "profile-menu-logout", "Sair", "logout");
    } else {
      items += createMenuLink("/login", "profile-menu-login", "Entrar");
      items += createMenuLink("/signup", "profile-menu-signup", "Cadastrar-se");
    }
    return '<ul aria-labelledby="' + button.id + '" id="' + menuId + '" class="data-[state=\'closed\']:animate-fade-popover-out m-0 text-sm p-2 overscroll-contain shadow-[var(--ds-shadow-menu)] rounded-[12px] outline-none bg-[var(--ds-background-100)] overflow-x-hidden overflow-y-auto [-webkit-overflow-scrolling:_touch] max-sm:rounded-b-none max-sm:![width:_unset]" data-geist-menu="" data-version="v1" role="menu" tabindex="-1" style="width: 190px;">' + items + "</ul>";
  }

  function systemThemeSvg() {
    return '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" style="color:currentColor"><path fill="currentColor" fill-rule="evenodd" d="M2.5 5.25C2.5 3.45 3.96 2 5.75 2h4.5c1.8 0 3.25 1.46 3.25 3.25V14h-11V5.25M5.75 3.5C4.78 3.5 4 4.28 4 5.25v7.25h8V5.25c0-.97-.78-1.75-1.75-1.75zM5 5.5a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1V9H5zm3.5 6H11V10H8.5z" clip-rule="evenodd"></path></svg>';
  }

  function lightThemeSvg() {
    return '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" style="color:currentColor"><path fill="currentColor" fill-rule="evenodd" d="M8.75 2v-.75h-1.5V3h1.5V2M8 10a2 2 0 1 0 0-4 2 2 0 0 0 0 4m0 1.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7m.75 1.5v1.75h-1.5V13zM13 7.25h1.75v1.5H13zm-11 0h-.75v1.5H3v-1.5H2m9-3.32.54-.53.17-.17.53-.53 1.06 1.06-.53.53-.17.17-.53.53zm-7.77 7.78-.53.53 1.06 1.06.53-.53.17-.17.53-.53L3.93 11l-.53.53zM3.93 5l-.53-.53-.17-.17-.53-.53L3.76 2.7l.53.53.17.17.53.53zm7.78 7.78.53.53 1.06-1.06-.53-.53-.17-.17-.53-.53L11 12.07l.53.53z" clip-rule="evenodd"></path></svg>';
  }

  function darkThemeSvg() {
    return '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" style="color:currentColor"><path fill="currentColor" d="m6.3 3.3.7.25A4.25 4.25 0 0 0 12.45 9l.96.96-.08.2A5.75 5.75 0 1 1 6.04 2.6zM5.25 4.76a4.24 4.24 0 1 0 6 5.99H11a5.75 5.75 0 0 1-5.75-6M12.5 3.5h1.25V5H12.5v1.25H11V5H9.75V3.5H11V2.25h1.5zM7 3.55l-.7-.25-.26-.7z"></path></svg>';
  }

  function createTeamSvg() {
    return '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" data-glyph="circular" style="color:currentColor"><path fill="currentColor" fill-rule="evenodd" d="M14.5 8a6.5 6.5 0 1 1-13 0 6.5 6.5 0 0 1 13 0M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0M8.75 4.25v3h3v1.5h-3v3h-1.5v-3h-3v-1.5h3v-3z" clip-rule="evenodd"></path></svg>';
  }

  function logoutSvg() {
    return '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" style="color:currentColor"><path fill="currentColor" fill-rule="evenodd" d="M2.5 13.5h4.25V15H2a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1h4.75v1.5H2.5zm9.94-6.25-1.97-1.97-.53-.53L11 3.69l.53.53 3.07 3.07a1 1 0 0 1 0 1.42l-3.07 3.07-.53.53-1.06-1.06.53-.53 1.97-1.97H5v-1.5h7.44" clip-rule="evenodd"></path></svg>';
  }

  function themeControl(value, label, svg, checked) {
    var id = "theme-switch-" + value + "-profile";
    return '<span class="h-full"><input aria-label="' + label + '" class="appearance-none p-0 m-0 outline-none absolute peer" id="' + id + '" type="radio" value="' + value + '"' + (checked ? ' checked=""' : "") + '><label class="group rounded-full flex items-center justify-center bg-none size-8 m-0 relative data-[small]:size-6 cursor-pointer hover:text-[var(--ds-gray-1000)] text-[var(--ds-gray-700)] peer-checked:shadow-[0_0_0_1px_var(--ds-gray-400),0px_1px_2px_0px_var(--ds-gray-alpha-100)] peer-checked:text-[var(--ds-gray-1000)] peer-checked:bg-[var(--ds-background-100)] peer-checked:[&_svg]:!text-[var(--accents-8)] peer-focus-visible:shadow-[var(--ds-focus-ring)] peer-focus-visible:text-[var(--ds-gray-1000)] peer-focus-visible:[&_svg]:!text-[var(--accents-8)]" data-small="" for="' + id + '"><span class="sr-only">' + label + '</span><span class="relative z-[1] size-4">' + svg + "</span></label></span>";
  }

  function savedTheme() {
    try {
      var value = localStorage.getItem("zeit-theme") || "system";
      return value === "light" || value === "dark" ? value : "system";
    } catch (error) {
      return "system";
    }
  }

  function adminMenuMarkup() {
    var user = currentSession.user || {};
    var name = escapeHtml(user.displayName || user.name || "Administrador");
    var email = escapeHtml(user.email || "");
    var theme = savedTheme();
    var itemClass = "cursor-pointer focus-visible:outline-2 outline-[var(--ds-focus-color)] outline-offset-4 flex w-full items-center justify-between h-10 px-2 text-sm rounded-md text-left no-underline text-gray-900! hover:text-gray-1000! hover:bg-gray-alpha-100 focus-visible:text-gray-1000! focus-visible:bg-gray-alpha-100 outline-none focus-visible:outline-none";

    return '' +
      '<div id="' + adminMenuId + '" popover="manual" role="menu" tabindex="-1" class="w-64 m-0 p-2 rounded-xl bg-background-100 outline-none shadow-[var(--ds-shadow-menu)] text-[14px] !text-gray-900 z-[calc(var(--header-zindex,100)+2)] opacity-0 open:opacity-100 starting:open:opacity-0 transition-opacity open:duration-100" data-cdp-scope="{&quot;name&quot;:&quot;avatar_menu&quot;}">' +
        '<div class="geist-sr-only">O nome da sua conta é ' + name + ' e seu e-mail é ' + email + '</div>' +
        '<div aria-hidden="true" class="flex flex-col gap-1 px-2 pt-3 pb-2"><p class="m-0 text-[14px] font-medium !text-gray-1000">' + name + '</p><p class="m-0 text-[14px] text-gray-900 truncate">' + email + '</p></div>' +
        '<a href="/admin" role="menuitem" data-testid="avatar-popover/dashboard" data-cdp-track="{&quot;event&quot;:&quot;avatar_menu_item_clicked&quot;}" data-zone="2d9dc9" class="' + itemClass + '" data-prefetch="true">Painel</a>' +
        '<a href="/admin" role="menuitem" data-testid="avatar-popover/settings" data-profile-action="account-settings" data-cdp-track="{&quot;event&quot;:&quot;avatar_menu_item_clicked&quot;}" data-zone="2d9dc9" class="' + itemClass + '" data-prefetch="true">Configurações da conta</a>' +
        '<a href="/admin" role="menuitem" data-testid="avatar-popover/new-team" data-profile-action="create-team" data-cdp-track="{&quot;event&quot;:&quot;avatar_menu_item_clicked&quot;}" data-zone="2d9dc9" class="' + itemClass + '" data-prefetch="true"><span class="flex-1">Criar equipe</span>' + createTeamSvg() + '</a>' +
        '<hr class="h-px bg-gray-alpha-400 my-2 -mx-2 border-none">' +
        '<div class="flex flex-row items-center justify-between h-10 px-2 text-gray-900" role="none" data-testid="avatar-popover/theme"><span>Tema</span><fieldset class="isolate flex shadow-[var(--ds-shadow-border)] rounded-full h-8 w-fit p-0 border-0 m-0 data-[small]:h-6 theme-switcher_root__DrzBE" data-small=""><legend class="sr-only">Selecione um tema de exibição:</legend>' +
          themeControl("system", "sistema", systemThemeSvg(), theme === "system") +
          themeControl("light", "claro", lightThemeSvg(), theme === "light") +
          themeControl("dark", "escuro", darkThemeSvg(), theme === "dark") +
        '</fieldset></div>' +
        '<hr class="h-px bg-gray-alpha-400 my-2 -mx-2 border-none">' +
        '<button type="button" role="menuitem" class="flex w-full items-center justify-between h-10 px-2 text-sm rounded-md text-left bg-transparent text-gray-900! cursor-pointer hover:text-gray-1000! hover:bg-gray-alpha-100 focus-visible:text-gray-1000! focus-visible:bg-gray-alpha-100 outline-none focus-visible:outline-none" data-testid="avatar-popover/logout" data-profile-action="logout" data-cdp-track="{&quot;event&quot;:&quot;avatar_menu_item_clicked&quot;}"><span class="flex-1">Sair</span>' + logoutSvg() + '</button>' +
        '<hr class="h-px bg-gray-alpha-400 my-2 -mx-2 border-none">' +
        '<div class="mt-3 mb-1.5"><a href="/admin" role="menuitem" data-testid="avatar-popover/upgrade-to-pro" data-profile-action="upgrade" data-cdp-track="{&quot;event&quot;:&quot;avatar_menu_item_clicked&quot;}" data-zone="2d9dc9" class="avatar-popover_upgradeButton__sbONt cursor-pointer focus-visible:outline-2 outline-[var(--ds-focus-color)] outline-offset-4 flex w-full items-center justify-center h-8 text-sm px-3 rounded-md font-medium bg-gray-1000 !text-background-100 [&_*]:!text-background-100 outline-none focus-visible:outline-none no-underline" data-prefetch="true">Fazer upgrade para Pro</a></div>' +
      '</div>';
  }

  function selectMenuItem(menu, item) {
    Array.from(menu.querySelectorAll("[role='menuitem']")).forEach(function (menuItem) {
      if (menuItem === item) menuItem.setAttribute("data-selected", "");
      else menuItem.removeAttribute("data-selected");
    });
  }

  function bindMenuHover(menu) {
    var items = Array.from(menu.querySelectorAll("[role='menuitem']"));
    if (items[0]) selectMenuItem(menu, items[0]);
    items.forEach(function (item) {
      item.addEventListener("mouseenter", function () { selectMenuItem(menu, item); });
    });
  }

  function bindMenuActions(menu) {
    var logout = menu.querySelector('[data-profile-action="logout"]');
    if (logout) {
      logout.addEventListener("click", function (event) {
        event.preventDefault();
        Promise.reject(new Error("Sem autentica??o neste site p?blico"))
          .catch(function () {})
          .finally(function () { window.location.href = "/"; });
      });
    }
  }

  function positionMenu(button, menuElement, width) {
    var rect = button.getBoundingClientRect();
    var menuWidth = width || 190;
    menuElement.style.position = "fixed";
    menuElement.style.top = Math.round(rect.bottom + 8) + "px";
    menuElement.style.left = Math.round(Math.max(8, rect.right - menuWidth)) + "px";
    menuElement.style.width = menuWidth + "px";
    menuElement.style.margin = "0";
    menuElement.style.transformOrigin = "top right";
  }

  function openAdminMenu(button, portal) {
    portal.innerHTML = adminMenuMarkup();
    var menu = portal.querySelector("#" + adminMenuId);
    positionMenu(button, menu, 256);
    bindMenuHover(menu);
    bindMenuActions(menu);
    if (typeof menu.showPopover === "function") {
      try { menu.showPopover(); } catch (error) {}
    }
    menu.focus();
    return true;
  }

  function openSimpleMenu(button, portal) {
    var wrapper = document.createElement("div");
    wrapper.className = "z-[2001] [--border-color:var(--accents-2)]";
    wrapper.setAttribute("data-popper-reference-hidden", "false");
    wrapper.setAttribute("data-popper-escaped", "false");
    wrapper.setAttribute("data-popper-placement", "bottom-end");
    wrapper.innerHTML = simpleMenuMarkup(button);
    portal.appendChild(wrapper);
    positionMenu(button, wrapper, 190);
    var menu = wrapper.querySelector("ul");
    bindMenuHover(menu);
    bindMenuActions(menu);
    menu.focus();
    return true;
  }

  function openMenu() {
    var button = getButton();
    var portal = document.createElement("div");
    if (!button) return false;
    closeMenu();
    button.setAttribute("aria-expanded", "true");
    button.setAttribute("data-is-open", "true");
    portal.id = portalId;
    document.body.appendChild(portal);

    if (currentSession.authenticated && currentSession.role === "administrador") {
      return openAdminMenu(button, portal);
    }
    return openSimpleMenu(button, portal);
  }

  function toggleMenu(event) {
    var wasOpen = overlayManager.isActive("profile");

    event.preventDefault();
    event.stopPropagation();
    overlayManager.toggle("profile", { source: "launcher", opener: event.currentTarget || getButton() });

    if (wasOpen) return;

    refreshSession().then(function () {
      if (overlayManager.isActive("profile")) {
        overlayManager.open("profile", { source: "session-refresh" });
      }
    });
  }

  overlayManager.register("profile", {
    group: "header",
    blocksScroll: false,
    dismissOnEscape: true,
    dismissOnOutsidePointer: true,
    dismissOnScroll: true,
    dismissOnResize: true,
    closeGroupsOnOpen: ["context-menu"],
    getBoundary: function () {
      return document.getElementById(adminMenuId) || document.getElementById(portalId);
    },
    getTrigger: getButton,
    open: openMenu,
    close: closeMenu,
  });

  function installAdminPopoverStyle() {
    if (document.getElementById("avatar-popover-runtime-style")) return;
    var style = document.createElement("style");
    style.id = "avatar-popover-runtime-style";
    style.textContent =
      "#avatar-popover-menu{box-sizing:border-box;opacity:1!important;}" +
      "#avatar-popover-menu:popover-open{opacity:1!important;}" +
      "#avatar-popover-menu [role=menuitem][data-selected]:not(.avatar-popover_upgradeButton__sbONt){background:var(--ds-gray-alpha-100);color:var(--ds-gray-1000)!important;}" +
      "#avatar-popover-menu button{font:inherit;}";
    document.head.appendChild(style);
  }

  function init() {
    var button = getButton();
    if (!button) return;
    installAdminPopoverStyle();
    refreshSession();
    button.addEventListener("click", toggleMenu);
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init, { once: true });
  else init();
})();


/* Original source module: 11-ask-ai */
(function () {
  var openWidthClass = "w-[360px]";
  var closedWidthClass = "w-0";
  var apiUrl = ""; // Static public website: no API endpoint.
  var messages = [];
  var isLoading = false;
  var isWaitingForResponse = false;
  var abortController = null;
  var activeRequestId = 0;
  var loadingFrameIndex = 0;
  var loadingStartedAt = 0;
  var loadingTimer = null;
  var responseDelayTimer = null;
  var typingTimer = null;
  var typingLastFrameAt = 0;
  var typingFrameIntervalMs = 16;
  var loadingFrames = ["⠹", "⠼", "⠴", "⠦", "⠧", "⠇"];
  var minimumLoadingMs = 650;
  var overlayManager = window.UIOverlayManager;

  function normalize(text) {
    return (text || "").replace(/\s+/g, " ").trim();
  }

  function stripThinking(text) {
    return String(text || "")
      .replace(/<think[\s\S]*?<\/think>/gi, "")
      .replace(/<think[\s\S]*$/gi, "")
      .trim();
  }

  function stripEmojis(text) {
    return String(text || "").replace(/[\u203c-\u3299]|\p{Extended_Pictographic}/gu, "").trim();
  }

  function cleanAssistantContent(text) {
    return stripEmojis(stripThinking(text));
  }

  function escapeHtml(value) {
    return String(value || "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function renderInline(text) {
    var fragments = [];
    var html = escapeHtml(text)
      .replace(/&amp;lt;br\s*\/?&amp;gt;/gi, "<br>")
      .replace(/&lt;br\s*\/?&gt;/gi, "<br>");

    function preserve(fragment) {
      var token = "ASKAIFRAGMENT" + fragments.length + "END";
      fragments.push(fragment);
      return token;
    }

    html = html.replace(/!\[([^\]]*)\]\((https?:\/\/[^)\s]+|\/[^)\s]+)\)/g, function (_, alt, src) {
      return preserve(
        '<img class="ask-ai-course-image" src="' +
          escapeHtml(src) +
          '" alt="' +
          escapeHtml(alt) +
          '" loading="lazy">'
      );
    });

    html = html.replace(/\[([^\]]+)\]\((https?:\/\/[^)\s]+|\/[^)\s]+)\)/g, function (_, label, href) {
      return preserve(
        '<a class="link_link__hbWKh no-underline link_blend__JjXMT ask-ai-course-link" href="' +
          escapeHtml(href) +
          '">' +
          label +
          "</a>"
      );
    });

    html = html
      .replace(/(^|[\s(>])((?:\/academy\/)[a-z0-9][a-z0-9/_-]*)/gi, function (_, prefix, href) {
        return (
          prefix +
          preserve(
            '<a class="link_link__hbWKh no-underline link_blend__JjXMT ask-ai-course-link" href="' +
              escapeHtml(href) +
              '">' +
              escapeHtml(href) +
              "</a>"
          )
        );
      })
      .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");

    fragments.forEach(function (fragment, index) {
      html = html.replace("ASKAIFRAGMENT" + index + "END", fragment);
    });

    return html;
  }

  function renderMarkdown(text) {
    var lines = String(text || "").split(/\r?\n/);
    var html = [];
    var inList = false;

    function closeList() {
      if (inList) {
        html.push("</ul>");
        inList = false;
      }
    }

    lines.forEach(function (line) {
      var trimmed = line.trim();

      if (!trimmed) {
        closeList();
        return;
      }

      if (/^#{2,3}\s+/.test(trimmed)) {
        closeList();
        html.push('<h3 class="font-semibold text-lg">' + renderInline(trimmed.replace(/^#{2,3}\s+/, "")) + "</h3>");
        return;
      }

      if (/^[-*]\s+/.test(trimmed)) {
        if (!inList) {
          html.push('<ul class="list-disc list-inside pl-2">');
          inList = true;
        }

        html.push('<li class="my-1">' + renderInline(trimmed.replace(/^[-*]\s+/, "")) + "</li>");
        return;
      }

      if (/^\d+\.\s+/.test(trimmed)) {
        if (!inList) {
          html.push('<ul class="list-disc list-inside pl-2">');
          inList = true;
        }

        html.push('<li class="my-1">' + renderInline(trimmed.replace(/^\d+\.\s+/, "")) + "</li>");
        return;
      }

      closeList();
      html.push('<p class="leading-6">' + renderInline(trimmed) + "</p>");
    });

    closeList();
    return '<div class="space-y-4">' + html.join("") + "</div>";
  }

  function getChatPanel() {
    return Array.from(document.querySelectorAll("aside")).find(function (aside) {
      var title = aside.querySelector("h2");
      return title && title.textContent.trim() === "Pergunte à IA";
    });
  }

  function getAskButtons() {
    return Array.from(document.querySelectorAll('button[aria-label="Pergunte à IA"]'));
  }

  function getLogContent(panel) {
    var log = panel.querySelector('[role="log"]');
    return log && log.querySelector(".p-4");
  }

  function getInput(panel) {
    return panel.querySelector('textarea[name="message"]');
  }

  function getInputBlock(panel) {
    var input = getInput(panel);
    return input && input.closest(".p-4");
  }

  function getBottomArea(panel) {
    var inputBlock = getInputBlock(panel);
    return inputBlock && inputBlock.parentElement;
  }

  function getSuggestionsArea(panel) {
    var bottomArea = getBottomArea(panel);
    return bottomArea && bottomArea.firstElementChild;
  }

  function getTip(panel) {
    var suggestionsArea = getSuggestionsArea(panel);
    return suggestionsArea && suggestionsArea.querySelector("p");
  }

  function getForm(panel) {
    var input = getInput(panel);
    return input && input.closest("form");
  }

  function getSubmitButton(panel) {
    var form = getForm(panel);
    return form && form.querySelector("button");
  }

  function getClearButton(panel) {
    return panel.querySelector('button[aria-label="Clear chat"]');
  }

  function getCopyButtons(panel) {
    return Array.from(panel.querySelectorAll('button[aria-label="Copy chat as markdown"]'));
  }

  function getCourses() {
    var seen = {};
    return Array.from(document.querySelectorAll('main a[href^="/academy/"]'))
      .map(function (link) {
        var title = normalize(link.querySelector("h3") && link.querySelector("h3").textContent);
        var description = normalize(link.querySelector("p") && link.querySelector("p").textContent);
        var imageElement = link.querySelector("img");
        var image = imageElement && imageElement.getAttribute("src");
        var href = link.getAttribute("href");

        if (!title || !description || !href || seen[href]) {
          return null;
        }

        seen[href] = true;
        return {
          title: title,
          description: description,
          href: href,
          image: image || "/src/img/card-og/og.png",
        };
      })
      .filter(Boolean);
  }

  function setButtonsExpanded(isOpen) {
    getAskButtons().forEach(function (button) {
      button.setAttribute("aria-expanded", isOpen ? "true" : "false");
    });
  }

  function setHeaderLogoCompact(isCompact) {
    var logoText = document.querySelector("[data-logo-text]");

    if (logoText) {
      logoText.toggleAttribute("hidden", isCompact);
    }
  }

  function setActionButtonEnabled(button, enabled) {
    if (!button) {
      return;
    }

    button.disabled = !enabled;
    button.toggleAttribute("disabled", !enabled);
    button.classList.toggle("opacity-50", !enabled);
    button.classList.toggle("shadow-none", !enabled);
  }

  function applyPanelLayout(panel) {
    var shell = panel.firstElementChild && panel.firstElementChild.firstElementChild;
    var log = panel.querySelector('[role="log"]');
    var logViewport = panel.querySelector('[role="log"] > div');
    var bottomArea = getBottomArea(panel);
    var suggestionsArea = getSuggestionsArea(panel);
    var suggestionsViewport = suggestionsArea && suggestionsArea.querySelector("[data-radix-scroll-area-viewport]");
    var suggestionsRoot = suggestionsViewport && suggestionsViewport.parentElement;

    if (shell) {
      shell.style.display = "flex";
      shell.style.flexDirection = "column";
      shell.style.height = "100vh";
      shell.style.minHeight = "0";
    }

    if (log) {
      log.style.flex = "1 1 auto";
      log.style.minHeight = "0";
      log.style.whiteSpace = "normal";
    }

    if (logViewport) {
      /* Keep the text column width stable before and after the scrollbar appears. */
      logViewport.style.overflowY = "scroll";
      logViewport.style.scrollbarGutter = "stable";
      logViewport.style.overflowAnchor = "none";
    }

    if (bottomArea) {
      bottomArea.style.flex = "0 0 auto";
    }

    if (suggestionsArea) {
      suggestionsArea.style.flex = "0 0 auto";
    }

    if (suggestionsRoot) {
      suggestionsRoot.style.height = "auto";
      suggestionsRoot.style.overflow = "visible";
    }

    if (suggestionsViewport) {
      suggestionsViewport.style.height = "auto";
      suggestionsViewport.style.overflow = "visible";
    }
  }

  function isPanelOpen(panel) {
    return panel.classList.contains(openWidthClass);
  }

  function updateSubmitButton(panel) {
    var input = getInput(panel);
    var button = getSubmitButton(panel);
    var hasText = normalize(input && input.value).length > 0;

    if (!button) {
      return;
    }

    if (isLoading) {
      button.disabled = false;
      button.removeAttribute("disabled");
      button.setAttribute("aria-label", "Parar geração");
      button.setAttribute("data-ai-stop", "true");
      button.setAttribute("data-prefix", "true");
      button.classList.remove("w-8", "p-0");
      button.innerHTML =
        '<span class="mr-0.5 min-w-5 shrink-0 flex items-center justify-center">' +
        '<svg class="h-4 w-4" data-testid="geist-icon" height="16" stroke-linejoin="round" viewBox="0 0 16 16" width="16" style="color: currentcolor;">' +
        '<path fill-rule="evenodd" clip-rule="evenodd" d="M14.5 8C14.5 11.5899 11.5899 14.5 8 14.5C4.41015 14.5 1.5 11.5899 1.5 8C1.5 4.41015 4.41015 1.5 8 1.5C11.5899 1.5 14.5 4.41015 14.5 8ZM16 8C16 12.4183 12.4183 16 8 16C3.58172 16 0 12.4183 0 8C0 3.58172 3.58172 0 8 0C12.4183 0 16 3.58172 16 8ZM10.5 5.5H5.5V10.5H10.5V5.5Z" fill="currentColor"></path>' +
        "</svg></span><span class=\"whitespace-nowrap inline-block px-1.5\">Parar</span>";
      return;
    }

    button.removeAttribute("data-ai-stop");
    button.setAttribute("aria-label", "Submit");
    button.setAttribute("data-prefix", "false");
    button.classList.add("h-8", "w-8", "p-0");
    button.disabled = !hasText;
    button.toggleAttribute("disabled", !hasText);
    button.innerHTML =
      '<span class="truncate px-1.5 inline-flex items-center justify-center shrink-0">' +
      '<svg class="h-4 w-4" data-testid="geist-icon" height="16" stroke-linejoin="round" viewBox="0 0 16 16" width="16" style="color: currentcolor;">' +
      '<path fill-rule="evenodd" clip-rule="evenodd" d="M8.70711 1.39644C8.31659 1.00592 7.68342 1.00592 7.2929 1.39644L2.21968 6.46966L1.68935 6.99999L2.75001 8.06065L3.28034 7.53032L7.25001 3.56065V14.25V15H8.75001V14.25V3.56065L12.7197 7.53032L13.25 8.06065L14.3107 6.99999L13.7803 6.46966L8.70711 1.39644Z" fill="currentColor"></path>' +
      "</svg></span>";
  }

  function updateCounter(panel) {
    var input = getInput(panel);
    var counter = panel.querySelector('[aria-live="polite"]');
    var length = (input && input.value ? input.value : "").length;

    if (input && length > 1000) {
      input.value = input.value.slice(0, 1000);
      length = 1000;
    }

    if (counter) {
      counter.textContent = length + "/1000";
    }
  }

  function updateMode(panel) {
    var hasMessages = messages.length > 0 || isLoading;
    var suggestionsArea = getSuggestionsArea(panel);
    var tip = getTip(panel);

    if (suggestionsArea) {
      suggestionsArea.style.display = hasMessages ? "none" : "";
    }

    if (tip) {
      tip.style.display = hasMessages ? "none" : "";
    }

    setActionButtonEnabled(getClearButton(panel), hasMessages);
    getCopyButtons(panel).forEach(function (button) {
      setActionButtonEnabled(button, messages.length > 0);
    });
    updateCounter(panel);
    updateSubmitButton(panel);
  }

  function renderUserMessage(content) {
    return (
      '<div class="group flex w-full items-end justify-end gap-4 py-4 is-user [&amp;&gt;div]:max-w-[80%] mt-4">' +
      '<div class="relative flex flex-col gap-2 rounded-[12px] px-4 py-2 text-sm group-[.is-user]:bg-gray-1000 group-[.is-user]:dark:bg-gray-100 group-[.is-user]:text-white group-[.is-user]:dark:text-gray-1000">' +
      '<div class="is-user:dark"><div class="inline"><div class="size-full text-wrap inline"><div class="space-y-4">' +
      '<p class="leading-6">' + escapeHtml(content) + "</p>" +
      "</div></div></div></div>" +
      '<svg width="18" height="14" viewBox="0 0 18 14" class="absolute -bottom-[0.5px] right-[2.5px] translate-x-1/2 group-[.is-user]:fill-gray-1000 group-[.is-user]:dark:fill-gray-100">' +
      '<path d="M0.866025 8.80383L11.2583 0.803833C11.2583 0.803833 12.0621 9.5 17.2583 13.1961C12.0621 13.1961 0.866025 8.80383 0.866025 8.80383Z"></path>' +
      "</svg></div></div>"
    );
  }

  function renderAssistantMessage(content, actionsHtml, options) {
    var settings = options || {};
    var pending = settings.pending === true;
    var index = Number.isInteger(settings.index) ? settings.index : -1;
    var messageAttrs = index >= 0 ? ' data-ai-message-index="' + index + '"' : "";
    var bodyHtml = pending
      ? '<div data-ai-stream-text="true" class="space-y-4" style="line-height:1.5rem;overflow-wrap:anywhere"></div>'
      : renderMarkdown(content);

    return (
      '<div class="max-w-[95%] text-sm leading-relaxed"' + messageAttrs + (pending ? ' data-ai-pending="true"' : "") + '>' +
      '<div class="inline"><div class="size-full text-wrap inline" data-ai-assistant-content="true">' +
      bodyHtml +
      "</div></div>" +
      (pending ? "" : renderReferencedCourses(content)) +
      (pending ? "" : (actionsHtml || "")) +
      "</div>"
    );
  }

  function renderReferencedCourses(content) {
    var text = String(content || "");

    if (/!\[[^\]]*\]\((?:https?:\/\/[^)\s]+|\/[^)\s]+)\)/.test(text)) {
      return "";
    }

    var matches = getCourses().filter(function (course) {
      return text.indexOf(course.href) !== -1;
    });

    if (!matches.length) {
      return "";
    }

    return (
      '<div class="ask-ai-course-cards">' +
      matches
        .map(function (course) {
          return (
            '<a class="ask-ai-course-card" href="' +
            escapeHtml(course.href) +
            '" aria-label="Abrir curso ' +
            escapeHtml(course.title) +
            '">' +
            '<img src="' +
            escapeHtml(course.image) +
            '" alt="" loading="lazy">' +
            '<span><strong>' +
            escapeHtml(course.title) +
            '</strong><small>' +
            escapeHtml(course.description) +
            "</small></span></a>"
          );
        })
        .join("") +
      "</div>"
    );
  }

  function renderResponseActions(index) {
    var buttonClass =
      "outline-none m-0 border-0 align-baseline no-underline group/trigger relative cursor-pointer select-none transform translate-z-0 flex text-[var(--themed-fg,_var(--ds-background-100))] font-medium !px-(--geist-gap-half) max-w-full items-center justify-center transition-[border-color, background,color,transform,box-shadow] duration-[150ms] ease-in-out data-[focus]:transition-none data-[focus]:shadow-[var(--ds-focus-ring)] [&_svg]:shrink-0 disabled:cursor-not-allowed aria-disabled:cursor-not-allowed disabled:text-[var(--ds-gray-700)] disabled:bg-[var(--ds-gray-100)] aria-disabled:text-[var(--ds-gray-700)] aria-disabled:bg-[var(--ds-gray-100)] disabled:![--themed-border:_var(--ds-gray-400)] [--x-padding:6px] [--height:32px] geist-new-themed geist-new-tertiary geist-new-tertiary-fill rounded-md bg-transparent [--themed-border:_transparent] [--themed-hover-bg:_var(--ds-gray-alpha-200)] [--lighten-color:_rgba(255,_255,_255,_0.8)] data-hover:![--themed-border:_var(--ds-gray-alpha-200)] p-0 w-[var(--geist-form-small-height)] h-[32px] text-(length:--geist-form-small-font) [--spinner-size:16px] data-hover:bg-[var(--themed-hover-bg,_hsl(0,_0%,_22%))] dark-theme:data-hover:bg-[var(--themed-hover-bg,_hsl(0,_0%,_80%))] data-hover:[--themed-border:var(--themed-hover-bg,_var(--ds-gray-200))] data-hover:disabled:bg-[var(--ds-gray-100)] !text-gray-800";

    return (
      '<div class="mt-1 flex items-center -gap-0.5" data-ai-actions="' + index + '">' +
      '<div class="flex items-center gap-0.5">' +
      '<button type="button" aria-label="Thumb up" class="' + buttonClass + '" data-ai-feedback="up">' +
      '<span class="truncate px-1.5 inline-flex items-center justify-center shrink-0">' +
      '<svg data-testid="geist-icon" height="16" stroke-linejoin="round" viewBox="0 0 16 16" width="16" style="width: 16px; height: 16px; color: currentcolor;">' +
      '<path fill-rule="evenodd" clip-rule="evenodd" d="M6.89531 2.23972C6.72984 2.12153 6.5 2.23981 6.5 2.44315V5.25001C6.5 6.21651 5.7165 7.00001 4.75 7.00001H2.5V13.5H12.1884C12.762 13.5 13.262 13.1096 13.4011 12.5532L14.4011 8.55318C14.5984 7.76425 14.0017 7.00001 13.1884 7.00001H9.25H8.5V6.25001V3.51458C8.5 3.43384 8.46101 3.35807 8.39531 3.31114L6.89531 2.23972ZM5 2.44315C5 1.01975 6.6089 0.191779 7.76717 1.01912L9.26717 2.09054C9.72706 2.41904 10 2.94941 10 3.51458V5.50001H13.1884C14.9775 5.50001 16.2903 7.18133 15.8563 8.91698L14.8563 12.917C14.5503 14.1412 13.4503 15 12.1884 15H1.75H1V14.25V6.25001V5.50001H1.75H4.75C4.88807 5.50001 5 5.38808 5 5.25001V2.44315Z" fill="currentColor"></path>' +
      "</svg></span></button>" +
      '<button type="button" aria-label="Thumb down" class="' + buttonClass + '" data-ai-feedback="down">' +
      '<span class="truncate px-1.5 inline-flex items-center justify-center shrink-0">' +
      '<svg data-testid="geist-icon" height="16" stroke-linejoin="round" viewBox="0 0 16 16" width="16" style="width: 16px; height: 16px; color: currentcolor;">' +
      '<path fill-rule="evenodd" clip-rule="evenodd" d="M6.89531 13.7603C6.72984 13.8785 6.5 13.7602 6.5 13.5569V10.75C6.5 9.7835 5.7165 9 4.75 9H2.5V2.5H12.1884C12.762 2.5 13.262 2.89037 13.4011 3.44683L14.4011 7.44683C14.5984 8.23576 14.0017 9 13.1884 9H9.25H8.5V9.75V12.4854C8.5 12.5662 8.46101 12.6419 8.39531 12.6889L6.89531 13.7603ZM5 13.5569C5 14.9803 6.6089 15.8082 7.76717 14.9809L9.26717 13.9095C9.72706 13.581 10 13.0506 10 12.4854V10.5H13.1884C14.9775 10.5 16.2903 8.81868 15.8563 7.08303L14.8563 3.08303C14.5503 1.85882 13.4503 1 12.1884 1H1.75H1V1.75V9.75V10.5H1.75H4.75C4.88807 10.5 5 10.6119 5 10.75V13.5569Z" fill="currentColor"></path>' +
      "</svg></span></button></div>" +
      '<button type="button" aria-label="Copy chat as markdown" data-testid="copy/button" title="Copy conversation as markdown" class="' + buttonClass + '" data-copy-message-index="' + index + '">' +
      '<span class="truncate px-1.5 inline-flex items-center justify-center shrink-0">' +
      '<div class="relative size-4"><div class="absolute transition-all duration-200 ease-in-out opacity-0 scale-50">' +
      '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" style="color: currentcolor;">' +
      '<path fill="currentColor" fill-rule="evenodd" d="m15.56 4-.53.53-8.8 8.8c-.68.68-1.78.68-2.47 0l.53-.54-.53.53-2.79-2.79L.44 10 1.5 8.94l.53.53 2.8 2.8c.1.09.25.09.35 0l8.79-8.8.53-.53z" clip-rule="evenodd"></path>' +
      '</svg></div><div class="absolute transition-all duration-200 ease-in-out opacity-100 scale-100">' +
      '<svg viewBox="0 0 16 16" height="16" width="16" data-slot="geist-icon" style="color: currentcolor;">' +
      '<path fill="currentColor" fill-rule="evenodd" d="M8.25 2c.14 0 .25.11.25.25V3H10v-.75C10 1.28 9.22.5 8.25.5h-5.5C1.78.5 1 1.28 1 2.25v7.5c0 .97.78 1.75 1.75 1.75H4.5V10H2.75a.25.25 0 0 1-.25-.25v-7.5c0-.14.11-.25.25-.25zm5 4c.14 0 .25.11.25.25v7.5q-.02.23-.25.25h-5.5a.25.25 0 0 1-.25-.25v-7.5c0-.14.11-.25.25-.25zm0 9.5c.97 0 1.75-.78 1.75-1.75v-7.5c0-.97-.78-1.75-1.75-1.75h-5.5C6.78 4.5 6 5.28 6 6.25v7.5c0 .97.78 1.75 1.75 1.75z" clip-rule="evenodd"></path>' +
      "</svg></div></div></span></button></div>"
    );
  }

  function renderAssistantMessageFromState(message, index) {
    return renderAssistantMessage(
      message.content,
      !message.pending && message.content ? renderResponseActions(index) : "",
      { pending: message.pending === true, index: index }
    );
  }

  function renderLoading() {
    return (
      '<div class="max-w-[95%] text-sm leading-relaxed">' +
      '<div class="pt-2 h-12 animate-pulse text-sm text-gray-900" data-ai-loading="true">' +
      '<span class="inline-block w-4 text-center " data-ai-loading-spinner="true">' + loadingFrames[loadingFrameIndex] + "</span> Buscando cursos" +
      "</div></div>"
    );
  }

  function scrollLogToBottom(panel) {
    var log = panel.querySelector('[role="log"] > div');
    if (log) {
      log.scrollTop = Math.max(0, log.scrollHeight - log.clientHeight);
    }
  }

  function getAssistantMessageNode(panel, index) {
    return panel.querySelector('[data-ai-message-index="' + index + '"]');
  }

  function findNextStreamingSpecial(text, fromIndex) {
    var candidates = [
      text.indexOf("![", fromIndex),
      text.indexOf("[", fromIndex),
      text.indexOf("**", fromIndex),
      text.indexOf("/academy/", fromIndex),
    ].filter(function (index) {
      return index >= 0;
    });

    return candidates.length ? Math.min.apply(Math, candidates) : text.length;
  }

  function registerStreamingText(state, parent, value, rawStart, container) {
    var node = document.createTextNode("");

    parent.appendChild(node);
    state.textParts.push({
      node: node,
      container: container || null,
      value: value,
      rawStart: rawStart,
      rawEnd: rawStart + value.length,
    });

    return node;
  }

  function appendStreamingInline(state, parent, text, absoluteStart) {
    var index = 0;

    while (index < text.length) {
      var rest = text.slice(index);
      var image = rest.match(/^!\[([^\]]*)\]\((https?:\/\/[^)\s]+|\/[^)\s]+)\)/);
      var link = rest.match(/^\[([^\]]+)\]\((https?:\/\/[^)\s]+|\/[^)\s]+)\)/);
      var bold = rest.match(/^\*\*([^*]+)\*\*/);
      var academy = rest.match(/^\/academy\/[a-z0-9][a-z0-9\/_-]*/i);
      var element;
      var labelStart;

      if (image) {
        element = document.createElement("img");
        element.className = "ask-ai-course-image";
        element.src = image[2];
        element.alt = image[1];
        element.loading = "lazy";
        element.hidden = true;
        parent.appendChild(element);
        state.revealParts.push({
          node: element,
          rawStart: absoluteStart + index,
          rawEnd: absoluteStart + index + image[0].length,
          revealOnComplete: true,
        });
        index += image[0].length;
        continue;
      }

      if (link) {
        element = document.createElement("a");
        element.className = "link_link__hbWKh no-underline link_blend__JjXMT ask-ai-course-link";
        element.href = link[2];
        element.hidden = true;
        parent.appendChild(element);
        labelStart = absoluteStart + index + 1;
        registerStreamingText(state, element, link[1], labelStart, element);
        index += link[0].length;
        continue;
      }

      if (bold) {
        element = document.createElement("strong");
        element.hidden = true;
        parent.appendChild(element);
        labelStart = absoluteStart + index + 2;
        registerStreamingText(state, element, bold[1], labelStart, element);
        index += bold[0].length;
        continue;
      }

      if (academy) {
        element = document.createElement("a");
        element.className = "link_link__hbWKh no-underline link_blend__JjXMT ask-ai-course-link";
        element.href = academy[0];
        element.hidden = true;
        parent.appendChild(element);
        registerStreamingText(state, element, academy[0], absoluteStart + index, element);
        index += academy[0].length;
        continue;
      }

      var next = findNextStreamingSpecial(text, index + 1);
      var plain = text.slice(index, next);

      if (!plain) {
        plain = text.charAt(index);
        next = index + 1;
      }

      registerStreamingText(state, parent, plain, absoluteStart + index, null);
      index = next;
    }
  }

  function buildStreamingMarkdown(streamNode, fullText) {
    var state = {
      fullText: fullText,
      blocks: [],
      textParts: [],
      revealParts: [],
    };
    var cursor = 0;
    var currentList = null;

    streamNode.replaceChildren();

    while (cursor <= fullText.length) {
      var newline = fullText.indexOf("\n", cursor);
      var rawEnd = newline >= 0 ? newline : fullText.length;
      var rawLine = fullText.slice(cursor, rawEnd).replace(/\r$/, "");
      var trimmed = rawLine.trim();
      var leading = rawLine.length - rawLine.trimStart().length;
      var content = rawLine.slice(leading);
      var contentStart = cursor + leading;
      var block;
      var prefix;

      if (trimmed) {
        if (/^#{2,3}\s+/.test(content)) {
          currentList = null;
          prefix = content.match(/^#{2,3}\s+/)[0];
          content = content.slice(prefix.length);
          contentStart += prefix.length;
          block = document.createElement("h3");
          block.className = "font-semibold text-lg";
          block.hidden = true;
          streamNode.appendChild(block);
          state.blocks.push({ node: block, rawStart: contentStart });
          appendStreamingInline(state, block, content, contentStart);
        } else if (/^[-*]\s+/.test(content) || /^\d+\.\s+/.test(content)) {
          prefix = (content.match(/^[-*]\s+/) || content.match(/^\d+\.\s+/))[0];
          content = content.slice(prefix.length);
          contentStart += prefix.length;

          if (!currentList) {
            currentList = document.createElement("ul");
            currentList.className = "list-disc list-inside pl-2";
            currentList.hidden = true;
            streamNode.appendChild(currentList);
            state.blocks.push({ node: currentList, rawStart: contentStart });
          }

          block = document.createElement("li");
          block.className = "my-1";
          block.hidden = true;
          currentList.appendChild(block);
          state.blocks.push({ node: block, rawStart: contentStart });
          appendStreamingInline(state, block, content, contentStart);
        } else {
          currentList = null;
          block = document.createElement("p");
          block.className = "leading-6";
          block.hidden = true;
          streamNode.appendChild(block);
          state.blocks.push({ node: block, rawStart: contentStart });
          appendStreamingInline(state, block, content, contentStart);
        }
      } else {
        currentList = null;
      }

      if (newline < 0) {
        break;
      }

      cursor = newline + 1;
    }

    streamNode._askAiStreamState = state;
    return state;
  }

  function updateStreamingMarkdown(streamNode, fullText, rawCursor) {
    var state = streamNode._askAiStreamState;

    if (!state || state.fullText !== fullText) {
      state = buildStreamingMarkdown(streamNode, fullText);
    }

    state.blocks.forEach(function (part) {
      part.node.hidden = rawCursor <= part.rawStart;
    });

    state.textParts.forEach(function (part) {
      var visibleLength = Math.max(0, Math.min(part.value.length, rawCursor - part.rawStart));

      part.node.textContent = part.value.slice(0, visibleLength);
      if (part.container) {
        part.container.hidden = visibleLength === 0;
      }
    });

    state.revealParts.forEach(function (part) {
      part.node.hidden = part.revealOnComplete ? rawCursor < part.rawEnd : rawCursor <= part.rawStart;
    });
  }

  function updatePendingAssistantText(panel, index, fullText, rawCursor) {
    var messageNode = getAssistantMessageNode(panel, index);
    var streamNode = messageNode && messageNode.querySelector('[data-ai-stream-text="true"]');

    if (!streamNode) {
      return false;
    }

    /* The streaming structure is built once. Only text nodes/visibility change per frame. */
    updateStreamingMarkdown(streamNode, fullText, rawCursor);
    scrollLogToBottom(panel);
    return true;
  }

  function finalizeAssistantMessage(panel, index, message) {
    var messageNode = getAssistantMessageNode(panel, index);
    var contentNode = messageNode && messageNode.querySelector('[data-ai-assistant-content="true"]');

    if (!messageNode || !contentNode) {
      renderMessages(panel);
      return;
    }

    messageNode.removeAttribute("data-ai-pending");
    contentNode.innerHTML = renderMarkdown(message.content);
    messageNode.querySelectorAll('[data-ai-final-extras="true"]').forEach(function (node) {
      node.remove();
    });

    messageNode.insertAdjacentHTML(
      "beforeend",
      '<div data-ai-final-extras="true" style="display:contents">' +
        renderReferencedCourses(message.content) +
        renderResponseActions(index) +
      "</div>"
    );

    updateMode(panel);
    scrollLogToBottom(panel);
  }

  function renderMessages(panel) {
    var logContent = getLogContent(panel);

    if (!logContent) {
      return;
    }

    logContent.innerHTML = messages
      .map(function (message, index) {
        return message.role === "user" ? renderUserMessage(message.content) : renderAssistantMessageFromState(message, index);
      })
      .join("") + (isWaitingForResponse ? renderLoading() : "");

    updateMode(panel);
    scrollLogToBottom(panel);
  }

  function getMarkdownTranscript() {
    return messages
      .map(function (message) {
        return (message.role === "user" ? "Usuário" : "IA") + ":\n" + message.content;
      })
      .join("\n\n");
  }

  function copyText(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      return navigator.clipboard.writeText(text);
    }

    var input = document.createElement("textarea");
    input.value = text;
    input.style.position = "fixed";
    input.style.opacity = "0";
    document.body.appendChild(input);
    input.select();
    document.execCommand("copy");
    input.remove();
    return Promise.resolve();
  }

  function setCopyLayerState(layer, isVisible) {
    if (!layer) {
      return;
    }

    layer.classList.toggle("opacity-100", isVisible);
    layer.classList.toggle("scale-100", isVisible);
    layer.classList.toggle("opacity-0", !isVisible);
    layer.classList.toggle("scale-50", !isVisible);
  }

  function setCopyButtonCopied(button, isCopied) {
    var status;
    var iconWrap;
    var layers;

    if (!button) {
      return;
    }

    status = button.querySelector("[data-ai-copy-status]");
    iconWrap = button.querySelector(".relative.size-4");
    layers = iconWrap ? Array.from(iconWrap.children) : [];

    if (isCopied && !status) {
      var span = button.querySelector("span");

      if (span) {
        span.insertAdjacentHTML("afterbegin", '<div aria-live="assertive" class="sr-only" role="status" data-ai-copy-status="true">Copied!</div>');
      }
    }

    if (!isCopied && status) {
      status.remove();
    }

    setCopyLayerState(layers[0], isCopied);
    setCopyLayerState(layers[1], !isCopied);
  }

  function showCopyConfirmation(button) {
    if (!button) {
      return;
    }

    window.clearTimeout(button._askAiCopyTimer);
    setCopyButtonCopied(button, true);
    button._askAiCopyTimer = window.setTimeout(function () {
      setCopyButtonCopied(button, false);
      button._askAiCopyTimer = null;
    }, 1500);
  }

  function copyAndConfirm(button, text) {
    if (!text) {
      return;
    }

    copyText(text)
      .then(function () {
        showCopyConfirmation(button);
      })
      .catch(function () {
        showCopyConfirmation(button);
      });
  }

  function setActionHover(button, isHovering) {
    var feedbackType;

    if (!button) {
      return;
    }

    feedbackType = button.getAttribute("data-ai-feedback");

    if (button.getAttribute("data-feedback-disabled") === "true") {
      if (button.getAttribute("data-selected") === "true") {
        button.style.color = isHovering ? (feedbackType === "up" ? "rgb(29, 78, 216)" : "rgb(185, 28, 28)") : "";
      }

      button.style.background = "var(--ds-gray-alpha-200)";
      return;
    }

    button.style.background = isHovering ? "var(--ds-gray-alpha-200)" : "";
  }

  function resetChat(panel) {
    activeRequestId += 1;
    messages = [];
    isLoading = false;
    isWaitingForResponse = false;

    if (abortController) {
      abortController.abort();
      abortController = null;
    }

    stopLoadingAnimation();
    stopResponseDelay();
    stopTypingAnimation();

    var input = getInput(panel);
    var logContent = getLogContent(panel);

    if (input) {
      input.value = "";
    }

    if (logContent) {
      logContent.innerHTML = "";
    }

    updateMode(panel);
  }

  function setErrorMessage(panel, message) {
    messages.push({ role: "assistant", content: message });
    isLoading = false;
    isWaitingForResponse = false;
    abortController = null;
    stopLoadingAnimation();
    stopTypingAnimation();
    renderMessages(panel);
  }

  function startLoadingAnimation(panel) {
    stopLoadingAnimation();
    loadingFrameIndex = 0;
    loadingTimer = window.setInterval(function () {
      var spinner;

      loadingFrameIndex = (loadingFrameIndex + 1) % loadingFrames.length;
      spinner = panel.querySelector("[data-ai-loading-spinner]");

      if (spinner) {
        spinner.textContent = loadingFrames[loadingFrameIndex];
      }
    }, 120);
  }

  function stopLoadingAnimation() {
    if (loadingTimer) {
      window.clearInterval(loadingTimer);
      loadingTimer = null;
    }
  }

  function stopTypingAnimation() {
    if (typingTimer !== null) {
      window.cancelAnimationFrame(typingTimer);
      typingTimer = null;
    }
    typingLastFrameAt = 0;
  }

  function stopResponseDelay() {
    if (responseDelayTimer) {
      window.clearTimeout(responseDelayTimer);
      responseDelayTimer = null;
    }
  }

  function typeAssistantReply(panel, message, fullText) {
    var cursor = 0;
    var chunkSize = Math.max(2, Math.ceil(fullText.length / 240));
    var messageIndex = messages.indexOf(message);

    stopTypingAnimation();
    renderMessages(panel);
    typingLastFrameAt = 0;

    function tick(timestamp) {
      if (!message.pending) {
        typingTimer = null;
        typingLastFrameAt = 0;
        return;
      }

      if (!typingLastFrameAt || timestamp - typingLastFrameAt >= typingFrameIntervalMs) {
        typingLastFrameAt = timestamp;
        cursor = Math.min(fullText.length, cursor + chunkSize);
        message.content = fullText.slice(0, cursor);

        if (!updatePendingAssistantText(panel, messageIndex, fullText, cursor)) {
          /* Recover once if the panel DOM was replaced by SPA navigation. */
          renderMessages(panel);
          updatePendingAssistantText(panel, messageIndex, fullText, cursor);
        }

        if (cursor >= fullText.length) {
          message.pending = false;
          isLoading = false;
          typingTimer = null;
          typingLastFrameAt = 0;
          finalizeAssistantMessage(panel, messageIndex, message);
          return;
        }
      }

      typingTimer = window.requestAnimationFrame(tick);
    }

    typingTimer = window.requestAnimationFrame(tick);
  }

  function cancelCurrentWork(panel) {
    var lastMessage;

    activeRequestId += 1;

    if (abortController) {
      abortController.abort();
      abortController = null;
    }

    stopLoadingAnimation();
    stopResponseDelay();
    stopTypingAnimation();
    isLoading = false;
    isWaitingForResponse = false;

    lastMessage = messages[messages.length - 1];

    if (lastMessage && lastMessage.role === "assistant" && lastMessage.pending) {
      if (lastMessage.content) {
        var pendingNode = getAssistantMessageNode(panel, messages.length - 1);
        var streamNode = pendingNode && pendingNode.querySelector('[data-ai-stream-text="true"]');
        var visibleText = streamNode ? streamNode.innerText.trim() : "";

        lastMessage.content = visibleText || lastMessage.content.replace(/[!\[\]()*]+/g, " ").replace(/\s+/g, " ").trim();
        lastMessage.pending = false;
      } else {
        messages.pop();
      }
    }

    renderMessages(panel);
  }

  function requestAssistantReply(panel) {
    var requestId = ++activeRequestId;

    abortController = new AbortController();

    return Promise.reject(new Error("Sem IA conectada neste website p?blico."))
      .then(function (response) {
        return response.json().then(function (body) {
          if (!response.ok) {
            throw new Error(body && body.error ? body.error : "Groq API request failed.");
          }

          return body;
        });
      })
      .then(function (body) {
        var content;
        var elapsed;
        var delay;

        if (requestId !== activeRequestId) {
          return;
        }

        content = cleanAssistantContent(body.content) || "I could not generate a response.";
        elapsed = Date.now() - loadingStartedAt;
        delay = Math.max(0, minimumLoadingMs - elapsed);

        responseDelayTimer = window.setTimeout(function () {
          var assistantMessage;

          responseDelayTimer = null;

          if (requestId !== activeRequestId) {
            return;
          }

          stopLoadingAnimation();
          isWaitingForResponse = false;
          abortController = null;
          assistantMessage = { role: "assistant", content: "", pending: true };
          messages.push(assistantMessage);
          typeAssistantReply(panel, assistantMessage, content);
        }, delay);
      })
      .catch(function (error) {
        if (requestId !== activeRequestId) {
          return;
        }

        if (error.name === "AbortError") {
          isLoading = false;
          isWaitingForResponse = false;
          abortController = null;
          stopLoadingAnimation();
          stopTypingAnimation();
          renderMessages(panel);
          return;
        }

        setErrorMessage(panel, error.message || "I could not connect to the AI service.");
      });
  }

  function sendMessage(panel, text) {
    var input = getInput(panel);
    var content = normalize(text);

    if (!content || isLoading) {
      return;
    }

    if (input) {
      input.value = "";
    }

    messages.push({ role: "user", content: content });
    isLoading = true;
    isWaitingForResponse = true;
    loadingStartedAt = Date.now();
    startLoadingAnimation(panel);
    renderMessages(panel);
    requestAssistantReply(panel);
  }

  function openPanel(panel) {
    if (!panel) {
      return false;
    }

    applyPanelLayout(panel);
    panel.classList.remove(closedWidthClass, "overflow-hidden");
    panel.classList.add(openWidthClass);
    panel.setAttribute("aria-hidden", "false");
    document.documentElement.setAttribute("data-ask-ai-open", "");
    setButtonsExpanded(true);
    setHeaderLogoCompact(true);

    window.requestAnimationFrame(function () {
      var input = getInput(panel);
      if (input) {
        input.focus();
      }
    });
    return true;
  }

  function closePanel(panel) {
    if (!panel) {
      return false;
    }

    panel.classList.remove(openWidthClass);
    panel.classList.add(closedWidthClass, "overflow-hidden");
    panel.setAttribute("aria-hidden", "true");
    document.documentElement.removeAttribute("data-ask-ai-open");
    setButtonsExpanded(false);
    setHeaderLogoCompact(false);
    return true;
  }

  function togglePanel(source) {
    overlayManager.toggle("ask-ai", { source: source || "launcher" });
  }

  overlayManager.register("ask-ai", {
    group: "panel",
    blocksScroll: false,
    dismissOnEscape: false,
    closeGroupsOnOpen: ["modal", "context-menu"],
    open: function () {
      return openPanel(getChatPanel());
    },
    close: function () {
      return closePanel(getChatPanel());
    },
  });

  function init() {
    var panel = getChatPanel();
    var form;
    var input;

    if (!panel) {
      return;
    }

    /*
     * A navigation or cached DOM restoration must never make a closed panel
     * inherit the compact/open desktop header. Start from one canonical closed
     * state; only the launcher is allowed to open it afterwards.
     */
    panel.classList.remove(openWidthClass);
    panel.classList.add(closedWidthClass, "overflow-hidden");
    panel.setAttribute("aria-hidden", "true");
    document.documentElement.removeAttribute("data-ask-ai-open");
    setButtonsExpanded(false);
    setHeaderLogoCompact(false);

    applyPanelLayout(panel);
    updateMode(panel);

    form = getForm(panel);
    input = getInput(panel);

    if (input) {
      input.addEventListener("input", function () {
        updateMode(panel);
      });
    }

    if (form) {
      form.addEventListener("submit", function (event) {
        event.preventDefault();

        if (isLoading) {
          cancelCurrentWork(panel);
          return;
        }

        sendMessage(panel, input && input.value);
      });
    }

    document.addEventListener(
      "pointerdown",
      function (event) {
        var stopButton =
          event.target && event.target.closest
            ? event.target.closest('button[data-ai-stop="true"]')
            : null;

        if (!stopButton || !panel.contains(stopButton)) {
          return;
        }

        event.preventDefault();
        event.stopPropagation();
        cancelCurrentWork(panel);
      },
      true
    );

    document.addEventListener("click", function (event) {
      var target = event.target;
      var askButton;
      var closeButton;
      var clearButton;
      var copyButton;
      var feedbackButton;
      var stopButton;
      var suggestionButton;

      if (!target || !target.closest) {
        return;
      }

      askButton = target.closest('button[aria-label="Pergunte à IA"]');
      closeButton = target.closest('button[aria-label="Close chat"]');
      clearButton = target.closest('button[aria-label="Clear chat"]');
      copyButton = target.closest('button[aria-label="Copy chat as markdown"]');
      feedbackButton = target.closest("[data-ai-feedback]");
      stopButton = target.closest('button[data-ai-stop="true"]');
      suggestionButton = target.closest('[data-radix-scroll-area-viewport] button');

      if (stopButton && panel.contains(stopButton)) {
        event.preventDefault();
        event.stopPropagation();
        cancelCurrentWork(panel);
        return;
      }

      if (askButton) {
        event.preventDefault();

        togglePanel(askButton.hasAttribute("data-academy-ask-ai") ? "academy" : "launcher");
        return;
      }

      if (closeButton && panel.contains(closeButton)) {
        event.preventDefault();
        overlayManager.close("ask-ai", { source: "close-button" });
        return;
      }

      if (clearButton && panel.contains(clearButton)) {
        event.preventDefault();
        resetChat(panel);
        return;
      }

      if (copyButton && panel.contains(copyButton)) {
        var messageIndex = copyButton.getAttribute("data-copy-message-index");

        event.preventDefault();

        if (messageIndex !== null) {
          var copiedMessage = messages[Number(messageIndex)];

          if (copiedMessage && copiedMessage.content) {
            copyAndConfirm(copyButton, copiedMessage.content);
          }

          return;
        }

        if (messages.length > 0) {
          copyAndConfirm(copyButton, getMarkdownTranscript());
        }

        return;
      }

      if (feedbackButton && panel.contains(feedbackButton)) {
        var actions = feedbackButton.closest("[data-ai-actions]");

        event.preventDefault();

        if (actions && actions.getAttribute("data-feedback-locked") === "true") {
          return;
        }

        if (actions) {
          actions.setAttribute("data-feedback-locked", "true");
          Array.from(actions.querySelectorAll("[data-ai-feedback]")).forEach(function (button) {
            button.style.background = "var(--ds-gray-alpha-200)";
            button.style.color = "";
            button.style.cursor = "default";
            button.style.userSelect = "none";
            button.classList.add("opacity-50", "shadow-none");
            button.setAttribute("data-feedback-disabled", "true");
            button.removeAttribute("data-selected");
          });
        }

        feedbackButton.setAttribute("data-selected", "true");
        feedbackButton.classList.add(feedbackButton.getAttribute("data-ai-feedback") === "up" ? "hover:text-blue-700!" : "hover:text-red-700!");
        return;
      }

      if (suggestionButton && panel.contains(suggestionButton)) {
        event.preventDefault();
        sendMessage(panel, suggestionButton.textContent);
      }
    });

    document.addEventListener("pointerover", function (event) {
      var button;

      if (!event.target || !event.target.closest) {
        return;
      }

      button = event.target.closest("[data-ai-actions] button");

      if (!button || !panel.contains(button) || button.contains(event.relatedTarget)) {
        return;
      }

      setActionHover(button, true);
    });

    document.addEventListener("pointerout", function (event) {
      var button;

      if (!event.target || !event.target.closest) {
        return;
      }

      button = event.target.closest("[data-ai-actions] button");

      if (!button || !panel.contains(button) || button.contains(event.relatedTarget)) {
        return;
      }

      setActionHover(button, false);
    });

    document.addEventListener("keydown", function (event) {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "i") {
        event.preventDefault();
        togglePanel("keyboard");
      }
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();

// Static-page route guard for routes not published by this demonstration.
document.addEventListener('click', function(e) {
  const link=e.target.closest && e.target.closest('a[href]');
  if(!link)return;
  const href=link.getAttribute('href') || '';
  if(href.startsWith('/') && !href.startsWith('/hostinger-simulator-mcp/')) e.preventDefault();
}, true);
