
# Alt1 Electron (name pending)
This project is an experimental rewrite of the Alt1 Toolkit in Typescript, Electron and React. The project is currently in an experimental state, it is not clear yet if this could become a replacement for C# Alt1.

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

- [libxcb](https://xcb.freedesktop.org/) with the Composite and SHM extensions
- [libxcb-wm](https://gitlab.freedesktop.org/xorg/lib/libxcb-wm)
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
- [procps](http://procps-ng.sourceforge.net/)

### Arch (pacman)

```console
# pacman -S pkg-config libxcb xcb-util-wm procps-ng
```

### Debian/Ubuntu (apt)

```console
# apt install pkg-config libxcb-dev libxcb-shm-dev libxcb-composite-dev libxcb-ewmh-dev libxcb-record-dev libxcb-shape-dev libprocps-dev
```

### Gentoo (portage)

```console
# emerge --ask --noreplace dev-util/pkgconf x11-libs/libxcb x11-libs/xcb-util-wm sys-process/procps
```



## Mac Errata

* To (re)generate the xcode project (for debugging and other helpful development activities):

  ```bash
  node-gyp configure --debug -- -f xcode
  ```

  * Note that node-gyp helpfully (stupidly) does not provide a mechanism to change the output path from `build`. This means that your xcode project file (directory) will be completely blown away every single time npm does anything. I mitigate this on my personal by moving the `build/binding.xcodeproj` directory to `.xcode` and working from there; updating my `.xcode/binding.xcodeproj` only when I need(ed) to make changes to the `binding.gyp`.  YMMV

#### Gatekeeper, Accessibility, Screen Recording 

alt1-electron will only work if the electron application has both:

* Accessibility API enabled: <img src="/Users/user/Library/Application Support/typora-user-images/image-20220821071358114.png" alt="image-20220821071358114" style="zoom:50%;" />
* Screen Recording enabled: <img src="/Users/user/Library/Application Support/typora-user-images/image-20220821071428113.png" alt="image-20220821071428113" style="zoom:50%;" />

Running `npm run ui` _should_ request the permissions using the native (os-level) workflow. However, in a development environment with changing electron versions etc it may be necessary to _remove_ the `Electron.app` from the above permissions list to allow it to "re-request" the permissions. 

For some unknown reason, macos does _not_ do a great job of handling _updates_ to applications that have been granted the above permissions. You may keep getting the popup asking for 'screen recording' or 'control' and when visiting the list in system preferences you'll notice that `Electron.app` is already checked. From here you'll scratch your head and say: 'wtf you already have permissions'. To fix this situation:

* Remove `Electron.app` from BOTH `Accessibility` AND `Screen Recording`
* Kill/stop `npm run ui` (or if you're running it in Xcode stop the alt1-electron process)
* Start alt1-electron either by `npm run ui` or by running the application in Xcode
* You should see the popup(s) _again_ but _this time_ checking the box will actually give the `Electron.app` the permissions

The above will only be necessary _ONCE_ per `Electron.app` version / build change. This means: whenever `<project>/node_modules/electron/dist/Electron.app` changes, you will likely have to do this.

# Why rewrite?

### Clean slate
The architecture of present day Alt1 has been dictated by choices made 7 years ago. The Alt1 Toolkit was the result of a lot of experimenting and around poking in the dark. Many attempted features never worked out or have been scrapped or replaced. The many dead ends and design changes have built up to weigh down the code over the years and it is time to start over.

### Better and easier UI
There is currently no usable UI framework in C#. Alt1 is mostly built with winforms. Microsoft intended to replace winforms with WPF 10 years ago, however that turned out to be such a uniquely garbage UI framework that it didn't help. Since then Microsoft tried UWP apps which ended with a similar fate. In short UI in C# is a dead end. Current UI in Alt1 is either classic winforms or almost completely drawn by hand with 2d APIs. Using HTML and CSS with React makes UI trivial, beautiful and maintainable.

### Shared code with apps
Currently any non-app screen detection features have to be implemented from scratch in C#. Using JS allows sharing code between apps and the framework, this would simplify some features a lot and remove maintenance overhead.

### Browser integration
Currently communication with apps is slow and limited. Electron has much better browser integration for stuff like error handling and complex data types. There is also the option for service worker integration and a native API to offload high performance code.

### Cross-platform
This has been the most long standing request. Starting from scratch with other platforms in mind is an order of magnitude easier than trying to backport it. Electron is cross-platform by default, so only minimal platform specific code is needed.

# Project status

See [contributing.md](./contributing.md) for information on how to contribute to this project.

**Currently functional**
- [x] Basic app functionality
- [x] Overlay API
- [x] Capture API
- [x] Appconfig and saved apps
- [x] Window pinning
- [x] Multiclient support
- [x] OpenGL capture using old DLLs
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
- [ ] Shippableness in general
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
- [x] MacOS
	- [x] Basics
	- [o] Window events API (partial)
	- [x] Window pinning
	- [x] Capture
		- [x] Window

**TODO**
- Actually implement capture method toggle
- Add toggle in Injectdll for rgba capture instead of bgra
- Improve RS client close detection
- Fix RS client opening detection pinning on the loading screen
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
Direct access to JS runtime and memory of arraybuffers is now possible. Possibly capture directly into app controlled memory and implement C++ accelerated image detect fast paths.

### Different app styles
Support for Guide style apps that are easy to minimize and take up the center screen. 2 years ago RS Pocketbook was interested in merging into Alt1 like this, others are also possible.
