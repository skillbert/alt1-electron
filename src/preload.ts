import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("electronAPI", {
    addThirdPartyPlugin: (url: string) => ipcRenderer.send("add-third-party-plugin", url),
    navigate: (url: string) => ipcRenderer.send("navigate-url", url)
});
