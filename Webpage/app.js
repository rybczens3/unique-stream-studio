const API_BASE = "/portal/api";
const state = {
  token: localStorage.getItem("portalToken"),
  user: null,
  role: null,
};

const view = document.getElementById("view");
const statusEl = document.getElementById("status");
const navEl = document.getElementById("appNav");
const sessionInfoEl = document.getElementById("sessionInfo");

function setStatus(message, tone = "info") {
  if (!message) {
    statusEl.textContent = "";
    statusEl.className = "status";
    return;
  }
  statusEl.textContent = message;
  statusEl.className = `status show ${tone}`;
}

function setSession(user) {
  state.user = user;
  state.role = user?.role ?? null;
  if (user) {
    sessionInfoEl.innerHTML = `Zalogowano jako <strong>${user.username}</strong> (${user.role})`;
  } else {
    sessionInfoEl.textContent = "Brak aktywnej sesji";
  }
  renderNav();
}

function requireRole(roles) {
  return state.role && roles.includes(state.role);
}

async function apiFetch(path, options = {}) {
  const headers = new Headers(options.headers || {});
  headers.set("Accept", "application/json");
  if (options.body && !headers.has("Content-Type")) {
    headers.set("Content-Type", "application/json");
  }
  if (state.token) {
    headers.set("Authorization", `Bearer ${state.token}`);
  }

  const response = await fetch(`${API_BASE}${path}`, {
    ...options,
    headers,
  });

  if (response.status === 204) {
    return null;
  }

  let payload = null;
  const contentType = response.headers.get("Content-Type") || "";
  if (contentType.includes("application/json")) {
    payload = await response.json();
  } else {
    payload = await response.text();
  }

  if (!response.ok) {
    const detail = payload?.detail || payload || response.statusText;
    throw new Error(detail);
  }
  return payload;
}

async function bootstrapSession() {
  if (!state.token) {
    setSession(null);
    return;
  }
  try {
    const account = await apiFetch("/account/me");
    setSession(account);
  } catch (error) {
    localStorage.removeItem("portalToken");
    state.token = null;
    setSession(null);
  }
}

function renderNav() {
  const links = [
    { href: "#/plugins", label: "Lista wtyczek" },
  ];
  if (requireRole(["developer", "admin"])) {
    links.push({ href: "#/my-plugins", label: "Moje wtyczki" });
    links.push({ href: "#/plugins/new", label: "Dodaj wtyczkę" });
  }
  if (requireRole(["admin"])) {
    links.push({ href: "#/admin/queue", label: "Kolejka zatwierdzeń" });
    links.push({ href: "#/admin/users", label: "Użytkownicy" });
  }
  links.push({ href: "#/login", label: state.user ? "Sesja" : "Logowanie" });

  navEl.innerHTML = links
    .map(
      (link) =>
        `<a href="${link.href}" class="${location.hash === link.href ? "active" : ""}">${link.label}</a>`
    )
    .join("");
}

function renderLoading() {
  view.innerHTML = `
    <div class="card">
      <p>Ładowanie danych...</p>
    </div>
  `;
}

function renderLogin() {
  view.innerHTML = `
    <div class="grid two">
      <div class="card">
        <h2>Logowanie</h2>
        <form class="form" id="loginForm">
          <div>
            <label for="username">Nazwa użytkownika</label>
            <input id="username" name="username" placeholder="np. admin" required />
          </div>
          <div>
            <label for="password">Hasło</label>
            <input id="password" name="password" type="password" required />
          </div>
          <button class="button" type="submit">Zaloguj</button>
        </form>
      </div>
      <div class="card">
        <h2>Sesja</h2>
        <p>${state.user ? `Aktywny użytkownik: <strong>${state.user.username}</strong>` : "Brak aktywnej sesji."}</p>
        <div class="actions">
          <button class="button secondary" id="logoutBtn" ${state.user ? "" : "disabled"}>Wyloguj</button>
        </div>
        <p class="helper">Użyj kont seeded w API: admin/admin, developer/dev, user/user.</p>
      </div>
    </div>
  `;

  const form = document.getElementById("loginForm");
  const logoutBtn = document.getElementById("logoutBtn");

  form?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(form);
    const payload = Object.fromEntries(formData.entries());
    setStatus("Logowanie...", "info");
    try {
      const result = await apiFetch("/auth/login", {
        method: "POST",
        body: JSON.stringify(payload),
      });
      state.token = result.access_token;
      localStorage.setItem("portalToken", state.token);
      setSession({ username: result.username, role: result.role });
      setStatus("Zalogowano pomyślnie.", "info");
      location.hash = "#/plugins";
    } catch (error) {
      setStatus(`Błąd logowania: ${error.message}`, "warning");
    }
  });

  logoutBtn?.addEventListener("click", () => {
    state.token = null;
    localStorage.removeItem("portalToken");
    setSession(null);
    setStatus("Wylogowano.", "info");
  });
}

