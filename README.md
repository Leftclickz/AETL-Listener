# AETL-Listener
Created by __*Dayton Heywood*__ for CeeCam Corporation

## Description

AETL-Listener is a program that works alongside **AETL-Generator** (the Master) to watch for Adobe After Effects project files and feed them into the Adobe After Effects renderer. 

This program is intended to be run alongside many other computers also running this program watching a _'hot folder'_ where AETL-Generator will be generating said project files. 

There are built-in safety checks to ensure Listeners don't grab the same project as well as the possibility that a Listener attempts to grab an incomplete project file. 

With multiple computers running AETL-Listener and a singular master running AETL-Generator every week, you can reliably expect video footage to update every week with new data without any user input whatsoever.

## Requirements

	Adobe After Effects CC
	AETL-Generator highly recommended

## How To Use

AETL-Listener has 2 arguments that need to be satisfied to run and has no other settings.

 	-i {input folder}. This is where your hot folder is located. This should be wherever AETL-Generator is spitting out projects.
 	-ae {adobe render exe}. This must be an absolute path to aerender.exe (this is an executable located in your Adober After Effects directory)
  
 
## Usage

AETL-Listener is intended to run silently in the background and uses almost no CPU when not actively processing project files. 

When AETL-Listener finds a project file it can work with, it will "lock" it, preventing other Listeners from accessing it. The lock will be timestamped for archiving purposes.

