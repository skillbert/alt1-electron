import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import fetch from "node-fetch";
import { dialog, Menu, MenuItem, Tray } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs, handleSchemeCommand } from "./schemehandler";
import { readJsonWithBOM, relPath, sameDomainResolve, schemestring, UserError, weborigin } from "./lib";
import { InstalledApp, identifyApp } from "./appconfig";
import { captureDesktop, getProcessMainWindow, getProcessesByName, getWindowMeta, setPinParent, OSWindow } from "./native";

(global as any).native = require("./native");

//TODO this is needed for current native module, need to make it context aware
app.allowRendererProcessReuse = false;
if (!app.requestSingleInstanceLock()) { app.exit(); }
app.setAsDefaultProtocolClient(schemestring, undefined, [__non_webpack_require__.main!.filename]);
handleSchemeArgs(process.argv);
app.on("second-instance", (e, argv, cwd) => handleSchemeArgs(argv));
app.on('window-all-closed', e => e.preventDefault());
app.once('ready', () => {

	fetch(`${weborigin}/data/alt1/defaultapps.json`).then(r => readJsonWithBOM(r)).then(async (r: { folder: string, name: string, url: string }[]) => {
		for (let appbase of r) { await identifyApp(new URL(`${weborigin}${appbase.url}`)); }
		let stats = installedApps.find(a => a.configUrl == "http://localhost/apps/alt1/notepad/appconfig.json")!;
		openApp(stats);
	});

	globalShortcut.register("Alt+1", () => { });

	drawTray();
	initIpcApi();
});

type ActiveWnd = {
	appConfig: InstalledApp,
	webContents: WebContents,
	window: BrowserWindow
}

export var installedApps: InstalledApp[] = [];
let iconpath = relPath(require("!file-loader!./imgs/alt1icon.png").default);
console.log(iconpath);
console.log(path.resolve(iconpath));
var alt1icon = nativeImage.createFromPath(iconpath);
var wins: ActiveWnd[] = [];
var tray: Tray | null = null;

export function openApp(app: InstalledApp) {
	var wnd = new BrowserWindow({
		webPreferences: { nodeIntegration: true, webviewTag: true, enableRemoteModule: true },
		frame: false,
		//alwaysOnTop: true,
		width: app.defaultWidth,
		height: app.defaultHeight
	});
	let winentry: ActiveWnd = { window: wnd, appConfig: app, webContents: wnd.webContents };

	let rswnd = getProcessMainWindow(getProcessesByName("rs2client.exe")[0]);

	setPinParent(wnd.getNativeWindowHandle(), rswnd);
	wnd.loadFile(path.resolve(__dirname, "appframe/index.html"));
	//wnd.webContents.openDevTools();
	wnd.once("close", () => { wins = wins.filter(w => w != winentry) });

	wins.push(winentry);
}

function drawTray() {
	tray = new Tray(alt1icon);
	tray.setToolTip("Alt1 Lite");
	tray.on("click", e => {
		let menu: MenuItemConstructorOptions[] = [];
		for (let app of installedApps) {
			menu.push({
				label: app.appName,
				icon: app.iconCached ? nativeImage.createFromDataURL(app.iconCached).resize({ height: 20, width: 20 }) : undefined,
				click: openApp.bind(null, app),
			});
		}
		menu.push({ type: "separator" });
		menu.push({ label: "Settings", click: showSettings });
		menu.push({ label: "Exit", click: e => app.quit() });
		menu.push({ label: "Restart", click: e => { app.relaunch(); app.quit(); } });
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
			identifyApp(url);
		} catch (e) {
			console.error(e);
		}
	});

	ipcMain.on("capture", (e, x, y, w, h) => {
		try {
			e.returnValue = { value: { width: w, height: h, data: captureDesktop(x, y, w, h) } };
		} catch (err) {
			e.returnValue = { error: "" + err };
		}
	});

	ipcMain.on("rsinfo", (e) => {
		let [pid] = getProcessesByName("rs2client.exe");
		if (!pid) {
			e.returnValue = { value: null };
		} else {
			e.returnValue = { value: getWindowMeta(getProcessMainWindow(pid)) };
		}
	});

	ipcMain.handle("whoami", (e) => {
		let wnd = wins.find(q => q.webContents == e.sender);
		return wnd?.appConfig;
	});
}