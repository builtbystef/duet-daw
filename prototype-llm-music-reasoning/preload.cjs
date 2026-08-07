// PROTOTYPE — disposable.
const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("api", {
  getInit: () => ipcRenderer.invoke("get-init"),
  run: (opts) => ipcRenderer.invoke("run", opts),
  abort: () => ipcRenderer.invoke("abort"),
  onEvent: (cb) => ipcRenderer.on("agent-event", (_e, payload) => cb(payload)),
});
