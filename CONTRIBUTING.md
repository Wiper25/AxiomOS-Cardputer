# Contributing

Thanks for helping AxiomOS grow.

## Dev setup

```bash
pio run -e m5stack-stamps3
pio run -t upload
pio device monitor
```

Merge a flashable image:

```bash
python scripts/merge_firmware.py --version 0.1.0
```

## Guidelines

- Match existing C++ style (namespaces `axiom::*`, terse UI strings)
- Don't commit `.pio/`, secrets, or huge binaries
- User-facing changes → note in `CHANGELOG.md`
- New board → document pins in `docs/DEVICES.md` and `src/core/config.h`

## PR checklist

- [ ] Builds with `pio run`
- [ ] Tested on Cardputer ADV (or noted as untested)
- [ ] Docs updated if install/devices change
