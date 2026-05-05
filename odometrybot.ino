#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "index.h"

// ==========================================
// 1. HARDWARE CONFIGURATION
// ==========================================
// L298N Motor Driver
const int IN1 = 33; // Left Forward
const int IN2 = 32; // Left Backward
const int IN3 = 25; // Right Forward
const int IN4 = 26; // Right Backward

// Encoders
ESP32Encoder encoderLeft;
ESP32Encoder encoderRight;
const int LEFT_ENC_A = 14;
const int LEFT_ENC_B = 13;
const int RIGHT_ENC_A = 4;
const int RIGHT_ENC_B = 5;

// MPU6050
Adafruit_MPU6050 mpu;
const int I2C_SDA = 18;
const int I2C_SCL = 19;
const int MPU_INT = 15;

// ==========================================
// 2. ODOMETRY CONSTANTS & VARIABLES
// ==========================================
const float WHEEL_DIAMETER = 0.065; 
const float TICKS_PER_REV = 345.0;  
const float DIST_PER_TICK = (PI * WHEEL_DIAMETER) / TICKS_PER_REV;

float x_pos = 0.0;
float y_pos = 0.0;
float theta = 0.0; // Heading

long last_ticks_left = 0;
long last_ticks_right = 0;

float gyroZ_offset = 0.0; // To counteract resting drift

unsigned long last_odom_time = 0;
unsigned long last_telemetry_time = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==========================================
// 3. MOTOR CONTROL FUNCTIONS
// ==========================================
// ==========================================
// 3. MOTOR CONTROL FUNCTIONS
// ==========================================
// Set speed between 0 and 255. 
// (Note: Below ~80, the motors might whine but not have enough torque to move)
const int DRIVE_SPEED = 100; 

void stopMotors() {
  analogWrite(IN1, 0); analogWrite(IN2, 0);
  analogWrite(IN3, 0); analogWrite(IN4, 0);
}
void moveForward() {
  analogWrite(IN1, DRIVE_SPEED); analogWrite(IN2, 0);
  analogWrite(IN3, DRIVE_SPEED); analogWrite(IN4, 0);
}
void moveBackward() {
  analogWrite(IN1, 0); analogWrite(IN2, DRIVE_SPEED);
  analogWrite(IN3, 0); analogWrite(IN4, DRIVE_SPEED);
}
void turnLeft() {
  analogWrite(IN1, 0); analogWrite(IN2, DRIVE_SPEED); // Left wheel backward
  analogWrite(IN3, DRIVE_SPEED); analogWrite(IN4, 0); // Right wheel forward
}
void turnRight() {
  analogWrite(IN1, DRIVE_SPEED); analogWrite(IN2, 0); // Left wheel forward
  analogWrite(IN3, 0); analogWrite(IN4, DRIVE_SPEED); // Right wheel backward
}

// ==========================================
// 4. WEBSOCKET HANDLER
// ==========================================
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (!error) {
      const char* command = doc["command"];
      if (strcmp(command, "F") == 0) moveForward();
      else if (strcmp(command, "B") == 0) moveBackward();
      else if (strcmp(command, "L") == 0) turnLeft();
      else if (strcmp(command, "R") == 0) turnRight();
      else if (strcmp(command, "S") == 0) stopMotors();
    }
  }
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Setup Motors
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  // Setup Encoders
  encoderLeft.attachFullQuad(LEFT_ENC_A, LEFT_ENC_B);
  encoderRight.attachFullQuad(RIGHT_ENC_A, RIGHT_ENC_B);
  encoderLeft.clearCount();
  encoderRight.clearCount();

  // Setup MPU6050 Custom Pins
  pinMode(MPU_INT, INPUT); // Declare INT pin (optional for our current polling loop)
  Wire.begin(I2C_SDA, I2C_SCL); // Pass custom SDA and SCL pins here
  
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050! Check wiring.");
    while (1) { delay(10); }
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Calibrate Gyro (DO NOT MOVE BOT DURING THIS)
  Serial.println("Calibrating Gyro... Do not touch bot!");
  for (int i = 0; i < 200; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    gyroZ_offset += g.gyro.z;
    delay(10);
  }
  gyroZ_offset /= 200.0;
  Serial.println("Calibration complete!");

  // Connect to WiFi
  WiFi.begin("Pixel_Azad", "33cocoon");
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! Open this IP in your browser:");
  Serial.println(WiFi.localIP());

  // Setup Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();

  last_odom_time = millis();
}

