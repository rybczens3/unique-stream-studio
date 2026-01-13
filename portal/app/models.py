from pydantic import BaseModel, Field


class PluginMetadata(BaseModel):
    id: str = Field(..., description="Unique plugin identifier")
    name: str
    version: str
    compatibility: str
    package_url: str
    sha256: str
    signature: str


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


class PluginCreateRequest(BaseModel):
    id: str
    name: str
    compatibility: str


class PluginVersionRequest(BaseModel):
    version: str
    package_url: str
    sha256: str
    signature: str
