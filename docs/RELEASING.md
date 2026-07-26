# Maintainer release process

Official Book Pet updates are signed. The private key must never be committed,
printed in CI logs, attached to a release, or shared with contributors.

## One-time repository setup

1. Keep the RSA-4096 private key in secure, backed-up secret storage.
2. Create a `release-signing` GitHub environment with:
   - a required maintainer approval;
   - deployments limited to version tags matching `v*.*.*`; and
   - `BOOKPET_OTA_SIGNING_KEY` stored as an **environment secret**, never a
     repository-wide secret.
3. Configure GitHub Pages to deploy from **GitHub Actions**.
4. Protect release tags and restrict who can change release workflows or
   environment secrets.

The public key is stored at `keys/book-pet-x3-update-public.pem` and embedded in
`src/UpdatePublicKey.h`.

## Create a release

1. Update `VERSION`, `src/BookPetVersion.h`, and `CHANGELOG.md`.
2. Build and test both environments:

   ```sh
   pio run -e xteink_x3
   pio run -e xteink_x3_release
   ```

3. Hardware-test the release environment on an X3:
   - install the signed package from SD;
   - upload the signed package through the local browser portal;
   - confirm a deliberately corrupted signature is rejected without reboot;
   - boot the new OTA slot and restore the previous slot; and
   - enter hold-Back recovery and confirm the screen remains open.
4. Merge the release commit to `main`.
5. Create and push the matching tag, such as `v0.5.0`.

The release workflow then:

1. checks that the tag matches `VERSION` and points to a commit already on
   `main`;
2. builds the signature-enforcing firmware;
3. pauses for approval before a fresh isolated job can read the environment
   signing secret;
4. signs only the already-built application with RSA-4096/SHA-256;
5. verifies the signed application in a separate non-secret publishing job;
6. creates the merged ESP32-C3 factory image;
7. generates SHA-256 checksums, the ESP Web Tools manifest, and the stable
   online-update manifest;
8. creates the GitHub release; and
9. deploys the installer and stable update to GitHub Pages.

If publishing needs to be retried without moving a release tag, run the
**Release firmware** workflow manually and provide the existing tag. The
workflow checks out and verifies that tag instead of rebuilding an untagged
branch.

The publishing job verifies the already-built factory image against the exact
bootloader, partition table, OTA selector, and signed application. It neither
downloads new executable packaging tools nor keeps repository credentials in
the checkout. The build job uses a hash-locked Python tool set and verifies the
pinned pioarduino platform archive before compilation.

## Local package verification

Maintainers with access to the private key can reproduce the package:

```sh
pio run -e xteink_x3_release
python scripts/package_release.py --private-key /secure/path/release.pem
cd dist
shasum -a 256 -c SHA256SUMS
```

The generated `dist/` directory is ignored by Git.

## Key loss or compromise

Losing the private key means existing release builds cannot accept newly signed
official OTA files. Recovery requires a USB factory install containing a new
public key.

If the private key may be compromised:

1. remove or rotate the GitHub secret immediately;
2. stop publishing the stable manifest;
3. disclose the affected release range;
4. ship a USB recovery build with a new trust key; and
5. never reuse the compromised key.