// ==========================================
// 6. MAIN LOOP (SENSOR FUSION)
// ==========================================
void loop() {
  ws.cleanupClients();
  unsigned long current_time = millis();

  // Run Odometry Math very fast (every 20ms) for accurate integration
  if (current_time - last_odom_time >= 20) {
    float dt = (current_time - last_odom_time) / 1000.0; // Time step in seconds

    // 1. Read Encoders for Distance (Forward/Backward movement)
    // NOTE: If one wheel tracks backwards, add a negative sign here!
    long current_ticks_left = encoderLeft.getCount();
    long current_ticks_right = encoderRight.getCount();

    float d_left = (current_ticks_left - last_ticks_left) * DIST_PER_TICK;
    float d_right = (current_ticks_right - last_ticks_right) * DIST_PER_TICK;
    
    last_ticks_left = current_ticks_left;
    last_ticks_right = current_ticks_right;

    float d_center = (d_right + d_left) / 2.0;

    // 2. Read MPU6050 for Rotation
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    
    // Apply calibration offset to Z-axis (rad/s)
    float gyro_z_rad = g.gyro.z - gyroZ_offset;
    
    // Deadband filter to stop micro-drifting when completely still
    if(abs(gyro_z_rad) < 0.03) gyro_z_rad = 0.0; 

    // Integrate angular velocity to get angle (Theta)
    theta += gyro_z_rad * dt;

    // Normalize Theta between -PI and PI
    while (theta > PI) theta -= 2.0 * PI;
    while (theta <= -PI) theta += 2.0 * PI;

    // 3. Update Global Position
    x_pos += d_center * cos(theta);
    y_pos += d_center * sin(theta);

    last_odom_time = current_time;
  }

  // Send Telemetry slower (every 150ms) to prevent crashing the WebSocket
  if (current_time - last_telemetry_time >= 150) {
    if (ws.count() > 0) {
      StaticJsonDocument<200> txDoc;
      txDoc["x"] = x_pos;
      txDoc["y"] = y_pos;
      txDoc["theta"] = theta;
      
      char jsonString[200];
      serializeJson(txDoc, jsonString);
      ws.textAll(jsonString);
    }
    last_telemetry_time = current_time;
  }
}
#ifndef INDEX_H
#define INDEX_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Odometry & Turtle Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
        body { text-align: center; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #1e1e1e; color: #f0f0f0; margin: 0; padding: 10px; }
        h2 { margin-bottom: 5px; color: #00bcd4; }
        .telemetry { background: #2a2a2a; padding: 10px; border-radius: 8px; margin-bottom: 15px; font-size: 1.1em; display: inline-block; }
        span { font-weight: bold; color: #00bcd4; }
        canvas { border: 2px solid #00bcd4; background-color: #2a2a2a; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.5); margin-bottom: 20px; }
        
        .d-pad { display: grid; grid-template-columns: repeat(3, 70px); gap: 10px; justify-content: center; margin-bottom: 20px; }
        button { width: 70px; height: 70px; font-size: 24px; font-weight: bold; border-radius: 15px; background-color: #00bcd4; color: #1e1e1e; border: none; cursor: pointer; user-select: none; touch-action: manipulation; box-shadow: 0 4px #0097a7; transition: 0.1s; }
        button:active { background-color: #4dd0e1; box-shadow: 0 0 #0097a7; transform: translateY(4px); }
        .empty { visibility: hidden; }
    </style>
</head>
<body>
    <h2>Robot Visualizer</h2>
    <div class="telemetry">
        X: <span id="x_val">0.00</span>m | Y: <span id="y_val">0.00</span>m | &theta;: <span id="t_val">0.00</span>rad
    </div>
    <br>
    
    <canvas id="mapCanvas" width="400" height="400"></canvas>

    <div class="d-pad">
        <div class="empty"></div>
        <button onmousedown="sendCommand('F')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('F')" ontouchend="sendCommand('S')">&#8593;</button>
        <div class="empty"></div>
        
        <button onmousedown="sendCommand('L')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('L')" ontouchend="sendCommand('S')">&#8592;</button>
        <button onmousedown="sendCommand('S')" ontouchstart="sendCommand('S')">&#9632;</button>
        <button onmousedown="sendCommand('R')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('R')" ontouchend="sendCommand('S')">&#8594;</button>
        
        <div class="empty"></div>
        <button onmousedown="sendCommand('B')" onmouseup="sendCommand('S')" ontouchstart="sendCommand('B')" ontouchend="sendCommand('S')">&#8595;</button>
        <div class="empty"></div>
    </div>

    <script>
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;
        var canvas = document.getElementById('mapCanvas');
        var ctx = canvas.getContext('2d');
        
        // Setup Map Starting Point (middle of canvas)
        var startX = canvas.width / 2; 
        var startY = canvas.height / 2; 
        var scale = 50; // 50 pixels per 1 meter

        // Variables to hold current robotic pose
        var currentX = 0;
        var currentY = 0;
        var currentTheta = 0;

        // Path history array to store all previous [x, y] points
        var pathHistory = [];

        function initWebSocket() {
            websocket = new WebSocket(gateway);
            websocket.onmessage = function(event) {
                var data = JSON.parse(event.data);
                
                // 1. Update text spans (Telemetry)
                document.getElementById('x_val').innerText = data.x.toFixed(2);
                document.getElementById('y_val').innerText = data.y.toFixed(2);
                document.getElementById('t_val').innerText = data.theta.toFixed(2);
                
                // 2. Convert meters to canvas pixel coordinates
                var drawX = startX + (data.x * scale);
                var drawY = startY - (data.y * scale); // Canvas Y is inverted (0 is top)
                
                // 3. Store this new position in history
                pathHistory.push({x: drawX, y: drawY});
                
                // 4. Update current pose for the turtle drawing
                currentX = drawX;
                currentY = drawY;
                currentTheta = data.theta; // Angle (heading) in radians

                // 5. Trigger a redraw of the entire scene
                redrawMap();
            };
        }

        // Function to redraw the path trail AND the turtle
        function redrawMap() {
            // A. Clear the previous frame (erase everything)
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            // B. Draw the historical Trail (jaggered path line)
            if (pathHistory.length > 1) {
                ctx.strokeStyle = "#444"; // Grey color for path trail
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(pathHistory[0].x, pathHistory[0].y);
                
                for (var i = 1; i < pathHistory.length; i++) {
                    ctx.lineTo(pathHistory[i].x, pathHistory[i].y);
                }
                ctx.stroke();
            }

            // C. Draw the "Turtle" (Triangle Pointer at current position/heading)
            drawTurtle(currentX, currentY, currentTheta);
        }

        // Draws a rotated triangle representing the robot
        function drawTurtle(x, y, theta) {
            var size = 12; // Radius size of the turtle triangle

            ctx.save(); // Save the current unrotated drawing state
            
            // Move drawing context to robot position
            ctx.translate(x, y);
            
            // Rotate context to robot heading (radians).
            // We negate theta because canvas clockwise is opposite to standard counter-clockwise
            ctx.rotate(-theta); 
            
            // Draw the Triangle (Pointing forward along the context's new X-axis)
            ctx.fillStyle = "#00bcd4"; // Cyan color for active robot
            ctx.beginPath();
            
            // Points define a forward-pointing triangle relative to center (0,0)
            ctx.moveTo(size, 0);               // Tip pointing forward
            ctx.lineTo(-size / 2, size / 1.5);  // Rear Right
            ctx.lineTo(-size / 2, -size / 1.5); // Rear Left
            
            ctx.fill();
            ctx.closePath();
            
            ctx.restore(); // Restore back to default unrotated state
        }

        function sendCommand(cmd) {
            if (websocket.readyState === WebSocket.OPEN) {
                websocket.send(JSON.stringify({command: cmd}));
            }
        }
        
        window.addEventListener('load', initWebSocket);
    </script>
</body>
</html>
)rawliteral";

#endif