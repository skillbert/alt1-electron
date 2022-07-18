## Contributing to Alt1 Electron
For any questions and code discussions you can talk in the `#development` channel in the [RuneApps discord](https://discord.gg/f24MRWm).

## Pull requests
- PRs are meant for single fixes/issues/features, refrain from making unrelated code changes while working on your feature.
  - A PR can only be accepted or rejected in its entirety. If you add an unrelated rejected change the PR will be rejected in its entirety.
- If your PR changes how existing code works without adding anything major, please discuss it in the discord first, there is probably a good reason for the old behavior.
- Please discuss any large projects in the discord server first, there may be someone working on it already, or there might be different plans already.

## Coding style
- There are currently no strict coding guidelines yet. Try to use common sense and blend in with surrounding code style.
- The typescript part of the project is written with VS Code on default formatting settings.
  - Tabs for indent.
  - No strict line width limits but keep it reasonable.
  - Run the formatter before saving changes `alt+shift+F`.
- Various tools are used for the native plugin depending on the target platform, try to check your commits for excessive whitespace changes before making them.
- The only thing worse than a bad coding style is having 5 different coding styles in the same project.
