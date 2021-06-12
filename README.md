## <ins>__[Discord server](https://discord.gg/YBQCRDG)__</ins> | <ins>__[Site](https://k2vr.tech/)__</ins>

![Build](https://github.com/KimihikoAkayasaki/KinectToVR/workflows/Build/badge.svg)

Automatic calibration scripts are written by **[コレヂャン](https://github.com/korejan)**<br>
(Also general help with just everything related to real programming)<br>
KinectToVR base is **[Sharky's](https://github.com/sharkyh20/)**<br>
Rest is probably written by **[公彦赤屋先](https://github.com/KimihikoAkayasaki)**<br>
**[Triping](https://github.com/TripingPC)** organizes the whole project.<br>

## License
This project is licensed under the GNU GPL v3 License 

## Build
You'll need:
 - Visual Studio 2019 (with: C++, v142 tools, ATL)<br>or just build tools for same (see GitHub Actions script)
 - Kinect SDK 1.8 & 2.0 installed and visible in PATH
 - Working installation of SteamVR for testing

Follow **[GitHub Actions script](https://github.com/KimihikoAkayasaki/KinectToVR/blob/master/.github/workflows/main.yml)**, or:<br>

- Clone Valve's ```OpenVR``` to ```external/``` (eventually remove ```-master``` from folder name)<br>
- Restore NuGet packages for ```VRInputEmulator``` and ```KinectToVR```
- Build ```lib_vrinputemulator``` (another solution in ```external/```) in ```x64/Release```
- Build all in ```KinectToVR``` in ```x64/Release```

## Deploy
Grab all needed files from your current KinectToVR installation folder.<br>
This also applies to OpenVR driver folders structure and files.

## This Branch
This is a fork of the original [KinectToVR project](https://github.com/KinectToVR/KinectToVR), designed to allow a LAN connection between this server and the [corresponding client](https://github.com/TonyGamer/KinectToVRShadowClient).<br>
The program is quite finicky to set up and has quite a few issues which I imght fix in the future.<br>
**Only the Kinect V1 (Kinect 360) sensor is compatible with this project at the moment**

## First time setup
- Set up the ZeroTierOne on both your local Windows machine and on the Shadow PC. You will want to note the IP address for the Shadow PC when you do this.
- Install the server on the Shadow PC
- Install the client on your local machine (This is the server, the repo for the client is linked above)

## Installing the server
- Install the original KinectToVR project on Shadow
- Download the file called ```KinectV1Process.exe``` in the releases section
- Replace the corresponding file, which can be found in ```C:/K2EX```

## Use
- Launch SteamVR on Shadow and connect your headset
- Launch the KinectToVR server (KinectV1Process.exe on Shadow)
- Launch the KinectToVR client (KinectV1Process.exe on your local machine)
- Type in Shadow's IP in the command prompt which opens on your local machine

If all has gone well, the GUI on the KinectToVR server should appear, at which point it should, for the most part, behave like a normal instance of KinectToVR
If this is not the case, you will need to restart the server and try again

## Notes
- I highly recommend putting Virtual Desktop on both your local machine **and** Shadow. This makes the initial setup process much easier in my opinion
- If the KinectToVR Server stops responding, you will need to restart the server and reconnect to it manually on the KinectToVR client
- If the hip lagging behind bothers you, I recommend setting up owoTrack. The tracking quality from it is much better than the Kinect, and it helps mask the latency from the Kinect
- Some times the tracking will stop working altogether when you leave sight of the Kinect for an extended period of time. If this happens then you must restart both the Server and Client processes
- There is a noticable amount of latency, but from what I can tell it is mostly from the Kinect sensor itself, not the networking aspect added by this program

If you have any issues with this program, feel free to create an issue on the repository and/or send me a message on Discord at TonyGamer#7947