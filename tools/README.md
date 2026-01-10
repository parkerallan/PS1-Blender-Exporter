Modified version of PNG TO TIM convert utility v0.2 - by Orion_ [2024]https://orionsoft.games/
**I had to swap the R and B color channels for this tool to display color correctly for me.**
Upload the texture 320px right of framebuffer area:
.\png2tim.exe -p 320 0 rikatexture.png;

Convert the TIM image, rikatexture_tim being the array name:
python bin2header.py rikatexture.tim rikatexture.h rikatexture_tim