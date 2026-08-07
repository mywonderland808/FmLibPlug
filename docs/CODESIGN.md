# Codesign / Notarize checklist (macOS)

Distribution builds of FmLibPlug (Standalone / AU / VST3 / CLAP) need Apple signing before Gatekeeper will accept them.

## Prerequisites

- Apple Developer account
- Developer ID Application certificate installed in Keychain
- App-specific password or `notarytool` keychain profile
- Hardened Runtime enabled for the app/plugin bundle

## Suggested flow

1. Build Release: `make configure-release && make plugins`
2. Codesign each bundle, e.g.
   - `codesign --force --deep --options runtime --sign "Developer ID Application: ..." path/to/FmLibPlug.app`
   - Repeat for `.vst3`, `.component`, `.clap`
3. Zip for notarization (preserve symlinks)
4. `xcrun notarytool submit ... --wait`
5. `xcrun stapler staple` on the signed artifacts
6. Smoke-launch Standalone and load in a host

## Notes

- Do not commit certificates or passwords
- LV2 / AUv3 may need extra packaging steps per host expectations
- This checklist is intentionally manual until CI secrets are available
