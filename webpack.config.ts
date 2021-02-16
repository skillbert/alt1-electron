import * as path from "path";
import Alt1Chain from "@alt1/webpack";


module.exports = [].concat(
	addMain() as any
);

function addMain() {
	var config = new Alt1Chain(path.resolve(__dirname, "./src/"), { nodejs: true, sourcemaps: true, ugly: false });

	config.chain.resolveLoader.modules.add(path.resolve(__dirname, "./node_modules"));
	config.chain.resolveLoader.modules.add(path.resolve(__dirname, "../libs/node_modules"));
	config.chain.resolveLoader.modules.add(path.resolve(__dirname, "../libs/alt1"));

	config.chain.target("node");
	config.entry("alt1lite", "./main.ts");
	config.entry("appframe/index", "./appframe/index.tsx");
	config.entry("appframe/alt1api", "./appframe/alt1api.ts");
	config.entry("overlayframe/index", "./overlayframe/index.tsx");
	config.entry("settings/index", "./settings/index.tsx");
	config.output(path.resolve(__dirname, "dist"));
	if (!config.opts.production) {
		config.chain.devtool("" as any);
	}
	//config.chain.devtool("cheap-module-eval-source-map");
	return config.toConfig();
}


