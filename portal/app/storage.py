import hashlib
from dataclasses import dataclass, field
from typing import Dict, List, Set


def package_bytes(plugin_id: str, version: str) -> bytes:
    return f"PLUGIN:{plugin_id}:{version}".encode("utf-8")


def compute_sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def signature_for(sha256: str) -> str:
    return f"sha256:{sha256}"


@dataclass
class PluginVersion:
    version: str
    package_url: str
    sha256: str
    signature: str


@dataclass
class PluginRecord:
    id: str
    name: str
    compatibility: str
    versions: List[PluginVersion] = field(default_factory=list)


ROLES: Set[str] = {"user", "developer", "admin"}
PERMISSIONS = {
    "user": {"plugins:read"},
    "developer": {"plugins:read", "plugins:write"},
    "admin": {"plugins:read", "plugins:write", "users:manage"},
}

USERS: Dict[str, Dict[str, str]] = {}

TOKENS: Dict[str, Dict[str, str]] = {}

PLUGINS: Dict[str, PluginRecord] = {}


def seed_users() -> None:
    USERS.update(
        {
            "admin": {"password": "admin123", "role": "admin"},
            "developer": {"password": "dev123", "role": "developer"},
            "user": {"password": "user123", "role": "user"},
        }
    )


def seed_plugins(base_url: str) -> None:
    versions = []
    for version in ["1.0.0", "1.1.0", "2.0.0"]:
        data = package_bytes("com.example.stream-overlay", version)
        sha256 = compute_sha256(data)
        versions.append(
            PluginVersion(
                version=version,
                package_url=f"{base_url}/plugins/com.example.stream-overlay/package?version={version}",
                sha256=sha256,
                signature=signature_for(sha256),
            )
        )

    PLUGINS["com.example.stream-overlay"] = PluginRecord(
        id="com.example.stream-overlay",
        name="Stream Overlay",
        compatibility="obs>=30.0.0",
        versions=versions,
    )

    data = package_bytes("com.example.chat-enhancer", "1.0.0")
    sha256 = compute_sha256(data)
    PLUGINS["com.example.chat-enhancer"] = PluginRecord(
        id="com.example.chat-enhancer",
        name="Chat Enhancer",
        compatibility="obs>=29.0.0",
        versions=[
            PluginVersion(
                version="1.0.0",
                package_url=f"{base_url}/plugins/com.example.chat-enhancer/package?version=1.0.0",
                sha256=sha256,
                signature=signature_for(sha256),
            )
        ],
    )
