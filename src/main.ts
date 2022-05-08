import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import { Menu, Tray } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs } from "./schemehandler";
import { patchImageDataShow, relPath, sameDomainResolve, schemestring } from "./lib";
import { identifyApp } from "./appconfig";
import { getActiveWindow, native, OSWindow, OSWindowPin, reloadAddon } from "./native";
import { detectInstances, getRsInstanceFromWnd, RsInstance, rsInstances, initRsInstanceTracking } from "./rsinstance";
import { OverlayCommand, Rectangle, RsClientState } from "./shared";
import { AppPermission, Bookmark, loadSettings, saveSettings, settings } from "./settings";
import { boundMethod } from "autobind-decorator";
import * as remoteMain from "@electron/remote/main";

if (process.env.NODE_ENV === "development") {
	patchImageDataShow();
	//exposed on global for debugging purposes
	(global as any).native = require("./native");
	(global as any).Alt1lite = require("./main");
}

export const managedWindows: ManagedWindow[] = [];
export function getManagedWindow(w: WebContents) { return managedWindows.find(q => q.window.webContents == w); }
export function getManagedAppWindow(id: number) { return managedWindows.find(q => q.appFrameId == id); }
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
loadSettings();
remoteMain.initialize();

app.on("before-quit", e => {
	rsInstances.forEach(c => c.close());
	saveSettings();
});
app.on("second-instance", (e, argv, cwd) => handleSchemeArgs(argv));
app.on("window-all-closed", e => e.preventDefault());
app.once("ready", () => {
	globalShortcut.register("Alt+1", alt1Pressed);
	drawTray();
	initIpcApi();
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

class ManagedWindow {
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
			skipTaskbar: true,
			minimizable: false,
			maximizable: false,
		});
		remoteMain.enable(this.window.webContents);

		this.nativeWindow = new OSWindow(this.window.getNativeWindowHandle());
		this.rsClient = rsclient;
		this.appConfig = app;

		this.windowPin = new OSWindowPin(this.nativeWindow, this.rsClient.window, "auto");
		this.windowPin.once("close", () => {
			this.window.close();
			app.wasOpen = true;
		});
		this.windowPin.on("moved", () => {
			app.lastRect = this.windowPin.getPinRect();
		});
		this.windowPin.setPinRect(posrect);

		this.window.loadFile(path.resolve(__dirname, "appframe/index.html"));
		this.window.once("close", () => {
			managedWindows.splice(managedWindows.indexOf(this), 1);
			this.windowPin.unpin();
			fixTooltip();
		});

		managedWindows.push(this);
	}
}

function drawTray() {
	tray = new Tray(alt1icon);
	tray.setToolTip("Alt1 Lite");
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
	tray.setContextMenu(menuinst);
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

function fixTooltip() {
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

function initIpcApi() {
	ipcMain.on("identifyapp", async (e, configurl) => {
		try {
			let url = sameDomainResolve(e.sender.getURL(), configurl);
			await identifyApp(url);
		} catch (e) {
			console.error(e);
		}
	});

	ipcMain.on("capturesync", (e, x, y, width, height) => {
		try {
			let wnd = getManagedAppWindow(e.sender.id);
			if (!wnd?.rsClient.window) { throw new Error("capture window not found"); }
			let capt = native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, { main: { x, y, width, height } });
			e.returnValue = { value: { width, height, data: capt.main } };
		} catch (err) {
			e.returnValue = { error: "" + err };
		}
	});

	//TODO remove
	ipcMain.handle("test", (e, buf) => { return buf; });

	ipcMain.on("rsbounds", (e) => {
		let wnd = getManagedAppWindow(e.sender.id);
		if (!wnd?.rsClient.window) { throw new Error("rs window not found"); }
		let state: RsClientState = {
			active: wnd.rsClient.isActive,
			clientRect: wnd.rsClient.window.getClientBounds(),
			lastBlurTime: wnd.rsClient.lastBlurTime,
			ping: 10,//TODO
			scaling: 1,//TODO
			captureMode: settings.captureMode
		};
		e.returnValue = { value: state };
	});

	ipcMain.handle("capture", (e, x, y, width, height) => {
		let wnd = getManagedAppWindow(e.sender.id);
		if (!wnd?.rsClient.window) { throw new Error("rs window not found"); }
		return native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, { main: { x, y, width, height } }).main;
	});

	ipcMain.handle("capturemulti", (e, rects: { [key: string]: Rectangle }) => {
		let wnd = getManagedAppWindow(e.sender.id);;
		if (!wnd?.rsClient.window) { throw new Error("rs window not found"); }
		return native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, rects);
	});

	ipcMain.on("settooltip", (e, text: string) => {
		let wnd = getManagedAppWindow(e.sender.id);
		if (!wnd) { throw new Error("api caller not found"); }
		wnd.activeTooltip = text;
		fixTooltip();
	});

	ipcMain.on("overlay", (e, commands: OverlayCommand[]) => {
		let wnd = getManagedAppWindow(e.sender.id);
		//TODO errors here are not rethrown in app, just swallow and log them
		if (!wnd?.rsClient.window) { throw new Error("rs window not found"); }
		wnd.rsClient.overlayCommands(wnd.appFrameId, commands);
	});
}
