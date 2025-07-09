# Monkype

A terminal-based clone of [Monkeytype](https://github.com/monkeytypegame/monkeytype/), built in C using `ncurses`. It features a clean TUI with real-time feedback, WPM and accuracy stats, customizable wordsets, and responsive input handling. Designed for performance, portability, and minimalism, it aims to bring the Monkeytype experience to the command line.

<p align="center"> <img alt="Printscreen" src="./assets/print.png"> </p>

## Get Started

Follow these simple steps to build and run **monkype** TUI on your own device:

**1. Clone this repo**
```bash
git clone https://github.com/Raffa064/monkype.git
cd monkype
```

**2. Run install script**
```bash
sudo ./install.sh
```
> [!TIP] 
It automaticaly build and install monkype and *monkype-scripts* into /usr/local/bin/

**3. Download dataset:**
```bash
monkype-get english
```
> [!TIP] 
With this command you can download any dataset from original [monkeytype repository](https://github.com/monkeytypegame/monkeytype/blob/master/frontend/static/languages/) by passing it's name on the first argument (without ".json" extension).

**3. Run executable
```bash
monkype
```

> [!TIP]
You can use `-h` option to see all available option.

