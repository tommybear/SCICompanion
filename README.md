# SCICompanion
SCI Companion - a complete IDE for Sierra SCI games (SCI0 to SCI1.1)

Official website:
http://scicompanion.com

General notes:
The bulk of the code is in SCICompanionLib\Src

SCICompanion is the .exe which is just a thin wrapper over SCICompanionLib

Fork of SCI Companion X, itself a fork of SCI Companion. 

Features
========

- Compiles on Visual Studio 2022
- Exports room Visual, Priority and Control BMPs out at the same time as separate bitmaps when extracting all resources.
- Can export any resource as a pure <resource_name>.raw file


## Clean-Room Rewrite (Preview)
A cross-platform .NET MAUI proof-of-concept lives under `Companion.Maui/`. See `docs/M0-Scope.md` for goals and `docs/SETUP.md` for build instructions during the Milestone 0 phase.
