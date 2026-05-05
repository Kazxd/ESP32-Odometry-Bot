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
