# Publish a versioned GitHub Release (Wiper25 SSH host alias).
param(
  [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path "dist/AxiomOS-Cardputer-ADV-v$Version-merged.bin")) {
  python -m platformio run -e m5stack-stamps3
  python scripts/merge_firmware.py --version $Version
}

git remote set-url origin git@github-wiper25:Wiper25/AxiomOS-Cardputer.git
git push -u origin main

if (-not (git rev-parse -q --verify "refs/tags/v$Version")) {
  git tag -a "v$Version" -m "AxiomOS v$Version"
}
git push origin "v$Version"

if (Get-Command gh -ErrorAction SilentlyContinue) {
  gh release create "v$Version" `
    "dist/AxiomOS-Cardputer-ADV-v$Version-merged.bin" `
    "dist/AxiomOS-Cardputer-ADV-v$Version-app.bin" `
    --title "AxiomOS v$Version" `
    --notes-file "docs/RELEASE_NOTES_v$Version.md"
} else {
  Write-Host "Tag v$Version pushed. CI workflow attaches bins if enabled."
  Write-Host "Watch: https://github.com/Wiper25/AxiomOS-Cardputer/actions"
}
