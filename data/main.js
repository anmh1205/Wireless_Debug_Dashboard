// Dashboard đơn giản, hiệu quả cho ESP32 - Tối ưu hóa độ trễ
// ---------------------------------

// Global variables
let ws = null; // WebSocket connection
let boxes = {}; // Box data store

// Khởi tạo dashboard
document.addEventListener("DOMContentLoaded", function () {
  console.log("Initializing dashboard");

  // Thiết lập event listeners
  document
    .getElementById("addBoxBtn")
    .addEventListener("click", showAddBoxModal);
  document.getElementById("saveBoxBtn").addEventListener("click", saveBox);
  document.querySelector(".close").addEventListener("click", hideAddBoxModal);
  window.addEventListener("click", function (event) {
    if (event.target === document.getElementById("addBoxModal")) {
      hideAddBoxModal();
    }
  });

  // Kết nối WebSocket
  connectWebSocket();
});

// Kết nối WebSocket
function connectWebSocket() {
  // Đóng kết nối cũ nếu có
  if (ws) {
    ws.close();
  }

  // Tạo kết nối WebSocket mới
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  const wsUrl = `${protocol}//${window.location.host}/ws`;

  console.log(`Connecting WebSocket: ${wsUrl}`);

  try {
    ws = new WebSocket(wsUrl);

    // Xử lý sự kiện WebSocket
    ws.onopen = function () {
      console.log("WebSocket connected");
      loadBoxes();
    };

    ws.onclose = function () {
      console.log("WebSocket disconnected, reconnecting...");
      setTimeout(connectWebSocket, 2000); // Kết nối lại sau 2 giây
    };

    // Xử lý tin nhắn WebSocket (tối ưu tốc độ)
    ws.onmessage = function (event) {
      try {
        const data = JSON.parse(event.data);

        // Xử lý nhanh nhất có thể dựa theo loại tin nhắn
        if (data.type === "update" && data.id && data.value !== undefined) {
          // Cập nhật trực tiếp vào DOM
          updateBoxValue(data.id, data.value);
        } else if (
          data.type === "boxes" &&
          data.boxes &&
          Array.isArray(data.boxes)
        ) {
          // Chỉ render boxes khi cần
          renderBoxes(data.boxes);
        }
      } catch (error) {
        console.error("WebSocket message error:", error);
      }
    };

    ws.onerror = function (error) {
      console.error("WebSocket error:", error);
    };
  } catch (e) {
    console.error("Error creating WebSocket:", e);
    // Thử kết nối lại sau 5 giây
    setTimeout(connectWebSocket, 5000);
  }
}

// Load danh sách boxes ban đầu
function loadBoxes() {
  fetch("/getBoxes")
    .then((response) => response.json())
    .then((data) => renderBoxes(data))
    .catch((error) => console.error("Error loading boxes:", error));
}

// Render boxes vào UI (tối ưu tốc độ)
function renderBoxes(boxData) {
  console.log(`Rendering ${boxData.length} boxes`);

  const container = document.getElementById("boxContainer");
  container.innerHTML = "";
  boxes = {};

  // Sử dụng document fragment để giảm số lần reflow
  const fragment = document.createDocumentFragment();

  boxData.forEach((box) => {
    boxes[box.id] = box;
    fragment.appendChild(createBoxElement(box));
  });

  container.appendChild(fragment);
}

// Tạo element box
function createBoxElement(box) {
  const boxDiv = document.createElement("div");
  boxDiv.className = "box";
  boxDiv.dataset.id = box.id;

  // Header
  const header = document.createElement("div");
  header.className = "box-header";

  const title = document.createElement("div");
  title.className = "box-title";
  title.textContent = box.name;

  const id = document.createElement("div");
  id.className = "box-id";
  id.textContent = `ID: ${box.id}`;

  header.appendChild(title);
  header.appendChild(id);

  // Value display
  const value = document.createElement("div");
  value.className = "box-value";
  value.id = `value-${box.id}`;
  value.textContent = box.value || "0";

  // Delete button
  const deleteBtn = document.createElement("button");
  deleteBtn.className = "delete-btn";
  deleteBtn.title = "Delete box";
  deleteBtn.innerHTML = '<span class="trash-icon">🗑️</span>';
  deleteBtn.addEventListener("click", () => deleteBox(box.id));

  boxDiv.appendChild(header);
  boxDiv.appendChild(value);
  boxDiv.appendChild(deleteBtn);

  return boxDiv;
}

// Cập nhật giá trị box (tối ưu tốc độ, không animation phức tạp)
function updateBoxValue(id, value) {
  const valueElement = document.getElementById(`value-${id}`);
  if (valueElement) {
    // Cập nhật DOM trực tiếp
    valueElement.textContent = value;

    // Hiệu ứng đơn giản sử dụng CSS
    valueElement.classList.add("updated");
    setTimeout(() => {
      valueElement.classList.remove("updated");
    }, 300);
  }
}

// Thêm box mới - đã xóa tham chiếu đến pendingRequests
function saveBox() {
  const name = document.getElementById("boxName").value.trim();
  const id = parseInt(document.getElementById("boxId").value);

  if (!name || isNaN(id)) {
    alert("Please enter both name and ID");
    return;
  }

  if (boxes[id]) {
    alert("A box with this ID already exists");
    return;
  }

  // Gửi request tạo box mới
  fetch("/addBox", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name, id }),
  })
    .then((response) => response.json())
    .then((data) => {
      if (data.success) {
        hideAddBoxModal();
      } else {
        alert("Failed to add box: " + data.message);
      }
    })
    .catch((error) => {
      console.error("Error adding box:", error);
      alert("An error occurred while adding the box");
    });
}

// Xóa box
function deleteBox(id) {
  if (confirm("Are you sure you want to delete this box?")) {
    fetch("/deleteBox", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id }),
    })
      .then((response) => response.json())
      .catch((error) => {
        console.error("Error deleting box:", error);
        alert("An error occurred while deleting the box");
      });
  }
}

// Hiện modal thêm box
function showAddBoxModal() {
  document.getElementById("boxName").value = "";
  document.getElementById("boxId").value = "";
  document.getElementById("addBoxModal").style.display = "block";
}

// Ẩn modal thêm box
function hideAddBoxModal() {
  document.getElementById("addBoxModal").style.display = "none";
}
