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
    sessionInfoEl.innerHTML = `Signed in as <strong>${user.username}</strong> (${user.role})`;
  } else {
    sessionInfoEl.textContent = "No active session";
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
    { href: "#/home", label: "Home" },
    { href: "#/plugins", label: "Plugins" },
  ];
  if (requireRole(["developer", "admin"])) {
    links.push({ href: "#/my-plugins", label: "My Plugins" });
    links.push({ href: "#/plugins/new", label: "Submit Plugin" });
  }
  if (requireRole(["admin"])) {
    links.push({ href: "#/admin/queue", label: "Approval Queue" });
    links.push({ href: "#/admin/users", label: "Users" });
    links.push({ href: "#/admin/audit", label: "Audit Logs" });
  }
  links.push({ href: "#/login", label: state.user ? "Session" : "Sign In" });

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
      <p>Loading data...</p>
    </div>
  `;
}

function renderHome() {
  view.innerHTML = `
    <div class="hero">
      <div class="card hero-card">
        <span class="pill">Creator-grade plugin management</span>
        <h2>Build, review, and ship plugins with studio-quality polish.</h2>
        <p>Streamline Portal keeps developers, admins, and users aligned with a unified workflow inspired by modern streaming dashboards.</p>
        <div class="hero-actions">
          <a class="button" href="#/plugins">Explore plugins</a>
          <a class="button secondary" href="#/login">Manage access</a>
        </div>
      </div>
      <div class="card">
        <h2>Live operations overview</h2>
        <p class="section-lead">Track your ecosystem in real time. Update releases, approve submissions, and support creators from a single command center.</p>
        <div class="stat-grid">
          <div class="stat-card">
            <p class="helper">Active plugins</p>
            <h3>128</h3>
          </div>
          <div class="stat-card">
            <p class="helper">Pending reviews</p>
            <h3>6</h3>
          </div>
          <div class="stat-card">
            <p class="helper">Creator teams</p>
            <h3>42</h3>
          </div>
          <div class="stat-card">
            <p class="helper">Avg. release cycle</p>
            <h3>3.2 days</h3>
          </div>
        </div>
      </div>
    </div>
    <div class="card">
      <h2 class="section-title">Everything you need to run your plugin marketplace</h2>
      <p class="section-lead">A cohesive experience for creators and administrators, designed with the sleek, dark aesthetic of leading streaming tools.</p>
      <div class="feature-grid">
        <div class="feature-card">
          <h3>Creator workspaces</h3>
          <p>Manage drafts, iterate on metadata, and ship updates with confidence.</p>
        </div>
        <div class="feature-card">
          <h3>Review pipelines</h3>
          <p>Approve submissions quickly with status tracking and release automation.</p>
        </div>
        <div class="feature-card">
          <h3>Secure sessions</h3>
          <p>Role-based access for admins, developers, and end-users.</p>
        </div>
        <div class="feature-card">
          <h3>Download insights</h3>
          <p>Keep users informed with compatibility, version history, and package integrity.</p>
        </div>
      </div>
    </div>
  `;
}

function renderLogin() {
  view.innerHTML = `
    <div class="grid two">
      <div class="card">
        <h2>Sign in</h2>
        <form class="form" id="loginForm">
          <div>
            <label for="username">Username</label>
            <input id="username" name="username" placeholder="e.g. admin" required />
          </div>
          <div>
            <label for="password">Password</label>
            <input id="password" name="password" type="password" required />
          </div>
          <button class="button" type="submit">Sign in</button>
        </form>
      </div>
      <div class="card">
        <h2>Session</h2>
        <p>${state.user ? `Active user: <strong>${state.user.username}</strong>` : "No active session."}</p>
        <div class="actions">
          <button class="button secondary" id="logoutBtn" ${state.user ? "" : "disabled"}>Sign out</button>
        </div>
        <p class="helper">Use seeded API accounts: admin/admin, developer/dev, user/user.</p>
      </div>
    </div>
  `;

  const form = document.getElementById("loginForm");
  const logoutBtn = document.getElementById("logoutBtn");

  form?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(form);
    const payload = Object.fromEntries(formData.entries());
    setStatus("Signing in...", "info");
    try {
      const result = await apiFetch("/auth/login", {
        method: "POST",
        body: JSON.stringify(payload),
      });
      state.token = result.access_token;
      localStorage.setItem("portalToken", state.token);
      setSession({ username: result.username, role: result.role });
      setStatus("Signed in successfully.", "success");
      location.hash = "#/plugins";
    } catch (error) {
      setStatus(`Sign-in failed: ${error.message}`, "warning");
    }
  });

  logoutBtn?.addEventListener("click", () => {
    state.token = null;
    localStorage.removeItem("portalToken");
    setSession(null);
    setStatus("Signed out.", "info");
  });
}

function renderPluginList() {
  view.innerHTML = `
      <div class="card">
      <div class="inline" style="justify-content: space-between;">
        <h2>Published plugins</h2>
        <form class="search" id="pluginSearch">
          <input name="query" placeholder="Search by name or ID" />
          <button class="button secondary" type="submit">Search</button>
        </form>
      </div>
      <div id="pluginList" class="grid" style="margin-top: 18px;"></div>
    </div>
  `;

  const listEl = document.getElementById("pluginList");
  const searchForm = document.getElementById("pluginSearch");

  async function load(query) {
    listEl.innerHTML = "<p>Loading...</p>";
    try {
      const plugins = await apiFetch(`/plugins${query ? `?query=${encodeURIComponent(query)}` : ""}`);
      if (!plugins.length) {
        listEl.innerHTML = "<div class=\"empty\">No results found.</div>";
        return;
      }
      listEl.innerHTML = plugins
        .map(
          (plugin) => `
          <div class="card">
            <h2>${plugin.name}</h2>
            <p>${plugin.id}</p>
            <div class="meta">
              <span class="tag">${plugin.version ?? "no version"}</span>
              <span class="tag">${plugin.compatibility}</span>
            </div>
            <div class="actions">
              <a class="button secondary" href="#/plugins/${plugin.id}">Details</a>
              <a class="button" href="${plugin.package_url}">Download</a>
            </div>
          </div>
        `
        )
        .join("");
    } catch (error) {
      listEl.innerHTML = `<div class="empty">Error: ${error.message}</div>`;
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
        <h2>My plugins</h2>
        ${
          plugins.length
            ? `
            <table class="table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Name</th>
                  <th>Status</th>
                  <th>Version</th>
                  <th>Actions</th>
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
                      <a class="button secondary" href="#/plugins/${plugin.id}">Preview</a>
                      <a class="button" href="#/plugins/${plugin.id}/edit">Edit</a>
                    </td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
            : `<div class="empty">You have not submitted any plugins yet.</div>`
        }
      </div>
    `;
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Unable to load data: ${error.message}</p></div>`;
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
    const owner = manageInfo?.owner ? `<span class="tag">Owner: ${manageInfo.owner}</span>` : "";

    view.innerHTML = `
      <div class="grid two">
        <div class="card">
          <h2>${plugin.name}</h2>
          <p>${plugin.id}</p>
          <div class="meta">
            <span class="tag">${plugin.version ?? "no version"}</span>
            <span class="tag">${plugin.compatibility}</span>
            ${status}
            ${owner}
          </div>
          <p class="helper" style="margin-top: 12px;">SHA256: ${plugin.sha256 ?? "-"}</p>
          <p class="helper">Signature: ${plugin.signature ?? "-"}</p>
          <div class="actions">
            <a class="button" href="${plugin.package_url}">Download</a>
            ${
              manageInfo
                ? `<a class="button secondary" href="#/plugins/${plugin.id}/edit">Edit</a>`
                : ""
            }
          </div>
          <div class="actions" id="detailActions"></div>
        </div>
        <div class="card">
          <h2>Versions</h2>
          ${
            versions.length
              ? `
            <table class="table">
              <thead>
                <tr>
                  <th>Version</th>
                  <th>Package</th>
                </tr>
              </thead>
              <tbody>
                ${versions
                  .map(
                    (version) => `
                  <tr>
                    <td>${version.version}</td>
                    <td><a href="${version.package_url}">Download</a></td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
              : `<div class="empty">No versions yet.</div>`
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
      actions.push({ label: "Submit for review", endpoint: "submit", className: "success" });
    }
    if (requireRole(["admin"]) && manageInfo.status === "submitted") {
      actions.push({ label: "Approve", endpoint: "approve", className: "success" });
      actions.push({ label: "Reject", endpoint: "reject", className: "danger" });
    }
    if (requireRole(["admin"]) && manageInfo.status === "approved") {
      actions.push({ label: "Publish", endpoint: "publish", className: "success" });
    }
    if (requireRole(["admin"]) && manageInfo.status === "published") {
      actions.push({ label: "Unpublish", endpoint: "unpublish", className: "warning" });
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
        let reason = "";
        if (endpoint === "reject") {
          reason = prompt("Rejection reason (optional):") ?? "";
        }
        try {
          const reasonParam = reason ? `?reason=${encodeURIComponent(reason)}` : "";
          await apiFetch(`/plugins/${pluginId}/${endpoint}${reasonParam}`, { method: "POST" });
          setStatus("Plugin status updated.", "success");
          renderPluginDetail(pluginId);
        } catch (error) {
          setStatus(`Error: ${error.message}`, "warning");
        }
      });
    });
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Unable to load details: ${error.message}</p></div>`;
  }
}

