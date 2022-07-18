
# Alt1 Electron (name pending)
This project is a experimental rewrite of the Alt1 Toolkit in Typescript, Electron and React. The project is currently in experimental state, it is not clear yet if this could become a replacement of C# Alt1.

# Build
You need a working nodejs installation including nodejs native build tools (is an option during installation) in order to compile Alt1.
```sh
# Install dependencies
npm i

# Build native modules
# After building once you will find cpp project files for visual studio/xcode depending on your platform
# You can then build and debug using that project and IDE
npm run native

# auto-build typescript/webpack
npm run watch

# Run
npm run ui
```

## Linux dependencies
pkg-config libxcb-dev libxcb-shm-dev libxcb-composite-dev libxcb-ewmh-dev libprocps-dev

# Why rewrite?

### Clean slate
The architecture of present day Alt1 has been dictated by choices made 7 years ago. The Alt1 Toolkit was the result of a lot of experimenting and around poking in the dark. Many attemped features never worked out or have been scrapped or replaced. The many dead ends and design changes have built up to weigh down the code over the years and it is time to start over.

### Better and easier UI
There is currently no usable UI framework in C#. Alt1 is mostly built with winforms. Microsoft intended to replace winforms with WPF 10 years ago, however that turned out to be such a uniquely garbage UI framework that it didn't help. Since then Microsoft tried UWP apps which ended with a similar fate. In short UI in C# is a dead end. Current UI in Alt1 is either classic winforms or almost completely drawn by hand with 2d api's. Using html and css with React makes UI trivial, beautiful and maintanable.

### Shared code with apps
Currently any non-app screen detection features have to be implemented from scratch in c#. Using js allows sharing code between apps and the framework, this would simplify some features a lot and remove maintenance overhead.

### Browser integration
Currently communication with apps is slow and limited. Electron has much better browser integration for stuff like error handling and complex data types. There is also the option for service worker integration and a native API to offload high performance code.

### Cross-platofrm
This has been the most long standing request. Starting from scratch with other platforms in mind is order of magnitude easier than trying to backport it. Electron is cross-platform by default, so only minimal platform specific code is needed.

# Project status

See [contributing.md](./contributing.md) for information on how to contribute to this project.

**Currently functional**
- [x] Basic app functionality
- [x] Overlay API
- [x] Capture API
- [x] Appconfig and saved apps
- [x] Window pinning
- [x] Multiclient support
- [x] OpenGL capture using old dll's
- [x] mp4 works! (twitch)
	- [ ] still no widevine CDM so no netflix
- [x] changes in app libs use new fast capture API
- [x] rightclick detection
- [x] basic alt+1 hotkey detection
- [ ] Toolbar
- [ ] Settings window
	- [ ] installed apps
	- [ ] capture mode previews and troubleshoot
- [ ] add app window
- [ ] browser handlers
	- [ ] alt1:// protocol from internal browser
	- [ ] remove toolbar on popups
	- [ ] rightclick menu
- [ ] Rewrite and publish OpenGL capture
- [ ] App resize visual snapping
- [ ] Shippebleness in general
- [ ] alt+1 hotkey
	- [ ] app triggers
- [ ] statusdaemon
- [ ] Independent modules
	- [ ] Screenshot sharing (alt+2)
	- [ ] Window manipulation tool (alt+3)
	
**Platform specific**
- [x] Windows
	- [x] Basics
	- [x] Window events API
	- [x] Window pinning
	- [x] Capture
		- [x] OpenGL
		- [x] Window
		- [ ] UI to toggle during runtime
- [x] Linux
	- [x] Basics
	- [x] Window events API
	- [x] Window pinning
	- [x] Capture
		- [x] Window
- [ ] MacOS
	- [ ] Basics
	- [ ] Window events API
	- [ ] Window pinning
	- [ ] Capture
		- [ ] Window

**TODO**
- Actually implement capture method toggle
- Add toggle in Injectdll for rgba capture instead of bgra
- Improve rs client close detection
- Fix rs client opening detection pinning on the loading screen
- Get rid of electron resize handles
- App frame css
- Many little used api calls
- Clean up native event situation for windows
- Enable contextisolation in appwindow
- Try to move RS specific constants from native code to ts/config files
- Think some more about the name


# Extension projects
These concepts don't exist in C# Alt1 but are now possible.

### Background apps
App functionality that runs without the app being visible using service workers.

### Native acceleration plugin
Direct access to js runtime and memory of arraybuffers is now possible. Possibly capture directly into app controlled memory and implement c++ accelerated image detect fast paths.

### Different app styles
Support for Guide style apps that are easy to minimize and take up the center screen. 2 years ago RS Pocketbook was interested in merging into Alt1 like this, others are also possible.
