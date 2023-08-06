import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import { Menu, Tray } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs } from "./schemehandler";
import { patchImageDataShow, relPath, sameDomainResolve, schemestring } from "./lib";
import { identifyApp } from "./appconfig";
import { getActiveWindow, native, OSWindow, OSWindowPin, reloadAddon } from "./native";
import { detectInstances, getRsInstanceFromWnd, RsInstance, rsInstances, initRsInstanceTracking, stopRsInstanceTracking } from "./rsinstance";
import { OverlayCommand, Rectangle, RsClientState } from "./shared";
import { AppPermission, Bookmark, loadSettings, saveSettings, settings } from "./settings";
import { boundMethod } from "autobind-decorator";
import * as remoteMain from "@electron/remote/main";
import { initIpcApi } from "./ipcapi";

if (process.env.NODE_ENV === "development") {
	patchImageDataShow();
	//exposed on global for debugging purposes
	(global as any).native = require("./native");
	(global as any).Alt1lite = require("./main");
}

export const managedWindows: ManagedWindow[] = [];
export function getManagedWindow(w: WebContents) { return managedWindows.find(q => q.window.webContents == w); }
export function getManagedAppWindow(id: number) { return managedWindows.find(q => q.appFrameId == id || q.window.webContents.id == id); }
var tray: Tray | null = null;
var alt1icon = nativeImage.createFromPath(relPath(require("!file-loader!./imgs/alt1icon.png").default));
var tooltipWindow: TooltipWindow | null = null;
var alwaysOpenDevtools = false;

app.on("browser-window-created", (e, wnd) => {
	if (alwaysOpenDevtools) {
		wnd.webContents.openDevTools({ mode: "undocked" });
	}
});

const originalCwd = process.cwd();
process.chdir(__dirname);
if (!app.requestSingleInstanceLock()) { app.exit(); }
app.setAsDefaultProtocolClient(schemestring, undefined, [__non_webpack_require__.main!.filename]);
handleSchemeArgs(process.argv);
loadSettings(); // Cannot await on top-level, so if config is missing, default settings are loaded later
remoteMain.initialize();

app.on("before-quit", e => {
	rsInstances.forEach(c => c.close());
	stopRsInstanceTracking();
	saveSettings();
});
app.on("second-instance", (e, argv, cwd) => handleSchemeArgs(argv));
app.on("window-all-closed", e => e.preventDefault());
app.once("ready", () => {
	if (process.platform === "darwin") {
		if (!app.accessibilitySupportEnabled) {
			app.setAccessibilitySupportEnabled(true);
		}
		if (app.dock) {
			app.dock.hide();
		}
	}

	globalShortcut.register("Alt+1", alt1Pressed);
	drawTray();
	initIpcApi(ipcMain);
	initRsInstanceTracking();
});

function alt1Pressed() {
	let rsinst = getRsInstanceFromWnd(getActiveWindow());
	try {
		if (!rsinst) {
			throw new Error("Alt+1 pressed but no active rs client found");
		}
		rsinst.alt1Pressed();
	} catch (e) {
		console.log("alt+1 hotkey read failed: " + e);
	}
}

export function openApp(app: Bookmark, inst?: RsInstance) {
	if (!inst) {
		detectInstances();
		inst = rsInstances[0];
	}
	if (!inst) { console.error("no rs instance found"); }
	else { new ManagedWindow(app, inst); }
}

export class ManagedWindow {
	appConfig: Bookmark;
	window: BrowserWindow;
	nativeWindow: OSWindow;
	windowPin: OSWindowPin;
	rsClient: RsInstance;
	appFrameId = -1;
	activeTooltip = "";

	constructor(app: Bookmark, rsclient: RsInstance) {
		let posrect = app.lastRect;
		if (!posrect) {
			posrect = {
				left: 20, top: 20, width: app.defaultWidth, height: app.defaultHeight,
				bot: 0, right: 0,
				pinning: ["top", "left"]
			};
			app.lastRect = posrect;
		}

		this.window = new BrowserWindow({
			webPreferences: { nodeIntegration: true, webviewTag: true, contextIsolation: false },
			frame: false,
			width: posrect.width,
			height: posrect.height,
			transparent: true,
			fullscreenable: false,
			resizable: false,//prevent electron from adding resize handlers that cover the dom around the border
			skipTaskbar: true,
			minimizable: false,
			maximizable: false,
			show: false,
		});
		remoteMain.enable(this.window.webContents);
		this.window.setVisibleOnAllWorkspaces(true, {visibleOnFullScreen: true});

		this.nativeWindow = new OSWindow(this.window.getNativeWindowHandle());
		this.rsClient = rsclient;
		this.appConfig = app;

		this.windowPin = new OSWindowPin(this.window, this.rsClient.window, "auto");
		this.windowPin.once("close", () => {
			this.window.close();
			app.wasOpen = true;
		});
		this.windowPin.setPinRect(posrect);

		this.window.loadFile(path.resolve(__dirname, "appframe/index.html"));
		this.window.once("close", () => {
			managedWindows.splice(managedWindows.indexOf(this), 1);
			this.rsClient.overlayWindow?.browser.webContents.send("clearoverlay", this.appFrameId);
			this.windowPin.unpin();
			fixTooltip();
		});

		// NOTE: it's very very VERY important that the window must be created with `show: false` and that window.show()
		// must be called AFTER creating the OSWindowPin. This allows us to manipulate the window before the WM mangles it.
		this.window.once('ready-to-show', () => {
			this.window.show();
			this.windowPin.synchPosition(); // fixes electron not showing up sometimes
		});
		managedWindows.push(this);
	}
}