function renderPluginList() {
  view.innerHTML = `
    <div class="card">
      <div class="inline" style="justify-content: space-between;">
        <h2>Lista opublikowanych wtyczek</h2>
        <form class="search" id="pluginSearch">
          <input name="query" placeholder="Szukaj po nazwie lub ID" />
          <button class="button secondary" type="submit">Szukaj</button>
        </form>
      </div>
      <div id="pluginList" class="grid" style="margin-top: 18px;"></div>
    </div>
  `;

  const listEl = document.getElementById("pluginList");
  const searchForm = document.getElementById("pluginSearch");

  async function load(query) {
    listEl.innerHTML = "<p>Ładowanie...</p>";
    try {
      const plugins = await apiFetch(`/plugins${query ? `?query=${encodeURIComponent(query)}` : ""}`);
      if (!plugins.length) {
        listEl.innerHTML = "<div class=\"empty\">Brak wyników.</div>";
        return;
      }
      listEl.innerHTML = plugins
        .map(
          (plugin) => `
          <div class="card">
            <h2>${plugin.name}</h2>
            <p>${plugin.id}</p>
            <div class="meta">
              <span class="tag">${plugin.version ?? "brak wersji"}</span>
              <span class="tag">${plugin.compatibility}</span>
            </div>
            <div class="actions">
              <a class="button secondary" href="#/plugins/${plugin.id}">Szczegóły</a>
              <a class="button" href="${plugin.package_url}">Pobierz</a>
            </div>
          </div>
        `
        )
        .join("");
    } catch (error) {
      listEl.innerHTML = `<div class="empty">Błąd: ${error.message}</div>`;
    }
  }

  searchForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const query = new FormData(searchForm).get("query");
    load(query);
  });

  load("");
}

