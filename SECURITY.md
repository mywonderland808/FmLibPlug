# Security Policy

## Supported versions

Security fixes are applied on the latest `main` release line (currently 1.3.x). Older tags are not backported unless noted in a release.

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security reports.

Prefer one of:

1. [GitHub private vulnerability reporting](https://github.com/mywonderland808/FmLibPlug/security/advisories/new) for this repository (when enabled), or
2. Email the maintainer via the GitHub profile for [@mywonderland808](https://github.com/mywonderland808) with subject `FmLibPlug security`.

Include steps to reproduce, affected version or commit, and impact. You should receive an acknowledgement within a few days when possible.

## Scope notes

FmLibPlug talks to local MIDI hardware and indexes user-chosen library folders. Reports that involve remote code execution, path traversal outside configured folders, or unsafe handling of untrusted `.syx` / `.dx7` input are especially welcome.

Do not include private keys, notarization credentials, or personal MIDI dumps in reports unless redacted.
