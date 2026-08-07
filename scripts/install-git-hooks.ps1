# Enable repo git hooks (blocks ai_secrets.h commits).
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)
git config core.hooksPath .githooks
Write-Host "OK: core.hooksPath=.githooks"
git config --get core.hooksPath
