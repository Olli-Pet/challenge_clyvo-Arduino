const MQTT_URL = "wss://broker.hivemq.com:8884/mqtt";
const TOPIC_TELEMETRIA = "fiap/iot/2tdsa/olliepet/telemetria";

const elements = {
  connection: document.getElementById("connectionStatus"),
  temperatureValue: document.getElementById("temperatureValue"),
  temperatureStatus: document.getElementById("temperatureStatus"),
  heartValue: document.getElementById("heartValue"),
  heartStatus: document.getElementById("heartStatus"),
  locationValue: document.getElementById("locationValue"),
  locationStatus: document.getElementById("locationStatus"),
  generalStatus: document.getElementById("generalStatus"),
  statusDescription: document.getElementById("statusDescription"),
  riskCard: document.getElementById("riskCard"),
  lastUpdate: document.getElementById("lastUpdate"),
  feed: document.getElementById("messageFeed"),
  chart: document.getElementById("telemetryChart")
};

const historico = {
  temperatura: [],
  batimentos: []
};

function setConnection(status, text) {
  elements.connection.classList.remove("connected", "error");

  if (status) {
    elements.connection.classList.add(status);
  }

  elements.connection.lastChild.nodeValue = ` ${text}`;
}

function formatNumber(value, decimals = 1) {
  return Number(value).toFixed(decimals);
}

function updatePill(element, text, type) {
  element.className = `pill ${type || ""}`.trim();
  element.textContent = text;
}

function addFeedItem(text) {
  const item = document.createElement("li");
  const time = new Date().toLocaleTimeString("pt-BR", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  });

  item.textContent = `${time} | ${text}`;
  elements.feed.prepend(item);

  while (elements.feed.children.length > 5) {
    elements.feed.removeChild(elements.feed.lastElementChild);
  }
}

function pushHistory(key, value) {
  historico[key].push(Number(value));

  if (historico[key].length > 20) {
    historico[key].shift();
  }
}

function drawChart() {
  const canvas = elements.chart;
  const ctx = canvas.getContext("2d");
  const width = canvas.width;
  const height = canvas.height;
  const padding = 42;
  const graphWidth = width - padding * 2;
  const graphHeight = height - padding * 2;

  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#fbfdff";
  ctx.fillRect(0, 0, width, height);

  ctx.strokeStyle = "#dce3ed";
  ctx.lineWidth = 1;

  for (let i = 0; i <= 4; i++) {
    const y = padding + (graphHeight / 4) * i;
    ctx.beginPath();
    ctx.moveTo(padding, y);
    ctx.lineTo(width - padding, y);
    ctx.stroke();
  }

  drawLine(ctx, historico.temperatura, "#dc2626", 30, 42, padding, graphWidth, graphHeight);
  drawLine(ctx, historico.batimentos, "#2563eb", 70, 130, padding, graphWidth, graphHeight);

  ctx.fillStyle = "#18202f";
  ctx.font = "bold 15px Arial";
  ctx.fillText("Temperatura", padding, 24);
  ctx.fillStyle = "#dc2626";
  ctx.fillRect(padding + 102, 14, 12, 12);
  ctx.fillStyle = "#18202f";
  ctx.fillText("Batimentos", padding + 136, 24);
  ctx.fillStyle = "#2563eb";
  ctx.fillRect(padding + 224, 14, 12, 12);
}

function drawLine(ctx, values, color, min, max, padding, graphWidth, graphHeight) {
  if (values.length < 2) {
    return;
  }

  ctx.strokeStyle = color;
  ctx.lineWidth = 4;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();

  values.forEach((value, index) => {
    const x = padding + (graphWidth / Math.max(values.length - 1, 1)) * index;
    const normalized = Math.max(0, Math.min(1, (value - min) / (max - min)));
    const y = padding + graphHeight - normalized * graphHeight;

    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });

  ctx.stroke();
}

function updateDashboard(data) {
  const temperatura = Number(data.temperatura);
  const batimentos = Number(data.batimentos);
  const distancia = Number(data.distancia_cm);
  const status = String(data.status || "normal");
  const localizacao = data.localizacao || "sem leitura";

  elements.temperatureValue.textContent = `${formatNumber(temperatura)} C`;
  elements.heartValue.textContent = `${Math.round(batimentos)} bpm`;
  elements.locationValue.textContent = `${Math.round(distancia)} cm`;

  updatePill(elements.temperatureStatus, temperatura >= 39.2 ? "Temperatura alta" : "Normal", temperatura >= 39.2 ? "danger" : "ok");
  updatePill(elements.heartStatus, batimentos > 115 ? "Acelerado" : "Estavel", batimentos > 115 ? "warn" : "ok");
  updatePill(elements.locationStatus, localizacao, distancia > 180 ? "danger" : "ok");

  elements.riskCard.classList.toggle("alert", status === "alerta");
  elements.generalStatus.textContent = status === "alerta" ? "Atencao ao pet" : "Pet monitorado";
  elements.statusDescription.textContent = status === "alerta"
    ? "Algum indicador saiu da zona segura simulada."
    : "Temperatura, batimentos e distancia estao dentro do esperado.";

  elements.lastUpdate.textContent = `Atualizado as ${new Date().toLocaleTimeString("pt-BR")}`;

  pushHistory("temperatura", temperatura);
  pushHistory("batimentos", batimentos);
  drawChart();

  addFeedItem(`${formatNumber(temperatura)} C, ${Math.round(batimentos)} bpm, ${localizacao}`);
}

function startMqtt() {
  if (!window.mqtt) {
    setConnection("error", "Biblioteca MQTT indisponivel");
    return;
  }

  const clientId = `ollie-dashboard-${Math.random().toString(16).slice(2)}`;
  const client = mqtt.connect(MQTT_URL, {
    clientId,
    clean: true,
    connectTimeout: 5000,
    reconnectPeriod: 3000
  });

  client.on("connect", () => {
    setConnection("connected", "MQTT conectado");
    client.subscribe(TOPIC_TELEMETRIA);
    addFeedItem("Dashboard inscrito no topico MQTT");
  });

  client.on("reconnect", () => setConnection("", "Reconectando MQTT"));
  client.on("error", () => setConnection("error", "Erro MQTT"));

  client.on("message", (topic, payload) => {
    if (topic !== TOPIC_TELEMETRIA) {
      return;
    }

    try {
      updateDashboard(JSON.parse(payload.toString()));
    } catch (error) {
      addFeedItem("Mensagem recebida fora do formato JSON");
    }
  });
}

drawChart();
startMqtt();