async function renderMyPlugins() {
  renderLoading();
  try {
    const plugins = await apiFetch("/plugins/mine");
    view.innerHTML = `
      <div class="card">
        <h2>Moje wtyczki</h2>
        ${
          plugins.length
            ? `
            <table class="table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Nazwa</th>
                  <th>Status</th>
                  <th>Wersja</th>
                  <th>Akcje</th>
                </tr>
              </thead>
              <tbody>
                ${plugins
                  .map(
                    (plugin) => `
                  <tr>
                    <td>${plugin.id}</td>
                    <td>${plugin.name}</td>
                    <td>${plugin.status}</td>
                    <td>${plugin.version ?? "-"}</td>
                    <td>
                      <a class="button secondary" href="#/plugins/${plugin.id}">Podgląd</a>
                      <a class="button" href="#/plugins/${plugin.id}/edit">Edytuj</a>
                    </td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
            : `<div class="empty">Nie masz jeszcze wtyczek.</div>`
        }
      </div>
    `;
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Nie udało się pobrać danych: ${error.message}</p></div>`;
  }
}

async function renderPluginDetail(pluginId) {
  renderLoading();
  try {
    const [plugin, versions] = await Promise.all([
      apiFetch(`/plugins/${pluginId}`),
      apiFetch(`/plugins/${pluginId}/versions`),
    ]);
    let manageInfo = null;
    if (state.token) {
      try {
        manageInfo = await apiFetch(`/plugins/${pluginId}/manage`);
      } catch (error) {
        manageInfo = null;
      }
    }

    const status = manageInfo?.status ? `<span class="tag">${manageInfo.status}</span>` : "";
    const owner = manageInfo?.owner ? `<span class="tag">Właściciel: ${manageInfo.owner}</span>` : "";

    view.innerHTML = `
      <div class="grid two">
        <div class="card">
          <h2>${plugin.name}</h2>
          <p>${plugin.id}</p>
          <div class="meta">
            <span class="tag">${plugin.version ?? "brak wersji"}</span>
            <span class="tag">${plugin.compatibility}</span>
            ${status}
            ${owner}
          </div>
          <p class="helper" style="margin-top: 12px;">SHA256: ${plugin.sha256 ?? "-"}</p>
          <p class="helper">Signature: ${plugin.signature ?? "-"}</p>
          <div class="actions">
            <a class="button" href="${plugin.package_url}">Pobierz</a>
            ${
              manageInfo
                ? `<a class="button secondary" href="#/plugins/${plugin.id}/edit">Edytuj</a>`
                : ""
            }
          </div>
          <div class="actions" id="detailActions"></div>
        </div>
        <div class="card">
          <h2>Wersje</h2>
          ${
            versions.length
              ? `
            <table class="table">
              <thead>
                <tr>
                  <th>Wersja</th>
                  <th>Pakiet</th>
                </tr>
              </thead>
              <tbody>
                ${versions
                  .map(
                    (version) => `
                  <tr>
                    <td>${version.version}</td>
                    <td><a href="${version.package_url}">Pobierz</a></td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
              : `<div class="empty">Brak wersji.</div>`
          }
        </div>
      </div>
    `;

    const detailActions = document.getElementById("detailActions");
    if (!manageInfo) {
      return;
    }

    const actions = [];
    if (requireRole(["developer", "admin"]) && ["draft", "rejected", "unpublished"].includes(manageInfo.status)) {
      actions.push({ label: "Wyślij do zatwierdzenia", endpoint: "submit", className: "success" });
    }
    if (requireRole(["admin"]) && manageInfo.status === "submitted") {
      actions.push({ label: "Zatwierdź", endpoint: "approve", className: "success" });
      actions.push({ label: "Odrzuć", endpoint: "reject", className: "danger" });
    }
    if (requireRole(["admin"]) && manageInfo.status === "approved") {
      actions.push({ label: "Opublikuj", endpoint: "publish", className: "success" });
    }
    if (requireRole(["admin"]) && manageInfo.status === "published") {
      actions.push({ label: "Wycofaj", endpoint: "unpublish", className: "warning" });
    }

    if (!actions.length) {
      detailActions.innerHTML = "";
      return;
    }

    detailActions.innerHTML = actions
      .map(
        (action) =>
          `<button class="button ${action.className}" data-endpoint="${action.endpoint}">${action.label}</button>`
      )
      .join("");

    detailActions.querySelectorAll("button").forEach((button) => {
      button.addEventListener("click", async () => {
        const endpoint = button.dataset.endpoint;
        try {
          await apiFetch(`/plugins/${pluginId}/${endpoint}`, { method: "POST" });
          setStatus("Zaktualizowano status wtyczki.", "info");
          renderPluginDetail(pluginId);
        } catch (error) {
          setStatus(`Błąd: ${error.message}`, "warning");
        }
      });
    });
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Nie udało się pobrać szczegółów: ${error.message}</p></div>`;
  }
}

async function renderPluginForm(mode, pluginId) {
  renderLoading();
  let plugin = { id: "", name: "", compatibility: "" };
  if (mode === "edit") {
    try {
      plugin = await apiFetch(`/plugins/${pluginId}/manage`);
    } catch (error) {
      view.innerHTML = `<div class="card"><p>Nie udało się pobrać danych: ${error.message}</p></div>`;
      return;
    }
  }

  view.innerHTML = `
    <div class="card">
      <h2>${mode === "edit" ? "Edytuj wtyczkę" : "Dodaj wtyczkę"}</h2>
      <form class="form" id="pluginForm">
        <div>
          <label for="id">ID wtyczki</label>
          <input id="id" name="id" value="${plugin.id}" ${mode === "edit" ? "disabled" : "required"} />
        </div>
        <div>
          <label for="name">Nazwa</label>
          <input id="name" name="name" value="${plugin.name}" required />
        </div>
        <div>
          <label for="compatibility">Kompatybilność</label>
          <input id="compatibility" name="compatibility" value="${plugin.compatibility}" required />
        </div>
        <button class="button" type="submit">Zapisz</button>
      </form>
    </div>
  `;

  const form = document.getElementById("pluginForm");
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(form);
    const payload = {
      id: plugin.id || formData.get("id"),
      name: formData.get("name"),
      compatibility: formData.get("compatibility"),
    };

    try {
      if (mode === "edit") {
        await apiFetch(`/plugins/${plugin.id}`, {
          method: "PUT",
          body: JSON.stringify(payload),
        });
        setStatus("Zapisano zmiany.", "info");
        location.hash = `#/plugins/${plugin.id}`;
        return;
      }
      await apiFetch("/plugins", {
        method: "POST",
        body: JSON.stringify(payload),
      });
      setStatus("Utworzono nową wtyczkę.", "info");
      location.hash = "#/my-plugins";
    } catch (error) {
      setStatus(`Błąd: ${error.message}`, "warning");
    }
  });
}

