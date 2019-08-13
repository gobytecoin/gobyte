
Debian
====================
This directory contains files used to package gobyted/gobyte-qt
for Debian-based Linux systems. If you compile gobyted/gobyte-qt yourself, there are some useful files here.

## gobyte: URI support ##


gobyte-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install gobyte-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your gobyte-qt binary to `/usr/bin`
and the `../../share/pixmaps/gobyte128.png` to `/usr/share/pixmaps`

gobyte-qt.protocol (KDE)

