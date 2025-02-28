import * as path from "path";
import { default as Alt1Chainimport } from "@alt1/webpack";

// @ts-ignore
const __dirname = import.meta.dirname;
// @ts-ignore somehow weird default.defeeault situation going on. need to get rid of Alt1Chain at some point anyway
const Alt1Chain = Alt1Chainimport.default as typeof Alt1Chainimport;

function addMain(env: Record<string, string | boolean>) {
	var config = new Alt1Chain(path.resolve(__dirname, "./src/"), { nodejs: true, sourcemaps: true, ugly: false }, env);

	config.entry("alt1lite", "./main.ts");
	config.entry("appframe/index", "./appframe/index.tsx");
	config.entry("appframe/alt1api", "./appframe/alt1api.ts");
	config.entry("overlayframe/index", "./overlayframe/index.tsx");
	config.entry("settings/index", "./settings/index.tsx");
	config.entry("tooltip/index", "./tooltip/index.tsx");
	config.entry("tests/index", "./tests/index.ts");
	config.output(path.resolve(__dirname, "dist"));
	if (!config.opts.production) {
		config.chain.devtool("eval" as any);
	}
	config.addExternal("canvas", null);
	config.addExternal("sharp", null);
	return config.toConfig();
}



let exportobj = (env: Record<string, string | boolean>) => [].concat(
	addMain(env) as any
);

export default exportobj;