from pydantic import BaseModel, Field


class PluginMetadata(BaseModel):
    id: str = Field(
        ...,
        description="Unique plugin identifier",
        min_length=3,
        max_length=64,
        pattern=r"^[a-z0-9]+([.-][a-z0-9]+)*$",
    )
    name: str = Field(..., min_length=3, max_length=120)
    version: str
    compatibility: str = Field(..., min_length=3, max_length=120)
    package_url: str
    sha256: str
    signature: str


class PluginRecordInfo(BaseModel):
    id: str = Field(
        ...,
        description="Unique plugin identifier",
        min_length=3,
        max_length=64,
        pattern=r"^[a-z0-9]+([.-][a-z0-9]+)*$",
    )
    name: str = Field(..., min_length=3, max_length=120)
    compatibility: str = Field(..., min_length=3, max_length=120)
    owner: str
    status: str
    version: str | None = None
    package_url: str | None = None
    sha256: str | None = None
    signature: str | None = None


class LoginRequest(BaseModel):
    username: str
    password: str


class TokenResponse(BaseModel):
    username: str
    role: str
    access_token: str
    refresh_token: str
    token_type: str = "bearer"


class AccountInfo(BaseModel):
    username: str
    role: str


class UserInfo(BaseModel):
    username: str
    role: str
    active: bool


class UserCreateRequest(BaseModel):
    username: str
    password: str
    role: str
    active: bool = True


class UserUpdateRequest(BaseModel):
    password: str | None = None
    role: str | None = None
    active: bool | None = None


class PluginCreateRequest(BaseModel):
    id: str = Field(..., min_length=3, max_length=64, pattern=r"^[a-z0-9]+([.-][a-z0-9]+)*$")
    name: str = Field(..., min_length=3, max_length=120)
    compatibility: str = Field(..., min_length=3, max_length=120)


class PluginUpdateRequest(BaseModel):
    name: str = Field(..., min_length=3, max_length=120)
    compatibility: str = Field(..., min_length=3, max_length=120)


class PluginVersionRequest(BaseModel):
    version: str
    package_url: str
    sha256: str
    signature: str


class AuditLogEntry(BaseModel):
    id: str
    timestamp: str
    actor: str
    action: str
    target: str
    reason: str | None = None