async function renderApprovalQueue() {
  renderLoading();
  try {
    const plugins = await apiFetch("/plugins/mine");
    const queue = plugins.filter((plugin) => plugin.status === "submitted");
    view.innerHTML = `
      <div class="card">
        <h2>Kolejka zatwierdzeń</h2>
        ${
          queue.length
            ? `
            <table class="table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Nazwa</th>
                  <th>Wersja</th>
                  <th>Akcje</th>
                </tr>
              </thead>
              <tbody>
                ${queue
                  .map(
                    (plugin) => `
                  <tr>
                    <td>${plugin.id}</td>
                    <td>${plugin.name}</td>
                    <td>${plugin.version ?? "-"}</td>
                    <td>
                      <button class="button success" data-action="approve" data-id="${plugin.id}">Zatwierdź</button>
                      <button class="button danger" data-action="reject" data-id="${plugin.id}">Odrzuć</button>
                    </td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
            : `<div class="empty">Brak wtyczek do zatwierdzenia.</div>`
        }
      </div>
    `;

    view.querySelectorAll("button[data-action]").forEach((button) => {
      button.addEventListener("click", async () => {
        const action = button.dataset.action;
        const id = button.dataset.id;
        try {
          await apiFetch(`/plugins/${id}/${action}`, { method: "POST" });
          setStatus("Zaktualizowano status.", "info");
          renderApprovalQueue();
        } catch (error) {
          setStatus(`Błąd: ${error.message}`, "warning");
        }
      });
    });
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Nie udało się pobrać kolejki: ${error.message}</p></div>`;
  }
}

async function renderUserManagement() {
  renderLoading();
  try {
    const users = await apiFetch("/admin/users");
    view.innerHTML = `
      <div class="grid two">
        <div class="card">
          <h2>Użytkownicy</h2>
          ${
            users.length
              ? `
            <table class="table">
              <thead>
                <tr>
                  <th>Użytkownik</th>
                  <th>Rola</th>
                  <th>Nowa rola</th>
                  <th>Status</th>
                  <th>Nowy status</th>
                  <th>Hasło</th>
                  <th>Akcje</th>
                </tr>
              </thead>
              <tbody>
                ${users
                  .map(
                    (user) => `
                  <tr>
                    <td>${user.username}</td>
                    <td>${user.role}</td>
                    <td>
                      <select data-role="${user.username}">
                        <option value="user">user</option>
                        <option value="developer">developer</option>
                        <option value="admin">admin</option>
                      </select>
                    </td>
                    <td>${user.active ? "Aktywne" : "Nieaktywne"}</td>
                    <td>
                      <select data-active="${user.username}">
                        <option value="true">Aktywne</option>
                        <option value="false">Nieaktywne</option>
                      </select>
                    </td>
                    <td><input type="password" data-pass="${user.username}" placeholder="Nowe hasło" /></td>
                    <td>
                      <button class="button" data-update="${user.username}">Zapisz</button>
                      <button class="button danger" data-delete="${user.username}">Usuń</button>
                    </td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
              : `<div class="empty">Brak użytkowników.</div>`
          }
        </div>
        <div class="card">
          <h2>Dodaj użytkownika</h2>
          <form class="form" id="userForm">
            <div>
              <label for="newUsername">Nazwa użytkownika</label>
              <input id="newUsername" name="username" required />
            </div>
            <div>
              <label for="newPassword">Hasło</label>
              <input id="newPassword" name="password" type="password" required />
            </div>
            <div>
              <label for="newRole">Rola</label>
              <select id="newRole" name="role">
                <option value="user">user</option>
                <option value="developer">developer</option>
                <option value="admin">admin</option>
              </select>
            </div>
            <div>
              <label for="newActive">Status konta</label>
              <select id="newActive" name="active">
                <option value="true" selected>Aktywne</option>
                <option value="false">Nieaktywne</option>
              </select>
            </div>
            <button class="button" type="submit">Dodaj</button>
          </form>
        </div>
      </div>
    `;

    view.querySelectorAll("select[data-role]").forEach((select) => {
      select.value = users.find((user) => user.username === select.dataset.role)?.role ?? "user";
    });
    view.querySelectorAll("select[data-active]").forEach((select) => {
      const user = users.find((entry) => entry.username === select.dataset.active);
      select.value = user?.active ? "true" : "false";
    });

    view.querySelectorAll("button[data-update]").forEach((button) => {
      button.addEventListener("click", async () => {
        const username = button.dataset.update;
        const role = view.querySelector(`select[data-role="${username}"]`).value;
        const password = view.querySelector(`input[data-pass="${username}"]`).value;
        const active = view.querySelector(`select[data-active="${username}"]`).value === "true";
        const payload = {};
        if (role) {
          payload.role = role;
        }
        if (password) {
          payload.password = password;
        }
        payload.active = active;
        try {
          await apiFetch(`/admin/users/${username}`, {
            method: "PATCH",
            body: JSON.stringify(payload),
          });
          setStatus("Zaktualizowano użytkownika.", "info");
          renderUserManagement();
        } catch (error) {
          setStatus(`Błąd: ${error.message}`, "warning");
        }
      });
    });

    view.querySelectorAll("button[data-delete]").forEach((button) => {
      button.addEventListener("click", async () => {
        const username = button.dataset.delete;
        if (!confirm(`Usunąć użytkownika ${username}?`)) {
          return;
        }
        try {
          await apiFetch(`/admin/users/${username}`, { method: "DELETE" });
          setStatus("Usunięto użytkownika.", "info");
          renderUserManagement();
        } catch (error) {
          setStatus(`Błąd: ${error.message}`, "warning");
        }
      });
    });

    const userForm = document.getElementById("userForm");
    userForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      const formData = new FormData(userForm);
      const payload = Object.fromEntries(formData.entries());
      try {
        await apiFetch("/admin/users", {
          method: "POST",
          body: JSON.stringify(payload),
        });
        setStatus("Utworzono użytkownika.", "info");
        renderUserManagement();
      } catch (error) {
        setStatus(`Błąd: ${error.message}`, "warning");
      }
    });
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Nie udało się pobrać użytkowników: ${error.message}</p></div>`;
  }
}

function route() {
  setStatus("");
  renderNav();
  const hash = location.hash || "#/plugins";
  const routes = [
    { pattern: /^#\/plugins$/, view: renderPluginList },
    { pattern: /^#\/plugins\/$/, view: renderPluginList },
    { pattern: /^#\/$/, view: renderPluginList },
    { pattern: /^#\/plugins\/new$/, view: () => renderPluginForm("create") },
    { pattern: /^#\/plugins\/([^/]+)\/edit$/, view: (id) => renderPluginForm("edit", id) },
    { pattern: /^#\/plugins\/([^/]+)$/, view: (id) => renderPluginDetail(id) },
    { pattern: /^#\/my-plugins$/, view: renderMyPlugins },
    { pattern: /^#\/admin\/queue$/, view: renderApprovalQueue },
    { pattern: /^#\/admin\/users$/, view: renderUserManagement },
    { pattern: /^#\/login$/, view: renderLogin },
  ];

  for (const route of routes) {
    const match = hash.match(route.pattern);
    if (match) {
      route.view(...match.slice(1));
      return;
    }
  }

  renderPluginList();
}

window.addEventListener("hashchange", route);

bootstrapSession().then(route);
