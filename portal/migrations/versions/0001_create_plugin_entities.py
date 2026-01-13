"""create plugin and addon entities

Revision ID: 0001_create_plugin_entities
Revises: 
Create Date: 2025-02-14 00:00:00.000000
"""
from __future__ import annotations

from alembic import op
import sqlalchemy as sa

# revision identifiers, used by Alembic.
revision = "0001_create_plugin_entities"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.create_table(
        "plugins",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("identifier", sa.String(length=255), nullable=False, unique=True),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("description", sa.Text(), nullable=False),
        sa.Column("developer", sa.String(length=255), nullable=False),
        sa.Column("publication_status", sa.String(length=50), nullable=False),
        sa.Column("compatibility", sa.String(length=255), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
    )
    op.create_index("ix_plugins_search", "plugins", ["name", "developer"])
    op.create_index("ix_plugins_name", "plugins", ["name"])
    op.create_index("ix_plugins_developer", "plugins", ["developer"])
    op.create_index("ix_plugins_publication_status", "plugins", ["publication_status"])
    op.create_index("ix_plugins_compatibility", "plugins", ["compatibility"])

    op.create_table(
        "plugin_versions",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("plugin_id", sa.Integer(), nullable=False),
        sa.Column("version", sa.String(length=64), nullable=False),
        sa.Column("compatibility", sa.String(length=255), nullable=False),
        sa.Column("publication_status", sa.String(length=50), nullable=False),
        sa.Column("binary_url", sa.String(length=1024), nullable=False),
        sa.Column("published_at", sa.DateTime(timezone=True), nullable=False),
        sa.ForeignKeyConstraint(["plugin_id"], ["plugins.id"]),
    )
    op.create_index("ix_plugin_versions_plugin_id", "plugin_versions", ["plugin_id"])
    op.create_index("ix_plugin_versions_version", "plugin_versions", ["version"])
    op.create_index(
        "ix_plugin_versions_search",
        "plugin_versions",
        ["version", "publication_status", "published_at"],
    )
    op.create_index("ix_plugin_versions_compatibility", "plugin_versions", ["compatibility"])
    op.create_index(
        "ix_plugin_versions_publication_status", "plugin_versions", ["publication_status"]
    )
    op.create_index("ix_plugin_versions_published_at", "plugin_versions", ["published_at"])

    op.create_table(
        "addons",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("description", sa.Text(), nullable=False),
        sa.Column("developer", sa.String(length=255), nullable=False),
        sa.Column("version", sa.String(length=64), nullable=False),
        sa.Column("publication_status", sa.String(length=50), nullable=False),
        sa.Column("compatibility", sa.String(length=255), nullable=False),
        sa.Column("binary_url", sa.String(length=1024), nullable=False),
        sa.Column("published_at", sa.DateTime(timezone=True), nullable=False),
    )
    op.create_index("ix_addons_search", "addons", ["name", "developer"])
    op.create_index("ix_addons_name", "addons", ["name"])
    op.create_index("ix_addons_developer", "addons", ["developer"])
    op.create_index("ix_addons_version", "addons", ["version"])
    op.create_index("ix_addons_publication_status", "addons", ["publication_status"])
    op.create_index("ix_addons_compatibility", "addons", ["compatibility"])
    op.create_index("ix_addons_published_at", "addons", ["published_at"])


def downgrade() -> None:
    op.drop_index("ix_addons_published_at", table_name="addons")
    op.drop_index("ix_addons_compatibility", table_name="addons")
    op.drop_index("ix_addons_publication_status", table_name="addons")
    op.drop_index("ix_addons_version", table_name="addons")
    op.drop_index("ix_addons_developer", table_name="addons")
    op.drop_index("ix_addons_name", table_name="addons")
    op.drop_index("ix_addons_search", table_name="addons")
    op.drop_table("addons")

    op.drop_index("ix_plugin_versions_published_at", table_name="plugin_versions")
    op.drop_index("ix_plugin_versions_publication_status", table_name="plugin_versions")
    op.drop_index("ix_plugin_versions_compatibility", table_name="plugin_versions")
    op.drop_index("ix_plugin_versions_search", table_name="plugin_versions")
    op.drop_index("ix_plugin_versions_version", table_name="plugin_versions")
    op.drop_index("ix_plugin_versions_plugin_id", table_name="plugin_versions")
    op.drop_table("plugin_versions")

    op.drop_index("ix_plugins_compatibility", table_name="plugins")
    op.drop_index("ix_plugins_publication_status", table_name="plugins")
    op.drop_index("ix_plugins_developer", table_name="plugins")
    op.drop_index("ix_plugins_name", table_name="plugins")
    op.drop_index("ix_plugins_search", table_name="plugins")
    op.drop_table("plugins")
