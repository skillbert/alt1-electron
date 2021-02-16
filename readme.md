# Build
```sh
# Install
npm i

# Build native modules
# After building once you will find cpp project files for visual studio/xcode depending on your platform
npm run uibuild

# Build 
npm run watch

# Run
npm run ui
```


# Alt1Lite (name pending)
This project is a experimental rewrite of the Alt1 Toolkit in Typescript Electron and React. It is not clear yet if this could become a replacement of C# Alt1.

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

**Currently functional**
- Basic app functionality
- Overlay API
- Capture API
- Appconfig and saved apps
- Window pinning
- Multiclient support
- OpenGL capture using old dll's
- mp4 works! (twitch)
	- still no widevine CDM so no netflix
- changes in app libs use new fast capture API

**Missing features**
- Toolbar
- Unix/Mac implementation of OS calls in os.h
- Settings window
	- capture mode previews and troubleshoot
- Rewrite OpenGl capture
- App resize visual snapping
- Shippebleness in general
- Screenshot sharing (alt+2)
- alt+1 hotkey
- rightclick detection and app cutout

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
- Think some more about the name