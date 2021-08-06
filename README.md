# AniGen
A tool which automates generation of sprite based animations.

## Build
Install the dependencies
* [args](https://github.com/Taywee/args)
* [SFML](https://www.sfml-dev.org/index.php)

so that cmake finds them. Then just run
```sh
$ git clone https://gitlab.com/Just2D/anigen.git
$ cd AniGen
$ mkdir build
$ cd build
$ cmake ..
```

## Usage
This is a command line tool. For a basic help and an overview of available parameters run
```sh
$ AniGen -h
```
To use it, we first create a new map called "Normal_Run.txt" from a few reference sprites.
```sh
$ AniGen create -i "Sprites/FullyAnimated/Template_Muscular_Black/Male_Skin_Muscular_Black_Combat_Hit.png" -i "Sprites/FullyAnimated/Template_Muscular_White/Male_Skin_White_Combat_Hit.png" -t  "Sprites/FullyAnimated/Template_Muscular_Black/Male_Skin_Muscular_Black_Normal_Run.png" -t "Sprites/FullyAnimated/Template_Muscular_White/Male_Skin_White_Normal_Run.png" -o "Normal_Run.txt" -m 8
```
Afterwards, we can use the map to generate new animation sprites by providing another reference image.
```sh
$ AniGen apply -i "Sprites/FullyAnimated/Template_Muscular_White/Male_Skin_White_Combat_Hit.png" -t "Normal_Run.txt" -o "Template_Muscular_White"
```
