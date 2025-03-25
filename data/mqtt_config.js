// Cấu hình MQTT WebSocket cho ESP32
(function () {
  console.log("MQTT Config loaded");

  // Kiểm tra kết nối MQTT WebSocket và cập nhật UI theo trạng thái
  function checkMqttWebSocketSupport() {
    const serverIP = window.location.hostname;
    const wsUrl = `ws://${serverIP}:9001/mqtt`;

    // Thử kết nối WebSocket đến MQTT port để kiểm tra
    try {
      const testWs = new WebSocket(wsUrl);

      testWs.onopen = function () {
        console.log("MQTT WebSocket port is accessible");
        document.getElementById("mqtt-status").textContent = "Available";
        document.getElementById("mqtt-status").style.color = "green";
        testWs.close();
      };

      testWs.onerror = function () {
        console.log("MQTT WebSocket port is not accessible");
        document.getElementById("mqtt-status").textContent = "Not Available";
        document.getElementById("mqtt-status").style.color = "red";
      };
    } catch (e) {
      console.error("Error testing MQTT WebSocket:", e);
      document.getElementById("mqtt-status").textContent = "Error";
      document.getElementById("mqtt-status").style.color = "red";
    }
  }

  // Chạy khi trang mqtt_info.html được tải
  document.addEventListener("DOMContentLoaded", function () {
    if (document.getElementById("mqtt-status")) {
      checkMqttWebSocketSupport();
    }
  });
})();
