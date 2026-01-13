from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import List, Optional

from fastapi import FastAPI, Header, HTTPException, Query
from fastapi.responses import Response

from .models import (
    AccountInfo,
    AuditLogEntry as AuditLogEntryResponse,
    LoginRequest,
    PluginCreateRequest,
    PluginMetadata,
    PluginRecordInfo,
    PluginUpdateRequest,
    PluginVersionRequest,
    TokenResponse,
    UserCreateRequest,
    UserInfo,
    UserUpdateRequest,
)
from .storage import (
    AUDIT_LOGS,
    PLUGINS,
    PERMISSIONS,
    ROLES,
    TOKENS,
    USERS,
    AuditLogEntry,
    PluginRecord,
    PluginVersion,
    compute_sha256,
    package_bytes,
    seed_users,
    seed_plugins,
    signature_for,
)

app = FastAPI(title="Plugin Portal API")

BASE_PATH = "/portal/api"

seed_plugins("http://localhost:8080" + BASE_PATH)
seed_users()


def get_session(authorization: Optional[str]) -> Optional[dict]:
    if not authorization:
        return None
    if not authorization.lower().startswith("bearer "):
        return None
    token = authorization.split(" ", 1)[1]
    return TOKENS.get(token)


def require_session(authorization: Optional[str]) -> dict:
    session = get_session(authorization)
    if not session:
        raise HTTPException(status_code=401, detail="Unauthorized")
    return session


def require_permission(session: dict, permission: str) -> None:
    role = session.get("role")
    allowed = PERMISSIONS.get(role, set())
    if permission not in allowed:
        raise HTTPException(status_code=403, detail="Forbidden")


def require_role(session: dict, roles: set[str]) -> None:
    if session.get("role") not in roles:
        raise HTTPException(status_code=403, detail="Forbidden")


def require_owner_or_admin(session: dict, plugin: PluginRecord) -> None:
    if session.get("role") == "admin":
        return
    if session.get("username") != plugin.owner:
        raise HTTPException(status_code=403, detail="Forbidden")


def latest_version_metadata(plugin: PluginRecord) -> Optional[PluginMetadata]:
    if not plugin.versions:
        return None
    latest = plugin.versions[-1]
    return PluginMetadata(
        id=plugin.id,
        name=plugin.name,
        version=latest.version,
        compatibility=plugin.compatibility,
        package_url=latest.package_url,
        sha256=latest.sha256,
        signature=latest.signature,
    )


def plugin_record_info(plugin: PluginRecord) -> PluginRecordInfo:
    latest = plugin.versions[-1] if plugin.versions else None
    return PluginRecordInfo(
        id=plugin.id,
        name=plugin.name,
        compatibility=plugin.compatibility,
        owner=plugin.owner,
        status=plugin.status,
        version=latest.version if latest else None,
        package_url=latest.package_url if latest else None,
        sha256=latest.sha256 if latest else None,
        signature=latest.signature if latest else None,
    )


def record_audit_event(actor: str, action: str, target: str, reason: Optional[str] = None) -> None:
    AUDIT_LOGS.append(
        AuditLogEntry(
            id=uuid.uuid4().hex,
            timestamp=datetime.now(timezone.utc).isoformat(),
            actor=actor,
            action=action,
            target=target,
            reason=reason,
        )
    )


@app.post(BASE_PATH + "/auth/login", response_model=TokenResponse)
def login(payload: LoginRequest) -> TokenResponse:
    user = USERS.get(payload.username)
    if not user or user["password"] != payload.password:
        raise HTTPException(status_code=401, detail="Invalid credentials")
    if not user.get("active", True):
        raise HTTPException(status_code=403, detail="Account is inactive")
    access_token = uuid.uuid4().hex
    refresh_token = uuid.uuid4().hex
    TOKENS[access_token] = {
        "username": payload.username,
        "role": user["role"],
        "refresh_token": refresh_token,
    }
    return TokenResponse(
        username=payload.username,
        role=user["role"],
        access_token=access_token,
        refresh_token=refresh_token,
    )


@app.get(BASE_PATH + "/account/me", response_model=AccountInfo)
def account_me(authorization: Optional[str] = Header(None)) -> AccountInfo:
    session = require_session(authorization)
    return AccountInfo(username=session["username"], role=session["role"])


@app.get(BASE_PATH + "/admin/plugins", response_model=List[PluginMetadata])
def list_plugins_admin(authorization: Optional[str] = Header(None)) -> List[PluginMetadata]:
    session = require_session(authorization)
    require_permission(session, "plugins:read")
    require_role(session, {"admin"})
    results: List[PluginMetadata] = []
    for plugin in PLUGINS.values():
        latest = latest_version_metadata(plugin)
        if not latest:
            continue
        results.append(latest)
    return results