async function renderPluginForm(mode, pluginId) {
  renderLoading();
  let plugin = { id: "", name: "", compatibility: "" };
  if (mode === "edit") {
    try {
      plugin = await apiFetch(`/plugins/${pluginId}/manage`);
    } catch (error) {
      view.innerHTML = `<div class="card"><p>Unable to load data: ${error.message}</p></div>`;
      return;
    }
  }

  view.innerHTML = `
    <div class="card">
      <h2>${mode === "edit" ? "Edit plugin" : "Submit a plugin"}</h2>
      <form class="form" id="pluginForm">
        <div>
          <label for="id">Plugin ID</label>
          <input id="id" name="id" value="${plugin.id}" ${mode === "edit" ? "disabled" : "required"} />
        </div>
        <div>
          <label for="name">Name</label>
          <input id="name" name="name" value="${plugin.name}" required />
        </div>
        <div>
          <label for="compatibility">Compatibility</label>
          <input id="compatibility" name="compatibility" value="${plugin.compatibility}" required />
        </div>
        <button class="button" type="submit">Save</button>
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
        setStatus("Changes saved.", "success");
        location.hash = `#/plugins/${plugin.id}`;
        return;
      }
      await apiFetch("/plugins", {
        method: "POST",
        body: JSON.stringify(payload),
      });
      setStatus("Plugin created.", "success");
      location.hash = "#/my-plugins";
    } catch (error) {
      setStatus(`Error: ${error.message}`, "warning");
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
        <h2>Approval queue</h2>
        ${
          queue.length
            ? `
            <table class="table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Name</th>
                  <th>Version</th>
                  <th>Actions</th>
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
                      <button class="button success" data-action="approve" data-id="${plugin.id}">Approve</button>
                      <button class="button danger" data-action="reject" data-id="${plugin.id}">Reject</button>
                    </td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
            : `<div class="empty">No submissions awaiting review.</div>`
        }
      </div>
    `;

    view.querySelectorAll("button[data-action]").forEach((button) => {
      button.addEventListener("click", async () => {
        const action = button.dataset.action;
        const id = button.dataset.id;
        let reason = "";
        if (action === "reject") {
          reason = prompt("Rejection reason (optional):") ?? "";
        }
        try {
          const reasonParam = reason ? `?reason=${encodeURIComponent(reason)}` : "";
          await apiFetch(`/plugins/${id}/${action}${reasonParam}`, { method: "POST" });
          setStatus("Status updated.", "success");
          renderApprovalQueue();
        } catch (error) {
          setStatus(`Error: ${error.message}`, "warning");
        }
      });
    });
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Unable to load queue: ${error.message}</p></div>`;
  }
}

async function renderUserManagement() {
  renderLoading();
  try {
    const users = await apiFetch("/admin/users");
    view.innerHTML = `
      <div class="grid two">
        <div class="card">
          <h2>Users</h2>
          ${
            users.length
              ? `
            <table class="table">
              <thead>
                <tr>
                  <th>User</th>
                  <th>Role</th>
                  <th>New role</th>
                  <th>Status</th>
                  <th>New status</th>
                  <th>Password</th>
                  <th>Actions</th>
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
                    <td>${user.active ? "Active" : "Inactive"}</td>
                    <td>
                      <select data-active="${user.username}">
                        <option value="true">Active</option>
                        <option value="false">Inactive</option>
                      </select>
                    </td>
                    <td><input type="password" data-pass="${user.username}" placeholder="New password" /></td>
                    <td>
                      <button class="button" data-update="${user.username}">Save</button>
                      <button class="button danger" data-delete="${user.username}">Delete</button>
                    </td>
                  </tr>
                `
                  )
                  .join("")}
              </tbody>
            </table>
          `
              : `<div class="empty">No users found.</div>`
          }
        </div>
        <div class="card">
          <h2>Add user</h2>
          <form class="form" id="userForm">
            <div>
              <label for="newUsername">Username</label>
              <input id="newUsername" name="username" required />
            </div>
            <div>
              <label for="newPassword">Password</label>
              <input id="newPassword" name="password" type="password" required />
            </div>
            <div>
              <label for="newRole">Role</label>
              <select id="newRole" name="role">
                <option value="user">user</option>
                <option value="developer">developer</option>
                <option value="admin">admin</option>
              </select>
            </div>
            <div>
              <label for="newActive">Account status</label>
              <select id="newActive" name="active">
                <option value="true" selected>Active</option>
                <option value="false">Inactive</option>
              </select>
            </div>
            <button class="button" type="submit">Add user</button>
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
        const currentRole = users.find((user) => user.username === username)?.role;
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
          let reasonParam = "";
          if (role && role !== currentRole) {
            const reason = prompt("Reason for role change (optional):") ?? "";
            reasonParam = reason ? `?reason=${encodeURIComponent(reason)}` : "";
          }
          await apiFetch(`/admin/users/${username}${reasonParam}`, {
            method: "PATCH",
            body: JSON.stringify(payload),
          });
          setStatus("User updated.", "success");
          renderUserManagement();
        } catch (error) {
          setStatus(`Error: ${error.message}`, "warning");
        }
      });
    });

    view.querySelectorAll("button[data-delete]").forEach((button) => {
      button.addEventListener("click", async () => {
        const username = button.dataset.delete;
        if (!confirm(`Delete user ${username}?`)) {
          return;
        }
        try {
          await apiFetch(`/admin/users/${username}`, { method: "DELETE" });
          setStatus("User deleted.", "success");
          renderUserManagement();
        } catch (error) {
          setStatus(`Error: ${error.message}`, "warning");
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
        setStatus("User created.", "success");
        renderUserManagement();
      } catch (error) {
        setStatus(`Error: ${error.message}`, "warning");
      }
    });
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Unable to load users: ${error.message}</p></div>`;
  }
}

