A real-time ocean renderer in Vulkan that generates waves with a Tessendorf-style spectral FFT, adds choppy displacement, cascaded detail to reduce tiling, temporal foam + spray, TAA, refraction/absorption shading, and a floating rubber duck driven by the same surface.


## Controls

### Camera
- **W/A/S/D** — move  
- **Mouse** — look  
- **Left Shift** — move faster 

### Ocean / Rendering
- **P** — toggle : *(single patch or full ocean)*
- **M** — wireframe toggle  
- **N** — light/dark water
- **0 / 1 / 2** — debug views : *(with 2 bringing you back to the original view)*  

### Wave Tuning 
- **[ / ]** — wave height down / up  
- **, / .** — choppiness down / up  
- **; / '** — swell amplitude down / up  
- **- / =** — swell speed down / up  
- **I / K** — exposure up / down  
- **O / L** — bloom strength up / down  
- **U / J** — wave speed faster / slower  

### Duck Controls
- **Up Arrow** — throttle forward  
- **Down Arrow** — throttle reverse  
- **Left / Right Arrow** — turn  
- **Q / E** — yaw nudge  
- **B** — reset duck near camera  