@app.get(BASE_PATH + "/plugins", response_model=List[PluginMetadata])
def list_plugins(
    query: Optional[str] = Query(default=None),
) -> List[PluginMetadata]:
    results: List[PluginMetadata] = []
    for plugin in PLUGINS.values():
        if plugin.status != "published":
            continue
        if query and query.lower() not in plugin.name.lower() and query.lower() not in plugin.id.lower():
            continue
        latest = latest_version_metadata(plugin)
        if not latest:
            continue
        results.append(latest)
    return results


@app.get(BASE_PATH + "/plugins/{plugin_id}", response_model=PluginMetadata)
def plugin_detail(plugin_id: str) -> PluginMetadata:
    plugin = PLUGINS.get(plugin_id)
    if not plugin or plugin.status != "published":
        raise HTTPException(status_code=404, detail="Plugin not found")
    latest = latest_version_metadata(plugin)
    if not latest:
        raise HTTPException(status_code=404, detail="Plugin not found")
    return latest


@app.get(BASE_PATH + "/plugins/{plugin_id}/versions", response_model=List[PluginMetadata])
def plugin_versions(plugin_id: str) -> List[PluginMetadata]:
    plugin = PLUGINS.get(plugin_id)
    if not plugin or plugin.status != "published":
        raise HTTPException(status_code=404, detail="Plugin not found")
    return [
        PluginMetadata(
            id=plugin.id,
            name=plugin.name,
            version=version.version,
            compatibility=plugin.compatibility,
            package_url=version.package_url,
            sha256=version.sha256,
            signature=version.signature,
        )
        for version in plugin.versions
    ]


@app.get(BASE_PATH + "/plugins/{plugin_id}/package")
def download_package(plugin_id: str, version: str = Query(...)) -> Response:
    plugin = PLUGINS.get(plugin_id)
    if not plugin or plugin.status != "published":
        raise HTTPException(status_code=404, detail="Plugin not found")
    version_entry = next((v for v in plugin.versions if v.version == version), None)
    if not version_entry:
        raise HTTPException(status_code=404, detail="Version not found")
    data = package_bytes(plugin_id, version)
    return Response(content=data, media_type="application/octet-stream")


@app.post(BASE_PATH + "/admin/plugins", response_model=PluginMetadata)
def create_plugin(
    payload: PluginCreateRequest,
    authorization: Optional[str] = Header(None),
) -> PluginMetadata:
    session = require_session(authorization)
    require_permission(session, "plugins:write")
    require_role(session, {"admin"})

    if payload.id in PLUGINS:
        raise HTTPException(status_code=409, detail="Plugin already exists")

    PLUGINS[payload.id] = plugin = PLUGINS.setdefault(
        payload.id,
        PluginRecord(
            id=payload.id,
            name=payload.name,
            compatibility=payload.compatibility,
            owner=session["username"],
            status="published",
            versions=[],
        ),
    )

    data = package_bytes(payload.id, "1.0.0")
    sha256 = compute_sha256(data)
    version = PluginVersion(
        version="1.0.0",
        package_url=f"http://localhost:8080{BASE_PATH}/plugins/{payload.id}/package?version=1.0.0",
        sha256=sha256,
        signature=signature_for(sha256),
    )
    plugin.versions.append(version)

    return PluginMetadata(
        id=plugin.id,
        name=plugin.name,
        version=version.version,
        compatibility=plugin.compatibility,
        package_url=version.package_url,
        sha256=version.sha256,
        signature=version.signature,
    )


