# Release checklist (maintainers)

1. Bump `kProjectVersion` in `src/core/config.h` and `AXIOMOS_VERSION` in `platformio.ini`
2. Update `CHANGELOG.md` + add `docs/RELEASE_NOTES_vX.Y.Z.md`
3. Commit on `main`, push
4. Tag and push:

```bash
git tag v0.1.0
git push origin main
git push origin v0.1.0
```

5. GitHub Actions builds and attaches:
   - `AxiomOS-Cardputer-ADV-vX.Y.Z-merged.bin` (@ `0x0`)
   - `AxiomOS-Cardputer-ADV-vX.Y.Z-app.bin` (@ `0x10000`)

Manual local assets:

```bash
pio run -e m5stack-stamps3
python scripts/merge_firmware.py --version 0.1.0
```

Then upload `dist/*` to the GitHub Release if CI is unavailable.
