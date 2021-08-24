# AniGen
A tool which automates generation of sprite based animations.

Starting with a set of completed animations, it works by constructing a mapping from one animation frame to the others.
To do this, it searches for each pixel in the target frames the pixel in the source frame which is most similar.
Then, this *transfer map* consisting of the coordinates of the source color for each pixel can be used to generate animations from the same animation source frame of a different object.

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
To use it, we first create a new map `Normal_Run.map` from a few reference sprites.
```sh
$ AniGen create -i "Sprites/FullyAnimated/Template_Muscular_Black/Male_Skin_Muscular_Black_Combat_Hit.png" -t  "Sprites/FullyAnimated/Template_Muscular_Black/Male_Skin_Muscular_Black_Normal_Run.png" -i "Sprites/FullyAnimated/Template_Muscular_BrownLight/Male_Skin_BrownLight_Combat_Hit.png" -t "Sprites/FullyAnimated/Template_Muscular_BrownLight/Male_Skin_BrownLight_Normal_Run.png" -o "Normal_Run.map" -m 8
```

We use two (input,target) pairs here for demonstration purposes only. With the default similarity measure this will produce the same result as a single sample because of ties during the comparisons and only an odd number of samples should be used.
After generation is finished, we can use the map to generate new animation sprites by providing another reference image.
```sh
$ AniGen apply -i "Sprites/FullyAnimated/Template_Muscular_White/Male_Skin_White_Combat_Hit.png" -t "Normal_Run.map" -o "Template_Muscular_White"
```

### Similarity
The similarity argument currently takes a string with the syntax
```
-s type_axb
-s type a x b m11 m21 ...; m21 m22 ...; ... // new format on head
```
where *type* is either `equality` or `blur` and (*a*,*b*) is the size of a centered kernel.

### Zone Map
A zone map is pair of frames which restricts the search space for each pixel to the source region which shares the same color.
This improves map generation performance and provides an avenue to manually tweak details of the map.
Regular sprites can serve as a zone map as long as all the colors in the target frame exist in the source frame. 

<img src="/Sprites/Demo/Mask_Combat_Hit.png"  width="320" height="320"><img src="/Sprites/Demo/Mask_Block_1H_Rtrn.png"  width="320" height="320">