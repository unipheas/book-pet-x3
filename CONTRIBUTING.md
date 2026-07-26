# Contributing to Book Pet

Thanks for helping make a tiny e-paper pet more delightful.

## Ways to contribute

- Report reproducible bugs
- Improve setup, recovery, or hardware documentation
- Propose pet behaviors and e-paper-friendly interactions
- Add tests or improve power and refresh behavior
- Create original monochrome artwork that can be distributed under the MIT
  License

## Development setup

1. Fork the repository and clone it with submodules:

   ```sh
   git clone --recurse-submodules https://github.com/YOUR-USERNAME/book-pet-x3.git
   cd book-pet-x3
   ```

2. Install [PlatformIO](https://platformio.org/) through VS Code or PlatformIO
   Core.
3. Build the firmware:

   ```sh
   pio run
   python3 scripts/validate_project.py
   ```

4. If you have an unlocked XTEINK X3 Developer Edition, connect it with the
   magnetic pogo cable, wake it, and upload:

   ```sh
   pio run --target upload
   ```

Never test unverified firmware on a locked/restricted-market device. A
successful compiler build is not the same as an on-device test.

## Pull requests

- Create a focused branch from `main`.
- Keep unrelated changes in separate pull requests.
- Run `pio run` and `python3 scripts/validate_project.py` before opening the
  pull request.
- Changes to installation, recovery, or firmware trust must also build
  `pio run -e xteink_x3_release` and be physically tested on an X3.
- Explain the user-visible behavior and e-paper refresh impact.
- Say exactly which hardware was tested, or clearly mark the change as
  compiler-only.
- Include screenshots or photos for visible UI changes when practical.
- Update the README or changelog when behavior changes.
- Disclose meaningful generative-AI assistance and name the tool used.
- Review and understand AI-assisted code before submitting it.
- Do not submit generated artwork or code when its license or origin cannot be
  established.

By contributing, you agree that your contribution may be distributed under
this project's MIT License.

See [AI_DISCLOSURE.md](AI_DISCLOSURE.md) for the project's own development
disclosure and the standard expected from AI-assisted contributions.

## Design principles

- Standalone first: core pet behavior must not require a network or account.
- E-paper native: prefer meaningful state changes over cosmetic animation.
- Recoverable: preserve a documented route back to known-good firmware.
- Honest verification: distinguish compilation, emulation, and physical-device
  testing.
- Small and understandable: keep the pet engine approachable for learners.

## Community expectations

Be kind, assume good intent, and give constructive feedback. Harassment,
discrimination, and personal attacks are not welcome.
