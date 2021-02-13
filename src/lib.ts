import * as path from "path";


declare global {
	//TODO webpack npm package should fix this
	var __non_webpack_require__: typeof require;
}

export class UserError extends Error { }


//export const rsClientExe = "notepad++.exe";
export const rsClientExe = "rs2client.exe";
export const schemestring = "alt1lite";
export const weborigin = "http://localhost";
export const configFile="./config.json";

//needed because node-fetch tries to be correct by choking on BOM
export async function readJsonWithBOM(res: { text(): Promise<string> }) {
	let text = await res.text()
	if (text.charCodeAt(0) === 0xFEFF) {
		text = text.substr(1)
	}
	return JSON.parse(text);
}

export function sameDomainResolve(base: URL | string, suburl: string) {
	if (typeof base == "string") { base = new URL(base); }
	let url = new URL(suburl, base);
	if (url.origin != base.origin) { throw new UserError("url must be from same domain as context"); }
	return url;
}

//Relative path from electron entry file
export function relPath(relpath: string) {
	return path.join(__dirname, relpath);
}