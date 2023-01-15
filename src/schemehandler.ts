import { dialog, Menu, MenuItem, Tray } from "electron/main";
import { openApp } from "./main";
import fetch from "node-fetch";
import { readJsonWithBOM, schemestring, UserError } from "./lib";
import { AppConfigImport } from "./appconfig";
import { settings } from "./settings";

function takeString(str: string, part: string) {
	if (!str.startsWith(part)) { throw new UserError("expected string not matched at " + str.slice(0, 20)); }
	return str.slice(part.length);
}
function takeRegex(str: string, reg: RegExp): [string, RegExpMatchArray] {
	let m = str.match(reg);
	if (!m || m.index != 0) { throw new UserError("expected string not matched at " + str.slice(0, 20)); }
	return [str.slice(m[0].length), m];
}

export function handleSchemeArgs(argv: string[]) {
	if (argv[argv.length - 1].startsWith(schemestring)) {
		handleSchemeCommand(argv[argv.length - 1]);
	}
}

export async function handleSchemeCommand(url: string) {
	try {
		let m: RegExpMatchArray;
		url = takeString(url, schemestring);
		[url] = takeRegex(url, /:(\/\/)?/);
		[url, m] = takeRegex(url, /(\w+)(\/|$)/);
		switch (m[1]) {
			case "addapp":
				let cnfurl = new URL(url);
				let res: AppConfigImport = await fetch(cnfurl.href).then(r => readJsonWithBOM(r));
				await settings.appconfig.installApp(cnfurl, res);
				break
			case "openapp":
				let app = settings.bookmarks.find(a => a.configUrl == url);
				if (!app) { throw new UserError("app not found"); }
				openApp(app);
				break;
			default:
				throw new UserError("unknown url command " + m[1]);
				break;
		}

	} catch (e) {
		if (e instanceof UserError) {
			dialog.showErrorBox("url command error", "" + e);
		}
		console.error(e);
	}

}