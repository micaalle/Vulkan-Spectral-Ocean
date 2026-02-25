A real-time ocean renderer in Vulkan that generates waves with a Tessendorf-style spectral FFT, adds choppy displacement, cascaded detail to reduce tiling, TAA, refraction/absorption shading, and a floating rubber duck driven by the same surface.


https://github.com/user-attachments/assets/08a0a335-040c-46e3-b52f-09ef9b38226b



https://github.com/user-attachments/assets/418f06e6-37fe-45a5-9624-0c426275ad80




https://github.com/user-attachments/assets/383aa974-ebae-4e81-ab07-d62bfb5a8da5


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
