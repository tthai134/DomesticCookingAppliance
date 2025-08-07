<h1>Full-Size Domestic Cooking Appliance with Battery Power Source</h1>

 ### [YouTube Short Demonstration](https://www.youtube.com/watch?v=0nUOkMyQPz0)
 ### [YouTube Proof of Concept](https://www.youtube.com/watch?v=bfTAb9pQBPk)

<h2>Description</h2>
This project is an embedded firmware solution for an electric stove and oven powered by grid and battery sources. Developed in C++ for the ESP32, the system safely manages power distribution while providing real-time user control and feedback. 
<br />

<h2>Features</h2>

- <b>🔧 Dual Burner Control</b> 
  - Adjustable via potentiometers
  - Software Pulse Width Modulation with relay control
  - Hysteresis filtering to prevent flicker and noise
- <b>🔥 Oven Control (Bake/Broil Modes)</b>
  - Potentiometer-based temperature setpoint
  - ADC-based thermal feedback (°F)
  - Automatic hysteresis-based bake relay cycling
  - Broil mode engages both bake and broil elements
- <b>⚡ Intelligent Power Management</b>
  - Supports grid (1500W max) and battery (3000W max) sources
  - Dynamically limits total system load based on real-time power usage
  - Enforces constraints to prevent overload (e.g. disables burners when oven is active under high load)
- <b>📟 OLED User Interface</b>
  - Real-time status display for:
    - Burner power (%)
    - Oven mode and temperature
    - Setpoint temperature
    - Battery level


<h2>Languages and Utilities Used</h2>

- <b>C++</b> 
- <b>Arduino</b>
- <b>Visual Studio Code</b>
- <b>PlatformIO</b>


<h2>User Interface Progression:</h2>

<p align="center">
First screen showing diagnostics: <br/>
<img src="https://i.imgur.com/7LJp2cU.jpeg" height="80%" width="80%"/>
<br />
<br />
Larger screen showing with one burner functionality:  <br/>
<img src="https://i.imgur.com/CSRzJu6.jpeg" height="80%" width="80%"/>
<br />
<br />
Both burner controls and oven functionality: <br/>
<img src="https://i.imgur.com/2HdVYBL.jpeg" height="80%" width="80%"/>
<br />
<br />
Added battery level percentage:  <br/>
<img src="https://i.imgur.com/WeEO8i7.jpeg" height="80%" width="80%"/>
<br />
<br />
Final version on prototype:  <br/>
<img src="https://i.imgur.com/HFcCuk0.png" height="80%" width="80%">
<br />
<br />


<!--
 ```diff
- text in red
+ text in green
! text in orange
# text in gray
@@ text in purple (and bold)@@
```
--!>