export function updateTray() {
	let menu: MenuItemConstructorOptions[] = [];
	for (let app of settings.bookmarks) {
		menu.push({
			label: app.appName,
			icon: app.iconCached ? nativeImage.createFromDataURL(app.iconCached).resize({ height: 20, width: 20 }) : undefined,
			click: openApp.bind(null, app, undefined),
		});
	}
	if (process.env.NODE_ENV === "development") {
		menu.push({ type: "separator" });
		menu.push({
			label: "Restart", click: e => {
				//relaunch uses the dir at time of call, there is no better way to give it the original dir
				process.chdir(originalCwd);
				app.relaunch();
				process.chdir(__dirname);
				app.quit();
			}
		});
		menu.push({ label: "Repin RS", click: e => { rsInstances.forEach(e => e.close()); detectInstances(); } });
		menu.push({ label: "Reload native addon", click: e => { reloadAddon(); } });
		menu.push({ label: "Hook dev tools", type: "checkbox", checked: alwaysOpenDevtools, click: e => alwaysOpenDevtools = !alwaysOpenDevtools });
	}
	menu.push({ type: "separator" });
	menu.push({ label: "Settings", click: showSettings });
	menu.push({ label: "Exit", click: e => app.quit() });
	let menuinst = Menu.buildFromTemplate(menu);
	tray?.setContextMenu(menuinst);
}

function drawTray() {
	if (!tray) {
		tray = new Tray(alt1icon);
		tray.on("click", e => tray!.popUpContextMenu());
	}
	tray.setToolTip("Alt1 Lite");
	updateTray();
}

let settingsWnd: BrowserWindow | null = null;
export function showSettings() {
	if (settingsWnd) {
		settingsWnd.focusOnWebView();
		return;
	}
	settingsWnd = new BrowserWindow({
		webPreferences: { nodeIntegration: true, webviewTag: true, contextIsolation: false },
	});
	settingsWnd.loadFile(path.resolve(__dirname, "settings/index.html"));
	settingsWnd.once("closed", e => settingsWnd = null);
	remoteMain.enable(settingsWnd.webContents);
}

//TODO add permission
export function* selectAppContexts(rsinstance: RsInstance | null, permission: AppPermission | "") {
	for (let wnd of managedWindows) {
		if (rsinstance && rsinstance != wnd.rsClient) { continue; }
		//TODO move this to method on appconfig instead
		if (permission && !wnd.appConfig.permissions.includes(permission)) { continue; }
		if (wnd.appFrameId == -1) { continue; }
		let webcontent = electron.webContents.fromId(wnd.appFrameId);
		yield webcontent;
	}
}

class TooltipWindow {
	wnd: BrowserWindow;
	interval = 0;
	text: string;
	loaded = false;
	constructor() {
		let wnd = new BrowserWindow({
			webPreferences: { nodeIntegration: true, contextIsolation: false },
			frame: false,
			transparent: true,
			show: false,
			skipTaskbar: true,
			alwaysOnTop: true,
			minWidth: 10, minHeight: 10,
			width: 10, height: 10,
			focusable: false
		});
		wnd.loadFile(path.resolve(__dirname, "tooltip/index.html"));
		wnd.once("ready-to-show", () => {
			wnd.show();
		});
		wnd.webContents.once("dom-ready", e => {
			this.loaded = true;
			this.setTooltip(this.text);
		});
		wnd.on("closed", e => {
			if (this.interval) { clearInterval(this.interval) }
			tooltipWindow = null;
		});
		wnd.setIgnoreMouseEvents(true);
		this.wnd = wnd;
		tooltipWindow = this;
		this.interval = setInterval(this.fixPosition, 20) as any;
	}
	@boundMethod
	fixPosition() {
		let pos = electron.screen.getCursorScreenPoint()
		this.wnd.setPosition(pos.x + 20, pos.y + 20, true);//TODO check if animate is appropriate in mac (and maybe lower poll rate)
	}
	setTooltip(text: string) {
		this.text = text;
		if (this.loaded) {
			this.wnd.webContents.send("settooltip", this.text);
		}
	}
	close() {
		this.wnd.close();
	}
}

export function fixTooltip() {
	let text = "";
	for (let wnd of managedWindows) {
		if (wnd.activeTooltip) {
			if (text) { text += "\n"; }
			text += wnd.activeTooltip;
		}
	}

	if (text) {
		if (!tooltipWindow) {
			tooltipWindow = new TooltipWindow();
		}
		tooltipWindow.setTooltip(text);
	}
	if (!text && tooltipWindow) {
		tooltipWindow.close();
	}
}

