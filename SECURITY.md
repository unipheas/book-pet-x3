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

Book Pet does not use networking, accounts, telemetry, or cloud services.
Nevertheless, firmware and flashing changes can affect device availability and
recovery. Reports involving unsafe flash layouts, boot failures, persistent
state corruption, or unexpected radio activation are in scope.

Only the unlocked XTEINK X3 Developer/overseas edition is supported. Mechanisms
for bypassing manufacturer locks are outside this project's support scope.

