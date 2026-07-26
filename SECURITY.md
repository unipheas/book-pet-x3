# Security Policy

## Supported versions

Security fixes are provided for the latest published release.

## Reporting a vulnerability

Please do not open a public issue for a vulnerability that could damage a
device, bypass firmware restrictions, expose private data, or create an unsafe
flashing path.

Use GitHub's **Report a vulnerability** feature on the repository Security tab.
Include:

- A clear description of the issue and its impact
- The exact XTEINK model and edition
- Firmware version or commit
- Reproduction steps
- Relevant logs without secrets or personal data
- A suggested fix, if available

You should receive an acknowledgement within seven days. Please allow time for
a fix and release before publishing technical details.

## Scope and safety

Book Pet has no accounts, telemetry, or cloud dependency. Normal pet play is
offline. Networking is only enabled when the owner explicitly starts
**Phone / Browser** updates, and home Wi-Fi credentials are kept in RAM only
for that update session.

Official release builds accept firmware only when its RSA-4096 signature
matches the public key embedded in Book Pet. Online releases also use HTTPS,
an allowlisted update origin, a versioned manifest, size checks, and SHA-256.
The private signing key is held outside the repository as a protected release
secret.

Firmware and flashing changes can affect device availability and recovery.
Reports involving signature bypass, unsafe flash layouts, boot failures,
persistent state corruption, credential retention, cross-site update requests,
or unexpected radio activation are in scope.

Only the unlocked XTEINK X3 Developer/overseas edition is supported. Mechanisms
for bypassing manufacturer locks are outside this project's support scope.
