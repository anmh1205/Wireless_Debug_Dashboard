// Debug logger chỉ ghi log và không can thiệp vào handler gốc
console.log("Debug module loading...");

// Biến để đánh dấu đã ghi log để tránh lặp lại
let loggedMessages = new Set();

// Theo dõi WebSocket mà không can thiệp vào handler
function setupWebSocketLogger() {
  if (!window.WebSocket) return;

  // Tạo proxy cho WebSocket constructor
  const OriginalWebSocket = window.WebSocket;
  window.WebSocket = function (url, protocols) {
    const socket = new OriginalWebSocket(url, protocols);

    // Log outgoing messages
    const originalSend = socket.send;
    socket.send = function (data) {
      console.log("[WebSocket OUT]", data);
      return originalSend.call(this, data);
    };

    // Setup message logging without changing handler
    socket.addEventListener(
      "message",
      function (event) {
        // Tránh log trùng lặp
        const messageHash =
          typeof event.data === "string"
            ? event.data.substring(0, 100)
            : "binary-data";
        if (!loggedMessages.has(messageHash)) {
          console.log("[WebSocket RAW]", event.data);
          loggedMessages.add(messageHash);

          // Auto-cleanup sau 1 giây
          setTimeout(() => {
            loggedMessages.delete(messageHash);
          }, 1000);
        }
      },
      { passive: true }
    ); // passive flag để không chặn các handler khác

    return socket;
  };
  console.log("WebSocket logger initialized");
}

// Monitor MQTT messages
function monitorMQTT() {
  if (!mqtt_client) return;

  const originalPublish = mqtt_client.send;
  mqtt_client.send = function (message) {
    console.log("[MQTT OUT]", message.destinationName, message.payloadString);
    return originalPublish.call(this, message);
  };

  const originalOnMessageArrived = mqtt_client.onMessageArrived;
  mqtt_client.onMessageArrived = function (message) {
    console.log("[MQTT IN]", message.destinationName, message.payloadString);
    if (originalOnMessageArrived) {
      originalOnMessageArrived.call(this, message);
    }
  };
}

// Thêm giám sát cho updateBoxValue
function monitorUpdateBoxValue() {
  const originalUpdateBoxValue = window.updateBoxValue;
  window.updateBoxValue = function (id, value) {
    console.log("[UI Update] Box ID:", id, "Value:", value);
    return originalUpdateBoxValue.call(this, id, value);
  };
}

// Khởi tạo debug logger khi trang đã tải xong
document.addEventListener("DOMContentLoaded", function () {
  console.log("Debug tools initializing...");
  setupWebSocketLogger();
  setTimeout(() => {
    monitorMQTT();
    monitorUpdateBoxValue();

    // Kiểm tra boxes đã được render chưa
    console.log("Currently rendered boxes:", boxes);
  }, 1000);
});

// Thêm hàm kiểm tra handler trùng lặp
window.checkDuplicateHandlers = function () {
  if (!ws) {
    console.log("WebSocket chưa được khởi tạo");
    return;
  }

  // In ra thông tin về event handler đã đăng ký
  console.log("Current WebSocket state:", ws.readyState);
  console.log("WebSocket handler:", ws.onmessage ? "Registered" : "None");
};
