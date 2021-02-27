import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import fetch from "node-fetch";
import { BrowserView, dialog, Menu, MenuItem, Tray, webContents } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs, handleSchemeCommand } from "./schemehandler";
import { patchImageDataShow, readJsonWithBOM, relPath, rsClientExe, sameDomainResolve, schemestring, weborigin } from "./lib";
import { identifyApp } from "./appconfig";
import { getActiveWindow, native, OSWindow, OSWindowPin, reloadAddon } from "./native";
import { detectInstances, getActiveInstance, RsInstance, rsInstances } from "./rsinstance";
import { OverlayCommand, Rectangle } from "./shared";
import { Bookmark, loadSettings, saveSettings, settings } from "./settings";


if (process.env.NODE_ENV === "development") {
	patchImageDataShow();
}

//exposed on global for debugging purposes
(global as any).native = require("./native");
(global as any).Alt1lite = require("./main");

export const managedWindows: ManagedWindow[] = [];
export function getManagedWindow(w: webContents) { return managedWindows.find(q => q.window.webContents == w); }
export function getManagedAppWindow(id: number) { return managedWindows.find(q => q.appFrameId == id); }
var tray: Tray | null = null;
var alt1icon = nativeImage.createFromPath(relPath(require("!file-loader!./imgs/alt1icon.png").default));

const originalCwd = process.cwd();
process.chdir(__dirname);
if (!app.requestSingleInstanceLock()) { app.exit(); }
app.setAsDefaultProtocolClient(schemestring, undefined, [__non_webpack_require__.main!.filename]);
handleSchemeArgs(process.argv);
loadSettings();

app.on("before-quit", e => {
	rsInstances.forEach(c => c.close());
	saveSettings();
});
app.on("second-instance", (e, argv, cwd) => handleSchemeArgs(argv));
app.on('window-all-closed', e => e.preventDefault());
app.once('ready', () => {

	globalShortcut.register("Alt+1", alt1Pressed);

	drawTray();
	initIpcApi();
});

function alt1Pressed() {
	let rsinst = getActiveInstance();
	try {
		if (!rsinst) {
			throw new Error("Alt+1 pressed but no active rs client found");
		}
		rsinst.alt1Pressed();
	} catch (e) {
		console.log("alt+1 hotkey read failed: " + e);
	}
}

function rsRightclicked() {

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

	constructor(app: Bookmark, rsclient: RsInstance) {
		this.window = new BrowserWindow({
			webPreferences: { nodeIntegration: true, webviewTag: true, enableRemoteModule: true },
			frame: false,
			width: app.defaultWidth,
			height: app.defaultHeight
		});

		this.rsClient = rsclient;
		this.appConfig = app;

		this.nativeWindow = new OSWindow(this.window.getNativeWindowHandle());
		this.windowPin = new OSWindowPin(this.nativeWindow, this.rsClient.window, "auto");
		this.windowPin.once("close", () => {
			this.window.close();
			app.wasOpen = true;
		});
		this.window.loadFile(path.resolve(__dirname, "appframe/index.html"));
		//this.window.webContents.openDevTools();
		this.window.once("close", () => {
			managedWindows.splice(managedWindows.indexOf(this), 1);
			this.windowPin.unpin();
		});

		managedWindows.push(this);
	}
}

function drawTray() {
	tray = new Tray(alt1icon);
	tray.setToolTip("Alt1 Lite");
	tray.on("click", e => {
		let menu: MenuItemConstructorOptions[] = [];
		for (let app of settings.bookmarks) {
			menu.push({
				label: app.appName,
				icon: app.iconCached ? nativeImage.createFromDataURL(app.iconCached).resize({ height: 20, width: 20 }) : undefined,
				click: openApp.bind(null, app, undefined),
			});
		}
		menu.push({ type: "separator" });
		menu.push({ label: "Settings", click: showSettings });
		menu.push({ label: "Exit", click: e => app.quit() });
		menu.push({
			label: "Restart", click: e => {
				//relaunch uses the dir at time of call, there is no better way to give it the original dir
				process.chdir(originalCwd);
				app.relaunch();
				process.chdir(__dirname);
				app.quit();
			}
		});
		menu.push({ label: "Reload native addon", click: e => { reloadAddon(); } });
		let menuinst = Menu.buildFromTemplate(menu);
		tray!.setContextMenu(menuinst);
		tray!.popUpContextMenu();
	});
}

let settingsWnd: BrowserWindow | null = null;
export function showSettings() {
	if (settingsWnd) {
		settingsWnd.focusOnWebView();
		return;
	}
	settingsWnd = new BrowserWindow({
		webPreferences: { nodeIntegration: true, webviewTag: true, enableRemoteModule: true },
	});
	settingsWnd.loadFile(path.resolve(__dirname, "settings/index.html"));
	//settingsWnd.webContents.openDevTools();
	settingsWnd.once("closed", e => settingsWnd = null);
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

	ipcMain.on("capturesync", (e, x, y, w, h) => {
		try {
			let wnd = getManagedAppWindow(e.sender.id);
			if (!wnd?.rsClient.window) { throw new Error("capture window not found"); }
			e.returnValue = { value: { width: w, height: h, data: native.captureWindow(wnd.rsClient.window.handle, x, y, w, h) } };
		} catch (err) {
			e.returnValue = { error: "" + err };
		}
	});

	//TODO remove
	ipcMain.handle("test", (e, buf) => { return buf; });

	ipcMain.on("rsbounds", (e) => {
		let wnd = getManagedAppWindow(e.sender.id);
		e.returnValue = { value: wnd?.rsClient.window.getClientBounds() };
	});

	ipcMain.handle("capture", (e, x, y, w, h) => {
		let wnd = getManagedAppWindow(e.sender.id);
		if (!wnd?.rsClient.window) { throw new Error("capture window not found"); }
		return native.captureWindow(wnd.rsClient.window.handle, x, y, w, h);
	});

	ipcMain.handle("capturemulti", (e, rects: { [key: string]: Rectangle }) => {
		let wnd = getManagedAppWindow(e.sender.id);;
		if (!wnd?.rsClient.window) { throw new Error("capture window not found"); }
		return native.captureWindowMulti(wnd.rsClient.window.handle, rects);
	});

	ipcMain.on("overlay", (e, commands: OverlayCommand[]) => {
		let wnd = getManagedAppWindow(e.sender.id);
		//TODO errors here are not rethrown in app, just swallow and log them
		if (!wnd?.rsClient.window) { throw new Error("capture window not found"); }
		wnd.rsClient.overlayCommands(wnd.appFrameId, commands);
	});
}
