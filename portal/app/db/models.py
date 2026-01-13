from __future__ import annotations

from datetime import datetime

from sqlalchemy import DateTime, ForeignKey, Index, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .base import Base


class Plugin(Base):
    __tablename__ = "plugins"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    identifier: Mapped[str] = mapped_column(String(255), unique=True, nullable=False)
    name: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    description: Mapped[str] = mapped_column(Text, nullable=False)
    developer: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    publication_status: Mapped[str] = mapped_column(String(50), index=True, nullable=False)
    compatibility: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)

    versions: Mapped[list[PluginVersion]] = relationship(
        back_populates="plugin", cascade="all, delete-orphan"
    )

    __table_args__ = (Index("ix_plugins_search", "name", "developer"),)


class PluginVersion(Base):
    __tablename__ = "plugin_versions"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    plugin_id: Mapped[int] = mapped_column(ForeignKey("plugins.id"), index=True, nullable=False)
    version: Mapped[str] = mapped_column(String(64), index=True, nullable=False)
    compatibility: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    publication_status: Mapped[str] = mapped_column(String(50), index=True, nullable=False)
    binary_url: Mapped[str] = mapped_column(String(1024), nullable=False)
    published_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True, nullable=False)

    plugin: Mapped[Plugin] = relationship(back_populates="versions")

    __table_args__ = (
        Index("ix_plugin_versions_search", "version", "publication_status", "published_at"),
    )


class Addon(Base):
    __tablename__ = "addons"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    description: Mapped[str] = mapped_column(Text, nullable=False)
    developer: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    version: Mapped[str] = mapped_column(String(64), index=True, nullable=False)
    publication_status: Mapped[str] = mapped_column(String(50), index=True, nullable=False)
    compatibility: Mapped[str] = mapped_column(String(255), index=True, nullable=False)
    binary_url: Mapped[str] = mapped_column(String(1024), nullable=False)
    published_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True, nullable=False)

    __table_args__ = (Index("ix_addons_search", "name", "developer"),)
