import { ipcRenderer } from "electron";
import type { Settings, Bookmark } from "../settings";
import type { CaptureMode } from "../native";
import * as React from "react";
import * as ReactDom from "react-dom";

import "./style.scss";
import "./index.html";

window.addEventListener("DOMContentLoaded", start);
ipcRenderer.on("settings-changed", e => {
	start();
});

async function start() {
	let settings: Settings = await ipcRenderer.invoke("getsettings");
	ReactDom.render(<SettingsComponent settings={settings} />, document.getElementById("root"));
}

interface SettingsProps {
	settings: Settings;
}

interface SettingsState {
	tab: string;
}

class SettingsComponent extends React.Component<SettingsProps, SettingsState> {
	constructor(props) {
		super(props);
		this.state = {
			tab: "apps"
		}
	}

	render() {
		let content = <h1>Invalid tab</h1>;

		if (this.state.tab == "apps") {
			content = <AppSettings bookmarks={this.props.settings.bookmarks}/>
		} else if (this.state.tab == "capture") {
			content = <CaptureSettings captureMode={this.props.settings.captureMode}/>
		}

		return (
			<React.Fragment>
				<button onClick={() => this.setState({tab: "apps"})}>Apps</button>
				<button onClick={() => this.setState({tab: "capture"})}>Capture</button>
				<hr/>
				{content}
			</React.Fragment>
		);
	}
}

interface AppSettingsProps {
	bookmarks: Array<Bookmark>;
}

interface AppSettingsState {
	configUrl: string;
}

class AppSettings extends React.Component<AppSettingsProps, AppSettingsState> {
	constructor(props) {
		super(props);
		this.state = {
			configUrl: ""
		}
	}

	addAppSubmit(e) {
		ipcRenderer.invoke("installapp", this.state.configUrl);
		this.setState({configUrl: ""});
		e.preventDefault();
	}

	render() {
		let apps = this.props.bookmarks.map(i => {
			return <tr key={i.appUrl}>
				<td><img width={20} height={20} src={i.iconCached}/></td>
				<td><span>{i.appName}</span></td>
				<td><button onClick={() => ipcRenderer.invoke("openapp", i.configUrl)}>Open</button></td>
				<td><button onClick={() => ipcRenderer.invoke("removeapp", i.configUrl)}>Remove</button></td>
			</tr>;
		})

		return <React.Fragment>
			<table><tbody>{apps}</tbody></table>
			<hr/>
			<form onSubmit={this.addAppSubmit.bind(this)}>
				<label>Config URL <input type="text" value={this.state.configUrl} onChange={e => this.setState({configUrl: e.target.value})}/></label>
				<button type="submit">Add App</button>
			</form>
		</React.Fragment>;
	}
}


function CaptureSettings(props) {
	let captureMode: CaptureMode = props.captureMode;

	let modechange = (e: React.ChangeEvent<HTMLInputElement>) => {
		captureMode = e.currentTarget.value as any;
	}
	return (
		<React.Fragment>
			<p>Capture mode</p>
			<label><input type="radio" value="opengl" name="captmode" onChange={modechange} checked={captureMode == "opengl"} />OpenGL</label>
			<label><input type="radio" value="window" name="captmode" onChange={modechange} checked={captureMode == "window"} />Window</label>
			<label><input type="radio" value="desktop" name="captmode" onChange={modechange} checked={captureMode == "desktop"} />Desktop</label>
			<label><input type="checkbox" onChange={e => { e.currentTarget.checked = true; }} checked={true} />Automatically detect capture mode</label>
		</React.Fragment>
	);
}