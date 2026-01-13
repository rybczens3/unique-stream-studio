# Plugin Metadata Schema

This document describes the plugin metadata payloads returned by the portal API.

## Core fields

| Field | Type | Description |
| --- | --- | --- |
| `id` | string | Unique plugin ID (reverse-DNS or slug). |
| `name` | string | Human-friendly name. |
| `version` | string | Semantic version string. |
| `compatibility` | string | OBS compatibility range (e.g. `obs>=30.0.0`). |
| `package_url` | string | Direct package download URL. |
| `sha256` | string | Hex SHA-256 checksum for package verification. |
| `signature` | string | Signature to validate authenticity. Placeholder format: `sha256:<sha256>`. |

## Example

```json
{
  "id": "com.example.my-plugin",
  "name": "My Plugin",
  "version": "1.2.0",
  "compatibility": "obs>=30.0.0",
  "package_url": "https://downloads.example.com/plugins/my-plugin-1.2.0.pkg",
  "sha256": "44b1385cbd9f0a4c1a4d3f43ffbd9d19fbe2e6920c1c1c2b1084f3b28f1cfa56",
  "signature": "sha256:44b1385cbd9f0a4c1a4d3f43ffbd9d19fbe2e6920c1c1c2b1084f3b28f1cfa56"
}
```
