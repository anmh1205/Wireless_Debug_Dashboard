// Tiện ích nhỏ để kiểm tra trùng lặp tin nhắn WebSocket
(function () {
  const originalWebSocket = window.WebSocket;

  // Lưu lại các tin nhắn đã nhận
  const receivedMessages = new Map();

  // Ghi đè lên WebSocket constructor
  window.WebSocket = function (url, protocols) {
    console.log(`[DuplicateDetector] Tạo WebSocket tới ${url}`);
    const socket = new originalWebSocket(url, protocols);

    // Ghi đè lên onmessage
    const originalOnMessage = socket.onmessage;

    socket.addEventListener(
      "message",
      function (event) {
        const messageHash = hashString(event.data);
        const now = Date.now();

        if (receivedMessages.has(messageHash)) {
          const { count, lastTime } = receivedMessages.get(messageHash);
          const newCount = count + 1;
          const timeDiff = now - lastTime;

          console.warn(
            `[DuplicateDetector] TIN NHẮN TRÙNG LẶP #${newCount} (${timeDiff}ms sau lần trước): `,
            event.data.substring(0, 100)
          );

          receivedMessages.set(messageHash, { count: newCount, lastTime: now });
        } else {
          console.log(
            `[DuplicateDetector] Tin nhắn mới: `,
            event.data.substring(0, 100)
          );
          receivedMessages.set(messageHash, { count: 1, lastTime: now });
        }
      },
      true
    ); // Sử dụng capture phase để đảm bảo chạy trước các handler khác

    return socket;
  };

  // Hàm băm chuỗi đơn giản
  function hashString(str) {
    let hash = 0;
    for (let i = 0; i < str.length; i++) {
      const char = str.charCodeAt(i);
      hash = (hash << 5) - hash + char;
      hash = hash & hash; // Convert to 32bit integer
    }
    return hash;
  }

  console.log(
    "[DuplicateDetector] Đã cài đặt detector để kiểm tra tin nhắn trùng lặp"
  );
})();
