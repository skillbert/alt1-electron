import { installedApps } from "./main";
import { readJsonWithBOM, sameDomainResolve, UserError } from "./lib";
import fetch from "node-fetch";

type AppPermission = "pixel" | "game" | "overlay";
export type AppConfigImport = {
	appName: string,
	description: string,
	appUrl: string,
	configUrl: string,
	iconUrl: string,
	defaultWidth: number,
	defaultHeight: number,
	minWidth: number,
	minHeight: number,
	maxWidth: number,
	maxHeight: number,
	permissions: string
}
export type InstalledApp = Bookmark & {
	configUrl: string,
	permissions: AppPermission[]
}
export type Bookmark = {
	appName: string,
	description: string,
	appUrl: string,
	iconUrl: string,
	iconCached: string,
	iconCachedTime: number,
	defaultWidth: number,
	defaultHeight: number,
	minWidth: number,
	minHeight: number,
	maxWidth: number,
	maxHeight: number
}

async function tryUpdateIcon(bm: Bookmark) {
	try {
		const cacheduration = 1000 * 60 * 60 * 5;
		if (bm.iconCachedTime > Date.now() - cacheduration) { return; }
		let res = await fetch(bm.iconUrl);
		let imgbuf = await res.arrayBuffer();
		let mimetype = res.headers.get("Content-Type")!;
		if (["image/png", "image/jpeg"].indexOf(mimetype) == -1) { throw new UserError("unexpected image mimetype, only jpeg and png accepted"); }
		let base64 = "data:" + mimetype + ";base64," + Buffer.from(imgbuf).toString("base64");
		bm.iconCached = base64;
		bm.iconCachedTime = Date.now();
	} catch (e) {
		console.error(e);
	}
}

export function installApp(url: URL, res: AppConfigImport) {
	if (installedApps.find(a => a.configUrl == url.href)) {
		throw new UserError("App is already installed");
	}
	let config: InstalledApp = {
		appName: "",
		description: "",
		configUrl: url.href,
		appUrl: "",
		iconUrl: "",
		iconCached: "",
		iconCachedTime: 0,
		defaultHeight: 500,
		defaultWidth: 370,
		minHeight: 20,
		minWidth: 20,
		maxWidth: 0,
		maxHeight: 0,
		permissions: []
	};
	updateAppconfig(config, res);
	installedApps.push(config);
	return config;
}
function updateAppconfig(prev: InstalledApp, config: AppConfigImport) {

	let entryurl = sameDomainResolve(prev.configUrl, config.appUrl);
	let iconurl = sameDomainResolve(prev.configUrl, config.iconUrl);

	prev.appName = config.appName;
	prev.description = config.description;
	prev.appUrl = entryurl.href;
	prev.iconUrl = iconurl.href;
	prev.minWidth = config.minWidth;
	prev.minHeight = config.minHeight;
	prev.maxWidth = config.maxWidth;
	prev.maxHeight = config.maxHeight;
	prev.defaultWidth = config.defaultWidth;
	prev.defaultHeight = config.defaultHeight;
	tryUpdateIcon(prev);
}

export async function identifyApp(url: URL) {
	let res: AppConfigImport = await fetch(url.href).then(r => readJsonWithBOM(r));
	//TODO typecheck result
	let prev = installedApps.find(q => q.configUrl == url.href);
	if (!prev) {
		//throw new Error("App is not installed yet");
		//TODO add app confirm ui
		return installApp(url, res);
	} else {
		updateAppconfig(prev, res);
		return prev;
	}
}
