"use strict";
const buttons = [...document.querySelectorAll("button[data-command]")];
const output = document.getElementById("output");
const state = document.getElementById("bridge-state");
const response = document.getElementById("response-state");
const allowed = new Set([
  "service.list", "service.report", "service.simulate.hosted",
  "service.evaluate.hosted", "service.evaluate.managed"
]);
const connected = Boolean(window.chrome && window.chrome.webview &&
                          typeof window.chrome.webview.postMessage === "function");
state.textContent = connected ? "Bridge local disponível" : "WebView2 indisponível";
buttons.forEach(button => {
  button.disabled = !connected;
  button.addEventListener("click", () => {
    const command = button.dataset.command;
    if (!connected || !allowed.has(command)) return;
    response.textContent = "Consultando fixture…";
    window.chrome.webview.postMessage(command);
  });
});
if (connected) {
  window.chrome.webview.addEventListener("message", event => {
    const data = event.data;
    if (typeof data !== "string" || data.length > 262144) {
      response.textContent = "Resposta inválida";
      return;
    }
    try {
      output.textContent = JSON.stringify(JSON.parse(data), null, 2);
      response.textContent = "Resposta sintética local";
    } catch (_) {
      output.textContent = "A resposta não é um JSON válido.";
      response.textContent = "Falha no formato";
    }
  });
  window.chrome.webview.postMessage("service.report");
} else {
  output.textContent = "Abra o aplicativo nativo simulator_desktop.exe para usar a ponte de leitura. Nenhum serviço externo será consultado.";
}
