from __future__ import annotations

import uuid
from typing import List, Optional

from fastapi import FastAPI, Header, HTTPException, Query
from fastapi.responses import Response

from .models import (
    AccountInfo,
    LoginRequest,
    PluginCreateRequest,
    PluginMetadata,
    PluginUpdateRequest,
    PluginVersionRequest,
    TokenResponse,
    UserCreateRequest,
    UserInfo,
    UserUpdateRequest,
)
from .storage import (
    PLUGINS,
    PERMISSIONS,
    ROLES,
    TOKENS,
    USERS,
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


@app.post(BASE_PATH + "/auth/login", response_model=TokenResponse)
def login(payload: LoginRequest) -> TokenResponse:
    user = USERS.get(payload.username)
    if not user or user["password"] != payload.password:
        raise HTTPException(status_code=401, detail="Invalid credentials")
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
    results: List[PluginMetadata] = []
    for plugin in PLUGINS.values():
        latest = plugin.versions[-1]
        results.append(
            PluginMetadata(
                id=plugin.id,
                name=plugin.name,
                version=latest.version,
                compatibility=plugin.compatibility,
                package_url=latest.package_url,
                sha256=latest.sha256,
                signature=latest.signature,
            )
        )
    return results


@app.get(BASE_PATH + "/plugins", response_model=List[PluginMetadata])
def list_plugins(
    query: Optional[str] = Query(default=None),
) -> List[PluginMetadata]:
    results: List[PluginMetadata] = []
    for plugin in PLUGINS.values():
        if query and query.lower() not in plugin.name.lower() and query.lower() not in plugin.id.lower():
            continue
        latest = plugin.versions[-1]
        results.append(
            PluginMetadata(
                id=plugin.id,
                name=plugin.name,
                version=latest.version,
                compatibility=plugin.compatibility,
                package_url=latest.package_url,
                sha256=latest.sha256,
                signature=latest.signature,
            )
        )
    return results


@app.get(BASE_PATH + "/plugins/{plugin_id}", response_model=PluginMetadata)
def plugin_detail(plugin_id: str) -> PluginMetadata:
    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")
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


@app.get(BASE_PATH + "/plugins/{plugin_id}/versions", response_model=List[PluginMetadata])
def plugin_versions(plugin_id: str) -> List[PluginMetadata]:
    plugin = PLUGINS.get(plugin_id)
    if not plugin:
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
    if not plugin:
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

    if payload.id in PLUGINS:
        raise HTTPException(status_code=409, detail="Plugin already exists")

    PLUGINS[payload.id] = plugin = PLUGINS.setdefault(
        payload.id,
        PluginRecord(id=payload.id, name=payload.name, compatibility=payload.compatibility, versions=[]),
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

    plugin = PLUGINS.get(plugin_id)
    if not plugin:
        raise HTTPException(status_code=404, detail="Plugin not found")

    plugin.name = payload.name
    plugin.compatibility = payload.compatibility
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


@app.delete(BASE_PATH + "/admin/plugins/{plugin_id}", status_code=204)
def delete_plugin(plugin_id: str, authorization: Optional[str] = Header(None)) -> None:
    session = require_session(authorization)
    require_permission(session, "plugins:write")

    if plugin_id not in PLUGINS:
        raise HTTPException(status_code=404, detail="Plugin not found")
    del PLUGINS[plugin_id]


@app.get(BASE_PATH + "/admin/users", response_model=List[UserInfo])
def list_users(authorization: Optional[str] = Header(None)) -> List[UserInfo]:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    return [UserInfo(username=username, role=user["role"]) for username, user in USERS.items()]


@app.post(BASE_PATH + "/admin/users", response_model=UserInfo)
def create_user(payload: UserCreateRequest, authorization: Optional[str] = Header(None)) -> UserInfo:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    if payload.role not in ROLES:
        raise HTTPException(status_code=400, detail="Invalid role")
    if payload.username in USERS:
        raise HTTPException(status_code=409, detail="User already exists")
    USERS[payload.username] = {"password": payload.password, "role": payload.role}
    return UserInfo(username=payload.username, role=payload.role)


@app.patch(BASE_PATH + "/admin/users/{username}", response_model=UserInfo)
def update_user(
    username: str, payload: UserUpdateRequest, authorization: Optional[str] = Header(None)
) -> UserInfo:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    user = USERS.get(username)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    if payload.role:
        if payload.role not in ROLES:
            raise HTTPException(status_code=400, detail="Invalid role")
        user["role"] = payload.role
    if payload.password:
        user["password"] = payload.password
    return UserInfo(username=username, role=user["role"])


@app.delete(BASE_PATH + "/admin/users/{username}", status_code=204)
def delete_user(username: str, authorization: Optional[str] = Header(None)) -> None:
    session = require_session(authorization)
    require_permission(session, "users:manage")
    if username not in USERS:
        raise HTTPException(status_code=404, detail="User not found")
    del USERS[username]
