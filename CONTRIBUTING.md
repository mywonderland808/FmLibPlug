# Contributing to FmLibPlug

Thanks for helping improve this AGPL-3.0 project.

## Ground rules

- **License:** contributions are accepted under the GNU Affero General Public License v3.0 (see [LICENSE](LICENSE)).
- **No third-party carts:** do not commit commercial or third-party DX7/TX7 voice banks. Keep personal libraries in gitignored folders such as `Presets/`. Synthetic fixtures belong under `Tests/fixtures/` (regenerate with `Tools/GenTestFixtures` when needed).
- **No secrets:** never commit certificates, `.env` files, Apple notarization passwords, or `Tests/hw-midi.local.json` (use the `.example` template).
- **Trademarks:** product name is **FmLibPlug**. Prefer compatibility wording (“DX7 / TX7–compatible”) over leading with Yamaha marks; see the disclaimer in [README.md](README.md).

## Development

```bash
make configure        # or configure-release
make build
make check            # unit tests (no MIDI)
```

Optional: `make check-smoke` (local `Presets/`), `make check-hw` (hardware; copy `Tests/hw-midi.local.json.example`).

Cross-platform notes: [docs/PLATFORM.md](docs/PLATFORM.md). macOS codesign checklist: [docs/CODESIGN.md](docs/CODESIGN.md).

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

- Types: `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `chore`, …
- Imperative, lowercase subject after the type (e.g. `fix: pace SysEx on host MIDI out`)
- Prefer one logical change per commit; put the **why** in the body when it is not obvious

Author commits as yourself; do not add AI tool attribution trailers.

## Pull requests

- Keep PRs focused; update [CHANGELOG.md](CHANGELOG.md) under `[Unreleased]` for user-visible changes.
- Run `make check` before opening the PR.
- Security-sensitive changes: see [SECURITY.md](SECURITY.md).
