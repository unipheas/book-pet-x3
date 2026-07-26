# Maintainer release process

Official Book Pet updates are signed. The private key must never be committed,
printed in CI logs, attached to a release, or shared with contributors.

## One-time repository setup

1. Keep the RSA-4096 private key in secure, backed-up secret storage.
2. Add its complete PEM value as the GitHub Actions secret
   `BOOKPET_OTA_SIGNING_KEY`.
3. Configure GitHub Pages to deploy from **GitHub Actions**.
4. Protect release tags and restrict who can change release workflows or
   repository secrets.

The public key is stored at `keys/book-pet-x3-update-public.pem` and embedded in
`src/UpdatePublicKey.h`.

## Create a release

1. Update `VERSION`, `src/BookPetVersion.h`, and `CHANGELOG.md`.
2. Build and test both environments:

   ```sh
   pio run -e xteink_x3
   pio run -e xteink_x3_release
   ```

3. Hardware-test the release environment on an X3.
4. Merge the release commit to `main`.
5. Create and push the matching tag, such as `v0.5.0`.

The release workflow then:

1. checks that the tag matches `VERSION`;
2. builds the signature-enforcing firmware;
3. signs the OTA application with RSA-4096/SHA-256;
4. creates the merged ESP32-C3 factory image;
5. generates SHA-256 checksums, the ESP Web Tools manifest, and the stable
   online-update manifest;
6. creates the GitHub release; and
7. deploys the installer and stable update to GitHub Pages.

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
