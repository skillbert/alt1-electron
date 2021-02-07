import * as React from "react";
import * as ReactDom from "react-dom";



import "./style.scss";
import "./index.html";


type Alt1Settings = ReturnType<typeof defaultSettings>

function defaultSettings() {
	return {
		captMode: "opengl" as "opengl" | "directx" | "desktop",
		captAutoDetect: true
	};
}


window.addEventListener("DOMContentLoaded", start);

let settings = defaultSettings();

function start() {
	ReactDom.render(<Settings />, document.getElementById("root"));
}


function Settings() {
	let modechange = (e: React.ChangeEvent<HTMLInputElement>) => {
		settings.captMode = e.currentTarget.value as any;
		start();
	}
	return (
		<React.Fragment>
			<p>Capture mode</p>
			<label><input type="radio" value="opengl" name="captmode" onChange={modechange} checked={settings.captMode == "opengl"} />OpenGL</label>
			<label><input type="radio" value="directx" name="captmode" onChange={modechange} checked={settings.captMode == "directx"} />DirectX</label>
			<label><input type="radio" value="desktop" name="captmode" onChange={modechange} checked={settings.captMode == "desktop"} />Desktop</label>
			<label><input type="checkbox" onChange={e => { settings.captAutoDetect = e.currentTarget.checked; start(); }} checked={settings.captAutoDetect} />Automatically detect capture mode</label>
		</React.Fragment>
	);
}