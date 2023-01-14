import { ipcRenderer } from "electron";
import type { settingsType, Bookmark } from "../settings";
import type { CaptureMode } from "../native";
import * as React from "react";
import * as ReactDom from "react-dom";

import "./style.scss";
import "./index.html";

window.addEventListener("DOMContentLoaded", start);

async function start() {
	let settings: settingsType = await ipcRenderer.invoke("getsettings");
	ReactDom.render(<Settings settings={settings} />, document.getElementById("root"));
}

interface SettingsProps {
	settings: settingsType;
}

interface SettingsState {
	tab: string;
}

class Settings extends React.Component<SettingsProps, SettingsState> {
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
		ipcRenderer.invoke("installapp", this.state.configUrl).then(() => start());
		e.preventDefault();
	}

	render() {
		let apps = this.props.bookmarks.map(i => {
			return <tr>
				<td><img width={20} height={20} src={i.iconCached}/></td>
				<td><span>{i.appName}</span></td>
				<td><button onClick={() => ipcRenderer.invoke("openapp", i.configUrl)}>Open</button></td>
				<td><button onClick={() => ipcRenderer.invoke("removeapp", i.configUrl).then(() => start())}>Remove</button></td>
			</tr>;
		})

		return <React.Fragment>
			<table>{apps}</table>
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
		start();
	}
	return (
		<React.Fragment>
			<p>Capture mode</p>
			<label><input type="radio" value="opengl" name="captmode" onChange={modechange} checked={captureMode == "opengl"} />OpenGL</label>
			<label><input type="radio" value="window" name="captmode" onChange={modechange} checked={captureMode == "window"} />Window</label>
			<label><input type="radio" value="desktop" name="captmode" onChange={modechange} checked={captureMode == "desktop"} />Desktop</label>
			<label><input type="checkbox" onChange={e => { e.currentTarget.checked = true; start(); }} checked={true} />Automatically detect capture mode</label>
		</React.Fragment>
	);
}