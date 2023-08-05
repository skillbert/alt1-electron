import { updateTray } from "./main";
import { readJsonWithBOM, sameDomainResolve, UserError } from "./lib";
import fetch from "node-fetch";
import { Bookmark, settings } from "./settings";
import Checks, { UservarType } from "../tempexternal/typecheck";

var checkAppConfigImport = Checks.obj({
	appName: Checks.str(),
	description: Checks.str(""),
	appUrl: Checks.str(),
	configUrl: Checks.str(),
	iconUrl: Checks.str(""),
	defaultWidth: Checks.num(400),
	defaultHeight: Checks.num(500),
	minWidth: Checks.num(0),
	minHeight: Checks.num(0),
	maxWidth: Checks.num(10000),
	maxHeight: Checks.num(10000),
	permissions: Checks.arr(Checks.strenum({ pixel: "Pixel", game: "Gamestate", overlay: "Overlay" }))
});
export type AppConfigImport = UservarType<typeof checkAppConfigImport>;

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
	if (settings.bookmarks.find(a => a.configUrl == url.href)) {
		throw new UserError("App is already installed");
	}
	let config: Bookmark = {
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
		permissions: [],
		lastRect: null,
		wasOpen: false
	};
	settings.bookmarks.push(config);
	updateAppconfig(config, res);
	return config;
}
function updateAppconfig(prev: Bookmark, config: AppConfigImport) {

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
	updateTray();
}

export async function identifyApp(url: URL) {
	try {
		let res: unknown = await fetch(url.href).then(r => readJsonWithBOM(r));
		var config = checkAppConfigImport.load(res, { defaultOnError: true });
	} catch (e) {
		console.log("failed to load appconfig from url: " + url);
		return;
	}
	//TODO typecheck result
	let prev = settings.bookmarks.find(q => q.configUrl == url.href);
	if (!prev) {
		//throw new Error("App is not installed yet");
		//TODO add app confirm ui
		return installApp(url, config);
	} else {
		updateAppconfig(prev, config);
		return prev;
	}
}
