# Plugin Portal Backend

This directory contains a minimal standalone backend for the plugin portal. It exposes endpoints for
listing/searching plugins, managing versions, downloading packages, and handling user/admin accounts.

## Running locally

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --reload --host 0.0.0.0 --port 8080
```

Base URL for the frontend client: `http://localhost:8080/portal/api`.

## API overview

- `GET /portal/api/plugins?query=` — list or search plugins.
- `GET /portal/api/plugins/{plugin_id}` — plugin details.
- `GET /portal/api/plugins/{plugin_id}/versions` — list versions.
- `GET /portal/api/plugins/{plugin_id}/package?version=` — download package.
- `POST /portal/api/auth/login` — login and return tokens.
- `GET /portal/api/account/me` — account metadata for current token.
- `POST /portal/api/admin/plugins` — admin-only create plugin.
- `POST /portal/api/admin/plugins/{plugin_id}/versions` — admin-only add version.

Authentication uses bearer tokens returned by `/auth/login`. Admin endpoints require role `admin`.

## Plugin metadata format

The portal returns metadata objects with these fields:

```json
{
  "id": "com.example.my-plugin",
  "name": "My Plugin",
  "version": "1.2.0",
  "compatibility": "obs>=30.0.0",
  "package_url": "https://downloads.example.com/plugins/my-plugin-1.2.0.pkg",
  "sha256": "<hex sha256>",
  "signature": "sha256:<hex sha256>"
}
```

- `id`: unique plugin identifier.
- `version`: semantic version string.
- `compatibility`: OBS compatibility range.
- `package_url`: direct download URL for the package.
- `sha256`: SHA-256 checksum of the package.
- `signature`: signature string (placeholder format `sha256:<sha256>` for now).

See `docs/metadata.md` for more details.
