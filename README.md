# AniGen
A tool which automates generation of sprite based animations.

Starting with a set of completed animations, it works by constructing a mapping from one animation frame to the others.
To do this, it searches for each pixel in the target frames the pixel in the source frame which is the most similar.
Then, this *transfer map* consisting of the coordinates of the source color for each pixel can be used to generate animations from the same animation source frame of a different object.

## Build
The dependencies are
* [args](https://github.com/Taywee/args)
* [SFML](https://www.sfml-dev.org/index.php)

You can either install them such that cmake's `find_package` can find them or use the provided submodules to build them automatically.
Then just open a command line at the target location and run
```sh
$ git clone https://gitlab.com/Just2D/anigen.git --recursive
$ cd anigen
$ mkdir build
$ cd build
$ cmake ..
```
The recursive clone is only necessary if you want to use the submodules.

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
-s "type a x b m11 m21 ...; m21 m22 ...; ..."
```
where *type* is one of `equality`, `blur`, `equalityrotinv`, `minequality`, `minblur`, `minequalityrotinv`  and (*a*,*b*) is the size of a centered kernel.
It follows an optional list of weights for the search kernel, with each row delimited by a `;`.

### Zone Map
A zone map is pair of frames which restricts the search space for each pixel to the source region which shares the same color.
This improves map generation performance and provides an avenue to manually tweak details of the map.
Regular sprites can serve as a zone map as long as all the colors in the target frame exist in the source frame. 

<img src="/Sprites/Demo/Mask_Combat_Hit.png"  width="320" height="320"><img src="/Sprites/Demo/Mask_Block_1H_Rtrn.png"  width="320" height="320">

### Tuning
If the results are unsatisfactory there are multiple parameters to adjust.
1. Increase the number of samples for the generation. Varied inputs reduce the ambiguity of the color matches and should therefore increase the accuracy of the map. For example, after manually tweaking a generated sprite, this sprite contains new information useful for the map generation. The following table illustratetes that while combinations with few samples can work well for specific cases, more inputs are indeed generally better.

|    inputs    |  0-error  |  1-error   |
|------------|-----------|------------|
|     1      | 0.0196875 | 0.0028125  |
|     1      |   0.0175  |  0.00375   |
|     2      | 0.0179687 | 0.00265625 |
| 3 template |  0.018125 | 0.00296875 |
|     3      | 0.0179687 |   0.0025   |
|     4      | 0.0179687 | 0.00265625 |
|     5      | 0.0179687 | 0.00171875 |

2. Add more details to the zone map. Instead of tweaking every single generated sprite, prominent wrongly mapped pixels can be forced to the correct color by adding small zones around them.
3. Use a different similarity measure. Results can differ vastly based on the method and kernel used. Experiments indicate that equality with evenly distributed kernel weights work well and the default is choosen in this manner to provide a compromise between speed and quality on different data. In the following table, the last entry is the result of an optimization via a genetic algorithm which demonstrates that better kernels may be possible but it should be treated with care since it depends on the test samples.

|                                   similarity_measure                    |  0-error  |  1-error   |
|-------------------------------------------------------------------------|-----------|------------|
|                            identity 1 x 1 1;                            |  0.023125 | 0.00765625 |
|                   equality 3 x 3 1 1 1; 1 1 1; 1 1 1;                   |  0.018125 |  0.001875  |
|                   equality 3 x 3 1 1 1; 1 3 1; 1 1 1;                   | 0.0179687 | 0.00234375 |
|                   equality 3 x 3 1 1 1; 1 11 1; 1 1 1;                  | 0.0190625 |  0.00375   |
|                   equality 3 x 3 0 1 0; 1 1 1; 0 1 0;                   | 0.0189063 | 0.00265625 |
|                   equality 3 x 3 1 0 1; 0 1 0; 1 0 1;                   | 0.0178125 | 0.00234375 |
|                     blur 3 x 3 1 1 1; 1 1 1; 1 1 1;                     | 0.0228125 | 0.0071875  |
|                     blur 3 x 3 1 1 1; 1 11 1; 1 1 1;                    |  0.020625 | 0.00453125 |
|                     blur 3 x 3 1 0 1; 0 1 0; 1 0 1;                     | 0.0214063 |  0.004375  |
| equality 3 x 3 0.845 0.529 0.732; 0.561 0.754 0.367; 0.758 0.141 0.243; | 0.0164062 | 0.00203125 |

See `scripts/experiments.py` for more.