@app.post(BASE_PATH + "/admin/plugins/{plugin_id}/versions", response_model=PluginMetadata)
def add_plugin_version(
    plugin_id: str,
    payload: PluginVersionRequest,
    authorization: Optional[str] = Header(None),
) -> PluginMetadata:
    session = require_session(authorization)
    require_permission(session, "plugins:write")
    require_role(session, {"admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")

    plugin.versions.append(
        PluginVersion(
            version=payload.version,
            package_url=payload.package_url,
            sha256=payload.sha256,
            signature=payload.signature,
        )
    )

    return PluginMetadata(
        id=plugin.id,
        name=plugin.name,
        version=payload.version,
        compatibility=plugin.compatibility,
        package_url=payload.package_url,
        sha256=payload.sha256,
        signature=payload.signature,
    )


@app.put(BASE_PATH + "/admin/plugins/{plugin_id}", response_model=PluginMetadata)
def update_plugin(
    plugin_id: str,
    payload: PluginUpdateRequest,
    authorization: Optional[str] = Header(None),
) -> PluginMetadata:
    session = require_session(authorization)
    require_permission(session, "plugins:write")
    require_role(session, {"admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")

    plugin.name = payload.name
    plugin.compatibility = payload.compatibility
    latest = plugin.versions[-1]
    record_audit_event(
        actor=session["username"],
        action="plugin.edited",
        target=plugin.id,
    )
    return PluginMetadata(
        id=plugin.id,
        name=plugin.name,
        version=latest.version,
        compatibility=plugin.compatibility,
        package_url=latest.package_url,
        sha256=latest.sha256,
        signature=latest.signature,
    )


@app.delete(BASE_PATH + "/admin/plugins/{plugin_id}", status_code=204)
def delete_plugin(
    plugin_id: str,
    authorization: Optional[str] = Header(None),
    reason: Optional[str] = Query(default=None),
) -> None:
    session = require_session(authorization)
    require_permission(session, "plugins:write")
    require_role(session, {"admin"})

    if plugin_id not in PLUGINS:
        raise HTTPException(status_code=404, detail="Plugin not found")
    del PLUGINS[plugin_id]
    record_audit_event(
        actor=session["username"],
        action="plugin.deleted",
        target=plugin_id,
        reason=reason,
    )


@app.post(BASE_PATH + "/plugins", response_model=PluginRecordInfo)
def create_plugin_self(
    payload: PluginCreateRequest,
    authorization: Optional[str] = Header(None),
) -> PluginRecordInfo:
    session = require_session(authorization)
    require_permission(session, "plugins:write")

    if payload.id in PLUGINS:
        raise HTTPException(status_code=409, detail="Plugin already exists")

    data = package_bytes(payload.id, "0.1.0")
    sha256 = compute_sha256(data)
    version = PluginVersion(
        version="0.1.0",
        package_url=f"http://localhost:8080{BASE_PATH}/plugins/{payload.id}/package?version=0.1.0",
        sha256=sha256,
        signature=signature_for(sha256),
    )

    PLUGINS[payload.id] = plugin = PluginRecord(
        id=payload.id,
        name=payload.name,
        compatibility=payload.compatibility,
        owner=session["username"],
        status="draft",
        versions=[version],
    )

    return plugin_record_info(plugin)


@app.get(BASE_PATH + "/plugins/mine", response_model=List[PluginRecordInfo])
def list_my_plugins(authorization: Optional[str] = Header(None)) -> List[PluginRecordInfo]:
    session = require_session(authorization)
    require_permission(session, "plugins:write")

    results: List[PluginRecordInfo] = []
    for plugin in PLUGINS.values():
        if session["role"] != "admin" and plugin.owner != session["username"]:
            continue
        results.append(plugin_record_info(plugin))
    return results


@app.get(BASE_PATH + "/plugins/{plugin_id}/manage", response_model=PluginRecordInfo)
def get_plugin_manage(plugin_id: str, authorization: Optional[str] = Header(None)) -> PluginRecordInfo:
    session = require_session(authorization)
    require_permission(session, "plugins:write")

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    require_owner_or_admin(session, plugin)
    return plugin_record_info(plugin)


@app.put(BASE_PATH + "/plugins/{plugin_id}", response_model=PluginRecordInfo)
def update_plugin_self(
    plugin_id: str,
    payload: PluginUpdateRequest,
    authorization: Optional[str] = Header(None),
) -> PluginRecordInfo:
    session = require_session(authorization)
    require_permission(session, "plugins:write")

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    require_owner_or_admin(session, plugin)

    plugin.name = payload.name
    plugin.compatibility = payload.compatibility
    record_audit_event(
        actor=session["username"],
        action="plugin.edited",
        target=plugin.id,
    )
    return plugin_record_info(plugin)


@app.delete(BASE_PATH + "/plugins/{plugin_id}", status_code=204)
def delete_plugin_self(
    plugin_id: str,
    authorization: Optional[str] = Header(None),
    reason: Optional[str] = Query(default=None),
) -> None:
    session = require_session(authorization)
    require_permission(session, "plugins:write")

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    require_owner_or_admin(session, plugin)
    del PLUGINS[plugin_id]
    record_audit_event(
        actor=session["username"],
        action="plugin.deleted",
        target=plugin_id,
        reason=reason,
    )


@app.post(BASE_PATH + "/plugins/{plugin_id}/submit", response_model=PluginRecordInfo)
def submit_plugin(plugin_id: str, authorization: Optional[str] = Header(None)) -> PluginRecordInfo:
    session = require_session(authorization)
    require_permission(session, "plugins:write")
    require_role(session, {"developer", "admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    require_owner_or_admin(session, plugin)
    if plugin.status not in {"draft", "rejected", "unpublished"}:
        raise HTTPException(status_code=400, detail="Plugin cannot be submitted")
    plugin.status = "submitted"
    return plugin_record_info(plugin)


@app.post(BASE_PATH + "/plugins/{plugin_id}/approve", response_model=PluginRecordInfo)
def approve_plugin(plugin_id: str, authorization: Optional[str] = Header(None)) -> PluginRecordInfo:
    session = require_session(authorization)
    require_role(session, {"admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    if plugin.status != "submitted":
        raise HTTPException(status_code=400, detail="Plugin cannot be approved")
    plugin.status = "approved"
    return plugin_record_info(plugin)


@app.post(BASE_PATH + "/plugins/{plugin_id}/reject", response_model=PluginRecordInfo)
def reject_plugin(
    plugin_id: str,
    authorization: Optional[str] = Header(None),
    reason: Optional[str] = Query(default=None),
) -> PluginRecordInfo:
    session = require_session(authorization)
    require_role(session, {"admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    if plugin.status != "submitted":
        raise HTTPException(status_code=400, detail="Plugin cannot be rejected")
    plugin.status = "rejected"
    record_audit_event(
        actor=session["username"],
        action="plugin.rejected",
        target=plugin.id,
        reason=reason,
    )
    return plugin_record_info(plugin)


@app.post(BASE_PATH + "/plugins/{plugin_id}/publish", response_model=PluginRecordInfo)
def publish_plugin(
    plugin_id: str,
    authorization: Optional[str] = Header(None),
    reason: Optional[str] = Query(default=None),
) -> PluginRecordInfo:
    session = require_session(authorization)
    require_role(session, {"admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    if plugin.status != "approved":
        raise HTTPException(status_code=400, detail="Plugin cannot be published")
    if not plugin.versions:
        raise HTTPException(status_code=400, detail="Plugin has no versions to publish")
    plugin.status = "published"
    record_audit_event(
        actor=session["username"],
        action="plugin.published",
        target=plugin.id,
        reason=reason,
    )
    return plugin_record_info(plugin)


@app.post(BASE_PATH + "/plugins/{plugin_id}/unpublish", response_model=PluginRecordInfo)
def unpublish_plugin(plugin_id: str, authorization: Optional[str] = Header(None)) -> PluginRecordInfo:
    session = require_session(authorization)
    require_role(session, {"admin"})

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
    if plugin.status != "published":
        raise HTTPException(status_code=400, detail="Plugin cannot be unpublished")
    plugin.status = "unpublished"
    return plugin_record_info(plugin)


@app.get(BASE_PATH + "/admin/users", response_model=List[UserInfo])
def list_users(authorization: Optional[str] = Header(None)) -> List[UserInfo]:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    require_role(session, {"admin"})
    return [
        UserInfo(username=username, role=user["role"], active=user.get("active", True))
        for username, user in USERS.items()
    ]


@app.post(BASE_PATH + "/admin/users", response_model=UserInfo)
def create_user(payload: UserCreateRequest, authorization: Optional[str] = Header(None)) -> UserInfo:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    require_role(session, {"admin"})
    if payload.role not in ROLES:
        raise HTTPException(status_code=400, detail="Invalid role")
    if payload.username in USERS:
        raise HTTPException(status_code=409, detail="User already exists")
    USERS[payload.username] = {
        "password": payload.password,
        "role": payload.role,
        "active": payload.active,
    }
    return UserInfo(username=payload.username, role=payload.role, active=payload.active)


@app.patch(BASE_PATH + "/admin/users/{username}", response_model=UserInfo)
def update_user(
    username: str,
    payload: UserUpdateRequest,
    authorization: Optional[str] = Header(None),
    reason: Optional[str] = Query(default=None),
) -> UserInfo:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    require_role(session, {"admin"})
    user = USERS.get(username)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    if payload.role:
        if payload.role not in ROLES:
            raise HTTPException(status_code=400, detail="Invalid role")
        if payload.role != user["role"]:
            record_audit_event(
                actor=session["username"],
                action="user.role_changed",
                target=username,
                reason=reason,
            )
        user["role"] = payload.role
    if payload.password:
        user["password"] = payload.password
    if payload.active is not None:
        user["active"] = payload.active
    return UserInfo(username=username, role=user["role"], active=user.get("active", True))


@app.delete(BASE_PATH + "/admin/users/{username}", status_code=204)
def delete_user(username: str, authorization: Optional[str] = Header(None)) -> None:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    require_role(session, {"admin"})
    if username not in USERS:
        raise HTTPException(status_code=404, detail="User not found")
    del USERS[username]


@app.get(BASE_PATH + "/admin/audit-logs", response_model=List[AuditLogEntryResponse])
def list_audit_logs(authorization: Optional[str] = Header(None)) -> List[AuditLogEntryResponse]:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    require_role(session, {"admin"})
    return [
        AuditLogEntryResponse(
            id=entry.id,
            timestamp=entry.timestamp,
            actor=entry.actor,
            action=entry.action,
            target=entry.target,
            reason=entry.reason,
        )
        for entry in AUDIT_LOGS
    ]
