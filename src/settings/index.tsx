import * as React from "react";
import {useEffect, useRef, useState} from "react";
import * as ReactDom from "react-dom";
import {ipcRenderer as ipc} from "electron";

import "./style.scss";
import "./index.html";
import {CaptureMode} from "../native";
import {Form, Tab, Tabs} from "react-bootstrap";
import {FlatImageData} from "../shared";
import CaptureTab from "./capture-tab";

interface Alt1Settings {
    captMode: CaptureMode;
    captAutoDetect: boolean;
    overrideScale: -1.0 | 1.0;
    compatibleAutoToggle: boolean;
}

const defaultSettings = (): Alt1Settings => ({
    captMode: "opengl",
    captAutoDetect: true,
    overrideScale: -1.0,
    compatibleAutoToggle: true,
});

window.addEventListener("DOMContentLoaded", () => {
    ReactDom.render(<Settings/>, document.getElementById("root"));
});

const Settings: React.FunctionComponent = () => {

    // TODO: useEffect on settings to send them to main process and persist in file system?

    return (
        <Tabs defaultActiveKey="capture">
            <Tab eventKey="capture" title="Capture" style={{padding: '0 10px'}}>
                <CaptureTab/>
            </Tab>
            <Tab eventKey="hotkeys" title="Hotkeys">
                Hotkeys
            </Tab>
        </Tabs>
    );

};