async function renderAuditLogs() {
  renderLoading();
  try {
    const logs = await apiFetch("/admin/audit-logs");
    const rows = logs
      .slice()
      .reverse()
      .map(
        (entry) => `
        <tr>
          <td>${entry.timestamp}</td>
          <td>${entry.actor}</td>
          <td>${entry.action}</td>
          <td>${entry.target}</td>
          <td>${entry.reason ? entry.reason : "-"}</td>
        </tr>
      `
      )
      .join("");
    view.innerHTML = `
      <div class="card">
        <h2>Audit logs</h2>
        ${
          logs.length
            ? `
            <table class="table">
              <thead>
                <tr>
                  <th>Timestamp</th>
                  <th>Actor</th>
                  <th>Action</th>
                  <th>Target</th>
                  <th>Reason</th>
                </tr>
              </thead>
              <tbody>
                ${rows}
              </tbody>
            </table>
          `
            : `<div class="empty">No audit log entries yet.</div>`
        }
      </div>
    `;
  } catch (error) {
    view.innerHTML = `<div class="card"><p>Unable to load audit logs: ${error.message}</p></div>`;
  }
}

function route() {
  setStatus("");
  renderNav();
  const hash = location.hash || "#/home";
  const routes = [
    { pattern: /^#\/home$/, view: renderHome },
    { pattern: /^#\/plugins$/, view: renderPluginList },
    { pattern: /^#\/plugins\/$/, view: renderPluginList },
    { pattern: /^#\/$/, view: renderHome },
    { pattern: /^#\/plugins\/new$/, view: () => renderPluginForm("create") },
    { pattern: /^#\/plugins\/([^/]+)\/edit$/, view: (id) => renderPluginForm("edit", id) },
    { pattern: /^#\/plugins\/([^/]+)$/, view: (id) => renderPluginDetail(id) },
    { pattern: /^#\/my-plugins$/, view: renderMyPlugins },
    { pattern: /^#\/admin\/queue$/, view: renderApprovalQueue },
    { pattern: /^#\/admin\/users$/, view: renderUserManagement },
    { pattern: /^#\/admin\/audit$/, view: renderAuditLogs },
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
